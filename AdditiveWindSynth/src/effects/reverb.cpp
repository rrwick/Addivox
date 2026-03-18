#include "reverb.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDefaultSampleRate = 44100.0;
constexpr double kDenormalFloor = 1.0e-18;
constexpr double kBypassThreshold = 1.0e-6;
constexpr double kHadamardScale = 0.3535533905932738;
constexpr double kMixSmoothingTimeSeconds = 0.020;
constexpr double kToneSmoothingTimeSeconds = 0.060;
constexpr double kStructureSmoothingTimeSeconds = 0.180;
constexpr int kNumEarlyTaps = 8;
constexpr int kNumDiffusers = 4;
constexpr int kNumDelayLines = 8;
constexpr int kNumTailDiffusers = 2;
constexpr int kNumLateDiffuserStages = 2;

using DelayValueArray = std::array<double, kNumDelayLines>;
using StereoPair = std::array<double, 2>;

constexpr std::array<double, kNumEarlyTaps> kEarlyTapTimesMs{4.9, 7.3, 10.8, 15.1, 21.4, 29.2, 39.7, 53.6};
constexpr std::array<double, kNumEarlyTaps> kEarlyTapGainsL{0.62, 0.48, 0.34, 0.24, 0.17, 0.12, 0.09, 0.07};
constexpr std::array<double, kNumEarlyTaps> kEarlyTapGainsR{0.10, 0.15, 0.23, 0.36, 0.47, 0.31, 0.19, 0.11};
constexpr std::array<double, kNumDiffusers> kDiffuserDelayMs{4.8, 7.2, 10.9, 15.7};
constexpr std::array<double, kNumDiffusers> kDiffuserFeedbacks{0.72, 0.68, 0.63, 0.57};
constexpr std::array<double, kNumTailDiffusers> kTailDiffuserDelayMs{20.6, 31.4};
constexpr std::array<double, kNumTailDiffusers> kTailDiffuserFeedbacks{0.56, 0.48};
constexpr std::array<std::array<double, kNumDelayLines>, kNumLateDiffuserStages> kLateDiffuserDelayMs{{
  {1.7, 2.3, 3.1, 4.2, 5.4, 6.8, 8.6, 10.7},
  {2.9, 4.1, 5.3, 6.7, 8.4, 10.5, 12.8, 15.6},
}};
constexpr std::array<std::array<double, kNumDelayLines>, kNumLateDiffuserStages> kLateDiffuserFeedbacks{{
  {0.58, 0.56, 0.54, 0.52, 0.50, 0.48, 0.46, 0.44},
  {0.46, 0.44, 0.42, 0.40, 0.38, 0.36, 0.34, 0.32},
}};
constexpr std::array<double, kNumDelayLines> kDelayTimesMs{21.7, 27.3, 33.1, 39.9, 46.9, 55.7, 66.1, 78.7};
constexpr std::array<double, kNumDelayLines> kModDepthMs{0.10, 0.12, 0.14, 0.17, 0.20, 0.24, 0.28, 0.33};
constexpr std::array<double, kNumDelayLines> kModRateHz{0.11, 0.07, 0.13, 0.17, 0.09, 0.19, 0.05, 0.15};
constexpr std::array<double, kNumDelayLines> kModPhaseOffsets{
  0.00,
  0.37 * kPi,
  0.71 * kPi,
  1.13 * kPi,
  1.61 * kPi,
  0.52 * kPi,
  1.87 * kPi,
  1.29 * kPi,
};
constexpr std::array<double, kNumDelayLines> kInputSigns{1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0};
constexpr std::array<double, kNumDelayLines> kSideSigns{1.0, -1.0, 1.0, -1.0, -1.0, 1.0, -1.0, 1.0};

double FlushDenormal(double value)
{
  return std::abs(value) < kDenormalFloor ? 0.0 : value;
}

double SmoothValue(double current, double target, double coefficient)
{
  return current + (coefficient * (target - current));
}

void SmoothTowards(double& current, double target, double coefficient)
{
  current = SmoothValue(current, target, coefficient);
}

double CutoffHzToCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : kDefaultSampleRate;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

DelayValueArray Hadamard8(const DelayValueArray& input)
{
  const double a0 = input[0] + input[1];
  const double a1 = input[0] - input[1];
  const double a2 = input[2] + input[3];
  const double a3 = input[2] - input[3];
  const double a4 = input[4] + input[5];
  const double a5 = input[4] - input[5];
  const double a6 = input[6] + input[7];
  const double a7 = input[6] - input[7];

  const double b0 = a0 + a2;
  const double b1 = a1 + a3;
  const double b2 = a0 - a2;
  const double b3 = a1 - a3;
  const double b4 = a4 + a6;
  const double b5 = a5 + a7;
  const double b6 = a4 - a6;
  const double b7 = a5 - a7;

  return {
    (b0 + b4) * kHadamardScale,
    (b1 + b5) * kHadamardScale,
    (b2 + b6) * kHadamardScale,
    (b3 + b7) * kHadamardScale,
    (b0 - b4) * kHadamardScale,
    (b1 - b5) * kHadamardScale,
    (b2 - b6) * kHadamardScale,
    (b3 - b7) * kHadamardScale,
  };
}

StereoPair DecodeStereo(const DelayValueArray& values)
{
  return {
    kHadamardScale
      * (values[0] + values[1] - values[2] - values[3]
        + values[4] + values[5] - values[6] - values[7]),
    kHadamardScale
      * (values[0] - values[1] + values[2] - values[3]
        - values[4] + values[5] - values[6] + values[7]),
  };
}

template <typename DiffuserBank>
double ProcessDiffuserChain(DiffuserBank& diffusers, double input)
{
  for(auto& diffuser : diffusers)
    input = diffuser.Process(input);
  return input;
}

void AdvanceWrappedPhase(double& phase, double increment)
{
  phase += increment;
  if(phase >= (2.0 * kPi))
    phase -= (2.0 * kPi);
}
} // namespace

void effects::Reverb::DelayLine::Resize(int size)
{
  buffer.assign(static_cast<std::size_t>(std::max(2, size)), 0.0);
  writeIndex = 0;
}

void effects::Reverb::DelayLine::Clear()
{
  std::fill(buffer.begin(), buffer.end(), 0.0);
  writeIndex = 0;
}

void effects::Reverb::DelayLine::Write(double input)
{
  if(buffer.empty())
    return;

  buffer[static_cast<std::size_t>(writeIndex)] = FlushDenormal(input);
  writeIndex = (writeIndex + 1) % static_cast<int>(buffer.size());
}

double effects::Reverb::DelayLine::Read(double delaySamples) const
{
  if(buffer.size() < 2)
    return 0.0;

  const double clampedDelay = std::clamp(delaySamples, 1.0, static_cast<double>(buffer.size() - 2));
  double readPos = static_cast<double>(writeIndex) - clampedDelay;
  if(readPos < 0.0)
    readPos += static_cast<double>(buffer.size());

  const int index0 = static_cast<int>(readPos);
  const int index1 = (index0 + 1) % static_cast<int>(buffer.size());
  const double frac = readPos - static_cast<double>(index0);
  const double sample0 = buffer[static_cast<std::size_t>(index0)];
  const double sample1 = buffer[static_cast<std::size_t>(index1)];
  return sample0 + ((sample1 - sample0) * frac);
}

void effects::Reverb::OnePoleLowpass::SetCutoffHz(double sampleRate, double cutoffHz)
{
  coefficient = CutoffHzToCoefficient(sampleRate, cutoffHz);
}

void effects::Reverb::OnePoleLowpass::Clear()
{
  state = 0.0;
}

double effects::Reverb::OnePoleLowpass::Process(double input)
{
  state += coefficient * (input - state);
  state = FlushDenormal(state);
  return state;
}

void effects::Reverb::OnePoleHighpass::SetCutoffHz(double sampleRate, double cutoffHz)
{
  coefficient = CutoffHzToCoefficient(sampleRate, cutoffHz);
}

void effects::Reverb::OnePoleHighpass::Clear()
{
  lowState = 0.0;
}

double effects::Reverb::OnePoleHighpass::Process(double input)
{
  lowState += coefficient * (input - lowState);
  lowState = FlushDenormal(lowState);
  return input - lowState;
}

void effects::Reverb::AllpassDiffuser::Reset(double sampleRate, double delayMs, double feedbackValue)
{
  delaySamples = std::max(1.0, Reverb::MillisecondsToSamples(delayMs, sampleRate));
  feedback = std::clamp(feedbackValue, -0.95, 0.95);
  delay.Resize(static_cast<int>(std::ceil(delaySamples)) + 2);
  Clear();
}

void effects::Reverb::AllpassDiffuser::Clear()
{
  delay.Clear();
}

double effects::Reverb::AllpassDiffuser::Process(double input)
{
  const double delayed = delay.Read(delaySamples);
  const double output = delayed - (feedback * input);
  delay.Write(input + (feedback * output));
  return FlushDenormal(output);
}

double effects::Reverb::MillisecondsToSamples(double milliseconds, double sampleRate)
{
  return (milliseconds * sampleRate) / 1000.0;
}

void effects::Reverb::Reset(double sampleRate, int blockSize)
{
  (void) blockSize;

  constexpr double kMaxEarlyTapScale = 1.55;
  constexpr double kMaxPreDelayMs = 44.0;
  constexpr double kMaxDelayScale = 2.50;
  constexpr double kMaxModulationScale = 2.75;

  mSampleRate = sampleRate > 0.0 ? sampleRate : kDefaultSampleRate;
  mMixSmoothingCoefficient = 1.0 - std::exp(-1.0 / (kMixSmoothingTimeSeconds * mSampleRate));
  mToneSmoothingCoefficient = 1.0 - std::exp(-1.0 / (kToneSmoothingTimeSeconds * mSampleRate));
  mStructureSmoothingCoefficient = 1.0 - std::exp(-1.0 / (kStructureSmoothingTimeSeconds * mSampleRate));
  mEarlyDelay.Resize(
    static_cast<int>(std::ceil(MillisecondsToSamples((kEarlyTapTimesMs.back() * kMaxEarlyTapScale) + 2.0, mSampleRate))) + 2);
  mPreDelay.Resize(static_cast<int>(std::ceil(MillisecondsToSamples(kMaxPreDelayMs, mSampleRate))) + 2);

  for(int i = 0; i < kNumDiffusers; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    mDiffusers[index].Reset(mSampleRate, kDiffuserDelayMs[index], kDiffuserFeedbacks[index]);
  }

  for(int i = 0; i < kNumTailDiffusers; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    mTailDiffusers[index].Reset(mSampleRate, kTailDiffuserDelayMs[index], kTailDiffuserFeedbacks[index]);
  }

  for(int i = 0; i < kNumDelayLines; ++i)
  {
    const std::size_t delayIndex = static_cast<std::size_t>(i);
    for(int stage = 0; stage < kNumLateDiffuserStages; ++stage)
    {
      const std::size_t stageIndex = static_cast<std::size_t>(stage);
      mLateDiffusers[delayIndex][stageIndex].Reset(
        mSampleRate,
        kLateDiffuserDelayMs[stageIndex][delayIndex],
        kLateDiffuserFeedbacks[stageIndex][delayIndex]);
    }
  }

  for(int i = 0; i < kNumDelayLines; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    const double maxDelayMs = (kDelayTimesMs[index] * kMaxDelayScale) + (kModDepthMs[index] * kMaxModulationScale) + 2.0;
    mDelayLines[index].Resize(static_cast<int>(std::ceil(MillisecondsToSamples(maxDelayMs, mSampleRate))) + 2);
    mModPhaseIncrement[index] = (2.0 * kPi * kModRateHz[index]) / mSampleRate;
  }

  mAmount = 0.0;
  mActive = false;
  Clear();
  UpdateTargetParameters();
  SnapCurrentParametersToTargets(true);
}

void effects::Reverb::Clear()
{
  mInputLowpass.Clear();
  mInputHighpass.Clear();
  mOutputLowpassLeft.Clear();
  mOutputLowpassRight.Clear();
  mEarlyDelay.Clear();
  mPreDelay.Clear();

  for(auto& diffuser : mDiffusers)
    diffuser.Clear();

  for(auto& diffuser : mTailDiffusers)
    diffuser.Clear();

  for(auto& lateDiffuserStages : mLateDiffusers)
  {
    for(auto& lateDiffuser : lateDiffuserStages)
      lateDiffuser.Clear();
  }

  for(auto& delayLine : mDelayLines)
    delayLine.Clear();

  for(auto& filter : mLoopDampingFilters)
    filter.Clear();

  for(int i = 0; i < kNumDelayLines; ++i)
    mModPhase[static_cast<std::size_t>(i)] = kModPhaseOffsets[static_cast<std::size_t>(i)];
}

void effects::Reverb::SetAmount(double amount)
{
  const double sanitizedAmount = std::clamp(amount, 0.0, 100.0);
  mAmount = sanitizedAmount;

  if(sanitizedAmount <= kBypassThreshold)
  {
    mTargetWetMix = 0.0;
    mTargetEarlyMix = 0.0;
    mTargetLateMix = 0.0;
    mTargetEarlySideScale = 0.0;
    mTargetLateSideScale = 0.0;
    mTargetAmbientBloom = 0.0;
    return;
  }

  UpdateTargetParameters();

  if(!mActive)
  {
    Clear();
    SnapCurrentParametersToTargets(true);
    mActive = true;
  }
}

void effects::Reverb::UpdateTargetParameters()
{
  const double r = std::clamp(mAmount * 0.01, 0.0, 1.0);
  const double rSquared = r * r;
  const double bloom = std::pow(r, 10.0);
  const double wetMix = 0.63 * r;
  const double earlyFraction = 0.20 + (0.75 * std::pow(1.0 - r, 1.5));
  const double delayScale = 0.72 + (1.68 * rSquared);
  const double earlyTapScale = 0.72 + (0.30 * r) + (0.46 * rSquared);
  const double rt60Seconds = 0.2 + (0.8 * r) + (7.0 * rSquared);
  const double preDelayMs = 1.0 + (4.0 * r) + (35.0 * rSquared);
  const double inputLowpassHz = 15000.0 - (6900.0 * r);
  const double loopDampingHz = 9200.0 - (6400.0 * r);
  const double outputLowpassHz = 11000.0 - (4600.0 * r);
  const double modulationScale = 0.15 + (0.85 * r) + (1.75 * rSquared);

  mTargetEarlyMix = wetMix * earlyFraction;
  mTargetLateMix = wetMix - mTargetEarlyMix;
  mTargetWetMix = mTargetEarlyMix + mTargetLateMix;
  mTargetEarlySideScale = 0.18 - (0.09 * r);
  mTargetLateSideScale = 0.02 + (0.24 * r);
  mTargetAmbientBloom = bloom;
  mTargetPreDelaySamples = std::max(1.0, MillisecondsToSamples(preDelayMs, mSampleRate));
  mTargetInputLowpassCoefficient = CutoffHzToCoefficient(mSampleRate, inputLowpassHz);
  mTargetInputHighpassCoefficient = CutoffHzToCoefficient(mSampleRate, 160.0 + (75.0 * r));
  mTargetOutputLowpassCoefficient = CutoffHzToCoefficient(mSampleRate, outputLowpassHz);

  for(int i = 0; i < kNumEarlyTaps; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    mTargetEarlyTapSamples[index] = MillisecondsToSamples(kEarlyTapTimesMs[index] * earlyTapScale, mSampleRate);
  }

  for(int i = 0; i < kNumDelayLines; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    const double scaledDelayMs = kDelayTimesMs[index] * delayScale;
    const double lineDampingScale = 1.0 - ((0.045 + (0.01 * r)) * static_cast<double>(i));
    mTargetBaseDelaySamples[index] = MillisecondsToSamples(scaledDelayMs, mSampleRate);
    mTargetFeedbackGains[index] = std::pow(10.0, (-3.0 * scaledDelayMs * 0.001) / rt60Seconds);
    mTargetModDepthSamples[index] = MillisecondsToSamples(kModDepthMs[index] * modulationScale, mSampleRate);
    mTargetLoopDampingCoefficients[index] = CutoffHzToCoefficient(
      mSampleRate,
      std::max(1700.0, loopDampingHz * lineDampingScale));
  }
}

void effects::Reverb::SnapCurrentParametersToTargets(bool startWetAtZero)
{
  mEarlyMix = startWetAtZero ? 0.0 : mTargetEarlyMix;
  mLateMix = startWetAtZero ? 0.0 : mTargetLateMix;
  mWetMix = mEarlyMix + mLateMix;
  mEarlySideScale = startWetAtZero ? 0.0 : mTargetEarlySideScale;
  mLateSideScale = startWetAtZero ? 0.0 : mTargetLateSideScale;
  mAmbientBloom = startWetAtZero ? 0.0 : mTargetAmbientBloom;
  mPreDelaySamples = mTargetPreDelaySamples;
  mInputLowpass.coefficient = mTargetInputLowpassCoefficient;
  mInputHighpass.coefficient = mTargetInputHighpassCoefficient;
  mOutputLowpassLeft.coefficient = mTargetOutputLowpassCoefficient;
  mOutputLowpassRight.coefficient = mTargetOutputLowpassCoefficient;

  for(int i = 0; i < kNumEarlyTaps; ++i)
    mEarlyTapSamples[static_cast<std::size_t>(i)] = mTargetEarlyTapSamples[static_cast<std::size_t>(i)];

  for(int i = 0; i < kNumDelayLines; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    mBaseDelaySamples[index] = mTargetBaseDelaySamples[index];
    mFeedbackGains[index] = mTargetFeedbackGains[index];
    mModDepthSamples[index] = mTargetModDepthSamples[index];
    mLoopDampingFilters[index].coefficient = mTargetLoopDampingCoefficients[index];
  }
}

void effects::Reverb::ProcessBlock(iplug::sample** outputs, int nFrames)
{
  if(!mActive || nFrames <= 0)
    return;

  for(int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex)
  {
    SmoothTowards(mEarlyMix, mTargetEarlyMix, mMixSmoothingCoefficient);
    SmoothTowards(mLateMix, mTargetLateMix, mMixSmoothingCoefficient);
    mWetMix = mEarlyMix + mLateMix;
    SmoothTowards(mEarlySideScale, mTargetEarlySideScale, mMixSmoothingCoefficient);
    SmoothTowards(mLateSideScale, mTargetLateSideScale, mMixSmoothingCoefficient);
    SmoothTowards(mAmbientBloom, mTargetAmbientBloom, mStructureSmoothingCoefficient);
    SmoothTowards(mPreDelaySamples, mTargetPreDelaySamples, mStructureSmoothingCoefficient);
    SmoothTowards(mInputLowpass.coefficient, mTargetInputLowpassCoefficient, mToneSmoothingCoefficient);
    SmoothTowards(mInputHighpass.coefficient, mTargetInputHighpassCoefficient, mToneSmoothingCoefficient);
    SmoothTowards(mOutputLowpassLeft.coefficient, mTargetOutputLowpassCoefficient, mToneSmoothingCoefficient);
    SmoothTowards(mOutputLowpassRight.coefficient, mTargetOutputLowpassCoefficient, mToneSmoothingCoefficient);

    for(int i = 0; i < kNumEarlyTaps; ++i)
    {
      const std::size_t index = static_cast<std::size_t>(i);
      mEarlyTapSamples[index] = SmoothValue(
        mEarlyTapSamples[index],
        mTargetEarlyTapSamples[index],
        mStructureSmoothingCoefficient);
    }

    for(int i = 0; i < kNumDelayLines; ++i)
    {
      const std::size_t index = static_cast<std::size_t>(i);
      SmoothTowards(mBaseDelaySamples[index], mTargetBaseDelaySamples[index], mStructureSmoothingCoefficient);
      SmoothTowards(mFeedbackGains[index], mTargetFeedbackGains[index], mToneSmoothingCoefficient);
      SmoothTowards(mModDepthSamples[index], mTargetModDepthSamples[index], mStructureSmoothingCoefficient);
      SmoothTowards(mLoopDampingFilters[index].coefficient, mTargetLoopDampingCoefficients[index], mToneSmoothingCoefficient);
    }

    const double dryLeft = outputs[0][sampleIndex];
    const double dryRight = outputs[1][sampleIndex];
    const double mid = 0.5 * (dryLeft + dryRight);
    const double side = 0.5 * (dryLeft - dryRight);

    double conditioned = mInputLowpass.Process(mid);
    conditioned = mInputHighpass.Process(conditioned);

    double earlyLeft = mEarlySideScale * side;
    double earlyRight = -mEarlySideScale * side;
    for(int tapIndex = 0; tapIndex < kNumEarlyTaps; ++tapIndex)
    {
      const std::size_t index = static_cast<std::size_t>(tapIndex);
      const double tap = mEarlyDelay.Read(mEarlyTapSamples[index]);
      earlyLeft += kEarlyTapGainsL[index] * tap;
      earlyRight += kEarlyTapGainsR[index] * tap;
    }
    mEarlyDelay.Write(conditioned);

    const double predelayed = mPreDelay.Read(mPreDelaySamples);
    mPreDelay.Write(conditioned);

    const double diffused = ProcessDiffuserChain(mTailDiffusers, ProcessDiffuserChain(mDiffusers, predelayed));

    DelayValueArray feedbackOutputs{};
    DelayValueArray lateOutputs{};
    DelayValueArray ambientOutputs{};
    for(int i = 0; i < kNumDelayLines; ++i)
    {
      const std::size_t index = static_cast<std::size_t>(i);
      double delaySamples = mBaseDelaySamples[index];
      delaySamples += mModDepthSamples[index] * std::sin(mModPhase[index]);
      AdvanceWrappedPhase(mModPhase[index], mModPhaseIncrement[index]);

      const double delayOutput = mDelayLines[index].Read(delaySamples);
      const double dampedOutput = mLoopDampingFilters[index].Process(delayOutput);
      feedbackOutputs[index] = dampedOutput;
      lateOutputs[index] = dampedOutput;

      double ambientOutput = dampedOutput;
      if(mAmbientBloom > 1.0e-4)
      {
        for(auto& lateDiffuser : mLateDiffusers[index])
          ambientOutput = lateDiffuser.Process(ambientOutput);
      }

      ambientOutputs[index] = ambientOutput;
    }

    const auto mixedFeedback = Hadamard8(feedbackOutputs);
    const double monoInjection = 0.25 * diffused;
    const double sideInjection = mLateSideScale * side;
    for(int i = 0; i < kNumDelayLines; ++i)
    {
      const std::size_t index = static_cast<std::size_t>(i);
      const double inputInjection = (kInputSigns[index] * monoInjection) + (kSideSigns[index] * sideInjection);
      mDelayLines[index].Write(inputInjection + (mFeedbackGains[index] * mixedFeedback[index]));
    }

    const auto lateStereo = DecodeStereo(lateOutputs);
    const auto ambientStereo = DecodeStereo(ambientOutputs);
    const double ambientMix = 0.35 * mAmbientBloom * mLateMix;

    const double wetLeft = mOutputLowpassLeft.Process(
      (mEarlyMix * earlyLeft) + (mLateMix * lateStereo[0]) + (ambientMix * ambientStereo[0]));
    const double wetRight = mOutputLowpassRight.Process(
      (mEarlyMix * earlyRight) + (mLateMix * lateStereo[1]) + (ambientMix * ambientStereo[1]));

    outputs[0][sampleIndex] = static_cast<iplug::sample>(dryLeft + wetLeft);
    outputs[1][sampleIndex] = static_cast<iplug::sample>(dryRight + wetRight);
  }

  if(mTargetWetMix <= kBypassThreshold && mWetMix <= 1.0e-4)
  {
    Clear();
    mActive = false;
  }

  if(mTargetAmbientBloom <= kBypassThreshold && mAmbientBloom <= 1.0e-4)
  {
    for(auto& lateDiffuserStages : mLateDiffusers)
    {
      for(auto& lateDiffuser : lateDiffuserStages)
        lateDiffuser.Clear();
    }
  }
}

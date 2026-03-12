#include "reverb.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDenormalFloor = 1.0e-18;
constexpr double kBypassThreshold = 1.0e-6;
constexpr double kHadamardScale = 0.3535533905932738;
constexpr int kNumEarlyTaps = 8;
constexpr int kNumDiffusers = 4;
constexpr int kNumDelayLines = 8;

constexpr std::array<double, kNumEarlyTaps> kEarlyTapTimesMs{4.9, 7.3, 10.8, 15.1, 21.4, 29.2, 39.7, 53.6};
constexpr std::array<double, kNumEarlyTaps> kEarlyTapGainsL{0.62, 0.48, 0.34, 0.24, 0.17, 0.12, 0.09, 0.07};
constexpr std::array<double, kNumEarlyTaps> kEarlyTapGainsR{0.10, 0.15, 0.23, 0.36, 0.47, 0.31, 0.19, 0.11};
constexpr std::array<double, kNumDiffusers> kDiffuserDelayMs{4.8, 7.2, 10.9, 15.7};
constexpr std::array<double, kNumDiffusers> kDiffuserFeedbacks{0.72, 0.68, 0.63, 0.57};
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

double CutoffHzToCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

std::array<double, kNumDelayLines> Hadamard8(const std::array<double, kNumDelayLines>& input)
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

  constexpr double kMaxEarlyTapScale = 1.30;
  constexpr double kMaxPreDelayMs = 24.0;
  constexpr double kMaxDelayScale = 1.36;

  mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  mMixSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.020 * mSampleRate));
  mToneSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.060 * mSampleRate));
  mStructureSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.180 * mSampleRate));
  mEarlyDelay.Resize(
    static_cast<int>(std::ceil(MillisecondsToSamples((kEarlyTapTimesMs.back() * kMaxEarlyTapScale) + 2.0, mSampleRate))) + 2);
  mPreDelay.Resize(static_cast<int>(std::ceil(MillisecondsToSamples(kMaxPreDelayMs, mSampleRate))) + 2);

  for(int i = 0; i < kNumDiffusers; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    mDiffusers[index].Reset(mSampleRate, kDiffuserDelayMs[index], kDiffuserFeedbacks[index]);
  }

  for(int i = 0; i < kNumDelayLines; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    const double maxDelayMs = (kDelayTimesMs[index] * kMaxDelayScale) + kModDepthMs[index] + 2.0;
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
  const double space = std::clamp(mAmount * 0.01, 0.0, 1.0);
  const double hall = space * space;
  const double delayScale = 0.78 + (0.22 * space) + (0.36 * hall);
  const double earlyTapScale = 0.80 + (0.18 * space) + (0.32 * hall);
  const double rt60Seconds = 0.38 + (0.55 * space) + (2.10 * hall);
  const double preDelayMs = 2.0 + (3.5 * space) + (16.0 * hall);
  const double inputLowpassHz = 11000.0 - (2000.0 * space) - (1200.0 * hall);
  const double loopDampingHz = 7000.0 - (1200.0 * space) - (2600.0 * hall);
  const double outputLowpassHz = 9000.0 - (800.0 * space) - (2200.0 * hall);
  const double modulationScale = 0.18 + (0.22 * space) + (0.65 * hall);

  mTargetEarlyMix = (0.12 * space) + (0.06 * hall);
  mTargetLateMix = (0.04 * space) + (0.24 * hall);
  mTargetWetMix = mTargetEarlyMix + mTargetLateMix;
  mTargetEarlySideScale = 0.16 - (0.04 * hall);
  mTargetLateSideScale = 0.04 + (0.08 * space) + (0.02 * hall);
  mTargetPreDelaySamples = std::max(1.0, MillisecondsToSamples(preDelayMs, mSampleRate));
  mTargetInputLowpassCoefficient = CutoffHzToCoefficient(mSampleRate, inputLowpassHz);
  mTargetInputHighpassCoefficient = CutoffHzToCoefficient(mSampleRate, 180.0 + (50.0 * space));
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
    mTargetBaseDelaySamples[index] = MillisecondsToSamples(scaledDelayMs, mSampleRate);
    mTargetFeedbackGains[index] = std::pow(10.0, (-3.0 * scaledDelayMs * 0.001) / rt60Seconds);
    mTargetModDepthSamples[index] = MillisecondsToSamples(kModDepthMs[index] * modulationScale, mSampleRate);
    mTargetLoopDampingCoefficients[index] = CutoffHzToCoefficient(
      mSampleRate,
      std::max(1600.0, loopDampingHz * (1.0 - (0.06 * static_cast<double>(i)))));
  }
}

void effects::Reverb::SnapCurrentParametersToTargets(bool startWetAtZero)
{
  mEarlyMix = startWetAtZero ? 0.0 : mTargetEarlyMix;
  mLateMix = startWetAtZero ? 0.0 : mTargetLateMix;
  mWetMix = mEarlyMix + mLateMix;
  mEarlySideScale = startWetAtZero ? 0.0 : mTargetEarlySideScale;
  mLateSideScale = startWetAtZero ? 0.0 : mTargetLateSideScale;
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
    mEarlyMix = SmoothValue(mEarlyMix, mTargetEarlyMix, mMixSmoothingCoefficient);
    mLateMix = SmoothValue(mLateMix, mTargetLateMix, mMixSmoothingCoefficient);
    mWetMix = mEarlyMix + mLateMix;
    mEarlySideScale = SmoothValue(mEarlySideScale, mTargetEarlySideScale, mMixSmoothingCoefficient);
    mLateSideScale = SmoothValue(mLateSideScale, mTargetLateSideScale, mMixSmoothingCoefficient);
    mPreDelaySamples = SmoothValue(mPreDelaySamples, mTargetPreDelaySamples, mStructureSmoothingCoefficient);
    mInputLowpass.coefficient = SmoothValue(
      mInputLowpass.coefficient,
      mTargetInputLowpassCoefficient,
      mToneSmoothingCoefficient);
    mInputHighpass.coefficient = SmoothValue(
      mInputHighpass.coefficient,
      mTargetInputHighpassCoefficient,
      mToneSmoothingCoefficient);
    mOutputLowpassLeft.coefficient = SmoothValue(
      mOutputLowpassLeft.coefficient,
      mTargetOutputLowpassCoefficient,
      mToneSmoothingCoefficient);
    mOutputLowpassRight.coefficient = SmoothValue(
      mOutputLowpassRight.coefficient,
      mTargetOutputLowpassCoefficient,
      mToneSmoothingCoefficient);

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
      mBaseDelaySamples[index] = SmoothValue(
        mBaseDelaySamples[index],
        mTargetBaseDelaySamples[index],
        mStructureSmoothingCoefficient);
      mFeedbackGains[index] = SmoothValue(
        mFeedbackGains[index],
        mTargetFeedbackGains[index],
        mToneSmoothingCoefficient);
      mModDepthSamples[index] = SmoothValue(
        mModDepthSamples[index],
        mTargetModDepthSamples[index],
        mStructureSmoothingCoefficient);
      mLoopDampingFilters[index].coefficient = SmoothValue(
        mLoopDampingFilters[index].coefficient,
        mTargetLoopDampingCoefficients[index],
        mToneSmoothingCoefficient);
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

    double diffused = predelayed;
    for(auto& diffuser : mDiffusers)
      diffused = diffuser.Process(diffused);

    std::array<double, kNumDelayLines> dampedOutputs{};
    for(int i = 0; i < kNumDelayLines; ++i)
    {
      const std::size_t index = static_cast<std::size_t>(i);
      double delaySamples = mBaseDelaySamples[index];
      delaySamples += mModDepthSamples[index] * std::sin(mModPhase[index]);
      mModPhase[index] += mModPhaseIncrement[index];
      if(mModPhase[index] >= (2.0 * kPi))
        mModPhase[index] -= (2.0 * kPi);

      const double delayOutput = mDelayLines[index].Read(delaySamples);
      dampedOutputs[index] = mLoopDampingFilters[index].Process(delayOutput);
    }

    const auto mixedFeedback = Hadamard8(dampedOutputs);
    const double monoInjection = 0.25 * diffused;
    const double sideInjection = mLateSideScale * side;
    for(int i = 0; i < kNumDelayLines; ++i)
    {
      const std::size_t index = static_cast<std::size_t>(i);
      const double inputInjection = (kInputSigns[index] * monoInjection) + (kSideSigns[index] * sideInjection);
      mDelayLines[index].Write(inputInjection + (mFeedbackGains[index] * mixedFeedback[index]));
    }

    const double lateLeft = kHadamardScale
      * (dampedOutputs[0] + dampedOutputs[1] - dampedOutputs[2] - dampedOutputs[3]
        + dampedOutputs[4] + dampedOutputs[5] - dampedOutputs[6] - dampedOutputs[7]);
    const double lateRight = kHadamardScale
      * (dampedOutputs[0] - dampedOutputs[1] + dampedOutputs[2] - dampedOutputs[3]
        - dampedOutputs[4] + dampedOutputs[5] - dampedOutputs[6] + dampedOutputs[7]);

    const double wetLeft = mOutputLowpassLeft.Process((mEarlyMix * earlyLeft) + (mLateMix * lateLeft));
    const double wetRight = mOutputLowpassRight.Process((mEarlyMix * earlyRight) + (mLateMix * lateRight));

    outputs[0][sampleIndex] = static_cast<iplug::sample>(dryLeft + wetLeft);
    outputs[1][sampleIndex] = static_cast<iplug::sample>(dryRight + wetRight);
  }

  if(mTargetWetMix <= kBypassThreshold && mWetMix <= 1.0e-4)
  {
    Clear();
    mActive = false;
  }
}

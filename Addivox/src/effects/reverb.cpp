#include "reverb.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kBypassThreshold = 1.0e-6;
constexpr double kAmbientClearThreshold = 1.0e-4;
constexpr double kHadamardScale = 0.3535533905932738;
constexpr double kAmbientMixScale = 0.35;
constexpr double kMonoInjectionScale = 0.25;
constexpr double kMixSmoothingTimeSeconds = 0.020;
constexpr double kToneSmoothingTimeSeconds = 0.060;
constexpr double kStructureSmoothingTimeSeconds = 0.180;
constexpr double kInputHighpassBaseHz = 160.0;
constexpr double kInputHighpassRangeHz = 75.0;
constexpr double kMinimumLoopDampingHz = 1700.0;
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
    0.00, 0.37 * dsp::kPi, 0.71 * dsp::kPi, 1.13 * dsp::kPi, 1.61 * dsp::kPi, 0.52 * dsp::kPi, 1.87 * dsp::kPi, 1.29 * dsp::kPi,
};
constexpr std::array<double, kNumDelayLines> kInputSigns{1.0, 1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0};
constexpr std::array<double, kNumDelayLines> kSideSigns{1.0, -1.0, 1.0, -1.0, -1.0, 1.0, -1.0, 1.0};

void SmoothTowards(double& current, double target, double coefficient) { current = dsp::SmoothValue(current, target, coefficient); }

DelayValueArray Hadamard8(const DelayValueArray& input) {
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
      (b0 + b4) * kHadamardScale, (b1 + b5) * kHadamardScale, (b2 + b6) * kHadamardScale, (b3 + b7) * kHadamardScale,
      (b0 - b4) * kHadamardScale, (b1 - b5) * kHadamardScale, (b2 - b6) * kHadamardScale, (b3 - b7) * kHadamardScale,
  };
}

StereoPair DecodeStereo(const DelayValueArray& values) {
  return {
      kHadamardScale * (values[0] + values[1] - values[2] - values[3] + values[4] + values[5] - values[6] - values[7]),
      kHadamardScale * (values[0] - values[1] + values[2] - values[3] - values[4] + values[5] - values[6] + values[7]),
  };
}

template <typename DiffuserBank> double ProcessDiffuserChain(DiffuserBank& diffusers, double input) {
  for (auto& diffuser : diffusers) input = diffuser.Process(input);
  return input;
}

void AdvanceWrappedPhase(double& phase, double increment) {
  phase += increment;
  if (phase >= (2.0 * dsp::kPi)) phase -= (2.0 * dsp::kPi);
}
} // namespace

void effects::Reverb::AllpassDiffuser::Reset(double sampleRate, double delayMs, double feedbackValue) {
  delaySamples = std::max(1.0, dsp::MillisecondsToSamples(delayMs, sampleRate));
  feedback = std::clamp(feedbackValue, -0.95, 0.95);
  delay.Resize(static_cast<int>(std::ceil(delaySamples)) + 2);
  Clear();
}

void effects::Reverb::AllpassDiffuser::Clear() { delay.Clear(); }

double effects::Reverb::AllpassDiffuser::Process(double input) {
  const double delayed = delay.Read(delaySamples);
  const double output = delayed - (feedback * input);
  delay.Write(input + (feedback * output));
  return dsp::FlushDenormal(output);
}

void effects::Reverb::Reset(double sampleRate, int blockSize) {
  (void)blockSize;

  constexpr double kMaxEarlyTapScale = 1.55;
  constexpr double kMaxPreDelayMs = 44.0;
  constexpr double kMaxDelayScale = 2.50;
  constexpr double kMaxModulationScale = 2.75;

  mSampleRate = sampleRate > 0.0 ? sampleRate : dsp::kDefaultSampleRate;
  mMixSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kMixSmoothingTimeSeconds);
  mToneSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kToneSmoothingTimeSeconds);
  mStructureSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kStructureSmoothingTimeSeconds);
  mEarlyDelay.Resize(static_cast<int>(std::ceil(dsp::MillisecondsToSamples((kEarlyTapTimesMs.back() * kMaxEarlyTapScale) + 2.0, mSampleRate))) + 2);
  mPreDelay.Resize(static_cast<int>(std::ceil(dsp::MillisecondsToSamples(kMaxPreDelayMs, mSampleRate))) + 2);

  for (std::size_t i = 0; i < mDiffusers.size(); ++i)
    mDiffusers[i].Reset(mSampleRate, kDiffuserDelayMs[i], kDiffuserFeedbacks[i]);

  for (std::size_t i = 0; i < mTailDiffusers.size(); ++i)
    mTailDiffusers[i].Reset(mSampleRate, kTailDiffuserDelayMs[i], kTailDiffuserFeedbacks[i]);

  for (std::size_t i = 0; i < mLateDiffusers.size(); ++i) {
    for (std::size_t stage = 0; stage < mLateDiffusers[i].size(); ++stage)
      mLateDiffusers[i][stage].Reset(mSampleRate, kLateDiffuserDelayMs[stage][i], kLateDiffuserFeedbacks[stage][i]);
  }

  for (std::size_t i = 0; i < mDelayLines.size(); ++i) {
    const double maxDelayMs = (kDelayTimesMs[i] * kMaxDelayScale) + (kModDepthMs[i] * kMaxModulationScale) + 2.0;
    mDelayLines[i].Resize(static_cast<int>(std::ceil(dsp::MillisecondsToSamples(maxDelayMs, mSampleRate))) + 2);
    mModPhaseIncrement[i] = (2.0 * dsp::kPi * kModRateHz[i]) / mSampleRate;
  }

  mAmount = 0.0;
  mActive = false;
  Clear();
  UpdateTargetParameters();
  SnapCurrentParametersToTargets(true);
}

void effects::Reverb::Clear() {
  mHasStoredSignal = false;
  mInputLowpass.Clear();
  mInputHighpass.Clear();
  mOutputLowpassLeft.Clear();
  mOutputLowpassRight.Clear();
  mEarlyDelay.Clear();
  mPreDelay.Clear();

  for (auto& diffuser : mDiffusers) diffuser.Clear();

  for (auto& diffuser : mTailDiffusers) diffuser.Clear();

  ClearLateDiffusers();

  for (auto& delayLine : mDelayLines) delayLine.Clear();

  for (auto& filter : mLoopDampingFilters) filter.Clear();

  mModPhase = kModPhaseOffsets;
}

void effects::Reverb::SetAmount(double amount) {
  const double sanitizedAmount = std::clamp(amount, 0.0, 100.0);
  mAmount = sanitizedAmount;

  if (sanitizedAmount <= kBypassThreshold) {
    mTargetWetMix = 0.0;
    mTargetEarlyMix = 0.0;
    mTargetLateMix = 0.0;
    mTargetEarlySideScale = 0.0;
    mTargetLateSideScale = 0.0;
    mTargetAmbientBloom = 0.0;
    return;
  }

  UpdateTargetParameters();

  if (!mActive) {
    Clear();
    SnapCurrentParametersToTargets(true);
    mActive = true;
  }
}

void effects::Reverb::UpdateTargetParameters() {
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
  mTargetPreDelaySamples = std::max(1.0, dsp::MillisecondsToSamples(preDelayMs, mSampleRate));
  mTargetInputLowpassCoefficient = dsp::CutoffHzToCoefficient(mSampleRate, inputLowpassHz);
  mTargetInputHighpassCoefficient = dsp::CutoffHzToCoefficient(mSampleRate, kInputHighpassBaseHz + (kInputHighpassRangeHz * r));
  mTargetOutputLowpassCoefficient = dsp::CutoffHzToCoefficient(mSampleRate, outputLowpassHz);

  for (std::size_t i = 0; i < mTargetEarlyTapSamples.size(); ++i)
    mTargetEarlyTapSamples[i] = dsp::MillisecondsToSamples(kEarlyTapTimesMs[i] * earlyTapScale, mSampleRate);

  for (std::size_t i = 0; i < mTargetBaseDelaySamples.size(); ++i) {
    const double scaledDelayMs = kDelayTimesMs[i] * delayScale;
    const double lineDampingScale = 1.0 - ((0.045 + (0.01 * r)) * static_cast<double>(i));
    mTargetBaseDelaySamples[i] = dsp::MillisecondsToSamples(scaledDelayMs, mSampleRate);
    mTargetFeedbackGains[i] = std::pow(10.0, (-3.0 * scaledDelayMs * 0.001) / rt60Seconds);
    mTargetModDepthSamples[i] = dsp::MillisecondsToSamples(kModDepthMs[i] * modulationScale, mSampleRate);
    mTargetLoopDampingCoefficients[i] = dsp::CutoffHzToCoefficient(mSampleRate, std::max(kMinimumLoopDampingHz, loopDampingHz * lineDampingScale));
  }
}

void effects::Reverb::SnapCurrentParametersToTargets(bool startWetAtZero) {
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

  mEarlyTapSamples     = mTargetEarlyTapSamples;
  mBaseDelaySamples    = mTargetBaseDelaySamples;
  mFeedbackGains       = mTargetFeedbackGains;
  mModDepthSamples     = mTargetModDepthSamples;
  for (std::size_t i = 0; i < mLoopDampingFilters.size(); ++i)
    mLoopDampingFilters[i].coefficient = mTargetLoopDampingCoefficients[i];
}

void effects::Reverb::SmoothParameters() {
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

  for (std::size_t i = 0; i < mEarlyTapSamples.size(); ++i)
    mEarlyTapSamples[i] = dsp::SmoothValue(mEarlyTapSamples[i], mTargetEarlyTapSamples[i], mStructureSmoothingCoefficient);

  for (std::size_t i = 0; i < mBaseDelaySamples.size(); ++i) {
    SmoothTowards(mBaseDelaySamples[i], mTargetBaseDelaySamples[i], mStructureSmoothingCoefficient);
    SmoothTowards(mFeedbackGains[i], mTargetFeedbackGains[i], mToneSmoothingCoefficient);
    SmoothTowards(mModDepthSamples[i], mTargetModDepthSamples[i], mStructureSmoothingCoefficient);
    SmoothTowards(mLoopDampingFilters[i].coefficient, mTargetLoopDampingCoefficients[i], mToneSmoothingCoefficient);
  }
}

void effects::Reverb::AdvanceSilentBlock(int nFrames) {
  for (int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex) {
    SmoothParameters();

    for (std::size_t i = 0; i < mModPhase.size(); ++i) AdvanceWrappedPhase(mModPhase[i], mModPhaseIncrement[i]);
  }
}

bool effects::Reverb::HasStoredSignal() const {
  if (mInputLowpass.state != 0.0 || mInputHighpass.lowState != 0.0 || mOutputLowpassLeft.state != 0.0 || mOutputLowpassRight.state != 0.0 ||
      mEarlyDelay.HasSignal() || mPreDelay.HasSignal())
    return true;

  for (const auto& diffuser : mDiffusers) {
    if (diffuser.delay.HasSignal()) return true;
  }

  for (const auto& diffuser : mTailDiffusers) {
    if (diffuser.delay.HasSignal()) return true;
  }

  for (const auto& lateDiffuserStages : mLateDiffusers) {
    for (const auto& lateDiffuser : lateDiffuserStages) {
      if (lateDiffuser.delay.HasSignal()) return true;
    }
  }

  for (const auto& delayLine : mDelayLines) {
    if (delayLine.HasSignal()) return true;
  }

  for (const auto& filter : mLoopDampingFilters) {
    if (filter.state != 0.0) return true;
  }

  return false;
}

effects::Reverb::StereoPair effects::Reverb::ProcessEarlyReflections(double conditioned, double side) {
  StereoPair early{mEarlySideScale * side, -mEarlySideScale * side};

  for (std::size_t i = 0; i < mEarlyTapSamples.size(); ++i) {
    const double tap = mEarlyDelay.Read(mEarlyTapSamples[i]);
    early[0] += kEarlyTapGainsL[i] * tap;
    early[1] += kEarlyTapGainsR[i] * tap;
  }

  mEarlyDelay.Write(conditioned);
  return early;
}

effects::Reverb::StereoPair effects::Reverb::ProcessLateReverb(double diffused, double side) {
  DelayValueArray feedbackOutputs{};
  DelayValueArray lateOutputs{};
  DelayValueArray ambientOutputs{};

  for (std::size_t i = 0; i < mDelayLines.size(); ++i) {
    double delaySamples = mBaseDelaySamples[i];
    delaySamples += mModDepthSamples[i] * std::sin(mModPhase[i]);
    AdvanceWrappedPhase(mModPhase[i], mModPhaseIncrement[i]);

    const double delayOutput = mDelayLines[i].Read(delaySamples);
    const double dampedOutput = mLoopDampingFilters[i].Process(delayOutput);
    feedbackOutputs[i] = dampedOutput;
    lateOutputs[i] = dampedOutput;

    double ambientOutput = dampedOutput;
    if (mAmbientBloom > kAmbientClearThreshold) {
      for (auto& lateDiffuser : mLateDiffusers[i]) ambientOutput = lateDiffuser.Process(ambientOutput);
    }

    ambientOutputs[i] = ambientOutput;
  }

  const auto mixedFeedback = Hadamard8(feedbackOutputs);
  const double monoInjection = kMonoInjectionScale * diffused;
  const double sideInjection = mLateSideScale * side;
  for (std::size_t i = 0; i < mDelayLines.size(); ++i) {
    const double inputInjection = (kInputSigns[i] * monoInjection) + (kSideSigns[i] * sideInjection);
    mDelayLines[i].Write(inputInjection + (mFeedbackGains[i] * mixedFeedback[i]));
  }

  const auto lateStereo = DecodeStereo(lateOutputs);
  const auto ambientStereo = DecodeStereo(ambientOutputs);
  const double ambientMix = kAmbientMixScale * mAmbientBloom * mLateMix;
  return {
      (mLateMix * lateStereo[0]) + (ambientMix * ambientStereo[0]),
      (mLateMix * lateStereo[1]) + (ambientMix * ambientStereo[1]),
  };
}

void effects::Reverb::ClearLateDiffusers() {
  for (auto& lateDiffuserStages : mLateDiffusers) {
    for (auto& lateDiffuser : lateDiffuserStages) lateDiffuser.Clear();
  }
}

void effects::Reverb::ProcessBlock(iplug::sample** outputs, int nFrames) {
  if (!mActive || nFrames <= 0) return;

  bool inputBlockSilent = true;
  for (int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex) {
    if (outputs[0][sampleIndex] != 0.0 || outputs[1][sampleIndex] != 0.0) {
      inputBlockSilent = false;
      mHasStoredSignal = true;
      break;
    }
  }

  if (inputBlockSilent && !mHasStoredSignal) {
    AdvanceSilentBlock(nFrames);

    if (mTargetWetMix <= kBypassThreshold && mWetMix <= 1.0e-4) {
      Clear();
      mActive = false;
    }

    return;
  }

  bool wetBlockSilent = true;
  for (int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex) {
    SmoothParameters();

    const double dryLeft = outputs[0][sampleIndex];
    const double dryRight = outputs[1][sampleIndex];
    const double mid = 0.5 * (dryLeft + dryRight);
    const double side = 0.5 * (dryLeft - dryRight);

    double conditioned = mInputLowpass.Process(mid);
    conditioned = mInputHighpass.Process(conditioned);

    const auto early = ProcessEarlyReflections(conditioned, side);
    const double predelayed = mPreDelay.Read(mPreDelaySamples);
    mPreDelay.Write(conditioned);
    const double diffused = ProcessDiffuserChain(mTailDiffusers, ProcessDiffuserChain(mDiffusers, predelayed));
    const auto late = ProcessLateReverb(diffused, side);

    const double wetLeft = mOutputLowpassLeft.Process((mEarlyMix * early[0]) + late[0]);
    const double wetRight = mOutputLowpassRight.Process((mEarlyMix * early[1]) + late[1]);
    if (wetLeft != 0.0 || wetRight != 0.0) wetBlockSilent = false;

    outputs[0][sampleIndex] = static_cast<iplug::sample>(dryLeft + wetLeft);
    outputs[1][sampleIndex] = static_cast<iplug::sample>(dryRight + wetRight);
  }

  if (mTargetWetMix <= kBypassThreshold && mWetMix <= 1.0e-4) {
    Clear();
    mActive = false;
  }

  if (mTargetAmbientBloom <= kBypassThreshold && mAmbientBloom <= kAmbientClearThreshold) ClearLateDiffusers();

  if (inputBlockSilent && wetBlockSilent && !HasStoredSignal()) mHasStoredSignal = false;
}

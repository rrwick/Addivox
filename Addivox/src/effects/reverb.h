#pragma once

#include "../dsp/shared.h"
#include "IPlugConstants.h"

#include <array>

namespace effects {
class Reverb {
public:
  void Reset(double sampleRate);
  void Clear();
  void SetAmount(double amount);
  bool IsActive() const { return mActive; }
  void ProcessBlock(iplug::sample** outputs, int nFrames);

private:
  static constexpr int kNumEarlyTaps = 8;
  static constexpr int kNumInputDiffusers = 6;
  static constexpr int kNumDelayLines = 8;
  static constexpr int kNumLateDiffuserStages = 2;
  using DelayLine = dsp::DelayLine;
  using OnePoleLowpass = dsp::OnePoleLowpass;
  using OnePoleHighpass = dsp::OnePoleHighpass;

  struct AllpassDiffuser {
    void Reset(double sampleRate, double delayMs, double feedback);
    void Clear() { delay.Clear(); }
    double Process(double input);

    DelayLine delay;
    double delaySamples{1.0};
    double feedback{0.7};
  };

  using EarlyTapArray = std::array<double, kNumEarlyTaps>;
  using DelayValueArray = std::array<double, kNumDelayLines>;
  using InputDiffuserArray = std::array<AllpassDiffuser, kNumInputDiffusers>;
  using LateDiffuserStageArray = std::array<AllpassDiffuser, kNumLateDiffuserStages>;
  using LateDiffuserArray = std::array<LateDiffuserStageArray, kNumDelayLines>;
  using DelayLineArray = std::array<DelayLine, kNumDelayLines>;
  using FilterArray = std::array<OnePoleLowpass, kNumDelayLines>;
  using StereoPair = std::array<double, 2>;

  void UpdateTargetParameters(double amount);
  void InitializeCurrentParameters();
  void SmoothParameters();
  void AdvanceSilentBlock(int nFrames);
  void DeactivateIfBypassed();
  bool HasStoredSignal() const;
  StereoPair ProcessEarlyReflections(double conditioned, double side);
  StereoPair ProcessLateReverb(double diffused, double side);
  void ClearLateDiffusers();
  StereoPair ProcessWetSample(double dryLeft, double dryRight);

  double mSampleRate{dsp::kDefaultSampleRate};
  bool mActive{false};
  double mMixSmoothingCoefficient{1.0};
  double mToneSmoothingCoefficient{1.0};
  double mStructureSmoothingCoefficient{1.0};
  double mEarlyMix{0.0};
  double mLateMix{0.0};
  double mEarlySideScale{0.0};
  double mLateSideScale{0.0};
  double mAmbientBloom{0.0};
  double mPreDelaySamples{0.0};
  double mTargetEarlyMix{0.0};
  double mTargetLateMix{0.0};
  double mTargetEarlySideScale{0.0};
  double mTargetLateSideScale{0.0};
  double mTargetAmbientBloom{0.0};
  double mTargetPreDelaySamples{0.0};
  double mTargetInputLowpassCoefficient{1.0};
  double mTargetInputHighpassCoefficient{1.0};
  double mTargetOutputLowpassCoefficient{1.0};
  bool mHasStoredSignal{false};
  OnePoleLowpass mInputLowpass;
  OnePoleHighpass mInputHighpass;
  DelayLine mEarlyDelay;
  DelayLine mPreDelay;
  OnePoleLowpass mOutputLowpassLeft;
  OnePoleLowpass mOutputLowpassRight;
  EarlyTapArray mEarlyTapSamples{};
  EarlyTapArray mTargetEarlyTapSamples{};
  InputDiffuserArray mInputDiffusers{};
  LateDiffuserArray mLateDiffusers{};
  DelayLineArray mDelayLines{};
  FilterArray mLoopDampingFilters{};
  DelayValueArray mBaseDelaySamples{};
  DelayValueArray mTargetBaseDelaySamples{};
  DelayValueArray mFeedbackGains{};
  DelayValueArray mTargetFeedbackGains{};
  DelayValueArray mModDepthSamples{};
  DelayValueArray mTargetModDepthSamples{};
  DelayValueArray mModPhase{};
  DelayValueArray mModPhaseIncrement{};
  DelayValueArray mTargetLoopDampingCoefficients{};
};
} // namespace effects

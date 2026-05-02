#pragma once

#include "../dsp/shared.h"
#include "IPlugConstants.h"

#include <array>

namespace effects {
class Reverb {
public:
  void Reset(double sampleRate, int blockSize);
  void Clear();
  void SetAmount(double amount);
  bool IsActive() const { return mActive; }
  void ProcessBlock(iplug::sample** outputs, int nFrames);

private:
  static constexpr int kNumEarlyTaps = 8;
  static constexpr int kNumDiffusers = 4;
  static constexpr int kNumDelayLines = 8;
  static constexpr int kNumTailDiffusers = 2;
  static constexpr int kNumLateDiffuserStages = 2;
  using DelayLine = dsp::DelayLine;
  using OnePoleLowpass = dsp::OnePoleLowpass;
  using OnePoleHighpass = dsp::OnePoleHighpass;

  struct AllpassDiffuser {
    void Reset(double sampleRate, double delayMs, double feedback);
    void Clear();
    double Process(double input);

    DelayLine delay;
    double delaySamples{1.0};
    double feedback{0.7};
  };

  using EarlyTapArray = std::array<double, kNumEarlyTaps>;
  using DelayValueArray = std::array<double, kNumDelayLines>;
  using DiffuserArray = std::array<AllpassDiffuser, kNumDiffusers>;
  using TailDiffuserArray = std::array<AllpassDiffuser, kNumTailDiffusers>;
  using LateDiffuserStageArray = std::array<AllpassDiffuser, kNumLateDiffuserStages>;
  using LateDiffuserArray = std::array<LateDiffuserStageArray, kNumDelayLines>;
  using DelayLineArray = std::array<DelayLine, kNumDelayLines>;
  using FilterArray = std::array<OnePoleLowpass, kNumDelayLines>;
  using StereoPair = std::array<double, 2>;

  void UpdateTargetParameters();
  void SnapCurrentParametersToTargets(bool startWetAtZero);
  void SmoothParameters();
  void AdvanceSilentBlock(int nFrames);
  bool HasStoredSignal() const;
  StereoPair ProcessEarlyReflections(double conditioned, double side);
  StereoPair ProcessLateReverb(double diffused, double side);
  void ClearLateDiffusers();

  double mSampleRate{dsp::kDefaultSampleRate};
  double mAmount{0.0};
  bool mActive{false};
  double mMixSmoothingCoefficient{1.0};
  double mToneSmoothingCoefficient{1.0};
  double mStructureSmoothingCoefficient{1.0};
  double mWetMix{0.0};
  double mEarlyMix{0.0};
  double mLateMix{0.0};
  double mEarlySideScale{0.0};
  double mLateSideScale{0.0};
  double mAmbientBloom{0.0};
  double mPreDelaySamples{0.0};
  double mTargetWetMix{0.0};
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
  DiffuserArray mDiffusers{};
  TailDiffuserArray mTailDiffusers{};
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

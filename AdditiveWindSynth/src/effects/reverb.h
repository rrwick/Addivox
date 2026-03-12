#pragma once

#include "IPlugConstants.h"

#include <array>
#include <vector>

namespace effects
{
class Reverb
{
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

  struct DelayLine
  {
    void Resize(int size);
    void Clear();
    void Write(double input);
    double Read(double delaySamples) const;

    std::vector<double> buffer;
    int writeIndex{0};
  };

  struct OnePoleLowpass
  {
    void SetCutoffHz(double sampleRate, double cutoffHz);
    void Clear();
    double Process(double input);

    double coefficient{1.0};
    double state{0.0};
  };

  struct OnePoleHighpass
  {
    void SetCutoffHz(double sampleRate, double cutoffHz);
    void Clear();
    double Process(double input);

    double coefficient{1.0};
    double lowState{0.0};
  };

  struct AllpassDiffuser
  {
    void Reset(double sampleRate, double delayMs, double feedback);
    void Clear();
    double Process(double input);

    DelayLine delay;
    double delaySamples{1.0};
    double feedback{0.7};
  };

  static double MillisecondsToSamples(double milliseconds, double sampleRate);
  void UpdateTargetParameters();
  void SnapCurrentParametersToTargets(bool startWetAtZero);

  double mSampleRate{44100.0};
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
  double mPreDelaySamples{0.0};
  double mTargetWetMix{0.0};
  double mTargetEarlyMix{0.0};
  double mTargetLateMix{0.0};
  double mTargetEarlySideScale{0.0};
  double mTargetLateSideScale{0.0};
  double mTargetPreDelaySamples{0.0};
  double mTargetInputLowpassCoefficient{1.0};
  double mTargetInputHighpassCoefficient{1.0};
  double mTargetOutputLowpassCoefficient{1.0};
  OnePoleLowpass mInputLowpass;
  OnePoleHighpass mInputHighpass;
  DelayLine mEarlyDelay;
  DelayLine mPreDelay;
  OnePoleLowpass mOutputLowpassLeft;
  OnePoleLowpass mOutputLowpassRight;
  std::array<double, kNumEarlyTaps> mEarlyTapSamples{};
  std::array<double, kNumEarlyTaps> mTargetEarlyTapSamples{};
  std::array<AllpassDiffuser, kNumDiffusers> mDiffusers{};
  std::array<DelayLine, kNumDelayLines> mDelayLines{};
  std::array<OnePoleLowpass, kNumDelayLines> mLoopDampingFilters{};
  std::array<double, kNumDelayLines> mBaseDelaySamples{};
  std::array<double, kNumDelayLines> mTargetBaseDelaySamples{};
  std::array<double, kNumDelayLines> mFeedbackGains{};
  std::array<double, kNumDelayLines> mTargetFeedbackGains{};
  std::array<double, kNumDelayLines> mModDepthSamples{};
  std::array<double, kNumDelayLines> mTargetModDepthSamples{};
  std::array<double, kNumDelayLines> mModPhase{};
  std::array<double, kNumDelayLines> mModPhaseIncrement{};
  std::array<double, kNumDelayLines> mTargetLoopDampingCoefficients{};
};
} // namespace effects

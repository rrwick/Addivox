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
  void UpdateParameters();

  double mSampleRate{44100.0};
  double mAmount{0.0};
  bool mActive{false};
  double mWetMix{0.0};
  double mEarlyMix{0.0};
  double mLateMix{0.0};
  double mEarlySideScale{0.0};
  double mLateSideScale{0.0};
  double mPreDelaySamples{0.0};
  OnePoleLowpass mInputLowpass;
  OnePoleHighpass mInputHighpass;
  DelayLine mEarlyDelay;
  DelayLine mPreDelay;
  OnePoleLowpass mOutputLowpassLeft;
  OnePoleLowpass mOutputLowpassRight;
  std::array<double, kNumEarlyTaps> mEarlyTapSamples{};
  std::array<AllpassDiffuser, kNumDiffusers> mDiffusers{};
  std::array<DelayLine, kNumDelayLines> mDelayLines{};
  std::array<OnePoleLowpass, kNumDelayLines> mLoopDampingFilters{};
  std::array<double, kNumDelayLines> mBaseDelaySamples{};
  std::array<double, kNumDelayLines> mFeedbackGains{};
  std::array<double, kNumDelayLines> mModDepthSamples{};
  std::array<double, kNumDelayLines> mModPhase{};
  std::array<double, kNumDelayLines> mModPhaseIncrement{};
};
} // namespace effects

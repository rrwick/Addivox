#pragma once

#include "IPlugConstants.h"

#include <array>

namespace effects
{
class Tone
{
public:
  void Reset(double sampleRate, int blockSize);
  void Clear();
  void SetAmount(double amount);
  bool IsActive() const { return mActive; }
  void ProcessBlock(iplug::sample** outputs, int nFrames);

private:
  static constexpr int kNumChannels = 2;

  struct OnePoleLowpass
  {
    void Clear();
    double Process(double input);

    double coefficient{1.0};
    double state{0.0};
  };

  struct ChannelState
  {
    OnePoleLowpass lowpass;
  };

  struct Parameters
  {
    double lowGain{1.0};
    double highGain{1.0};
    double trim{1.0};
    double lowpassCoefficient{1.0};
  };

  static double FlushDenormal(double value);
  static double SmoothValue(double current, double target, double coefficient);
  static double CutoffHzToCoefficient(double sampleRate, double cutoffHz);
  static double DbToLinear(double decibels);

  Parameters ComputeParameters(double amount) const;

  double mSampleRate{44100.0};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  double mTargetActiveMix{0.0};
  double mCurrentActiveMix{0.0};
  double mAmountSmoothingCoefficient{1.0};
  double mActivationSmoothingCoefficient{1.0};
  bool mActive{false};
  std::array<ChannelState, kNumChannels> mChannels{};
};
} // namespace effects

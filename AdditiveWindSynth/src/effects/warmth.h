#pragma once

#include "IPlugConstants.h"

#include <array>
#include <cstddef>

namespace effects
{
class Warmth
{
public:
  void Reset(double sampleRate, int blockSize);
  void Clear();
  void SetAmount(double amount);
  bool IsActive() const { return mActive; }
  void ProcessBlock(iplug::sample** outputs, int nFrames);

private:
  static constexpr int kNumChannels = 2;
  static constexpr int kOversamplingFactor = 2;

  struct OnePoleLowpass
  {
    void Clear();
    double Process(double input);

    double coefficient{1.0};
    double state{0.0};
  };

  struct DCBlocker
  {
    void Clear();
    double Process(double input);

    double coefficient{0.995};
    double previousInput{0.0};
    double previousOutput{0.0};
  };

  struct ChannelState
  {
    double previousInput{0.0};
    DCBlocker inputDcBlocker;
    DCBlocker outputDcBlocker;
    OnePoleLowpass toneFilter;
  };

  struct Parameters
  {
    double blend{0.0};
    double drive{1.0};
    double bias{0.0};
    double trim{1.0};
    double toneCoefficient{1.0};
  };

  static double FlushDenormal(double value);
  static double SmoothValue(double current, double target, double coefficient);
  static double CutoffHzToCoefficient(double sampleRate, double cutoffHz);
  static double DCBlockerCoefficient(double sampleRate, double cutoffHz);

  Parameters ComputeParameters(double amount) const;
  double ProcessOversampledSample(std::size_t channelIndex, double input, const Parameters& parameters);

  double mSampleRate{44100.0};
  double mOversampledRate{44100.0 * kOversamplingFactor};
  double mAmountSmoothingCoefficient{1.0};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  bool mActive{false};
  std::array<ChannelState, kNumChannels> mChannels{};
};
} // namespace effects

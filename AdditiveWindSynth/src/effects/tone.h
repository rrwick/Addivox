#pragma once

#include "IPlugConstants.h"

#include <array>
#include <complex>

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
  static constexpr int kNumCrossovers = 8;
  static constexpr int kNumBands = kNumCrossovers + 1;
  static constexpr int kTrimTableSize = 257;

  struct OnePoleLowpass
  {
    void Clear();
    double Process(double input);

    double coefficient{1.0};
    double state{0.0};
  };

  struct ChannelState
  {
    std::array<OnePoleLowpass, kNumCrossovers> crossovers;
  };

  struct Parameters
  {
    std::array<double, kNumBands> bandGains{};
    double trim{1.0};
  };

  static double FlushDenormal(double value);
  static double SmoothValue(double current, double target, double coefficient);
  static double ExponentialSmoothingCoefficient(double sampleRate, double timeSeconds);
  static double CutoffHzToCoefficient(double sampleRate, double cutoffHz);
  static double DbToLinear(double decibels);
  static std::complex<double> EvaluateLowpassResponse(double coefficient, double angularFrequency);

  Parameters ComputeParameters(double amount) const;
  double ComputeTrimForAmount(double amount) const;
  double LookupTrim(double amount) const;
  void UpdateCrossoverCoefficients();
  void UpdateTrimTable();
  static double ProcessTiltedSample(ChannelState& channel, double input, const Parameters& parameters);

  double mSampleRate{44100.0};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  double mTargetActiveMix{0.0};
  double mCurrentActiveMix{0.0};
  double mAmountSmoothingCoefficient{1.0};
  double mActivationSmoothingCoefficient{1.0};
  bool mActive{false};
  std::array<double, kNumCrossovers> mCrossoverCoefficients{};
  std::array<double, kTrimTableSize> mTrimTable{};
  std::array<ChannelState, kNumChannels> mChannels{};
};
} // namespace effects

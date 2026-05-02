#pragma once

#include "../dsp/shared.h"
#include "IPlugConstants.h"

#include <array>
#include <complex>

namespace effects {
class Tone {
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
  using BandGains = std::array<double, kNumBands>;
  using OnePoleLowpass = dsp::OnePoleLowpass;

  struct ChannelState {
    std::array<OnePoleLowpass, kNumCrossovers> crossovers;
  };

  struct Parameters {
    BandGains bandGains{};
    double trim{1.0};
  };

  static double ShapeAmount(double amount);
  static double DbToLinear(double decibels);
  static std::complex<double> EvaluateLowpassResponse(double coefficient, double angularFrequency);
  static BandGains ComputeBandGains(double amount);

  Parameters ComputeParameters(double amount) const;
  void AdvanceSilentBlock(int nFrames);
  bool HasStoredSignal() const;
  double ComputeTrimForAmount(double amount) const;
  std::complex<double> EvaluateTiltResponse(const BandGains& bandGains, double angularFrequency) const;
  double LookupTrim(double amount) const;
  void UpdateCrossoverCoefficients();
  void UpdateTrimTable();
  static double ProcessTiltedSample(ChannelState& channel, double input, const Parameters& parameters);

  double mSampleRate{dsp::kDefaultSampleRate};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  double mTargetActiveMix{0.0};
  double mCurrentActiveMix{0.0};
  double mAmountSmoothingCoefficient{1.0};
  double mActivationSmoothingCoefficient{1.0};
  bool mActive{false};
  bool mHasStoredSignal{false};
  std::array<double, kNumCrossovers> mCrossoverCoefficients{};
  std::array<double, kTrimTableSize> mTrimTable{};
  std::array<ChannelState, kNumChannels> mChannels{};
};
} // namespace effects

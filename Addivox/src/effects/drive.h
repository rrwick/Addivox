#pragma once

#include "../dsp/shared.h"
#include "IPlugConstants.h"

#include <array>
#include <cassert>
#include <cstddef>

#include "HIIR/FPUDownsampler2x.h"
#include "HIIR/FPUUpsampler2x.h"

namespace effects {
class Drive {
public:
  void Reset(double sampleRate, int blockSize);
  void Clear();
  void SetAmount(double amount);
  bool IsActive() const { return mActive; }
  void ProcessBlock(iplug::sample** outputs, int nFrames);

private:
  static constexpr double kDefaultSampleRate = 44100.0;
  static constexpr int kNumChannels = 2;
  static constexpr int kOversamplingFactor = 4;
  static constexpr int kFirstStageNumCoefs = 12;
  static constexpr int kSecondStageNumCoefs = 4;
  using OnePoleLowpass = dsp::OnePoleLowpass;

  struct DCBlocker {
    void Clear();
    double Process(double input);

    double coefficient{0.995};
    double previousInput{0.0};
    double previousOutput{0.0};
  };

  struct ChannelState {
    DCBlocker inputDcBlocker;
    DCBlocker outputDcBlocker;
    OnePoleLowpass toneFilter;
    hiir::Upsampler2xFPU<kFirstStageNumCoefs, double> upsampler2x;
    hiir::Upsampler2xFPU<kSecondStageNumCoefs, double> upsampler4x;
    hiir::Downsampler2xFPU<kSecondStageNumCoefs, double> downsampler4x;
    hiir::Downsampler2xFPU<kFirstStageNumCoefs, double> downsampler2x;
    double previousShaperInput{0.0};
    bool shaperStateInitialized{false};
  };

  struct Parameters {
    double blend{0.0};
    double drive{1.0};
    double bias{0.0};
    double biasOffset{0.0};
    double inverseDrive{1.0};
    double trim{1.0};
    double toneCoefficient{1.0};
  };

  static double DCBlockerCoefficient(double sampleRate, double cutoffHz);
  static double StableLogCosh(double value);

  Parameters ComputeParameters(double amount) const;
  void AdvanceSilentBlock(int nFrames);
  static double EvaluateShaper(double input, const Parameters& parameters);
  static double EvaluateShaperAntiderivative(double input, const Parameters& parameters);
  static double EvaluateShaperAdaa(ChannelState& channel, double input, const Parameters& parameters);
  static double ProcessOversampledSample(ChannelState& channel, double input, const Parameters& parameters);

  double mOversampledRate{kDefaultSampleRate * kOversamplingFactor};
  double mAmountSmoothingCoefficient{1.0};
  double mActivationSmoothingCoefficient{1.0};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  double mTargetActiveMix{0.0};
  double mCurrentActiveMix{0.0};
  bool mActive{false};
  bool mHasStoredSignal{false};
  std::array<ChannelState, kNumChannels> mChannels{};
};
} // namespace effects

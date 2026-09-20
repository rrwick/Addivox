#pragma once

#include "../dsp/gradient_noise.h"
#include "../dsp/shared.h"
#include "IPlugConstants.h"

#include <array>

namespace effects {
class Chorus {
public:
  void Reset(double sampleRate);
  void Clear();
  void SetAmount(double amount);
  bool IsActive() const { return mActive; }
  void ProcessBlock(iplug::sample** outputs, int nFrames);

private:
  static constexpr int kNumVoices = 8;
  static constexpr double kWetToneCutoffHz = 10000.0;
  using DelayLine = dsp::DelayLine;
  using OnePoleLowpass = dsp::OnePoleLowpass;
  using OnePoleHighpass = dsp::OnePoleHighpass;

  struct VoiceState {
    DelayLine delay;
    OnePoleLowpass toneFilter;
    double modPosition{0.0};
    dsp::CachedNoise1D noiseCache;
  };

  struct VoiceParameters {
    double scaleLeft{0.0};
    double scaleRight{0.0};
    double phaseIncrement{0.0};
    double baseDelaySamples{0.0};
  };

  struct Parameters {
    double dryMix{1.0};
    double wetMix{0.0};
    double depthSamples{0.0};
    std::array<VoiceParameters, kNumVoices> voices{};
  };

  Parameters ComputeParameters(double amount) const;
  std::array<double, 2> ProcessWetSample(double monoInput, const Parameters& parameters);
  void AdvanceSilentBlock(int nFrames);
  void DeactivateIfBypassed();
  bool HasStoredSignal() const;

  double mSampleRate{dsp::kDefaultSampleRate};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  bool mActive{false};
  bool mHasStoredSignal{false};
  double mAmountSmoothingCoefficient{1.0};
  double mToneSmoothingCoefficient{1.0};
  double mWetToneCoefficient{dsp::CutoffHzToCoefficient(mSampleRate, kWetToneCutoffHz)};
  OnePoleHighpass mInputHighpass;
  std::array<VoiceState, kNumVoices> mVoices{};
};
} // namespace effects

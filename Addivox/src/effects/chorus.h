#pragma once

#include "../dsp/shared.h"
#include "IPlugConstants.h"

#include <array>
#include <cstdint>

namespace effects {
class Chorus {
public:
  void Reset(double sampleRate, int blockSize);
  void Clear();
  void SetAmount(double amount);
  bool IsActive() const { return mActive; }
  void ProcessBlock(iplug::sample** outputs, int nFrames);

private:
  static constexpr int kNumVoices = 8;
  using DelayLine = dsp::DelayLine;
  using OnePoleLowpass = dsp::OnePoleLowpass;
  using OnePoleHighpass = dsp::OnePoleHighpass;

  struct VoiceState {
    DelayLine delay;
    OnePoleLowpass toneFilter;
    double modPosition{0.0};
    uint32_t modSeed{0u};
    int modLattice{-1};
    double modGradient0{0.0};
    double modGradient1{0.0};
  };

  void InitializeVoiceStates();
  void AdvanceSilentBlock(int nFrames);
  bool HasStoredSignal() const;

  double mSampleRate{dsp::kDefaultSampleRate};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  bool mActive{false};
  bool mHasStoredSignal{false};
  double mAmountSmoothingCoefficient{1.0};
  double mToneSmoothingCoefficient{1.0};
  OnePoleHighpass mInputHighpass;
  std::array<VoiceState, kNumVoices> mVoices{};
};
} // namespace effects

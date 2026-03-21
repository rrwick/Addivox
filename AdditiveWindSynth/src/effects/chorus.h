#pragma once

#include "IPlugConstants.h"
#include "shared.h"

#include <array>
#include <cstdint>

namespace effects
{
class Chorus
{
public:
  void Reset(double sampleRate, int blockSize);
  void Clear();
  void SetAmount(double amount);
  bool IsActive() const { return mActive; }
  void ProcessBlock(iplug::sample** outputs, int nFrames);

private:
  static constexpr int kNumVoices = 8;
  using DelayLine = shared::DelayLine;
  using OnePoleLowpass = shared::OnePoleLowpass;
  using OnePoleHighpass = shared::OnePoleHighpass;

  struct VoiceState
  {
    DelayLine delay;
    OnePoleLowpass toneFilter;
    double modPosition{0.0};
    uint32_t modSeed{0u};
  };

  void InitializeVoiceStates();

  double mSampleRate{shared::kDefaultSampleRate};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  bool mActive{false};
  double mAmountSmoothingCoefficient{1.0};
  double mToneSmoothingCoefficient{1.0};
  OnePoleHighpass mInputHighpass;
  std::array<VoiceState, kNumVoices> mVoices{};
};
} // namespace effects

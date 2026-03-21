#pragma once

#include "IPlugConstants.h"

#include <array>
#include <cstdint>
#include <vector>

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
    void Clear();
    double Process(double input);

    double coefficient{1.0};
    double state{0.0};
  };

  struct OnePoleHighpass
  {
    void Clear();
    double Process(double input);

    double coefficient{1.0};
    double lowState{0.0};
  };

  struct VoiceState
  {
    DelayLine delay;
    OnePoleLowpass toneFilter;
    double modPosition{0.0};
    uint32_t modSeed{0u};
  };

  void InitializeVoiceStates();

  double mSampleRate{44100.0};
  double mTargetAmount{0.0};
  double mCurrentAmount{0.0};
  bool mActive{false};
  double mAmountSmoothingCoefficient{1.0};
  double mToneSmoothingCoefficient{1.0};
  OnePoleHighpass mInputHighpass;
  std::array<VoiceState, kNumVoices> mVoices{};
};
} // namespace effects

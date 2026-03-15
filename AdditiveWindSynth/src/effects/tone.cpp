#include "tone.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDenormalFloor = 1.0e-18;
constexpr double kBypassThreshold = 1.0e-6;
} // namespace

double effects::Tone::FlushDenormal(double value)
{
  return std::abs(value) < kDenormalFloor ? 0.0 : value;
}

double effects::Tone::SmoothValue(double current, double target, double coefficient)
{
  return current + (coefficient * (target - current));
}

double effects::Tone::CutoffHzToCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 20.0, 0.45 * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

double effects::Tone::DbToLinear(double decibels)
{
  return std::pow(10.0, decibels / 20.0);
}

void effects::Tone::OnePoleLowpass::Clear()
{
  state = 0.0;
}

double effects::Tone::OnePoleLowpass::Process(double input)
{
  state += coefficient * (input - state);
  state = Tone::FlushDenormal(state);
  return state;
}

void effects::Tone::Reset(double sampleRate, int blockSize)
{
  (void) blockSize;

  mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  mAmountSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.024 * mSampleRate));
  mActivationSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.012 * mSampleRate));
  mTargetAmount = 0.0;
  mCurrentAmount = 0.0;
  mTargetActiveMix = 0.0;
  mCurrentActiveMix = 0.0;
  mActive = false;
  Clear();
}

void effects::Tone::Clear()
{
  const double defaultCoefficient = CutoffHzToCoefficient(mSampleRate, 1300.0);

  for(auto& channel : mChannels)
  {
    channel.lowpass.coefficient = defaultCoefficient;
    channel.lowpass.Clear();
  }
}

void effects::Tone::SetAmount(double amount)
{
  mTargetAmount = std::clamp(amount, -100.0, 100.0);
  mTargetActiveMix = std::abs(mTargetAmount) > kBypassThreshold ? 1.0 : 0.0;

  if(std::abs(mTargetAmount) > kBypassThreshold && !mActive)
  {
    Clear();
    mCurrentAmount = 0.0;
    mCurrentActiveMix = 0.0;
    mActive = true;
  }
}

effects::Tone::Parameters effects::Tone::ComputeParameters(double amount) const
{
  const double t = std::clamp(amount * 0.01, -1.0, 1.0);
  const double magnitude = std::abs(t);
  const double presence = std::sqrt(magnitude);
  const double shaped = std::copysign((0.2 * magnitude) + (0.8 * presence), t);
  const double tiltDb = 10.0 * shaped;

  Parameters parameters;
  parameters.lowGain = DbToLinear(-0.5 * tiltDb);
  parameters.highGain = DbToLinear(0.5 * tiltDb);
  parameters.trim = DbToLinear(-0.15 * std::abs(tiltDb));
  parameters.lowpassCoefficient = CutoffHzToCoefficient(mSampleRate, 1300.0 - (250.0 * shaped));
  return parameters;
}

void effects::Tone::ProcessBlock(iplug::sample** outputs, int nFrames)
{
  if(!mActive || nFrames <= 0)
    return;

  for(int frame = 0; frame < nFrames; ++frame)
  {
    mCurrentAmount = SmoothValue(mCurrentAmount, mTargetAmount, mAmountSmoothingCoefficient);
    mCurrentActiveMix = SmoothValue(mCurrentActiveMix, mTargetActiveMix, mActivationSmoothingCoefficient);
    const Parameters parameters = ComputeParameters(mCurrentAmount);
    const double activeMix = mCurrentActiveMix;

    for(int channelIndex = 0; channelIndex < kNumChannels; ++channelIndex)
    {
      ChannelState& channel = mChannels[static_cast<std::size_t>(channelIndex)];
      channel.lowpass.coefficient = parameters.lowpassCoefficient;

      const double input = outputs[channelIndex][frame];
      const double low = channel.lowpass.Process(input);
      const double high = input - low;
      const double toned =
        parameters.trim * ((parameters.lowGain * low) + (parameters.highGain * high));
      const double output = input + (activeMix * (toned - input));
      outputs[channelIndex][frame] = static_cast<iplug::sample>(FlushDenormal(output));
    }
  }

  if(std::abs(mTargetAmount) <= kBypassThreshold
     && std::abs(mCurrentAmount) <= kBypassThreshold
     && mCurrentActiveMix <= kBypassThreshold)
  {
    mCurrentAmount = 0.0;
    mCurrentActiveMix = 0.0;
    mTargetActiveMix = 0.0;
    mActive = false;
    Clear();
  }
}

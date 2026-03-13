#include "warmth.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDenormalFloor = 1.0e-18;
constexpr double kBypassThreshold = 1.0e-6;
} // namespace

double effects::Warmth::FlushDenormal(double value)
{
  return std::abs(value) < kDenormalFloor ? 0.0 : value;
}

double effects::Warmth::SmoothValue(double current, double target, double coefficient)
{
  return current + (coefficient * (target - current));
}

double effects::Warmth::CutoffHzToCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

double effects::Warmth::DCBlockerCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  return std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

void effects::Warmth::OnePoleLowpass::Clear()
{
  state = 0.0;
}

double effects::Warmth::OnePoleLowpass::Process(double input)
{
  state += coefficient * (input - state);
  state = Warmth::FlushDenormal(state);
  return state;
}

void effects::Warmth::DCBlocker::Clear()
{
  previousInput = 0.0;
  previousOutput = 0.0;
}

double effects::Warmth::DCBlocker::Process(double input)
{
  const double output = (input - previousInput) + (coefficient * previousOutput);
  previousInput = input;
  previousOutput = Warmth::FlushDenormal(output);
  return previousOutput;
}

void effects::Warmth::Reset(double sampleRate, int blockSize)
{
  (void) blockSize;

  mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  mOversampledRate = mSampleRate * static_cast<double>(kOversamplingFactor);
  mAmountSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.028 * mSampleRate));
  mTargetAmount = 0.0;
  mCurrentAmount = 0.0;
  mActive = false;
  Clear();
}

void effects::Warmth::Clear()
{
  for(auto& channel : mChannels)
  {
    channel.previousInput = 0.0;
    channel.inputDcBlocker.coefficient = DCBlockerCoefficient(mOversampledRate, 12.0);
    channel.inputDcBlocker.Clear();
    channel.outputDcBlocker.coefficient = DCBlockerCoefficient(mOversampledRate, 8.0);
    channel.outputDcBlocker.Clear();
    channel.toneFilter.coefficient = CutoffHzToCoefficient(mOversampledRate, 18000.0);
    channel.toneFilter.Clear();
  }
}

void effects::Warmth::SetAmount(double amount)
{
  mTargetAmount = std::clamp(amount, 0.0, 100.0);

  if(mTargetAmount > kBypassThreshold && !mActive)
  {
    Clear();
    mCurrentAmount = 0.0;
    mActive = true;
  }
}

effects::Warmth::Parameters effects::Warmth::ComputeParameters(double amount) const
{
  const double t = std::clamp(amount * 0.01, 0.0, 1.0);
  const double driveShape = t * t;

  Parameters parameters;
  parameters.blend = (0.20 * t) + (0.80 * driveShape);
  parameters.drive = 1.0 + (16.0 * driveShape);
  parameters.bias = 0.04 * driveShape * (0.35 + (0.65 * t));
  parameters.trim = std::pow(parameters.drive, -0.32) * (1.0 - (0.08 * t));
  parameters.toneCoefficient = CutoffHzToCoefficient(
    mOversampledRate,
    19000.0 - (12500.0 * std::pow(t, 1.15)));
  return parameters;
}

double effects::Warmth::ProcessOversampledSample(std::size_t channelIndex,
                                                 double input,
                                                 const Parameters& parameters)
{
  ChannelState& channel = mChannels[channelIndex];
  channel.toneFilter.coefficient = parameters.toneCoefficient;

  const double clean = input;
  const double filteredInput = channel.inputDcBlocker.Process(input);
  const double biasOffset = std::tanh(parameters.drive * parameters.bias);
  const double saturated = std::tanh(parameters.drive * (filteredInput + parameters.bias)) - biasOffset;
  const double softened = channel.toneFilter.Process(saturated);
  const double trimmed = channel.outputDcBlocker.Process(softened * parameters.trim);
  const double mixed = clean + (parameters.blend * (trimmed - clean));
  return FlushDenormal(mixed);
}

void effects::Warmth::ProcessBlock(iplug::sample** outputs, int nFrames)
{
  if(!mActive || nFrames <= 0)
    return;

  for(int frame = 0; frame < nFrames; ++frame)
  {
    mCurrentAmount = SmoothValue(mCurrentAmount, mTargetAmount, mAmountSmoothingCoefficient);
    const Parameters parameters = ComputeParameters(mCurrentAmount);

    for(int channelIndex = 0; channelIndex < kNumChannels; ++channelIndex)
    {
      ChannelState& channel = mChannels[static_cast<std::size_t>(channelIndex)];
      const double input = outputs[channelIndex][frame];
      const double midpoint = 0.5 * (channel.previousInput + input);
      const double subSample0 = ProcessOversampledSample(static_cast<std::size_t>(channelIndex), midpoint, parameters);
      const double subSample1 = ProcessOversampledSample(static_cast<std::size_t>(channelIndex), input, parameters);
      outputs[channelIndex][frame] = static_cast<iplug::sample>(0.5 * (subSample0 + subSample1));
      channel.previousInput = input;
    }
  }

  if(mTargetAmount <= kBypassThreshold && mCurrentAmount <= kBypassThreshold)
  {
    mCurrentAmount = 0.0;
    mActive = false;
    Clear();
  }
}

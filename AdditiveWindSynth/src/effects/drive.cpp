#include "drive.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDenormalFloor = 1.0e-18;
constexpr double kBypassThreshold = 1.0e-6;
constexpr double kAdaaDeltaThreshold = 1.0e-4;
constexpr double kLogTwo = 0.69314718055994530942;
constexpr std::array<double, 12> kOversamplingCoefs2x{
  0.036681502163648017,
  0.13654762463195794,
  0.27463175937945444,
  0.42313861743656711,
  0.56109869787919531,
  0.67754004997416184,
  0.76974183386322703,
  0.83988962484963892,
  0.89226081800387902,
  0.9315419599631839,
  0.96209454837808417,
  0.98781637073289585,
};
constexpr std::array<double, 4> kOversamplingCoefs4x{
  0.041893991997656171,
  0.16890348243995201,
  0.39056077292116603,
  0.74389574826847926,
};
} // namespace

double effects::Drive::FlushDenormal(double value)
{
  return std::abs(value) < kDenormalFloor ? 0.0 : value;
}

double effects::Drive::SmoothValue(double current, double target, double coefficient)
{
  return current + (coefficient * (target - current));
}

double effects::Drive::CutoffHzToCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

double effects::Drive::DCBlockerCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  return std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

double effects::Drive::StableLogCosh(double value)
{
  const double magnitude = std::abs(value);
  return magnitude + std::log1p(std::exp(-2.0 * magnitude)) - kLogTwo;
}

void effects::Drive::OnePoleLowpass::Clear()
{
  state = 0.0;
}

double effects::Drive::OnePoleLowpass::Process(double input)
{
  state += coefficient * (input - state);
  state = Drive::FlushDenormal(state);
  return state;
}

void effects::Drive::DCBlocker::Clear()
{
  previousInput = 0.0;
  previousOutput = 0.0;
}

double effects::Drive::DCBlocker::Process(double input)
{
  const double output = (input - previousInput) + (coefficient * previousOutput);
  previousInput = input;
  previousOutput = Drive::FlushDenormal(output);
  return previousOutput;
}

void effects::Drive::Reset(double sampleRate, int blockSize)
{
  (void) blockSize;

  mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  mOversampledRate = mSampleRate * static_cast<double>(kOversamplingFactor);
  mAmountSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.028 * mSampleRate));
  mTargetAmount = 0.0;
  mCurrentAmount = 0.0;
  mActive = false;

  for(auto& channel : mChannels)
  {
    channel.upsampler2x.set_coefs(kOversamplingCoefs2x.data());
    channel.upsampler4x.set_coefs(kOversamplingCoefs4x.data());
    channel.downsampler4x.set_coefs(kOversamplingCoefs4x.data());
    channel.downsampler2x.set_coefs(kOversamplingCoefs2x.data());
  }

  Clear();
}

void effects::Drive::Clear()
{
  for(auto& channel : mChannels)
  {
    channel.inputDcBlocker.coefficient = DCBlockerCoefficient(mOversampledRate, 12.0);
    channel.inputDcBlocker.Clear();
    channel.outputDcBlocker.coefficient = DCBlockerCoefficient(mOversampledRate, 8.0);
    channel.outputDcBlocker.Clear();
    channel.toneFilter.coefficient = CutoffHzToCoefficient(mOversampledRate, 18000.0);
    channel.toneFilter.Clear();
    channel.upsampler2x.clear_buffers();
    channel.upsampler4x.clear_buffers();
    channel.downsampler4x.clear_buffers();
    channel.downsampler2x.clear_buffers();
    channel.previousShaperInput = 0.0;
    channel.shaperStateInitialized = false;
  }
}

void effects::Drive::SetAmount(double amount)
{
  mTargetAmount = std::clamp(amount, 0.0, 100.0);

  if(mTargetAmount > kBypassThreshold && !mActive)
  {
    Clear();
    mCurrentAmount = 0.0;
    mActive = true;
  }
}

effects::Drive::Parameters effects::Drive::ComputeParameters(double amount) const
{
  const double t = std::clamp(amount * 0.01, 0.0, 1.0);
  const double presence = std::sqrt(t);
  const double driveShape = t * t;
  const double density = (0.35 * presence) + (0.65 * driveShape);

  Parameters parameters;
  parameters.blend = (0.18 * presence) + (0.74 * t);
  parameters.drive = 1.0 + (26.0 * density);
  parameters.bias = 0.07 * density * (0.25 + (0.75 * t));
  parameters.biasOffset = std::tanh(parameters.drive * parameters.bias);
  parameters.inverseDrive = 1.0 / parameters.drive;
  parameters.trim = std::pow(parameters.drive, -0.27) * (1.0 - (0.05 * t));
  parameters.toneCoefficient = CutoffHzToCoefficient(
    mOversampledRate,
    19000.0 - (9000.0 * presence) - (5000.0 * driveShape));
  return parameters;
}

double effects::Drive::EvaluateShaper(double input, const Parameters& parameters)
{
  return std::tanh(parameters.drive * (input + parameters.bias)) - parameters.biasOffset;
}

double effects::Drive::EvaluateShaperAntiderivative(double input, const Parameters& parameters)
{
  const double drivenInput = parameters.drive * (input + parameters.bias);
  return (StableLogCosh(drivenInput) * parameters.inverseDrive) - (parameters.biasOffset * input);
}

double effects::Drive::ProcessOversampledSample(std::size_t channelIndex,
                                                 double input,
                                                 const Parameters& parameters)
{
  ChannelState& channel = mChannels[channelIndex];
  channel.toneFilter.coefficient = parameters.toneCoefficient;

  const double clean = input;
  const double filteredInput = channel.inputDcBlocker.Process(input);
  double saturated = 0.0;

  if(!channel.shaperStateInitialized)
  {
    saturated = EvaluateShaper(filteredInput, parameters);
    channel.shaperStateInitialized = true;
  }
  else
  {
    const double delta = filteredInput - channel.previousShaperInput;

    if(std::abs(delta) > kAdaaDeltaThreshold)
    {
      const double antiderivative = EvaluateShaperAntiderivative(filteredInput, parameters);
      const double previousAntiderivative = EvaluateShaperAntiderivative(channel.previousShaperInput, parameters);
      saturated = (antiderivative - previousAntiderivative) / delta;
    }
    else
    {
      const double midpointInput = 0.5 * (filteredInput + channel.previousShaperInput);
      saturated = EvaluateShaper(midpointInput, parameters);
    }
  }

  channel.previousShaperInput = filteredInput;
  const double softened = channel.toneFilter.Process(saturated);
  const double trimmed = channel.outputDcBlocker.Process(softened * parameters.trim);
  const double mixed = clean + (parameters.blend * (trimmed - clean));
  return FlushDenormal(mixed);
}

void effects::Drive::ProcessBlock(iplug::sample** outputs, int nFrames)
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
      std::array<double, 2> up2xSamples{};
      std::array<double, 4> up4xSamples{};
      std::array<double, 2> down2xSamples{};

      channel.upsampler2x.process_sample(up2xSamples[0], up2xSamples[1], input);
      channel.upsampler4x.process_sample(up4xSamples[0], up4xSamples[1], up2xSamples[0]);
      channel.upsampler4x.process_sample(up4xSamples[2], up4xSamples[3], up2xSamples[1]);

      for(double& oversampledSample : up4xSamples)
      {
        oversampledSample = ProcessOversampledSample(static_cast<std::size_t>(channelIndex), oversampledSample, parameters);
      }

      down2xSamples[0] = channel.downsampler4x.process_sample(up4xSamples.data());
      down2xSamples[1] = channel.downsampler4x.process_sample(up4xSamples.data() + 2);
      outputs[channelIndex][frame] = static_cast<iplug::sample>(channel.downsampler2x.process_sample(down2xSamples.data()));
    }
  }

  if(mTargetAmount <= kBypassThreshold && mCurrentAmount <= kBypassThreshold)
  {
    mCurrentAmount = 0.0;
    mActive = false;
    Clear();
  }
}

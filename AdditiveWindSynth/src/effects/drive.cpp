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
constexpr double kMinimumCutoffHz = 1.0;
constexpr double kMaximumCutoffScale = 0.49;
constexpr double kAmountSmoothingTimeSeconds = 0.028;
constexpr double kActivationSmoothingTimeSeconds = 0.012;
constexpr double kInputDcCutoffHz = 12.0;
constexpr double kOutputDcCutoffHz = 8.0;
constexpr double kInitialToneCutoffHz = 18000.0;
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

double effects::Drive::ExponentialSmoothingCoefficient(double sampleRate, double timeSeconds)
{
  return 1.0 - std::exp(-1.0 / (sampleRate * timeSeconds));
}

double effects::Drive::CutoffHzToCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : kDefaultSampleRate;
  const double safeCutoff = std::clamp(cutoffHz, kMinimumCutoffHz, kMaximumCutoffScale * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

double effects::Drive::DCBlockerCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : kDefaultSampleRate;
  const double safeCutoff = std::clamp(cutoffHz, kMinimumCutoffHz, kMaximumCutoffScale * safeSampleRate);
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

  const double baseSampleRate = sampleRate > 0.0 ? sampleRate : kDefaultSampleRate;

  mOversampledRate = baseSampleRate * static_cast<double>(kOversamplingFactor);
  mAmountSmoothingCoefficient = ExponentialSmoothingCoefficient(baseSampleRate, kAmountSmoothingTimeSeconds);
  mActivationSmoothingCoefficient = ExponentialSmoothingCoefficient(baseSampleRate, kActivationSmoothingTimeSeconds);
  mTargetAmount = 0.0;
  mCurrentAmount = 0.0;
  mTargetActiveMix = 0.0;
  mCurrentActiveMix = 0.0;
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
    channel.inputDcBlocker.coefficient = DCBlockerCoefficient(mOversampledRate, kInputDcCutoffHz);
    channel.inputDcBlocker.Clear();
    channel.outputDcBlocker.coefficient = DCBlockerCoefficient(mOversampledRate, kOutputDcCutoffHz);
    channel.outputDcBlocker.Clear();
    channel.toneFilter.coefficient = CutoffHzToCoefficient(mOversampledRate, kInitialToneCutoffHz);
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
  mTargetActiveMix = mTargetAmount > kBypassThreshold ? 1.0 : 0.0;

  if(mTargetAmount > kBypassThreshold && !mActive)
  {
    Clear();
    mCurrentAmount = 0.0;
    mCurrentActiveMix = 0.0;
    mActive = true;
  }
}

effects::Drive::Parameters effects::Drive::ComputeParameters(double amount) const
{
  const double t = std::clamp(amount * 0.01, 0.0, 1.0);  // convert from [0, 100] to [0, 1]
  const double tSquared = t * t;
  const double density = (0.35 * t) + (0.65 * tSquared);
  const double drive = 1.0 + (26.0 * density);
  const double bias = 0.1 * tSquared;

  Parameters parameters;
  parameters.blend = t;
  parameters.drive = drive;
  parameters.bias = bias;
  parameters.biasOffset = std::tanh(drive * bias);
  parameters.inverseDrive = 1.0 / drive;
  parameters.trim = std::pow(drive, -0.20);
  parameters.toneCoefficient = CutoffHzToCoefficient(mOversampledRate, 19000.0 - (14000.0 * t));
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

double effects::Drive::EvaluateShaperAdaa(ChannelState& channel, double input, const Parameters& parameters)
{
  double saturated = 0.0;

  if(!channel.shaperStateInitialized)
  {
    channel.shaperStateInitialized = true;
    saturated = EvaluateShaper(input, parameters);
  }
  else
  {
    const double previousInput = channel.previousShaperInput;
    const double delta = input - previousInput;

    if(std::abs(delta) <= kAdaaDeltaThreshold)
    {
      saturated = EvaluateShaper(0.5 * (input + previousInput), parameters);
    }
    else
    {
      saturated =
        (EvaluateShaperAntiderivative(input, parameters) - EvaluateShaperAntiderivative(previousInput, parameters))
        / delta;
    }
  }

  channel.previousShaperInput = input;
  return saturated;
}

double effects::Drive::ProcessOversampledSample(std::size_t channelIndex, double input, const Parameters& parameters)
{
  ChannelState& channel = mChannels[channelIndex];
  channel.toneFilter.coefficient = parameters.toneCoefficient;

  const double filteredInput = channel.inputDcBlocker.Process(input);
  const double saturated = EvaluateShaperAdaa(channel, filteredInput, parameters);
  const double softened = channel.toneFilter.Process(saturated);
  const double processed = channel.outputDcBlocker.Process(softened * parameters.trim);
  return FlushDenormal(input + (parameters.blend * (processed - input)));
}

void effects::Drive::ProcessBlock(iplug::sample** outputs, int nFrames)
{
  if(!mActive || nFrames <= 0)
    return;

  for(int frame = 0; frame < nFrames; ++frame)
  {
    mCurrentAmount = SmoothValue(mCurrentAmount, mTargetAmount, mAmountSmoothingCoefficient);
    mCurrentActiveMix = SmoothValue(mCurrentActiveMix, mTargetActiveMix, mActivationSmoothingCoefficient);
    const Parameters parameters = ComputeParameters(mCurrentAmount);
    const double activeMix = mCurrentActiveMix;

    for(std::size_t channelIndex = 0; channelIndex < mChannels.size(); ++channelIndex)
    {
      ChannelState& channel = mChannels[channelIndex];
      const double input = outputs[channelIndex][frame];
      std::array<double, 2> upsampled2x{};
      std::array<double, 4> upsampled4x{};
      std::array<double, 2> downsampled2x{};

      channel.upsampler2x.process_sample(upsampled2x[0], upsampled2x[1], input);
      channel.upsampler4x.process_sample(upsampled4x[0], upsampled4x[1], upsampled2x[0]);
      channel.upsampler4x.process_sample(upsampled4x[2], upsampled4x[3], upsampled2x[1]);

      for(double& oversampledSample : upsampled4x)
      {
        oversampledSample = ProcessOversampledSample(channelIndex, oversampledSample, parameters);
      }

      downsampled2x[0] = channel.downsampler4x.process_sample(upsampled4x.data());
      downsampled2x[1] = channel.downsampler4x.process_sample(upsampled4x.data() + 2);
      const double processed = channel.downsampler2x.process_sample(downsampled2x.data());
      const double output = input + (activeMix * (processed - input));
      outputs[channelIndex][frame] = static_cast<iplug::sample>(FlushDenormal(output));
    }
  }

  if(mTargetAmount <= kBypassThreshold
     && mCurrentAmount <= kBypassThreshold
     && mCurrentActiveMix <= kBypassThreshold)
  {
    mCurrentAmount = 0.0;
    mCurrentActiveMix = 0.0;
    mTargetActiveMix = 0.0;
    mActive = false;
    Clear();
  }
}

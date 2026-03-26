#include "tone.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kBypassThreshold = 1.0e-6;
constexpr double kAmountSmoothingTimeSeconds = 0.024;
constexpr double kActivationSmoothingTimeSeconds = 0.012;
constexpr double kMaxSlopeDbPerOctave = 12.0;
constexpr double kMinimumResponseMagnitude = 1.0e-9;
constexpr double kTrimPeakBlend = 0.5;
constexpr int kTrimProbeCount = 256;
constexpr double kTrimProbeMinimumFrequencyHz = 20.0;
constexpr double kCrossoverMaximumScale = 0.45;
constexpr std::array<double, 8> kCrossoverFrequenciesHz{
  80.0, 160.0, 320.0, 640.0, 1280.0, 2560.0, 5120.0, 10240.0,
};
// One octave-wide bands around the pivot let the knob apply a true dB/octave tilt.
constexpr std::array<double, 9> kBandOctaveOffsets{
  -4.5,
  -3.5,
  -2.5,
  -1.5,
  -0.5,
  0.5,
  1.5,
  2.5,
  3.5,
};
} // namespace

double effects::Tone::ShapeAmount(double amount)
{
  const double clampedAmount = std::clamp(amount, -1.0, 1.0);
  return 2.0 * clampedAmount * std::abs(clampedAmount);
}

double effects::Tone::DbToLinear(double decibels)
{
  return std::pow(10.0, decibels / 20.0);
}

std::complex<double> effects::Tone::EvaluateLowpassResponse(double coefficient, double angularFrequency)
{
  const double pole = 1.0 - coefficient;
  const std::complex<double> unitDelay = std::polar(1.0, -angularFrequency);
  return coefficient / (1.0 - (pole * unitDelay));
}

effects::Tone::BandGains effects::Tone::ComputeBandGains(double amount)
{
  BandGains bandGains{};
  const double slopeDbPerOctave = kMaxSlopeDbPerOctave * ShapeAmount(amount);

  for(std::size_t index = 0; index < bandGains.size(); ++index)
  {
    bandGains[index] = DbToLinear(slopeDbPerOctave * kBandOctaveOffsets[index]);
  }

  return bandGains;
}

void effects::Tone::Reset(double sampleRate, int blockSize)
{
  (void) blockSize;

  mSampleRate = sampleRate > 0.0 ? sampleRate : dsp::kDefaultSampleRate;
  mAmountSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kAmountSmoothingTimeSeconds);
  mActivationSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kActivationSmoothingTimeSeconds);
  mTargetAmount = 0.0;
  mCurrentAmount = 0.0;
  mTargetActiveMix = 0.0;
  mCurrentActiveMix = 0.0;
  mActive = false;
  UpdateCrossoverCoefficients();
  UpdateTrimTable();
  Clear();
}

void effects::Tone::Clear()
{
  for(auto& channel : mChannels)
  {
    for(std::size_t index = 0; index < channel.crossovers.size(); ++index)
    {
      channel.crossovers[index].coefficient = mCrossoverCoefficients[index];
      channel.crossovers[index].Clear();
    }
  }
}

void effects::Tone::SetAmount(double amount)
{
  mTargetAmount = std::clamp(amount, -1.0, 1.0);
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
  const double clampedAmount = std::clamp(amount, -1.0, 1.0);
  Parameters parameters;
  parameters.bandGains = ComputeBandGains(clampedAmount);
  parameters.trim = LookupTrim(clampedAmount);
  return parameters;
}

double effects::Tone::ComputeTrimForAmount(double amount) const
{
  const BandGains bandGains = ComputeBandGains(amount);
  const double maxFrequencyHz = 0.5 * mSampleRate;
  double magnitudeSquareSum = 0.0;
  double peakMagnitude = 0.0;

  for(int probeIndex = 0; probeIndex < kTrimProbeCount; ++probeIndex)
  {
    const double frequencyT = (static_cast<double>(probeIndex) + 0.5) / static_cast<double>(kTrimProbeCount);
    const double probeFrequencyHz =
      kTrimProbeMinimumFrequencyHz * std::pow(maxFrequencyHz / kTrimProbeMinimumFrequencyHz, frequencyT);
    const double angularFrequency = (2.0 * dsp::kPi * probeFrequencyHz) / mSampleRate;
    const std::complex<double> response = EvaluateTiltResponse(bandGains, angularFrequency);
    const double magnitude = std::abs(response);
    magnitudeSquareSum += magnitude * magnitude;
    peakMagnitude = std::max(peakMagnitude, magnitude);
  }

  const double rmsMagnitude = std::sqrt(magnitudeSquareSum / static_cast<double>(kTrimProbeCount));
  const double rmsTrim = 1.0 / std::max(rmsMagnitude, kMinimumResponseMagnitude);
  const double peakTrim = 1.0 / std::max(peakMagnitude, kMinimumResponseMagnitude);
  // Blend average and worst-case normalization in log space to reduce overloads without neutering the effect.
  return std::pow(rmsTrim, 1.0 - kTrimPeakBlend) * std::pow(peakTrim, kTrimPeakBlend);
}

std::complex<double> effects::Tone::EvaluateTiltResponse(const BandGains& bandGains,
                                                         double angularFrequency) const
{
  // Each crossover contributes the difference between neighboring band gains, so the sum reconstructs the full tilt.
  std::complex<double> response = bandGains.back();

  for(std::size_t index = 0; index < mCrossoverCoefficients.size(); ++index)
  {
    response += (bandGains[index] - bandGains[index + 1]) * EvaluateLowpassResponse(mCrossoverCoefficients[index], angularFrequency);
  }

  return response;
}

double effects::Tone::LookupTrim(double amount) const
{
  const double tablePosition =
    0.5 * (std::clamp(amount, -1.0, 1.0) + 1.0) * static_cast<double>(kTrimTableSize - 1);
  const std::size_t index = static_cast<std::size_t>(tablePosition);
  const std::size_t nextIndex = std::min(index + 1, mTrimTable.size() - 1);
  const double fraction = tablePosition - static_cast<double>(index);
  return mTrimTable[index] + (fraction * (mTrimTable[nextIndex] - mTrimTable[index]));
}

void effects::Tone::UpdateCrossoverCoefficients()
{
  for(std::size_t index = 0; index < mCrossoverCoefficients.size(); ++index)
  {
    mCrossoverCoefficients[index] =
      dsp::CutoffHzToCoefficient(
        mSampleRate,
        kCrossoverFrequenciesHz[index],
        kTrimProbeMinimumFrequencyHz,
        kCrossoverMaximumScale);
  }
}

void effects::Tone::UpdateTrimTable()
{
  for(std::size_t index = 0; index < mTrimTable.size(); ++index)
  {
    const double amount = -1.0 + (2.0 * static_cast<double>(index) / static_cast<double>(mTrimTable.size() - 1));
    mTrimTable[index] = ComputeTrimForAmount(amount);
  }
}

double effects::Tone::ProcessTiltedSample(ChannelState& channel, double input, const Parameters& parameters)
{
  double tilted = 0.0;
  double previousLow = 0.0;

  for(std::size_t index = 0; index < channel.crossovers.size(); ++index)
  {
    const double currentLow = channel.crossovers[index].Process(input);
    const double band = currentLow - previousLow;
    tilted += parameters.bandGains[index] * band;
    previousLow = currentLow;
  }

  tilted += parameters.bandGains.back() * (input - previousLow);
  return dsp::FlushDenormal(parameters.trim * tilted);
}

void effects::Tone::ProcessBlock(iplug::sample** outputs, int nFrames)
{
  if(!mActive || nFrames <= 0)
    return;

  for(int frame = 0; frame < nFrames; ++frame)
  {
    mCurrentAmount = dsp::SmoothValue(mCurrentAmount, mTargetAmount, mAmountSmoothingCoefficient);
    mCurrentActiveMix = dsp::SmoothValue(mCurrentActiveMix, mTargetActiveMix, mActivationSmoothingCoefficient);
    const Parameters parameters = ComputeParameters(mCurrentAmount);
    const double activeMix = mCurrentActiveMix;

    for(std::size_t channelIndex = 0; channelIndex < mChannels.size(); ++channelIndex)
    {
      ChannelState& channel = mChannels[channelIndex];
      const double input = outputs[channelIndex][frame];
      const double toned = ProcessTiltedSample(channel, input, parameters);
      const double output = input + (activeMix * (toned - input));
      outputs[channelIndex][frame] = static_cast<iplug::sample>(dsp::FlushDenormal(output));
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

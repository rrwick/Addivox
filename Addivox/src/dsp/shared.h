#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace dsp
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDefaultSampleRate = 44100.0;
constexpr double kDenormalFloor = 1.0e-18;

inline double MillisecondsToSamples(double milliseconds, double sampleRate)
{
  return (milliseconds * sampleRate) / 1000.0;
}

inline double FlushDenormal(double value)
{
  return std::abs(value) < kDenormalFloor ? 0.0 : value;
}

inline double SmoothValue(double current, double target, double coefficient)
{
  return current + (coefficient * (target - current));
}

inline double ExponentialSmoothingCoefficient(double sampleRate, double timeSeconds)
{
  if(sampleRate <= 0.0 || timeSeconds <= 0.0)
    return 1.0;

  return 1.0 - std::exp(-1.0 / (sampleRate * timeSeconds));
}

inline double CutoffHzToCoefficient(double sampleRate,
                                    double cutoffHz,
                                    double minimumCutoffHz = 1.0,
                                    double maximumCutoffScale = 0.49,
                                    double defaultSampleRate = kDefaultSampleRate)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : defaultSampleRate;
  const double safeCutoff = std::clamp(cutoffHz, minimumCutoffHz, maximumCutoffScale * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

namespace detail
{
constexpr int kPanGainLookupTableSize = 2049;
constexpr double kPanGainLookupScale = 0.5 * static_cast<double>(kPanGainLookupTableSize - 1);

inline const std::array<std::array<float, 2>, kPanGainLookupTableSize>& PanGainLookupTable()
{
  static const auto table = []()
  {
    std::array<std::array<float, 2>, kPanGainLookupTableSize> gains{};
    for(int index = 0; index < kPanGainLookupTableSize; ++index)
    {
      const double pan = (static_cast<double>(index) / static_cast<double>(kPanGainLookupTableSize - 1)) * 2.0 - 1.0;
      const double angle = (pan + 1.0) * (kPi * 0.25);
      gains[static_cast<std::size_t>(index)] = {
        static_cast<float>(std::cos(angle)),
        static_cast<float>(std::sin(angle))};
    }
    return gains;
  }();
  return table;
}
} // namespace detail

inline void PanToGains(double pan, double& leftGain, double& rightGain)
{
  const double clampedPan = std::clamp(pan, -1.0, 1.0);
  const double lookupPosition = (clampedPan + 1.0) * detail::kPanGainLookupScale;
  const int index0 = static_cast<int>(lookupPosition);
  const int index1 = std::min(index0 + 1, detail::kPanGainLookupTableSize - 1);
  const double frac = lookupPosition - static_cast<double>(index0);
  const auto& table = detail::PanGainLookupTable();
  const auto& gain0 = table[static_cast<std::size_t>(index0)];
  const auto& gain1 = table[static_cast<std::size_t>(index1)];
  leftGain = static_cast<double>(gain0[0]) + ((static_cast<double>(gain1[0]) - static_cast<double>(gain0[0])) * frac);
  rightGain = static_cast<double>(gain0[1]) + ((static_cast<double>(gain1[1]) - static_cast<double>(gain0[1])) * frac);
}

inline std::array<double, 2> PanToGains(double pan)
{
  double leftGain = 0.0;
  double rightGain = 0.0;
  PanToGains(pan, leftGain, rightGain);
  return {leftGain, rightGain};
}

struct DelayLine
{
  void Resize(int size)
  {
    buffer.assign(static_cast<std::size_t>(std::max(2, size)), 0.0);
    writeIndex = 0;
  }

  void Clear()
  {
    std::fill(buffer.begin(), buffer.end(), 0.0);
    writeIndex = 0;
  }

  void Write(double input)
  {
    if(buffer.empty())
      return;

    buffer[static_cast<std::size_t>(writeIndex)] = FlushDenormal(input);
    writeIndex = (writeIndex + 1) % static_cast<int>(buffer.size());
  }

  double Read(double delaySamples) const
  {
    if(buffer.size() < 2)
      return 0.0;

    const double clampedDelay = std::clamp(delaySamples, 1.0, static_cast<double>(buffer.size() - 2));
    double readPos = static_cast<double>(writeIndex) - clampedDelay;
    if(readPos < 0.0)
      readPos += static_cast<double>(buffer.size());

    const int index0 = static_cast<int>(readPos);
    const int index1 = (index0 + 1) % static_cast<int>(buffer.size());
    const double frac = readPos - static_cast<double>(index0);
    const double sample0 = buffer[static_cast<std::size_t>(index0)];
    const double sample1 = buffer[static_cast<std::size_t>(index1)];
    return sample0 + ((sample1 - sample0) * frac);
  }

  std::vector<double> buffer;
  int writeIndex{0};
};

struct OnePoleLowpass
{
  void SetCutoffHz(double sampleRate,
                   double cutoffHz,
                   double minimumCutoffHz = 1.0,
                   double maximumCutoffScale = 0.49,
                   double defaultSampleRate = kDefaultSampleRate)
  {
    coefficient = CutoffHzToCoefficient(
      sampleRate,
      cutoffHz,
      minimumCutoffHz,
      maximumCutoffScale,
      defaultSampleRate);
  }

  void Clear()
  {
    state = 0.0;
  }

  double Process(double input)
  {
    state += coefficient * (input - state);
    state = FlushDenormal(state);
    return state;
  }

  double coefficient{1.0};
  double state{0.0};
};

struct OnePoleHighpass
{
  void SetCutoffHz(double sampleRate,
                   double cutoffHz,
                   double minimumCutoffHz = 1.0,
                   double maximumCutoffScale = 0.49,
                   double defaultSampleRate = kDefaultSampleRate)
  {
    coefficient = CutoffHzToCoefficient(
      sampleRate,
      cutoffHz,
      minimumCutoffHz,
      maximumCutoffScale,
      defaultSampleRate);
  }

  void Clear()
  {
    lowState = 0.0;
  }

  double Process(double input)
  {
    lowState += coefficient * (input - lowState);
    lowState = FlushDenormal(lowState);
    return input - lowState;
  }

  double coefficient{1.0};
  double lowState{0.0};
};

struct BiquadBandpass
{
  void SetBandpassHz(double sampleRate,
                     double centerHz,
                     double q,
                     double minimumCenterHz = 1.0,
                     double maximumCenterScale = 0.49,
                     double minimumQ = 0.1,
                     double maximumQ = 100.0,
                     double defaultSampleRate = kDefaultSampleRate)
  {
    const double safeSampleRate = sampleRate > 0.0 ? sampleRate : defaultSampleRate;
    const double safeCenterHz = std::clamp(centerHz, minimumCenterHz, maximumCenterScale * safeSampleRate);
    const double safeQ = std::clamp(q, minimumQ, maximumQ);
    const double omega = (2.0 * kPi * safeCenterHz) / safeSampleRate;
    const double sinOmega = std::sin(omega);
    const double cosOmega = std::cos(omega);
    const double alpha = sinOmega / (2.0 * safeQ);
    const double invA0 = 1.0 / (1.0 + alpha);

    b0 = alpha * invA0;
    b1 = 0.0;
    b2 = -alpha * invA0;
    a1 = (-2.0 * cosOmega) * invA0;
    a2 = (1.0 - alpha) * invA0;
  }

  void Clear()
  {
    z1 = 0.0;
    z2 = 0.0;
  }

  double Process(double input)
  {
    const double output = (b0 * input) + z1;
    z1 = FlushDenormal((b1 * input) - (a1 * output) + z2);
    z2 = FlushDenormal((b2 * input) - (a2 * output));
    return FlushDenormal(output);
  }

  double b0{1.0};
  double b1{0.0};
  double b2{0.0};
  double a1{0.0};
  double a2{0.0};
  double z1{0.0};
  double z2{0.0};
};
} // namespace dsp

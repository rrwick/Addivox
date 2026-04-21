#pragma once

#include <array>

#include "../settings/oscillator.h"

struct HarmonicVisualizerOscillator
{
  float frequencyHz{0.f};
  float level{0.f};
  float panLeftGain{0.f};
  float panRightGain{0.f};
};

struct HarmonicVisualizerNoiseBand
{
  float leftLevel{0.f};
  float rightLevel{0.f};
};

struct HarmonicVisualizerFrame
{
  static constexpr int kNumHarmonics = SimplePreset::kNumOscillators;
  static constexpr int kNumNoiseBands = NoiseBandProfile::kNumBands;
  using HarmonicArray = std::array<HarmonicVisualizerOscillator, kNumHarmonics>;
  using NoiseBandArray = std::array<HarmonicVisualizerNoiseBand, kNumNoiseBands>;

  HarmonicArray harmonics{};
  NoiseBandArray noiseBands{};
};

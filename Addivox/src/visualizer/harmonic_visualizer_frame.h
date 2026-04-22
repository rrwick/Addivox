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
  float frequencyHz{0.f};
  float level{0.f};
  float panLeftGain{0.f};
  float panRightGain{0.f};
};

struct HarmonicVisualizerFrame
{
  static constexpr int kNumHarmonics = SimplePreset::kNumOscillators;
  static constexpr int kNumNoiseBands = NoiseBandProfile::kNumBands;
  static constexpr int kNoiseComponentsPerBand = 20;
  static constexpr int kNumNoiseComponents = kNumNoiseBands * kNoiseComponentsPerBand;
  using HarmonicArray = std::array<HarmonicVisualizerOscillator, kNumHarmonics>;
  using NoiseComponentArray = std::array<HarmonicVisualizerNoiseBand, kNumNoiseComponents>;

  HarmonicArray harmonics{};
  NoiseComponentArray noiseComponents{};
};

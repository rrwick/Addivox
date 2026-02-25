#pragma once

#include <array>

#include "settings_oscillator.h"

struct HarmonicVisualizerOscillator
{
  float frequencyHz{0.f};
  float level{0.f};
  float panLeftGain{0.f};
  float panRightGain{0.f};
};

struct HarmonicVisualizerFrame
{
  static constexpr int kNumHarmonics = SimplePreset::kNumOscillators;
  using HarmonicArray = std::array<HarmonicVisualizerOscillator, kNumHarmonics>;

  HarmonicArray harmonics{};
};

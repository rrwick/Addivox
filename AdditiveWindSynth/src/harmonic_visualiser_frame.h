#pragma once

#include <array>

#include "settings.h"

struct HarmonicVisualiserOscillator
{
  float frequencyHz{0.f};
  float level{0.f};
  float panLeftGain{0.f};
  float panRightGain{0.f};
};

struct HarmonicVisualiserFrame
{
  static constexpr int kNumHarmonics = SimplePreset::kNumOscillators;
  using HarmonicArray = std::array<HarmonicVisualiserOscillator, kNumHarmonics>;

  HarmonicArray harmonics{};
};

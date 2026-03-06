#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline void AppendVariationTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[6],
    OscillatorParameter::intensity_variation_amplitude,
    {0.0, 10.0},
    "Controls depth of\nintensity variation\nfor each harmonic."
  });
  descriptors.push_back({
    kOscillatorTabTitles[7],
    OscillatorParameter::intensity_variation_rate,
    {0.0, 10.0},
    "Controls speed of\nintensity variation\nfor each harmonic."
  });
  descriptors.push_back({
    kOscillatorTabTitles[8],
    OscillatorParameter::pitch_variation_amplitude,
    {0.0, 10.0},
    "Controls depth of\npitch variation for\neach harmonic."
  });
  descriptors.push_back({
    kOscillatorTabTitles[9],
    OscillatorParameter::pitch_variation_rate,
    {0.0, 10.0},
    "Controls speed of\npitch variation for\neach harmonic."
  });
  descriptors.push_back({
    kOscillatorTabTitles[10],
    OscillatorParameter::pan_variation_amplitude,
    {0.0, 10.0},
    "Controls depth of\npan variation for\neach harmonic."
  });
  descriptors.push_back({
    kOscillatorTabTitles[11],
    OscillatorParameter::pan_variation_rate,
    {0.0, 10.0},
    "Controls speed of\npan variation for\neach harmonic."
  });
}
} // namespace editor
} // namespace plugin_ui

#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline void AppendBreathTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[1],
    OscillatorParameter::breath_power,
    {0.0, 100.0},
    "Controls how\nstrongly breath\nscales each\nharmonic level."
  });
}
} // namespace editor
} // namespace plugin_ui

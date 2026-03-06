#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline void AppendAttackReleaseTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[2],
    OscillatorParameter::attack,
    {0.0, 0.01},
    "Controls how\nquickly each\nharmonic ramps\nup at note start."
  });
  descriptors.push_back({
    kOscillatorTabTitles[3],
    OscillatorParameter::release,
    {0.0, 0.1},
    "Controls how\nquickly each\nharmonic fades\nafter note release."
  });
}
} // namespace editor
} // namespace plugin_ui

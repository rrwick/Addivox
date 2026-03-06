#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline void AppendPitchTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[4],
    OscillatorParameter::pitch,
    {-100.0, 100.0},
    "Controls static pitch\noffset per harmonic\nin cents."
  });
}
} // namespace editor
} // namespace plugin_ui

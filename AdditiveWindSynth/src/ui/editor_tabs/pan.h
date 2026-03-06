#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline void AppendPanTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[5],
    OscillatorParameter::pan,
    {-1.0, 1.0},
    "Controls stereo\nposition of each\nharmonic from left\nto right."
  });
}
} // namespace editor
} // namespace plugin_ui

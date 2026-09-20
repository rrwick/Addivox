#pragma once

#include "IPlugConstants.h"

namespace effects {
inline bool IsStereoBlockSilent(iplug::sample* const* outputs, int nFrames) {
  for (int frame = 0; frame < nFrames; ++frame) {
    if (outputs[0][frame] != 0.0 || outputs[1][frame] != 0.0) return false;
  }
  return true;
}
} // namespace effects

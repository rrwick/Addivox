#include "settings_effects.h"

#include "params.h"

#include <algorithm>

namespace effects_settings
{
EffectsSettings Sanitize(const EffectsSettings& settings)
{
  EffectsSettings sanitized = settings;
  sanitized.reverb = std::clamp(sanitized.reverb, 0.0, 100.0);
  return sanitized;
}

bool ApplyParam(int paramIdx, double value, EffectsSettings& settings)
{
  switch(paramIdx)
  {
    case kParamEffectsReverb:
      settings.reverb = std::clamp(value, 0.0, 100.0);
      return true;
    default:
      return false;
  }
}
} // namespace effects_settings

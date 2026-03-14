#include "effects.h"

#include "params.h"

#include <algorithm>

namespace effects_settings
{
EffectsSettings Sanitize(const EffectsSettings& settings)
{
  EffectsSettings sanitized = settings;
  sanitized.drive = std::clamp(sanitized.drive, 0.0, 100.0);
  sanitized.chorus = std::clamp(sanitized.chorus, 0.0, 100.0);
  sanitized.reverb = std::clamp(sanitized.reverb, 0.0, 100.0);
  return sanitized;
}

bool ApplyParam(int paramIdx, double value, EffectsSettings& settings)
{
  switch(paramIdx)
  {
    case kParamEffectsDrive:
      settings.drive = std::clamp(value, 0.0, 100.0);
      return true;
    case kParamEffectsChorus:
      settings.chorus = std::clamp(value, 0.0, 100.0);
      return true;
    case kParamEffectsReverb:
      settings.reverb = std::clamp(value, 0.0, 100.0);
      return true;
    default:
      return false;
  }
}
} // namespace effects_settings

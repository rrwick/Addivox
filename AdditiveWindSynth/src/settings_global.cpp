#include "settings_global.h"

#include "params.h"

#include <algorithm>

namespace global_settings
{
const std::array<int, 10> kModifierParamIndices{{
  kParamGlobalAttackScale,
  kParamGlobalPitchShift,
  kParamGlobalIntensityVariationAmplitudeScale,
  kParamGlobalPitchVariationAmplitudeScale,
  kParamGlobalPanVariationAmplitudeScale,
  kParamGlobalReleaseScale,
  kParamGlobalPanShift,
  kParamGlobalIntensityVariationRateScale,
  kParamGlobalPitchVariationRateScale,
  kParamGlobalPanVariationRateScale
}};

GlobalOscillatorModifiers Sanitize(const GlobalOscillatorModifiers& modifiers)
{
  GlobalOscillatorModifiers sanitized = modifiers;
  sanitized.attackScale = std::max(0.0, sanitized.attackScale);
  sanitized.releaseScale = std::max(0.0, sanitized.releaseScale);
  sanitized.panOffset = std::clamp(sanitized.panOffset, -1.0, 1.0);
  sanitized.intensityVariationAmplitudeScale = std::max(0.0, sanitized.intensityVariationAmplitudeScale);
  sanitized.intensityVariationRateScale = std::max(0.0, sanitized.intensityVariationRateScale);
  sanitized.pitchVariationAmplitudeScale = std::max(0.0, sanitized.pitchVariationAmplitudeScale);
  sanitized.pitchVariationRateScale = std::max(0.0, sanitized.pitchVariationRateScale);
  sanitized.panVariationAmplitudeScale = std::max(0.0, sanitized.panVariationAmplitudeScale);
  sanitized.panVariationRateScale = std::max(0.0, sanitized.panVariationRateScale);
  sanitized.portamentoTimeAtCC5MinSec = std::max(0.0, sanitized.portamentoTimeAtCC5MinSec);
  sanitized.portamentoTimeAtCC5MaxSec = std::max(0.0, sanitized.portamentoTimeAtCC5MaxSec);
  return sanitized;
}

bool ApplyParam(int paramIdx, double value, GlobalOscillatorModifiers& modifiers)
{
  switch(paramIdx)
  {
    case kParamGlobalAttackScale:
      modifiers.attackScale = std::max(0.0, value);
      return true;
    case kParamGlobalReleaseScale:
      modifiers.releaseScale = std::max(0.0, value);
      return true;
    case kParamGlobalPitchShift:
      modifiers.pitchOffsetCents = value;
      return true;
    case kParamGlobalPanShift:
      modifiers.panOffset = std::clamp(value, -1.0, 1.0);
      return true;
    case kParamGlobalIntensityVariationAmplitudeScale:
      modifiers.intensityVariationAmplitudeScale = std::max(0.0, value);
      return true;
    case kParamGlobalIntensityVariationRateScale:
      modifiers.intensityVariationRateScale = std::max(0.0, value);
      return true;
    case kParamGlobalPitchVariationAmplitudeScale:
      modifiers.pitchVariationAmplitudeScale = std::max(0.0, value);
      return true;
    case kParamGlobalPitchVariationRateScale:
      modifiers.pitchVariationRateScale = std::max(0.0, value);
      return true;
    case kParamGlobalPanVariationAmplitudeScale:
      modifiers.panVariationAmplitudeScale = std::max(0.0, value);
      return true;
    case kParamGlobalPanVariationRateScale:
      modifiers.panVariationRateScale = std::max(0.0, value);
      return true;
    case kParamPortamentoAtCC5Min:
      modifiers.portamentoTimeAtCC5MinSec = std::max(0.0, value);
      return true;
    case kParamPortamentoAtCC5Max:
      modifiers.portamentoTimeAtCC5MaxSec = std::max(0.0, value);
      return true;
    default:
      return false;
  }
}
} // namespace global_settings

#include "global.h"

#include "params.h"

#include <algorithm>
#include <cmath>

namespace
{
double SanitizeTuningCents(double value)
{
  const double roundedValue = std::round(value);
  const double clampedValue = std::clamp(roundedValue, -50.0, 50.0);
  return (clampedValue == 0.0) ? 0.0 : clampedValue;
}
} // namespace

namespace global_settings
{
GlobalVoiceSettings Sanitize(const GlobalVoiceSettings& settings)
{
  GlobalVoiceSettings sanitized = settings;
  sanitized.levelScale = std::max(0.0, sanitized.levelScale);
  sanitized.noiseSustainScale = std::max(0.0, sanitized.noiseSustainScale);
  sanitized.attackScale = std::max(0.0, sanitized.attackScale);
  sanitized.releaseScale = std::max(0.0, sanitized.releaseScale);
  sanitized.tuningCents = SanitizeTuningCents(sanitized.tuningCents);
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

bool ApplyParam(int paramIdx, double value, GlobalVoiceSettings& settings)
{
  switch(paramIdx)
  {
    case kParamGlobalLevel:
      settings.levelScale = std::max(0.0, value);
      return true;
    case kParamGlobalNoiseSustain:
      settings.noiseSustainScale = std::max(0.0, value);
      return true;
    case kParamGlobalAttackScale:
      settings.attackScale = std::max(0.0, value);
      return true;
    case kParamGlobalReleaseScale:
      settings.releaseScale = std::max(0.0, value);
      return true;
    case kParamGlobalTuning:
      settings.tuningCents = SanitizeTuningCents(value);
      return true;
    case kParamGlobalPanShift:
      settings.panOffset = std::clamp(value, -1.0, 1.0);
      return true;
    case kParamGlobalIntensityVariationAmplitudeScale:
      settings.intensityVariationAmplitudeScale = std::max(0.0, value);
      return true;
    case kParamGlobalIntensityVariationRateScale:
      settings.intensityVariationRateScale = std::max(0.0, value);
      return true;
    case kParamGlobalPitchVariationAmplitudeScale:
      settings.pitchVariationAmplitudeScale = std::max(0.0, value);
      return true;
    case kParamGlobalPitchVariationRateScale:
      settings.pitchVariationRateScale = std::max(0.0, value);
      return true;
    case kParamGlobalPanVariationAmplitudeScale:
      settings.panVariationAmplitudeScale = std::max(0.0, value);
      return true;
    case kParamGlobalPanVariationRateScale:
      settings.panVariationRateScale = std::max(0.0, value);
      return true;
    case kParamPortamentoAtCC5Min:
      settings.portamentoTimeAtCC5MinSec = std::max(0.0, value);
      return true;
    case kParamPortamentoAtCC5Max:
      settings.portamentoTimeAtCC5MaxSec = std::max(0.0, value);
      return true;
    default:
      return false;
  }
}
} // namespace global_settings

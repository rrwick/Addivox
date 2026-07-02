#include "global.h"

#include "params.h"

#include <algorithm>
#include <cmath>

namespace {
double SanitizeNonNegative(double value) { return std::isfinite(value) ? std::max(0.0, value) : 0.0; }

double SanitizePanOffset(double value) { return std::isfinite(value) ? std::clamp(value, -1.0, 1.0) : 0.0; }

double SanitizeTuningCents(double value) {
  if (!std::isfinite(value)) return 0.0;

  const double roundedValue = std::round(value);
  const double clampedValue = std::clamp(roundedValue, -50.0, 50.0);
  return (clampedValue == 0.0) ? 0.0 : clampedValue;
}
} // namespace

namespace global_settings {
GlobalVoiceSettings Sanitize(const GlobalVoiceSettings& settings) {
  GlobalVoiceSettings sanitized = settings;
  sanitized.levelScale = SanitizeNonNegative(sanitized.levelScale);
  sanitized.attackScale = SanitizeNonNegative(sanitized.attackScale);
  sanitized.releaseScale = SanitizeNonNegative(sanitized.releaseScale);
  sanitized.tuningCents = SanitizeTuningCents(sanitized.tuningCents);
  sanitized.panOffset = SanitizePanOffset(sanitized.panOffset);
  sanitized.levelVariationAmplitudeScale = SanitizeNonNegative(sanitized.levelVariationAmplitudeScale);
  sanitized.levelVariationRateScale = SanitizeNonNegative(sanitized.levelVariationRateScale);
  sanitized.pitchVariationAmplitudeScale = SanitizeNonNegative(sanitized.pitchVariationAmplitudeScale);
  sanitized.pitchVariationRateScale = SanitizeNonNegative(sanitized.pitchVariationRateScale);
  sanitized.panVariationAmplitudeScale = SanitizeNonNegative(sanitized.panVariationAmplitudeScale);
  sanitized.panVariationRateScale = SanitizeNonNegative(sanitized.panVariationRateScale);
  sanitized.portamentoTimeAtCC5MinSec = SanitizeNonNegative(sanitized.portamentoTimeAtCC5MinSec);
  sanitized.portamentoTimeAtCC5MaxSec = SanitizeNonNegative(sanitized.portamentoTimeAtCC5MaxSec);
  return sanitized;
}

bool ApplyParam(int paramIdx, double value, GlobalVoiceSettings& settings) {
  switch (paramIdx) {
  case kParamGlobalLevel:                        settings.levelScale = SanitizeNonNegative(value); return true;
  case kParamGlobalAttackScale:                  settings.attackScale = SanitizeNonNegative(value); return true;
  case kParamGlobalReleaseScale:                 settings.releaseScale = SanitizeNonNegative(value); return true;
  case kParamGlobalTuning:                       settings.tuningCents = SanitizeTuningCents(value); return true;
  case kParamGlobalPanShift:                     settings.panOffset = SanitizePanOffset(value); return true;
  case kParamGlobalLevelVariationAmplitudeScale: settings.levelVariationAmplitudeScale = SanitizeNonNegative(value); return true;
  case kParamGlobalLevelVariationRateScale:      settings.levelVariationRateScale = SanitizeNonNegative(value); return true;
  case kParamGlobalPitchVariationAmplitudeScale: settings.pitchVariationAmplitudeScale = SanitizeNonNegative(value); return true;
  case kParamGlobalPitchVariationRateScale:      settings.pitchVariationRateScale = SanitizeNonNegative(value); return true;
  case kParamGlobalPanVariationAmplitudeScale:   settings.panVariationAmplitudeScale = SanitizeNonNegative(value); return true;
  case kParamGlobalPanVariationRateScale:        settings.panVariationRateScale = SanitizeNonNegative(value); return true;
  case kParamPortamentoAtCC5Min:                 settings.portamentoTimeAtCC5MinSec = SanitizeNonNegative(value); return true;
  case kParamPortamentoAtCC5Max:                 settings.portamentoTimeAtCC5MaxSec = SanitizeNonNegative(value); return true;
  default:                                       return false;
  }
}
} // namespace global_settings

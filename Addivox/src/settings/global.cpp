#include "global.h"

#include "params.h"

#include <algorithm>
#include <array>
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
struct SettingDescriptor {
  int paramIdx;
  double GlobalVoiceSettings::* member;
  double (*sanitize)(double);
};

constexpr std::array<SettingDescriptor, 15> kSettingDescriptors{{
    {kParamGlobalLevel,                        &GlobalVoiceSettings::levelScale,                   SanitizeNonNegative},
    {kParamGlobalAttackScale,                  &GlobalVoiceSettings::attackScale,                  SanitizeNonNegative},
    {kParamGlobalReleaseScale,                 &GlobalVoiceSettings::releaseScale,                 SanitizeNonNegative},
    {kParamGlobalTuning,                       &GlobalVoiceSettings::tuningCents,                  SanitizeTuningCents},
    {kParamGlobalPanShift,                     &GlobalVoiceSettings::panOffset,                    SanitizePanOffset},
    {kParamGlobalLevelVariationAmplitudeScale, &GlobalVoiceSettings::levelVariationAmplitudeScale, SanitizeNonNegative},
    {kParamGlobalLevelVariationRateScale,      &GlobalVoiceSettings::levelVariationRateScale,      SanitizeNonNegative},
    {kParamGlobalPitchVariationAmplitudeScale, &GlobalVoiceSettings::pitchVariationAmplitudeScale, SanitizeNonNegative},
    {kParamGlobalPitchVariationRateScale,      &GlobalVoiceSettings::pitchVariationRateScale,      SanitizeNonNegative},
    {kParamGlobalPanVariationAmplitudeScale,   &GlobalVoiceSettings::panVariationAmplitudeScale,   SanitizeNonNegative},
    {kParamGlobalPanVariationRateScale,        &GlobalVoiceSettings::panVariationRateScale,        SanitizeNonNegative},
    {kParamPortamentoAtCC5Min,                 &GlobalVoiceSettings::portamentoTimeAtCC5MinSec,    SanitizeNonNegative},
    {kParamPortamentoAtCC5Max,                 &GlobalVoiceSettings::portamentoTimeAtCC5MaxSec,    SanitizeNonNegative},
    {kParamFormantsScale,                      &GlobalVoiceSettings::formantsScale,                SanitizeNonNegative},
    {kParamResonanceScale,                     &GlobalVoiceSettings::resonanceScale,               SanitizeNonNegative},
}};
} // namespace

namespace global_settings {
GlobalVoiceSettings Sanitize(const GlobalVoiceSettings& settings) {
  GlobalVoiceSettings sanitized = settings;
  for (const auto& setting : kSettingDescriptors) sanitized.*(setting.member) = setting.sanitize(sanitized.*(setting.member));
  return sanitized;
}

bool ApplyParam(int paramIdx, double value, GlobalVoiceSettings& settings) {
  for (const auto& setting : kSettingDescriptors) {
    if (setting.paramIdx != paramIdx) continue;
    settings.*(setting.member) = setting.sanitize(value);
    return true;
  }
  return false;
}
} // namespace global_settings

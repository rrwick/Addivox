#include "effects.h"

#include "params.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
struct SettingDescriptor {
  int paramIdx;
  double EffectsSettings::* member;
  double min;
  double max;
};

constexpr std::array<SettingDescriptor, 4> kSettingDescriptors{{
    {kParamEffectsDrive,  &EffectsSettings::drive,   0.0, 100.0},
    {kParamEffectsTone,   &EffectsSettings::tone,   -1.0,   1.0},
    {kParamEffectsChorus, &EffectsSettings::chorus,  0.0, 100.0},
    {kParamEffectsReverb, &EffectsSettings::reverb,  0.0, 100.0},
}};

double SanitizeSetting(const SettingDescriptor& setting, double value) { return std::isfinite(value) ? std::clamp(value, setting.min, setting.max) : 0.0; }
} // namespace

namespace effects_settings {
EffectsSettings Sanitize(const EffectsSettings& settings) {
  EffectsSettings sanitized = settings;
  for (const auto& setting : kSettingDescriptors) sanitized.*(setting.member) = SanitizeSetting(setting, sanitized.*(setting.member));
  return sanitized;
}

bool ApplyParam(int paramIdx, double value, EffectsSettings& settings) {
  for (const auto& setting : kSettingDescriptors) {
    if (setting.paramIdx != paramIdx) continue;
    settings.*(setting.member) = SanitizeSetting(setting, value);
    return true;
  }
  return false;
}
} // namespace effects_settings

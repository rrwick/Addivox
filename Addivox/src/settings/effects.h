#pragma once

namespace effects_settings {
#if defined(APP_API)
inline constexpr double kDefaultReverb = 50.0;
#else
inline constexpr double kDefaultReverb = 0.0;
#endif
} // namespace effects_settings

class EffectsSettings {
public:
  // Effect macro controls are stored in user-facing units.
  double drive{0.0};
  double tone{0.0};
  double chorus{0.0};
  double reverb{effects_settings::kDefaultReverb};
};

namespace effects_settings {
EffectsSettings Sanitize(const EffectsSettings& settings);
bool ApplyParam(int paramIdx, double value, EffectsSettings& settings);
} // namespace effects_settings

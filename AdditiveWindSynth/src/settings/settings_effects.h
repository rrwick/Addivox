#pragma once

class EffectsSettings
{
public:
  // Effect macro controls are stored in user-facing units.
  double reverb{0.0};
};

namespace effects_settings
{
EffectsSettings Sanitize(const EffectsSettings& settings);
bool ApplyParam(int paramIdx, double value, EffectsSettings& settings);
} // namespace effects_settings

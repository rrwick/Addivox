#pragma once

#include <array>

struct GlobalOscillatorModifiers
{
  // These are scalar modifiers, applied to all oscillators in a preset.
  double attackScale{1.0};
  double releaseScale{1.0};
  double pitchOffsetCents{0.0};
  double panOffset{0.0};
  double intensityVariationAmplitudeScale{1.0};
  double intensityVariationRateScale{1.0};
  double pitchVariationAmplitudeScale{1.0};
  double pitchVariationRateScale{1.0};
  double panVariationAmplitudeScale{1.0};
  double panVariationRateScale{1.0};

  // Portamento times at CC5 min and max, in seconds/semitone.
  double portamentoTimeAtCC5MinSec{0.0};
  double portamentoTimeAtCC5MaxSec{0.0};
};

namespace global_settings
{
extern const std::array<int, 10> kModifierParamIndices;
GlobalOscillatorModifiers Sanitize(const GlobalOscillatorModifiers& modifiers);
bool ApplyParam(int paramIdx, double value, GlobalOscillatorModifiers& modifiers);
} // namespace global_settings

#pragma once

struct GlobalVoiceSettings
{
  // These are global voice settings applied across the oscillators in a patch.
  double levelScale{1.0};
  double attackScale{1.0};
  double releaseScale{1.0};
  double tuningCents{0.0};
  double panOffset{0.0};
  double levelVariationAmplitudeScale{0.0};
  double levelVariationRateScale{1.0};
  double pitchVariationAmplitudeScale{0.0};
  double pitchVariationRateScale{1.0};
  double panVariationAmplitudeScale{0.0};
  double panVariationRateScale{1.0};

  // Portamento times at CC5 min and max, in seconds/semitone.
  double portamentoTimeAtCC5MinSec{0.001};
  double portamentoTimeAtCC5MaxSec{0.025};
};

namespace global_settings
{
GlobalVoiceSettings Sanitize(const GlobalVoiceSettings& settings);
bool ApplyParam(int paramIdx, double value, GlobalVoiceSettings& settings);
} // namespace global_settings

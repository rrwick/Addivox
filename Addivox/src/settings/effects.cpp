#include "effects.h"

#include "params.h"

#include <algorithm>
#include <cmath>

namespace {
double SanitizeClamped(double value, double lo, double hi) { return std::isfinite(value) ? std::clamp(value, lo, hi) : 0.0; }
} // namespace

namespace effects_settings {
EffectsSettings Sanitize(const EffectsSettings& settings) {
  EffectsSettings sanitized = settings;
  sanitized.drive = SanitizeClamped(sanitized.drive, 0.0, 100.0);
  sanitized.tone = SanitizeClamped(sanitized.tone, -1.0, 1.0);
  sanitized.chorus = SanitizeClamped(sanitized.chorus, 0.0, 100.0);
  sanitized.reverb = SanitizeClamped(sanitized.reverb, 0.0, 100.0);
  return sanitized;
}

bool ApplyParam(int paramIdx, double value, EffectsSettings& settings) {
  switch (paramIdx) {
  case kParamEffectsDrive:  settings.drive = SanitizeClamped(value, 0.0, 100.0); return true;
  case kParamEffectsTone:   settings.tone = SanitizeClamped(value, -1.0, 1.0); return true;
  case kParamEffectsChorus: settings.chorus = SanitizeClamped(value, 0.0, 100.0); return true;
  case kParamEffectsReverb: settings.reverb = SanitizeClamped(value, 0.0, 100.0); return true;
  default:                  return false;
  }
}
} // namespace effects_settings

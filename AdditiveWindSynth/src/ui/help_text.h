#pragma once

#include "../settings/settings_oscillator.h"

namespace plugin_ui
{
namespace help_text
{
namespace oscillator_tabs
{
using OscillatorParameter = OscillatorSettings::Parameter;

inline constexpr const char* kLevel =
  "Controls the intensity of each harmonic at full breath.";

inline constexpr const char* kBreath =
  "Controls how strongly breath scales each harmonic level.";

inline constexpr const char* kAttack =
  "Controls how quickly each harmonic ramps up at note start.";

inline constexpr const char* kRelease =
  "Controls how quickly each harmonic fades after note release.";

inline constexpr const char* kPitch =
  "Controls static pitch offset per harmonic in cents.";

inline constexpr const char* kPan =
  "Controls stereo position of each harmonic from left to right.";

inline constexpr const char* kIntensityVariationAmplitude =
  "Controls depth of intensity variation for each harmonic.";

inline constexpr const char* kIntensityVariationRate =
  "Controls speed of intensity variation for each harmonic.";

inline constexpr const char* kPitchVariationAmplitude =
  "Controls depth of pitch variation for each harmonic.";

inline constexpr const char* kPitchVariationRate =
  "Controls speed of pitch variation for each harmonic.";

inline constexpr const char* kPanVariationAmplitude =
  "Controls depth of pan variation for each harmonic.";

inline constexpr const char* kPanVariationRate =
  "Controls speed of pan variation for each harmonic.";

inline constexpr const char* kXRangeMin =
  "Controls the lowest harmonic that will be shown in the editor graph to the right. Use the X range controls to limit the harmonics for easier editing.";

inline constexpr const char* kXRangeMax =
  "Controls the highest harmonic that will be shown in the editor graph to the right. Use the X range controls to limit the harmonics for easier editing.";

inline constexpr const char* kAddButton =
  "Creates a new key note for the currently selected note.";

inline constexpr const char* kDeleteButton =
  "Removes the selected key note.";

inline const char* Get(OscillatorParameter parameter)
{
  switch(parameter)
  {
    case OscillatorParameter::intensity:
      return kLevel;
    case OscillatorParameter::breath_power:
      return kBreath;
    case OscillatorParameter::attack:
      return kAttack;
    case OscillatorParameter::release:
      return kRelease;
    case OscillatorParameter::pitch:
      return kPitch;
    case OscillatorParameter::pan:
      return kPan;
    case OscillatorParameter::intensity_variation_amplitude:
      return kIntensityVariationAmplitude;
    case OscillatorParameter::intensity_variation_rate:
      return kIntensityVariationRate;
    case OscillatorParameter::pitch_variation_amplitude:
      return kPitchVariationAmplitude;
    case OscillatorParameter::pitch_variation_rate:
      return kPitchVariationRate;
    case OscillatorParameter::pan_variation_amplitude:
      return kPanVariationAmplitude;
    case OscillatorParameter::pan_variation_rate:
      return kPanVariationRate;
    default:
      return "";
  }
}
} // namespace oscillator_tabs
} // namespace help_text
} // namespace plugin_ui

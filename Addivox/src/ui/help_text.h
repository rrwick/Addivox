#pragma once

#include "../settings/oscillator.h"
#include "../settings/params.h"

namespace plugin_ui
{
namespace help_text
{
namespace main_ui
{
inline constexpr const char* kKeyboard =
  "Click the keyboard to play notes in VIS mode and choose which note patch you are editing in EDIT mode. Computer-keyboard note triggers are active in VIS mode only.";

inline constexpr const char* kPitchBendWheel =
  "Bends the pitch of held notes. Right click to set the pitch bend range.";

inline constexpr const char* kAttack =
  "Scales the attack time for all oscillators. Higher values make notes bloom more slowly.";

inline constexpr const char* kRelease =
  "Scales the release time for all oscillators. Higher values let notes ring out longer after release.";

inline constexpr const char* kTranspose =
  "Transposes every played note in semitones. This control is not tied to a patch and will hold its value as the patch changes.";

inline constexpr const char* kTuning =
  "Applies a global tuning offset to all oscillators. This control is not tied to a patch and will hold its value as the patch changes.";

inline constexpr const char* kPanShift =
  "Applies a global stereo pan offset to all oscillators. This control is not tied to a patch and will hold its value as the patch changes.";

inline constexpr const char* kPortamento =
  "Sets the portamento time range, from the minimum at CC5=0 to the maximum at CC5=127.";

inline constexpr const char* kIntensityVariationAmplitude =
  "Scales the depth of level variation for all oscillators.";

inline constexpr const char* kIntensityVariationRate =
  "Scales the speed of level variation for all oscillators.";

inline constexpr const char* kPanVariationAmplitude =
  "Scales the depth of pan variation for all oscillators.";

inline constexpr const char* kPanVariationRate =
  "Scales the speed of pan variation for all oscillators.";

inline constexpr const char* kPitchVariationAmplitude =
  "Scales the depth of pitch variation for all oscillators.";

inline constexpr const char* kPitchVariationRate =
  "Scales the speed of pitch variation for all oscillators.";

inline constexpr const char* kLevel =
  "Sets the overall output level of the synth (all oscillators).";

inline constexpr const char* kDrive =
  "Adds warmth and harmonics to the sound. Higher values lead to a more distorted tone.";

inline constexpr const char* kTone =
  "Negative values darken the sound (boost low frequencies, suppress high frequencies), positive values brighten it (suppress low frequencies, boost high frequencies).";

inline constexpr const char* kChorus =
  "Adds a stereo chorus effect to widen and thicken the sound.";

inline constexpr const char* kReverb =
  "Adds a reverb effect to create a sense of space. Higher values make the reverb louder and longer-lasting. This control is not tied to a patch and will hold its value as the patch changes.";

inline const char* GetParam(int paramIdx)
{
  switch(paramIdx)
  {
    case kParamGlobalAttackScale:
      return kAttack;
    case kParamGlobalReleaseScale:
      return kRelease;
    case kParamGlobalTuning:
      return kTuning;
    case kParamGlobalPanShift:
      return kPanShift;
    case kParamGlobalIntensityVariationAmplitudeScale:
      return kIntensityVariationAmplitude;
    case kParamGlobalIntensityVariationRateScale:
      return kIntensityVariationRate;
    case kParamGlobalPitchVariationAmplitudeScale:
      return kPitchVariationAmplitude;
    case kParamGlobalPitchVariationRateScale:
      return kPitchVariationRate;
    case kParamGlobalPanVariationAmplitudeScale:
      return kPanVariationAmplitude;
    case kParamGlobalPanVariationRateScale:
      return kPanVariationRate;
    case kParamPortamentoAtCC5Min:
    case kParamPortamentoAtCC5Max:
      return kPortamento;
    case kParamGlobalLevel:
      return kLevel;
    case kParamEffectsReverb:
      return kReverb;
    case kParamEffectsDrive:
      return kDrive;
    case kParamEffectsChorus:
      return kChorus;
    case kParamEffectsTone:
      return kTone;
    case kParamTranspose:
      return kTranspose;
    default:
      return "";
  }
}
} // namespace main_ui

namespace oscillator_tabs
{
using OscillatorParameter = OscillatorSettings::Parameter;

inline constexpr const char* kLevel =
  "Controls the amplitude of each harmonic at full breath.";

inline constexpr const char* kEq =
  "Controls a frequency-based EQ. The EQ stays fixed in frequency as notes change, which makes it useful for creating formants.";

inline constexpr const char* kBreath =
  "Controls per-harmonic breath sensitivity. Higher values make the harmonic require more breath before it becomes prominent.";

inline constexpr const char* kAttack =
  "Controls how quickly each harmonic can increase in intensity.";

inline constexpr const char* kRelease =
  "Controls how quickly each harmonic can decrease in intensity.";

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

inline constexpr const char* kAllKeyNotes =
  "When enabled, this parameter stays synchronized across every key note. Turning it on copies the currently selected key note values to all key notes immediately.";

inline constexpr const char* kEditMode =
  "Selects the edit operation for this tab. Set writes values directly to the cursor position, nudge makes small gradual changes as you drag, smooth gently averages a harmonic with its neighbors, and draw line interpolates between the drag start and current harmonic in the displayed transform. The scope controls limit most edits to all harmonics, only even harmonics, or only odd harmonics. In the Level tab, normalize always rescales all harmonics.";

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

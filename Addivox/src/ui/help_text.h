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
  "Click the keyboard to audition notes and choose which key note preset you are editing. Computer-keyboard note triggers are active in Viz mode only.";

inline constexpr const char* kPitchBendWheel =
  "Bends the pitch of held notes. The bend range follows the synth pitch-bend settings.";

inline constexpr const char* kAttack =
  "Scales the note attack time for the whole synth (all oscillators). Higher values make notes bloom more slowly.";

inline constexpr const char* kRelease =
  "Scales the note release time for the whole synth (all oscillators). Higher values let notes ring out longer after release.";

inline constexpr const char* kTranspose =
  "Transposes every played note in semitones. This control is not tied to a preset and will hold its value as the preset changes.";

inline constexpr const char* kTuning =
  "Applies a global tuning offset in cents to the whole synth (all oscillators). This control is not tied to a preset and will hold its value as the preset changes.";

inline constexpr const char* kPanShift =
  "Applies a global stereo pan offset to the whole synth (all oscillators). This control is not tied to a preset and will hold its value as the preset changes.";

inline constexpr const char* kPortamento =
  "Sets the portamento time range, from the minimum at low CC5 values to the maximum at high CC5 values.";

inline constexpr const char* kIntensityVariationAmplitude =
  "Scales the depth of level variation across the whole synth (all oscillators).";

inline constexpr const char* kIntensityVariationRate =
  "Scales the speed of level variation across the whole synth (all oscillators).";

inline constexpr const char* kPanVariationAmplitude =
  "Scales the depth of pan variation across the whole synth (all oscillators).";

inline constexpr const char* kPanVariationRate =
  "Scales the speed of pan variation across the whole synth (all oscillators).";

inline constexpr const char* kPitchVariationAmplitude =
  "Scales the depth of pitch variation across the whole synth (all oscillators).";

inline constexpr const char* kPitchVariationRate =
  "Scales the speed of pitch variation across the whole synth (all oscillators).";

inline constexpr const char* kLevel =
  "Sets the overall output level of the synth (all oscillators).";

inline constexpr const char* kNoiseSustain =
  "Scales sustain noise across the whole synth. Higher values make all sustain noise louder relative to the harmonic sound.";

inline constexpr const char* kDrive =
  "Effect 1/4: Saturation. Adds warmth and harmonics to the sound. Higher values lead to a more distorted tone.";

inline constexpr const char* kTone =
  "Effect 2/4: Tilt EQ. Negative values darken the sound, positive values brighten it.";

inline constexpr const char* kChorus =
  "Effect 3/4: Chorus. Adds a stereo chorus effect to widen and thicken the sound.";

inline constexpr const char* kReverb =
  "Effect 4/4: Reverb. This control is not tied to a preset and will hold its value as the preset changes.";

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
    case kParamGlobalNoiseSustain:
      return kNoiseSustain;
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
  "Controls the intensity of each harmonic at full breath.";

inline constexpr const char* kEq =
  "Controls a frequency-based EQ in dB for the oscillator bank. The EQ stays fixed in frequency as notes change, which makes it useful for creating formants.";

inline constexpr const char* kNoiseSustain =
  "Controls sustain noise per fixed frequency band. The noise scales with breath level and stays fixed in Hz as notes change.";

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

inline constexpr const char* kAllKeyNotes =
  "When enabled, this parameter stays synchronized across every key note. Turning it on copies the currently selected key note values to all key notes immediately.";

inline constexpr const char* kEditMode =
  "Selects the edit operation for this tab. Set writes values directly to the cursor position, nudge makes small gradual changes as you drag, smooth gently averages a harmonic with its neighbors, and draw line interpolates between the drag start and current harmonic in the displayed transform. The scope controls limit most edits to all harmonics, only even harmonics, or only odd harmonics. In the Level tab, normalize always rescales all harmonics.";

inline constexpr const char* kNoiseBandEditMode =
  "Selects the edit operation for this tab. Set writes values directly to the cursor position, nudge makes small gradual changes as you drag, smooth gently averages a band with its neighbors, and draw line interpolates between the drag start and current band in the displayed transform.";

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

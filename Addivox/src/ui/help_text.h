#pragma once

#include "../settings/oscillator.h"
#include "../settings/params.h"

namespace plugin_ui {
namespace help_text {
namespace main_ui {
inline constexpr const char *kKeyboard =
    "Click the keyboard to play notes in VIS mode and choose which note patch you are editing in EDIT mode.";

inline constexpr const char *kPitchBendWheel =
    "Bends the pitch of held notes. Right click to set the pitch bend range.";

inline constexpr const char *kVisEditSwitch =
    "Switches the main panel between VIS mode and EDIT mode. VIS shows the live harmonic display. EDIT shows the per-note editor.";

inline constexpr const char *kPreviousPatch =
    "Loads the previous patch in the current patch group.";

inline constexpr const char *kNextPatch =
    "Loads the next patch in the current patch group.";

inline constexpr const char *kPatchManager =
    "Opens the patch menu for choosing, saving and importing patches.";

inline constexpr const char *kBreathMeter =
    "Shows the current breath input level.";

inline constexpr const char *kMainOutputMeter =
    "Shows the main stereo output level. Red bars at the top indicate the signal is at or above 0 dBFS and may clip when rendered or passed to later processing.";

inline constexpr const char *kAttack =
    "Scales the attack time for all oscillators. Higher values make notes fade in more slowly.";

inline constexpr const char *kRelease =
    "Scales the release time for all oscillators. Higher values make notes fade out more slowly.";

inline constexpr const char *kTranspose =
    "Transposes all played notes by this many semitones. "
    "This control is not tied to a patch and will hold its value as the patch changes.";

inline constexpr const char *kTuning =
    "Applies a global tuning offset to all oscillators. "
    "This control is not tied to a patch and will hold its value as the patch changes.";

inline constexpr const char *kPanShift =
    "Applies a global stereo pan offset to all oscillators. "
    "This control is not tied to a patch and will hold its value as the patch changes.";

inline constexpr const char *kPortamento =
    "Portamento range: CC5 varies min–max, CC65 selects min or max.";

inline constexpr const char *kLevelVariationAmplitude =
    "Scales the depth of level variation for all oscillators.";

inline constexpr const char *kLevelVariationRate =
    "Scales the speed of level variation for all oscillators.";

inline constexpr const char *kPanVariationAmplitude =
    "Scales the depth of pan variation for all oscillators.";

inline constexpr const char *kPanVariationRate =
    "Scales the speed of pan variation for all oscillators.";

inline constexpr const char *kPitchVariationAmplitude =
    "Scales the depth of pitch variation for all oscillators.";

inline constexpr const char *kPitchVariationRate =
    "Scales the speed of pitch variation for all oscillators.";

inline constexpr const char *kLevel =
    "Scales the output level for all oscillators.";

inline constexpr const char *kDrive =
    "Adds a distortion effect to the sound.";

inline constexpr const char *kTone =
    "Negative values darken the sound (boost low frequencies, suppress high frequencies), positive values brighten it "
    "(suppress low frequencies, boost high frequencies).";

inline constexpr const char *kChorus =
    "Adds a stereo chorus effect to the sound.";

inline constexpr const char *kReverb =
    "Adds a reverb effect to the sound. Higher values make the reverb louder and longer-lasting. "
    "This control is not tied to a patch and will hold its value as the patch changes.";

inline const char* GetParam(int paramIdx) {
  switch (paramIdx) {
  case kParamGlobalAttackScale:                  return kAttack;
  case kParamGlobalReleaseScale:                 return kRelease;
  case kParamGlobalTuning:                       return kTuning;
  case kParamGlobalPanShift:                     return kPanShift;
  case kParamGlobalLevelVariationAmplitudeScale: return kLevelVariationAmplitude;
  case kParamGlobalLevelVariationRateScale:      return kLevelVariationRate;
  case kParamGlobalPitchVariationAmplitudeScale: return kPitchVariationAmplitude;
  case kParamGlobalPitchVariationRateScale:      return kPitchVariationRate;
  case kParamGlobalPanVariationAmplitudeScale:   return kPanVariationAmplitude;
  case kParamGlobalPanVariationRateScale:        return kPanVariationRate;
  case kParamPortamentoAtCC5Min:
  case kParamPortamentoAtCC5Max:                 return kPortamento;
  case kParamGlobalLevel:                        return kLevel;
  case kParamEffectsReverb:                      return kReverb;
  case kParamEffectsDrive:                       return kDrive;
  case kParamEffectsChorus:                      return kChorus;
  case kParamEffectsTone:                        return kTone;
  case kParamTranspose:                          return kTranspose;
  default:                                       return "";
  }
}
} // namespace main_ui

namespace oscillator_tabs {
using OscillatorParameter = OscillatorSettings::Parameter;

inline constexpr const char *kEq =
    "Controls a frequency-based EQ for creating formants.";

inline constexpr const char *kLevel =
    "Controls the level of each harmonic at full breath.";

inline constexpr const char *kBreath =
    "Controls per-harmonic breath sensitivity. Higher values make the harmonic require more breath before it becomes prominent.";

inline constexpr const char *kAttack =
    "Controls how quickly each harmonic can increase in level.";

inline constexpr const char *kRelease =
    "Controls how quickly each harmonic can decrease in level.";

inline constexpr const char *kPitch =
    "Controls static pitch offset of each harmonic in cents.";

inline constexpr const char *kPan =
    "Controls stereo position of each harmonic from left to right.";

inline constexpr const char *kLevelVariationAmplitude =
    "Controls depth of level variation for each harmonic.";

inline constexpr const char *kLevelVariationRate =
    "Controls speed of level variation for each harmonic.";

inline constexpr const char *kPitchVariationAmplitude =
    "Controls depth of pitch variation for each harmonic.";

inline constexpr const char *kPitchVariationRate =
    "Controls speed of pitch variation for each harmonic.";

inline constexpr const char *kPanVariationAmplitude =
    "Controls depth of pan variation for each harmonic.";

inline constexpr const char *kPanVariationRate =
    "Controls speed of pan variation for each harmonic.";

inline constexpr const char *kMacrosMode =
    "Switches between the two ways of shaping the curve:\n"
    "Macro: shape the curve with a few knobs.\n\n"
    "Detail: draw the curve by hand, one harmonic at a time.";

inline constexpr const char *kMacroLevelWidth =
    "Sets the width of the harmonic profile. Higher values use more harmonics.";

inline constexpr const char *kMacroLevelShape =
    "Sets the shape of the harmonic profile. Lower values make the curve fall off quickly, "
    "higher values make it fall off slowly.";

inline constexpr const char *kMacroLevelFund =
    "Sets the strength of the fundamental harmonic and its immediate neighbors. Higher values make the "
    "fundamental more prominent, while lower values make it quieter.";

inline constexpr const char *kMacroLevelOddEven =
    "Tilts the balance between odd and even harmonics. Left favours odd harmonics, right favours even harmonics.";

inline constexpr const char *kMacroBreathBase =
    "Sets breath power at harmonic 1. Lower powers respond more strongly to gentle breath, higher powers need more breath.";

inline constexpr const char *kMacroBreathTop =
    "Sets breath power at harmonic 100. At minimum the curve is flat, at maximum the top is 100.";

inline constexpr const char *kMacroBreathShape =
    "Bends the transition from Base to Top. Lower settings rise more gradually near harmonic 1; higher settings rise faster.";

inline constexpr const char *kMacroBreathOddEven =
    "Left favours odd harmonics by adding breath power to even harmonics. "
    "Right favours even harmonics by adding breath power to odd harmonics.";

inline constexpr const char *kMacroAttackBase =
    "Sets attack time at Position.";

inline constexpr const char *kMacroAttackSlope =
    "Sets the slope of the ramp away from Position.";

inline constexpr const char *kMacroAttackPosition =
    "Sets the harmonic at which Base is applied. The slope is applied away from this point.";

inline constexpr const char *kMacroAttackOddEven =
    "Left favours odd harmonics by adding attack time to even harmonics. "
    "Right favours even harmonics by adding attack time to odd harmonics.";

inline constexpr const char *kMacroReleaseBase =
    "Sets release time at Position.";

inline constexpr const char *kMacroReleaseSlope =
    "Sets the slope of the ramp away from Position.";

inline constexpr const char *kMacroReleasePosition =
    "Sets the harmonic at which Base is applied. The slope is applied away from this point.";

inline constexpr const char *kMacroReleaseOddEven =
    "Left favours odd harmonics by adding release time to odd harmonics. "
    "Right favours even harmonics by adding release time to even harmonics.";

inline constexpr const char *kXRangeMin =
    "Controls the lowest harmonic that will be shown in the editor.";

inline constexpr const char *kXRangeMax =
    "Controls the highest harmonic that will be shown in the editor.";

inline constexpr const char *kYTransform =
    "Changes the vertical response of the editor graph. This only affects how values are displayed, not the actual values.\n\n"
    "linear: bar height directly represents value. Twice the height = twice the value.\n\n"
    "square root: expands smaller values on screen, making them easier to edit.\n\n"
    "pseudo-log: strongly expands smaller values on screen, making them easier to edit.";

inline constexpr const char *kEditMode =
    "Selects the edit operation:\n\n"
    "set: writes values directly to the cursor position.\n\n"
    "nudge: makes small gradual changes as you drag.\n\n"
    "smooth: gently averages a harmonic with its neighbors.\n\n"
    "draw line: interpolates a line between the drag start and drag end positions.";

inline constexpr const char *kEditScope =
    "Controls which harmonics can be edited:\n\n"
    "all: all harmonics are editable\n\n"
    "even: even harmonics are editable (odd harmonics are locked)\n\n"
    "odd: odd harmonics are editable (even harmonics are locked)";

inline constexpr const char *kHarmonicSetShape =
    "Applies a preset shape to this parameter. The edit scope limits which harmonics are changed.";

inline constexpr const char *kHarmonicActions =
    "Runs the selected operation. Shortcut keys are shown in brackets. The edit scope limits which harmonics are changed.\n\n"
    "scale up: increases values.\n\n"
    "scale down: decreases values.\n\n"
    "toward max: moves values closer to the top of the range.\n\n"
    "away from max: moves values away from the top of the range.\n\n"
    "bend up: bends the shape upward while keeping its low and high points.\n\n"
    "bend down: bends the shape downward while keeping its low and high points.";

inline constexpr const char *kHarmonicLevelActions =
    "Runs the selected operation. Shortcut keys are shown in brackets. Except normalize, the edit scope limits which harmonics are changed.\n\n"
    "scale up: increases values.\n\n"
    "scale down: decreases values.\n\n"
    "toward max: moves values closer to the top of the range.\n\n"
    "away from max: moves values away from the top of the range.\n\n"
    "bend up: bends the shape upward while keeping its low and high points.\n\n"
    "bend down: bends the shape downward while keeping its low and high points.\n\n"
    "normalize: scales all harmonic levels toward a target RMS level while limiting peaks.";

inline constexpr const char *kHarmonicSignedActions =
    "Runs the selected operation. Shortcut keys are shown in brackets. The edit scope limits which harmonics are changed.\n\n"
    "scale up: increases distance from zero.\n\n"
    "scale down: decreases distance from zero.\n\n"
    "shift up: adds a small positive offset.\n\n"
    "shift down: adds a small negative offset.\n\n"
    "invert: flips values around zero.";

inline constexpr const char *kEqSetShape = "Applies a preset EQ curve.";

inline constexpr const char *kEqActions =
    "Runs the selected operation. Shortcut keys are shown in brackets.\n\n"
    "scale up: makes boosts and cuts stronger.\n\n"
    "scale down: makes boosts and cuts gentler.\n\n"
    "shift up: raises the curve by 1 dB.\n\n"
    "shift down: lowers the curve by 1 dB.\n\n"
    "shift right: moves the curve to higher frequencies.\n\n"
    "shift left: moves the curve to lower frequencies.\n\n"
    "normalize: centers the curve around 0 dB.\n\n"
    "invert: turns boosts into cuts and cuts into boosts.";

inline constexpr const char *kAllNotes =
    "When enabled, this parameter stays synchronized across every key note. Turning it on copies the current values to all key notes.";

inline constexpr const char *kAddButton =
    "Creates a new key note at the currently selected note.";

inline constexpr const char *kDeleteButton =
    "Removes the selected key note.";

inline constexpr const char *kRestoreButton =
    "Restores this tab to the values it had when opened.";

inline const char* Get(OscillatorParameter parameter) {
  switch (parameter) {
  case OscillatorParameter::level:                     return kLevel;
  case OscillatorParameter::breath_power:              return kBreath;
  case OscillatorParameter::attack:                    return kAttack;
  case OscillatorParameter::release:                   return kRelease;
  case OscillatorParameter::pitch:                     return kPitch;
  case OscillatorParameter::pan:                       return kPan;
  case OscillatorParameter::level_variation_amplitude: return kLevelVariationAmplitude;
  case OscillatorParameter::level_variation_rate:      return kLevelVariationRate;
  case OscillatorParameter::pitch_variation_amplitude: return kPitchVariationAmplitude;
  case OscillatorParameter::pitch_variation_rate:      return kPitchVariationRate;
  case OscillatorParameter::pan_variation_amplitude:   return kPanVariationAmplitude;
  case OscillatorParameter::pan_variation_rate:        return kPanVariationRate;
  default:                                             return "";
  }
}

inline const char* GetHarmonicActions(OscillatorParameter parameter) {
  switch (parameter) {
  case OscillatorParameter::level:                     return kHarmonicLevelActions;
  case OscillatorParameter::pitch:
  case OscillatorParameter::pan:                       return kHarmonicSignedActions;
  case OscillatorParameter::breath_power:
  case OscillatorParameter::attack:
  case OscillatorParameter::release:
  case OscillatorParameter::level_variation_amplitude:
  case OscillatorParameter::level_variation_rate:
  case OscillatorParameter::pitch_variation_amplitude:
  case OscillatorParameter::pitch_variation_rate:
  case OscillatorParameter::pan_variation_amplitude:
  case OscillatorParameter::pan_variation_rate:        return kHarmonicActions;
  default:                                             return "";
  }
}
} // namespace oscillator_tabs
} // namespace help_text
} // namespace plugin_ui

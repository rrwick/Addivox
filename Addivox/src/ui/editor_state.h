#pragma once

#include <array>

#include "../midi/breath_control.h"
#include "../settings/oscillator.h"

namespace plugin_ui
{

enum class EditorLevelTransform
{
  Linear,
  SquareRoot,
  PseudoLog
};

enum class EditorOscillatorEditMode
{
  Set,
  Nudge,
  Smooth,
  DrawLine
};

enum class EditorOscillatorEditScope
{
  All,
  Even,
  Odd
};

struct EditorState
{
  CompoundPreset compoundPreset{};
  BreathCCSource breathCCSource{kDefaultBreathCCSource};
  int selectedMidiNote{60};
  bool editMode{false};
  int selectedTabIndex{0};
  int oscillatorXRangeMin{1};
  int oscillatorXRangeMax{SimplePreset::kNumOscillators};
  EditorLevelTransform levelTransform{EditorLevelTransform::Linear};
  EditorLevelTransform breathTransform{EditorLevelTransform::Linear};
  EditorLevelTransform attackTransform{EditorLevelTransform::Linear};
  EditorLevelTransform releaseTransform{EditorLevelTransform::Linear};
  EditorLevelTransform pitchTransform{EditorLevelTransform::Linear};
  EditorLevelTransform panTransform{EditorLevelTransform::Linear};
  std::array<EditorOscillatorEditMode, OscillatorSettings::kNumParameters> oscillatorEditModes = [] {
    std::array<EditorOscillatorEditMode, OscillatorSettings::kNumParameters> result{};
    result.fill(EditorOscillatorEditMode::Set);
    return result;
  }();
  std::array<EditorOscillatorEditScope, OscillatorSettings::kNumParameters> oscillatorEditScopes = [] {
    std::array<EditorOscillatorEditScope, OscillatorSettings::kNumParameters> result{};
    result.fill(EditorOscillatorEditScope::All);
    return result;
  }();
  std::array<EditorLevelTransform, 6> variationTransforms{
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear};
};

} // namespace plugin_ui

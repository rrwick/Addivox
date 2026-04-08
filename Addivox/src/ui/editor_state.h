#pragma once

#include <array>

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
  SetAll,
  SetOdd,
  SetEven
};

struct EditorState
{
  CompoundPreset compoundPreset{};
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
    result.fill(EditorOscillatorEditMode::SetAll);
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

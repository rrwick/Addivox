#pragma once

#include <array>

#include "../settings/presets.h"
#include "../settings/settings_oscillator.h"

namespace plugin_ui
{

enum class EditorLevelTransform
{
  Linear,
  SquareRoot,
  PseudoLog
};

struct EditorState
{
  CompoundPreset compoundPreset{presets::MakeBrassCompoundPreset()};
  int selectedMidiNote{presets::kBrassCompoundPresetKeyNotes[3]};
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
  std::array<EditorLevelTransform, 6> variationTransforms{
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear,
    EditorLevelTransform::Linear};
};

} // namespace plugin_ui

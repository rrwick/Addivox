#pragma once

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
  int levelXRangeMin{1};
  int levelXRangeMax{SimplePreset::kNumOscillators};
  EditorLevelTransform levelTransform{EditorLevelTransform::Linear};
};

} // namespace plugin_ui

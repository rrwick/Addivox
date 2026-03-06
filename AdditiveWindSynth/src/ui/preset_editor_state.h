#pragma once

#include "../settings/presets.h"
#include "../settings/settings_oscillator.h"

namespace plugin_ui
{

enum class PresetEditorLevelTransform
{
  Linear,
  SquareRoot,
  PseudoLog
};

struct PresetEditorState
{
  CompoundPreset compoundPreset{presets::MakeBrassCompoundPreset()};
  int selectedMidiNote{presets::kBrassCompoundPresetKeyNotes[3]};
  bool editMode{false};
  int selectedTabIndex{0};
  int levelXRangeMin{1};
  int levelXRangeMax{SimplePreset::kNumOscillators};
  PresetEditorLevelTransform levelTransform{PresetEditorLevelTransform::Linear};
};

} // namespace plugin_ui

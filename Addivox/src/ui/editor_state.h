#pragma once

#include <array>
#include <cstddef>
#include <mutex>

#include "../midi/breath_control.h"
#include "../settings/oscillator.h"

namespace plugin_ui {

enum class EditorLevelTransform { Linear, SquareRoot, PseudoLog };

// Display transforms used at startup and when entering Macro mode.
inline constexpr std::array<EditorLevelTransform, OscillatorSettings::kNumParameters> kOscillatorTabTransforms{{
    EditorLevelTransform::PseudoLog,  // Level
    EditorLevelTransform::SquareRoot, // Breath
    EditorLevelTransform::SquareRoot, // Attack
    EditorLevelTransform::SquareRoot, // Release
    EditorLevelTransform::PseudoLog,  // Pitch
    EditorLevelTransform::Linear,     // Pan
    EditorLevelTransform::SquareRoot, // LvlVarAmt
    EditorLevelTransform::SquareRoot, // LvlVarRate
    EditorLevelTransform::SquareRoot, // PchVarAmt
    EditorLevelTransform::SquareRoot, // PchVarRate
    EditorLevelTransform::SquareRoot, // PanVarAmt
    EditorLevelTransform::SquareRoot, // PanVarRate
}};

inline constexpr EditorLevelTransform GetOscillatorTabTransform(OscillatorSettings::Parameter parameter) {
  return kOscillatorTabTransforms[static_cast<std::size_t>(parameter)];
}

enum class EditorOscillatorEditMode { Set, Nudge, Smooth, DrawLine };

enum class EditorOscillatorEditScope { All, Even, Odd };

struct EditorState {
  // Guards compoundPatch and the plugin's active-patch metadata. Hosts may call SerializeState/UnserializeState
  // from any thread (AUv3 XPC, VST3), racing main-thread UI edits — copying or iterating compoundPatch while
  // another thread reassigns it crashes. Recursive because locked helpers call each other. This must stay a leaf
  // lock: never call anything that can take iPlug2's params mutex (SerializeParams, RestorePreset, Send*ToDSP,
  // FinalizePatchRecall) while holding it, since iPlug2 calls back into patch-locking code under that mutex.
  std::recursive_mutex patchMutex{};
  CompoundPatch compoundPatch{};
  BreathCCSource breathCCSource{kDefaultBreathCCSource};
  int portamentoCC{5};
  int pitchBendRange{2};
  bool harmonicVisualizerEnabled{true};
  int selectedMidiNote{60};
  bool editMode{false};
  int selectedTabIndex{0};
  int oscillatorXRangeMin{1};
  int oscillatorXRangeMax{SimplePatch::kNumOscillators};
  std::array<EditorLevelTransform, OscillatorSettings::kNumParameters> oscillatorTransforms{kOscillatorTabTransforms};
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
};

} // namespace plugin_ui

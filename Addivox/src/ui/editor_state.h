#pragma once

#include <array>
#include <mutex>

#include "../midi/breath_control.h"
#include "../settings/oscillator.h"

namespace plugin_ui {

enum class EditorLevelTransform { Linear, SquareRoot, PseudoLog };

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
  int pitchBendRange{2};
  bool harmonicVisualizerEnabled{true};
  int selectedMidiNote{60};
  bool editMode{false};
  int selectedTabIndex{0};
  int oscillatorXRangeMin{1};
  int oscillatorXRangeMax{SimplePatch::kNumOscillators};
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
  std::array<EditorLevelTransform, 6> variationTransforms{EditorLevelTransform::Linear, EditorLevelTransform::Linear, EditorLevelTransform::Linear,
                                                          EditorLevelTransform::Linear, EditorLevelTransform::Linear, EditorLevelTransform::Linear};
};

} // namespace plugin_ui

#pragma once

#include "IControls.h"
#include "IPlug_include_in_plug_hdr.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "midi/breath_control.h"
#include "settings/oscillator.h"
#include "ui/editor_state.h"
#include "visualizer/harmonic_visualizer_sender.h"

constexpr int kMaxFactoryPatches = 128;

namespace plugin_ui {
namespace editor {
struct EditorContext;
} // namespace editor
} // namespace plugin_ui

namespace patch_io {
struct PatchDocument;
} // namespace patch_io

#if IPLUG_DSP
#include "synth/synth_engine.h"
#endif

enum EControlTags {
  kCtrlTagMeter = 0,
  kCtrlTagBreathMeter,
  kCtrlTagHarmonicVisualizer,
  kCtrlTagEditorTabs,
  kCtrlTagKeyboard,
  kCtrlTagBender,
  kCtrlTagAboutBox,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class Addivox final : public Plugin {
public:
  Addivox(const InstanceInfo& info);
  void SendMidiMsgFromUI(const IMidiMsg& msg) override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  void OnRestoreState() override;
  void OnHostIdentified() override;
  void OnUIOpen() override;
  void OnUIClose() override;
  bool OnHostRequestingAboutBox() override;
  bool OnHostRequestingProductHelp() override;
  void OnParamChangeUI(int paramIdx, EParamSource source) override;

#if IPLUG_DSP // http://bit.ly/2S64BDd
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnIdle() override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;
#endif

private:
  enum class PatchSource { Unknown, Factory, User };

  struct PatchCatalogEntry {
    int id{-1};
    PatchSource source{PatchSource::Unknown};
    int factoryIndex{-1};
    std::string name;
    std::string path;
    std::string groupKey;
    std::vector<std::string> menuPath;
  };

  void ApplyPatchDocument(const patch_io::PatchDocument& document);
  void EnsureStandaloneStateInitialized();
  bool LoadStandaloneState();
  bool SaveStandaloneState() const;
  void SaveStandaloneStateIfNeeded(bool force = false);
  void MarkStandaloneStateDirty();
  void ResetStandaloneStateToDefaults();
  void SetPitchBendRange(int pitchBendRange);
  void SetBreathCCSource(BreathCCSource source);
  void SetHarmonicVisualizerEnabled(bool enabled);
  bool HasPendingRestoredState() const;
  void ApplyPendingRestoredState();
  void SendBreathControlFromUI(double value, int channel, int offset);
  void PromptLoadPatchFromFile();
  void PromptSavePatchToFile();
  void PromptImportPatchFromFile();
  void PromptImportPatchCollection();
  void ShowActivePatchInFileBrowser();
  void HandlePatchManagerAction(int action, int patchId);
  void RebuildPatchCatalog();
  void ReconcileActivePatchIdentity();
  void RestoreFactoryPatch(int patchIdx);
  void LoadUserPatchByPath(const std::string& path);
  void LoadPatchById(int patchId);
  void CyclePatchInCurrentGroup(int direction);
  std::string SerializeCurrentPatchSnapshot() const;
  void SetActivePatchCleanSnapshotFromCurrentState();
  void MarkActivePatchDirty();
  void ClearActivePatchDirty();
  bool ShowAboutBox();
  bool OpenOnlineDocs();
  void LoadBuiltInPatches();
  void RefreshEditorUI(bool resetOscillatorRestoreStates = false);
  void SyncPitchBendRangeUI();

  std::array<bool, 128> mActiveUIMIDINotes{};
  int mNumActiveUIMIDINotes{0};
  std::shared_ptr<plugin_ui::EditorState> mEditorState;
  std::shared_ptr<plugin_ui::editor::EditorContext> mEditorContext;
  std::string mActivePatchDisplayName;
  std::string mPendingRestoredStatePatchName;
  std::string mPendingRestoredPatchCleanSnapshot;
  bool mPendingRestoredPatchDirty{false};
  bool mPendingRestoredPatchHasDirtyState{false};
  bool mPendingRestoredPatchHasCleanSnapshot{false};
  PatchSource mPendingRestoredPatchSource{PatchSource::Unknown};
  int mPendingRestoredFactoryPatchIdx{-1};
  std::string mPendingRestoredPatchPath;
  std::string mPendingRestoredPatchGroupKey;
  bool mPendingRestoredPatchHasIdentity{false};
  std::string mUserPatchDirectory;
  std::string mActivePatchPath;
  std::string mActivePatchGroupKey{"Factory"};
  PatchSource mActivePatchSource{PatchSource::Unknown};
  int mActiveFactoryPatchIdx{-1};
  std::vector<std::string> mFactoryPatchPaths;
  std::vector<PatchCatalogEntry> mPatchCatalog;
  std::string mActivePatchCleanSnapshot;
  int mRestoringFactoryPatchIdx{-1};
  bool mActivePatchDirty{false};
  bool mSuppressPatchDirtyTracking{false};
  std::array<bool, 128> mQwertyMidiKeysDown{};
  int mQwertyMidiBaseNote{48};
  bool mWasQwertyKeyboardInEditMode{false};
  int mLastQwertyMIDINote{-1};
  int mPitchBendRange{2};
  bool mStandaloneStateInitialized{false};
  bool mSuppressStandaloneStatePersistence{false};
  uint64_t mStandaloneStateRevision{0};
  uint64_t mStandaloneStateSavedRevision{0};
  int64_t mStandaloneStateLastDirtyMillis{0};
  BreathCCSource mBreathCCSource{kDefaultBreathCCSource};
  std::atomic<bool> mHarmonicVisualizerEnabled{true};
  std::atomic<bool> mHarmonicVisualizerBlankPending{false};

#if IPLUG_DSP // http://bit.ly/2S64BDd
  using VisualizerFrame = SynthEngine::VisualizerFrame;

  SynthEngine mDSP;
  IPeakAvgSender<2> mMeterSender;
  HarmonicVisualizerSender<VisualizerFrame> mHarmonicVisualizerSender;
  BreathCCInputTracker mBreathCCInputTracker;
  std::atomic<double> mBreathLevel{1.0};
  double mLastSentBreathLevel{-1.};
#endif
};

#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
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

constexpr int kMaxFactoryPresets = 128;

namespace plugin_ui
{
namespace editor
{
struct EditorContext;
} // namespace editor
} // namespace plugin_ui

namespace preset_io
{
struct PresetDocument;
} // namespace preset_io

#if IPLUG_DSP
#include "synth/synth_engine.h"
#endif

enum EControlTags
{
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

class Addivox final : public Plugin
{
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
  enum class PresetSource
  {
    Unknown,
    Factory,
    User
  };

  struct PresetCatalogEntry
  {
    int id{-1};
    PresetSource source{PresetSource::Unknown};
    int factoryIndex{-1};
    std::string name;
    std::string path;
    std::string groupKey;
    std::vector<std::string> menuPath;
  };

  void ApplyPresetDocument(const preset_io::PresetDocument& document);
  void EnsureStandaloneStateInitialized();
  bool LoadStandaloneState();
  bool SaveStandaloneState() const;
  void SaveStandaloneStateIfNeeded(bool force = false);
  void MarkStandaloneStateDirty();
  void ResetStandaloneStateToDefaults();
  void SetPitchBendRange(int pitchBendRange);
  void SetBreathCCSource(BreathCCSource source);
  void SetHarmonicVisualizerEnabled(bool enabled);
  void SendBreathControlFromUI(double value, int channel, int offset);
  void PromptLoadPresetFromFile();
  void PromptSavePresetToFile();
  void PromptImportPresetFromFile();
  void PromptImportPresetCollection();
  void ShowActivePresetInFileBrowser();
  void HandlePresetManagerAction(int action, int presetId);
  void RebuildPresetCatalog();
  void RestoreFactoryPreset(int presetIdx);
  void LoadUserPresetByPath(const std::string& path);
  void LoadPresetById(int presetId);
  void CyclePresetInCurrentGroup(int direction);
  std::string SerializeCurrentPresetSnapshot() const;
  void SetActivePresetCleanSnapshotFromCurrentState();
  void MarkActivePresetDirty();
  void ClearActivePresetDirty();
  bool ShowAboutBox();
  bool OpenOnlineDocs();
  void LoadBuiltInPresets();
  void RefreshEditorUI(bool resetOscillatorRestoreStates = false);
  void SyncPitchBendRangeUI();

  std::array<bool, 128> mActiveUIMIDINotes{};
  int mNumActiveUIMIDINotes{0};
  std::shared_ptr<plugin_ui::EditorState> mEditorState;
  std::shared_ptr<plugin_ui::editor::EditorContext> mEditorContext;
  std::string mActivePresetDisplayName;
  std::string mPendingRestoredStatePresetName;
  std::string mPendingRestoredPresetCleanSnapshot;
  bool mPendingRestoredPresetHasCleanSnapshot{false};
  std::string mUserPresetDirectory;
  std::string mActivePresetPath;
  std::string mActivePresetGroupKey{"Factory"};
  PresetSource mActivePresetSource{PresetSource::Unknown};
  std::vector<std::string> mFactoryPresetPaths;
  std::vector<PresetCatalogEntry> mPresetCatalog;
  std::string mActivePresetCleanSnapshot;
  int mRestoringFactoryPresetIdx{-1};
  bool mActivePresetDirty{false};
  bool mSuppressPresetDirtyTracking{false};
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

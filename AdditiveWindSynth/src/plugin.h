#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include <array>
#include <atomic>
#include <memory>
#include <string>

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
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class AdditiveWindSynth final : public Plugin
{
public:
  AdditiveWindSynth(const InstanceInfo& info);
  void SendMidiMsgFromUI(const IMidiMsg& msg) override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  void OnRestoreState() override;
  void OnUIOpen() override;
  void OnUIClose() override;

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
  void ApplyPresetDocument(const preset_io::PresetDocument& document);
  void PromptLoadPresetFromFile();
  void PromptSavePresetToFile();
  void LoadBuiltInPresets();
  void RefreshEditorUI(bool resetOscillatorRestoreStates = false);

  std::array<bool, 128> mActiveUIMIDINotes{};
  int mNumActiveUIMIDINotes{0};
  std::shared_ptr<plugin_ui::EditorState> mEditorState;
  std::shared_ptr<plugin_ui::editor::EditorContext> mEditorContext;
  std::string mActivePresetDisplayName;
  std::string mPendingRestoredStatePresetName;
  std::string mUserPresetDirectory;

#if IPLUG_DSP // http://bit.ly/2S64BDd
  using VisualizerFrame = SynthEngine::VisualizerFrame;

  SynthEngine mDSP;
  IPeakAvgSender<2> mMeterSender;
  HarmonicVisualizerSender<VisualizerFrame> mHarmonicVisualizerSender;
  int mLastQwertyMIDINote{-1};
  std::atomic<double> mBreathLevel{1.0};
  double mLastSentBreathLevel{-1.};
#endif
};

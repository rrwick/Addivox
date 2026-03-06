#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include <atomic>
#include <memory>

#include "settings/settings_oscillator.h"
#include "ui/preset_editor_state.h"
#include "visualizer/harmonic_visualizer_sender.h"

const int kNumPresets = 1;

#if IPLUG_DSP
#include "synth/synth_engine.h"
#endif

enum EControlTags
{
  kCtrlTagMeter = 0,
  kCtrlTagBreathMeter,
  kCtrlTagHarmonicVisualizer,
  kCtrlTagPresetEditorTabs,
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
  std::shared_ptr<plugin_ui::PresetEditorState> mPresetEditorState;

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

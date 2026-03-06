#include "plugin.h"
#include "IPlug_include_in_plug_src.h"
#include "settings/params.h"
#include "settings/settings_global.h"
#include "preset_editor_messages.h"
#include "ui/transformations.h"
#include "ui/ui_layout.h"

#include <cmath>

AdditiveWindSynth::AdditiveWindSynth(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
, mPresetEditorState(std::make_shared<plugin_ui::PresetEditorState>())
{
  const auto formatPseudoLogScaleDisplay = [](double value, WDL_String& str) {
    int decimals = 2;
    if (value >= 10.0)  decimals = 1;
    if (value >= 100.0) decimals = 0;
    const double scale = (decimals == 2 ? 100.0 : (decimals == 1 ? 10.0 : 1.0));
    const double rounded = std::round(value * scale) / scale;
    str.SetFormatted(32, "%.*f\xC3\x97", decimals, rounded);
  };

  GetParam(kParamGain)->InitDouble("Gain", 1.0, 0., 10.0, 0.01, "", 0, "",
    transformations::GetGainPseudoLogShape(),
    iplug::IParam::kUnitCustom,
    formatPseudoLogScaleDisplay);

  const auto initPseudoLogScale = [this, formatPseudoLogScaleDisplay](int paramIdx, const char* name, double defaultValue = 1.0) {
    GetParam(paramIdx)->InitDouble(name, defaultValue, 0., 100., 0.01, "", 0, "",
      transformations::GetGlobalPseudoLogShape(),
      iplug::IParam::kUnitCustom,
      formatPseudoLogScaleDisplay);
  };
  initPseudoLogScale(kParamGlobalAttackScale, "Global Attack");
  initPseudoLogScale(kParamGlobalReleaseScale, "Global Release");
  GetParam(kParamGlobalPitchShift)->InitDouble("Global Pitch Shift", 0., -50., 50., 0.1, "cents");
  GetParam(kParamGlobalPanShift)->InitDouble("Global Pan Shift", 0., -1., 1., 0.01, "", iplug::IParam::kFlagSignDisplay);
  initPseudoLogScale(kParamGlobalIntensityVariationAmplitudeScale, "Global Intensity Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalIntensityVariationRateScale, "Global Intensity Variation Rate");
  initPseudoLogScale(kParamGlobalPitchVariationAmplitudeScale, "Global Pitch Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPitchVariationRateScale, "Global Pitch Variation Rate");
  initPseudoLogScale(kParamGlobalPanVariationAmplitudeScale, "Global Pan Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPanVariationRateScale, "Global Pan Variation Rate");
  const auto& portamentoShape = transformations::GetPortamentoPseudoLogShape();
  GetParam(kParamPortamentoAtCC5Min)->InitDouble("Portamento (CC5=0)", 0.001, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
  GetParam(kParamPortamentoAtCC5Max)->InitDouble("Portamento (CC5=127)", 0.025, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
    
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachSVGBackground("background.svg");
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableMultiTouch(true);
    
#ifdef OS_WEB
    pGraphics->AttachPopupMenuControl();
#endif

    // pGraphics->EnableLiveEdit(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Roboto-Bold", ROBOTO_BOLD_FN);
    pGraphics->LoadFont("Roboto-Black", ROBOTO_BLACK_FN);
    plugin_ui::AttachMainControls(
      pGraphics,
      mPresetEditorState,
      kParamGain,
      global_settings::kModifierParamIndices,
      kParamPortamentoAtCC5Min,
      kParamPortamentoAtCC5Max,
      kCtrlTagHarmonicVisualizer,
      kCtrlTagPresetEditorTabs,
      kCtrlTagKeyboard,
      kCtrlTagBender,
      kCtrlTagBreathMeter,
      kCtrlTagMeter);
    
    pGraphics->SetQwertyMidiKeyHandlerFunc([this, pGraphics](const IMidiMsg& msg) {
      plugin_ui::HandleQwertyMidi(pGraphics, kCtrlTagKeyboard, mLastQwertyMIDINote, msg);
    });
  };
#endif
}

#if IPLUG_DSP
void AdditiveWindSynth::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  (void) inputs;
  mDSP.ProcessBlock(outputs, nFrames);
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
  mHarmonicVisualizerSender.ProcessBlock(
    nFrames,
    kCtrlTagHarmonicVisualizer,
    [this](VisualizerFrame& frame) { mDSP.GetVisualizerFrame(frame); });
}

void AdditiveWindSynth::OnIdle()
{
  mMeterSender.TransmitData(*this);
  mHarmonicVisualizerSender.TransmitData(*this);
  const double breathLevel = mBreathLevel.load(std::memory_order_relaxed);
  if(breathLevel != mLastSentBreathLevel)
  {
    SendControlValueFromDelegate(kCtrlTagBreathMeter, breathLevel);
    mLastSentBreathLevel = breathLevel;
  }
}

void AdditiveWindSynth::OnReset()
{
  mDSP.Reset(GetSampleRate(), GetBlockSize());

  // Make the meter respond more quickly to changes in level.
  mMeterSender.Reset(GetSampleRate());
  mMeterSender.SetDecayTimeMs(50.0, GetSampleRate());
  mMeterSender.SetPeakHoldTimeMs(250.0, GetSampleRate());
  
  mHarmonicVisualizerSender.Reset(GetSampleRate(), PLUG_FPS);
  mLastQwertyMIDINote = -1;
  mBreathLevel.store(1.0, std::memory_order_relaxed);
  mLastSentBreathLevel = -1.;
}

void AdditiveWindSynth::ProcessMidiMsg(const IMidiMsg& msg)
{
  if(msg.StatusMsg() == IMidiMsg::kControlChange && msg.ControlChangeIdx() == IMidiMsg::kBreathController)
  {
    mBreathLevel.store(static_cast<double>(msg.ControlChange(IMidiMsg::kBreathController)), std::memory_order_relaxed);
  }

  mDSP.ProcessMidiMsg(msg);
  SendMidiMsg(msg);
}

void AdditiveWindSynth::OnParamChange(int paramIdx)
{
  mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

bool AdditiveWindSynth::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if(ctrlTag == kCtrlTagPresetEditorTabs
    && msgTag == preset_editor_messages::kMsgTagSetKeyNoteOscillatorParameter
    && dataSize == sizeof(preset_editor_messages::SetKeyNoteOscillatorParameterPayload)
    && pData)
  {
    const auto* payload = static_cast<const preset_editor_messages::SetKeyNoteOscillatorParameterPayload*>(pData);
    if(payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters)
      return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    return mDSP.SetKeyNoteOscillatorParameter(payload->midiNote, payload->oscillatorIndex, parameter, payload->value);
  }

  if(ctrlTag == kCtrlTagPresetEditorTabs
    && msgTag == preset_editor_messages::kMsgTagSetKeyNoteOscillatorParameterValues
    && dataSize == sizeof(preset_editor_messages::SetKeyNoteOscillatorParameterValuesPayload)
    && pData)
  {
    const auto* payload = static_cast<const preset_editor_messages::SetKeyNoteOscillatorParameterValuesPayload*>(pData);
    if(payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters)
      return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    return mDSP.SetKeyNoteOscillatorParameterValues(payload->midiNote, parameter, payload->values);
  }

  if(ctrlTag == kCtrlTagPresetEditorTabs
    && msgTag == preset_editor_messages::kMsgTagAddKeyNotePreset
    && dataSize == sizeof(preset_editor_messages::KeyNotePresetPayload)
    && pData)
  {
    const auto* payload = static_cast<const preset_editor_messages::KeyNotePresetPayload*>(pData);
    return mDSP.AddKeyNotePreset(payload->midiNote);
  }

  if(ctrlTag == kCtrlTagPresetEditorTabs
    && msgTag == preset_editor_messages::kMsgTagRemoveKeyNotePreset
    && dataSize == sizeof(preset_editor_messages::KeyNotePresetPayload)
    && pData)
  {
    const auto* payload = static_cast<const preset_editor_messages::KeyNotePresetPayload*>(pData);
    return mDSP.RemoveKeyNotePreset(payload->midiNote);
  }

  if(ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    mDSP.mSynth.SetPitchBendRange(bendRange);
  }
  
  return false;
}
#endif

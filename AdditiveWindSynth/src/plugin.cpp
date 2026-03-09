#include "plugin.h"
#include "IPlug_include_in_plug_src.h"
#include "settings/params.h"
#include "editor_messages.h"
#include "ui/transformations.h"
#include "ui/layout.h"

#include <cmath>

AdditiveWindSynth::AdditiveWindSynth(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
, mEditorState(std::make_shared<plugin_ui::EditorState>())
{
  const auto formatPseudoLogScaleDisplay = [](double value, WDL_String& str) {
    int decimals = 2;
    if (value >= 10.0)  decimals = 1;
    if (value >= 100.0) decimals = 0;
    const double scale = (decimals == 2 ? 100.0 : (decimals == 1 ? 10.0 : 1.0));
    const double rounded = std::round(value * scale) / scale;
    str.SetFormatted(32, "%.*f\xC3\x97", decimals, rounded);
  };

  const auto initPseudoLogScale = [this, formatPseudoLogScaleDisplay](int paramIdx, const char* name, double defaultValue = 1.0) {
    GetParam(paramIdx)->InitDouble(name, defaultValue, 0., 100., 0.01, "", 0, "",
      transformations::GetGlobalPseudoLogShape(),
      iplug::IParam::kUnitCustom,
      formatPseudoLogScaleDisplay);
  };

  GetParam(kParamGlobalLevel)->InitDouble("Level", 1.0, 0., 10.0, 0.01, "", 0, "", transformations::GetLevelPseudoLogShape(), iplug::IParam::kUnitCustom, formatPseudoLogScaleDisplay);
  initPseudoLogScale(kParamGlobalAttackScale, "Attack");
  initPseudoLogScale(kParamGlobalReleaseScale, "Release");
  GetParam(kParamGlobalPitchShift)->InitDouble("Pitch Shift", 0., -50., 50., 0.1, "cents");
  GetParam(kParamGlobalPanShift)->InitDouble("Pan Shift", 0., -1., 1., 0.01, "", iplug::IParam::kFlagSignDisplay);
  initPseudoLogScale(kParamGlobalIntensityVariationAmplitudeScale, "Level Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalIntensityVariationRateScale, "Level Variation Rate");
  initPseudoLogScale(kParamGlobalPitchVariationAmplitudeScale, "Pitch Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPitchVariationRateScale, "Pitch Variation Rate");
  initPseudoLogScale(kParamGlobalPanVariationAmplitudeScale, "Pan Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPanVariationRateScale, "Pan Variation Rate");
  const auto& portamentoShape = transformations::GetPortamentoPseudoLogShape();
  GetParam(kParamPortamentoAtCC5Min)->InitDouble("Portamento (min)", 0.001, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
  GetParam(kParamPortamentoAtCC5Max)->InitDouble("Portamento (max)", 0.025, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
    
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
      mEditorState,
      kCtrlTagHarmonicVisualizer,
      kCtrlTagEditorTabs,
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

void AdditiveWindSynth::SendMidiMsgFromUI(const IMidiMsg& msg)
{
  const IMidiMsg::EStatusMsg status = msg.StatusMsg();
  const bool isNoteOn = (status == IMidiMsg::kNoteOn) && (msg.Velocity() > 0);
  const bool isNoteOff = (status == IMidiMsg::kNoteOff) || ((status == IMidiMsg::kNoteOn) && (msg.Velocity() == 0));

  if(!isNoteOn && !isNoteOff)
  {
    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  const int midiNote = msg.NoteNumber();
  if(midiNote < 0 || midiNote >= static_cast<int>(mActiveUIMIDINotes.size()))
  {
    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  const auto sendBreathFromUI = [this, &msg](double value) {
    IMidiMsg breathMsg;
    breathMsg.MakeControlChangeMsg(IMidiMsg::kBreathController, value, msg.Channel(), msg.mOffset);
    Plugin::SendMidiMsgFromUI(breathMsg);
  };

  if(isNoteOn)
  {
    if(!mActiveUIMIDINotes[static_cast<size_t>(midiNote)])
    {
      if(mNumActiveUIMIDINotes == 0)
        sendBreathFromUI(1.0);

      mActiveUIMIDINotes[static_cast<size_t>(midiNote)] = true;
      ++mNumActiveUIMIDINotes;
    }

    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  bool releasedActiveUINote = false;
  if(mActiveUIMIDINotes[static_cast<size_t>(midiNote)])
  {
    mActiveUIMIDINotes[static_cast<size_t>(midiNote)] = false;
    --mNumActiveUIMIDINotes;
    releasedActiveUINote = true;
  }

  Plugin::SendMidiMsgFromUI(msg);

  if(releasedActiveUINote && mNumActiveUIMIDINotes == 0)
    sendBreathFromUI(0.0);
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
  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagSetKeyNoteOscillatorParameter
    && dataSize == sizeof(editor_messages::SetKeyNoteOscillatorParameterPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetKeyNoteOscillatorParameterPayload*>(pData);
    if(payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters)
      return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    return mDSP.SetKeyNoteOscillatorParameter(payload->midiNote, payload->oscillatorIndex, parameter, payload->value);
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagSetKeyNoteOscillatorParameterValues
    && dataSize == sizeof(editor_messages::SetKeyNoteOscillatorParameterValuesPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetKeyNoteOscillatorParameterValuesPayload*>(pData);
    if(payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters)
      return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    return mDSP.SetKeyNoteOscillatorParameterValues(payload->midiNote, parameter, payload->values);
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagAddKeyNotePreset
    && dataSize == sizeof(editor_messages::KeyNotePresetPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::KeyNotePresetPayload*>(pData);
    return mDSP.AddKeyNotePreset(payload->midiNote);
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagRemoveKeyNotePreset
    && dataSize == sizeof(editor_messages::KeyNotePresetPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::KeyNotePresetPayload*>(pData);
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

#include "plugin.h"
#include "IPlug_include_in_plug_src.h"
#include "ui_layout.h"

AdditiveWindSynth::AdditiveWindSynth(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");
    
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableMultiTouch(true);
    
#ifdef OS_WEB
    pGraphics->AttachPopupMenuControl();
#endif

    // pGraphics->EnableLiveEdit(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    plugin_ui::AttachMainControls(pGraphics, kParamGain, kCtrlTagKeyboard, kCtrlTagBender, kCtrlTagBreathMeter, kCtrlTagMeter);
    
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
}

void AdditiveWindSynth::OnIdle()
{
  mMeterSender.TransmitData(*this);
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
  mMeterSender.Reset(GetSampleRate());
  mLastQwertyMIDINote = -1;
  mBreathLevel.store(1.f, std::memory_order_relaxed);
  mLastSentBreathLevel = -1.;
}

void AdditiveWindSynth::ProcessMidiMsg(const IMidiMsg& msg)
{
  if(msg.StatusMsg() == IMidiMsg::kControlChange && msg.ControlChangeIdx() == IMidiMsg::kBreathController)
  {
    mBreathLevel.store(static_cast<float>(msg.ControlChange(IMidiMsg::kBreathController)), std::memory_order_relaxed);
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
  if(ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    mDSP.mSynth.SetPitchBendRange(bendRange);
  }
  
  return false;
}
#endif

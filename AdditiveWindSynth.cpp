#include "AdditiveWindSynth.h"
#include "IPlug_include_in_plug_src.h"

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

    // Full UI: 1000 x 500
    const IRECT wheelsBounds = IRECT::MakeXYWH(5.f, 370.f, 35.f, 120.f);
    const IRECT keyboardBounds = IRECT::MakeXYWH(45.f, 370.f, 945.f, 120.f);
    const IRECT breathMeterBounds = IRECT::MakeXYWH(900.f, 10.f, 40.f, 200.f);
    const IRECT outMeterBounds = IRECT::MakeXYWH(950.f, 10.f, 40.f, 200.f);
    const IRECT gainKnobBounds = IRECT::MakeXYWH(900.f, 220.f, 90.f, 90.f);

    pGraphics->AttachControl(new IVKeyboardControl(keyboardBounds, 21, 108), kCtrlTagKeyboard);
    pGraphics->AttachControl(new IWheelControl(wheelsBounds), kCtrlTagBender);
    pGraphics->AttachControl(new IVKnobControl(gainKnobBounds, kParamGain, "Gain"));
    pGraphics->AttachControl(new IVMeterControl<1>(breathMeterBounds, "Breath"), kCtrlTagBreathMeter);
    pGraphics->AttachControl(new IVLEDMeterControl<2>(outMeterBounds, "Out"), kCtrlTagMeter);
    
//#ifdef OS_IOS
//    if(!IsOOPAuv3AppExtension())
//    {
//      pGraphics->AttachControl(new IVButtonControl(XYWH(uiBounds.R - 110.f, 10.f, 100.f, 100.f), [pGraphics](IControl* pCaller) {
//                               dynamic_cast<IGraphicsIOS*>(pGraphics)->LaunchBluetoothMidiDialog(pCaller->GetRECT().L, pCaller->GetRECT().MH());
//                               SplashClickActionFunc(pCaller);
//                             }, "BTMIDI"));
//    }
//#endif
    
    pGraphics->SetQwertyMidiKeyHandlerFunc([this, pGraphics](const IMidiMsg& msg) {
      auto* keyboard = pGraphics->GetControlWithTag(kCtrlTagKeyboard)->As<IVKeyboardControl>();
      const int note = msg.NoteNumber();
      const bool noteOn = (msg.StatusMsg() == IMidiMsg::kNoteOn) && (msg.Velocity() > 0);

      if(noteOn)
      {
        if(mLastQwertyMIDINote >= 0 && mLastQwertyMIDINote != note)
          keyboard->SetNoteFromMidi(mLastQwertyMIDINote, false);

        keyboard->SetNoteFromMidi(note, true);
        mLastQwertyMIDINote = note;
      }
      else
      {
        if(note == mLastQwertyMIDINote)
        {
          keyboard->SetNoteFromMidi(note, false);
          mLastQwertyMIDINote = -1;
        }
      }
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

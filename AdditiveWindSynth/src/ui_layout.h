#pragma once

#include "IControls.h"
#include "harmonic_visualizer_control.h"

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

inline void AttachMainControls(
  IGraphics* pGraphics,
  int gainParamIdx,
  int harmonicVisualizerTag,
  int keyboardTag,
  int benderTag,
  int breathMeterTag,
  int outMeterTag)
{
  // Full UI: 1000 x 500
  const IRECT harmonicVisualizerBounds = IRECT::MakeXYWH(45.f, 20.f, 845.f, 280.f);
  const IRECT wheelsBounds = IRECT::MakeXYWH(5.f, 370.f, 35.f, 120.f);
  const IRECT keyboardBounds = IRECT::MakeXYWH(45.f, 370.f, 945.f, 120.f);
  const IRECT breathMeterBounds = IRECT::MakeXYWH(900.f, 10.f, 40.f, 200.f);
  const IRECT outMeterBounds = IRECT::MakeXYWH(950.f, 10.f, 40.f, 200.f);
  const IRECT gainKnobBounds = IRECT::MakeXYWH(900.f, 220.f, 90.f, 90.f);

  pGraphics->AttachControl(new HarmonicVisualizerControl(harmonicVisualizerBounds), harmonicVisualizerTag);
  pGraphics->AttachControl(new IVKeyboardControl(keyboardBounds, 21, 108), keyboardTag);
  pGraphics->AttachControl(new IWheelControl(wheelsBounds), benderTag);
  pGraphics->AttachControl(new IVKnobControl(gainKnobBounds, gainParamIdx, "Gain"));
  pGraphics->AttachControl(new IVMeterControl<1>(breathMeterBounds, "Breath"), breathMeterTag);
  pGraphics->AttachControl(new IVLEDMeterControl<2>(outMeterBounds, "Out"), outMeterTag);
}

inline void HandleQwertyMidi(IGraphics* pGraphics, int keyboardTag, int& lastQwertyMIDINote, const IMidiMsg& msg)
{
  auto* keyboard = pGraphics->GetControlWithTag(keyboardTag)->As<IVKeyboardControl>();
  const int note = msg.NoteNumber();
  const bool noteOn = (msg.StatusMsg() == IMidiMsg::kNoteOn) && (msg.Velocity() > 0);

  if(noteOn)
  {
    if(lastQwertyMIDINote >= 0 && lastQwertyMIDINote != note)
      keyboard->SetNoteFromMidi(lastQwertyMIDINote, false);

    keyboard->SetNoteFromMidi(note, true);
    lastQwertyMIDINote = note;
  }
  else if(note == lastQwertyMIDINote)
  {
    keyboard->SetNoteFromMidi(note, false);
    lastQwertyMIDINote = -1;
  }
}
} // namespace plugin_ui

#pragma once

#include "IControls.h"
#include "colour.h"
#include "layered_svg_knob_control.h"
#include "../visualizer/harmonic_visualizer_control.h"

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace theme
{
inline IVStyle BaseStyle(bool showLabel = true, bool showValue = true)
{
  return DEFAULT_STYLE
    .WithShowLabel(showLabel)
    .WithShowValue(showValue)
    .WithLabelText(IText(13.f, colour::ui::kLabelText, "Roboto-Regular", EAlign::Center, EVAlign::Top))
    .WithValueText(IText(12.f, colour::ui::kValueText, "Roboto-Regular", EAlign::Center, EVAlign::Bottom))
    .WithDrawShadows(false)
    .WithDrawFrame(true)
    .WithFrameThickness(1.15f)
    .WithRoundness(0.2f)
    .WithColor(kBG, COLOR_TRANSPARENT)
    .WithColor(kFG, colour::ui::kControlBody)
    .WithColor(kPR, colour::ui::kAccentPrimary)
    .WithColor(kFR, colour::ui::kControlFrame)
    .WithColor(kHL, colour::ui::kControlHighlight)
    .WithColor(kSH, colour::ui::kControlShadow)
    .WithColor(kX1, colour::ui::kAccentPrimary)
    .WithColor(kX2, colour::ui::kAccentSecondary)
    .WithColor(kX3, colour::ui::kAccentPrimary);
}

inline IVStyle MeterStyle()
{
  return BaseStyle(true, false)
    .WithRoundness(0.1f)
    .WithColor(kBG, IColor{128, 0, 0, 0})
    .WithColor(kFG, colour::ui::kMeterForeground)
    .WithColor(kX1, colour::ui::kAccentSecondary)
    .WithColor(kHL, colour::ui::kMeterHighlight);
}
} // namespace theme

inline void AttachMainControls(
  IGraphics* pGraphics,
  int gainParamIdx,
  const std::array<int, 10>& globalModifierParamIdxs,
  int portamentoAtCC5MinParamIdx,
  int portamentoAtCC5MaxParamIdx,
  int harmonicVisualizerTag,
  int keyboardTag,
  int benderTag,
  int breathMeterTag,
  int outMeterTag)
{
  // Full UI: 1000 x 550

  float knob_size = 52.f;
  const IVStyle knobStyle = theme::BaseStyle();
  const IText compactLabelText = IText(14.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Near);
  const IText compactValueText = IText(14.f, colour::ui::kValueText, "Roboto-Black", EAlign::Near);
  const IText compactValueTextRightAlign = IText(14.f, colour::ui::kValueText, "Roboto-Black", EAlign::Far);
  const IText portamentoValueText = IText(12.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center);
  const IVStyle meterStyle = theme::MeterStyle();
  const ISVG knobFixedSVG = pGraphics->LoadSVG("knob-fixed.svg");
  const ISVG knobRotatingSVG = pGraphics->LoadSVG("knob-rotating.svg");
  const auto attachKnob = [&](const IRECT& bounds, int paramIdx) {
    pGraphics->AttachControl(new LayeredSVGKnobControl(bounds, knobFixedSVG, knobRotatingSVG, paramIdx));
  };

  // The top right panel has the output meter.
  using OutMeterLEDRange = IVLEDMeterControl<2>::LEDRange;
  constexpr int kOutMeterNumBars = 26;  // Count and range are chosen so the first red bar is centered on 0 dBFS and therefore begins lighting just above 0 dB. So seeing the meter hit red indicates potential clipping.
  constexpr float kOutMeterLowDB = -73.5f;
  constexpr float kOutMeterHighDB = 4.5f;
  constexpr float kOutMeterStepDB = (kOutMeterHighDB - kOutMeterLowDB) / static_cast<float>(kOutMeterNumBars);
  const std::array<IColor, kOutMeterNumBars> outMeterLEDColors = {
    colour::visualizer::LED26, colour::visualizer::LED25,  // top levels (>0 dB) are bright red
    colour::visualizer::LED24, colour::visualizer::LED23, colour::visualizer::LED22, colour::visualizer::LED21,
    colour::visualizer::LED20, colour::visualizer::LED19, colour::visualizer::LED18, colour::visualizer::LED17,
    colour::visualizer::LED16, colour::visualizer::LED15, colour::visualizer::LED14, colour::visualizer::LED13,
    colour::visualizer::LED12, colour::visualizer::LED11, colour::visualizer::LED10, colour::visualizer::LED09,
    colour::visualizer::LED08, colour::visualizer::LED07, colour::visualizer::LED06, colour::visualizer::LED05,
    colour::visualizer::LED04, colour::visualizer::LED03, colour::visualizer::LED02, colour::visualizer::LED01
  };
  std::vector<OutMeterLEDRange> outMeterLEDRanges;
  outMeterLEDRanges.reserve(kOutMeterNumBars);
  for(int i = 0; i < kOutMeterNumBars; ++i)
  {
    const float lowDB = kOutMeterLowDB + (kOutMeterStepDB * static_cast<float>(i));
    const float highDB = lowDB + kOutMeterStepDB;
    outMeterLEDRanges.emplace_back(lowDB, highDB, 1, outMeterLEDColors[static_cast<std::size_t>(i)]);
  }
  const IRECT outMeterBounds = IRECT::MakeXYWH(853.f, 11.f, 226.f, 46.f);
  auto* meter = new IVLEDMeterControl<2>(outMeterBounds, "", meterStyle, EDirection::Horizontal, {}, kOutMeterNumBars, outMeterLEDRanges);
  meter->SetResponse(IVMeterControl<2>::EResponse::Linear); // disables marker drawing
  pGraphics->AttachControl(meter, outMeterTag);

  // The largest panel contains the breath meter on the left and the visualizer taking up the rest
  // of the space.
  const IRECT breathMeterBounds = IRECT::MakeXYWH(12.f, 113.f, 20.f, 266.f);
  const IRECT harmonicVisualizerBounds = IRECT::MakeXYWH(38.f, 74.f, 798.f, 344.f);
  pGraphics->AttachControl(new IVMeterControl<1>(breathMeterBounds, "", meterStyle), breathMeterTag);
  pGraphics->AttachControl(new HarmonicVisualizerControl(harmonicVisualizerBounds), harmonicVisualizerTag);

  // The bottom panel has the pitch bend wheel on the left and the keyboard taking up the rest of
  // the space.
  const IRECT wheelsBounds = IRECT::MakeXYWH(6.f, 522.f, 35.f, 114.f);
  const IRECT keyboardBounds = IRECT::MakeXYWH(40.f, 522.f, 1038.f, 114.f);
  pGraphics->AttachControl(new IWheelControl(wheelsBounds), benderTag);
  pGraphics->AttachControl(new IVKeyboardControl(keyboardBounds, 21, 108), keyboardTag);

  // Envelope panel: x=186, y=428, w=212, h=84
  const IRECT attackKnobBounds = IRECT::MakeMidXYWH(216.f, 480.5f, knob_size, knob_size);
  const IRECT attackLabelBounds = IRECT::MakeXYWH(245.f, 468.f, 70.f, 12.f);
  const IRECT attackValueBounds = IRECT::MakeXYWH(245.f, 481.f, 70.f, 12.f);
  const IRECT releaseKnobBounds = IRECT::MakeMidXYWH(312.f, 480.5f, knob_size, knob_size);
  const IRECT releaseLabelBounds = IRECT::MakeXYWH(343.f, 468.f, 70.f, 12.f);
  const IRECT releaseValueBounds = IRECT::MakeXYWH(343.f, 481.f, 70.f, 12.f);
  auto* attackValueControl = new ICaptionControl(attackValueBounds, globalModifierParamIdxs[0], compactValueText, COLOR_TRANSPARENT, true);
  auto* releaseValueControl = new ICaptionControl(releaseValueBounds, globalModifierParamIdxs[5], compactValueText, COLOR_TRANSPARENT, true);
  attackValueControl->SetIgnoreMouse(true); attackValueControl->DisablePrompt(true);
  releaseValueControl->SetIgnoreMouse(true); releaseValueControl->DisablePrompt(true);
  pGraphics->AttachControl(attackValueControl);
  pGraphics->AttachControl(releaseValueControl);
  attachKnob(attackKnobBounds, globalModifierParamIdxs[0]);
  attachKnob(releaseKnobBounds, globalModifierParamIdxs[5]);
  pGraphics->AttachControl(new ITextControl(attackLabelBounds, "Attack", compactLabelText));
  pGraphics->AttachControl(new ITextControl(releaseLabelBounds, "Release", compactLabelText));

  // Pitch panel: x=400, y=428, w=262, h=84
  const IRECT pitchKnobBounds = IRECT::MakeMidXYWH(431.f, 480.5f, knob_size, knob_size);
  const IRECT pitchLabelBounds = IRECT::MakeXYWH(464.f, 468.f, 70.f, 12.f);
  const IRECT pitchValueBounds = IRECT::MakeXYWH(464.f, 481.f, 70.f, 12.f);
  const IRECT portamentoSliderBounds = IRECT::MakeXYWH(524.f, 474.f, 140.f, 30.f);
  const IRECT portamentoMinValueBounds = IRECT::MakeXYWH(527.5f, 470.f, 50.f, 20.f);
  const IRECT portamentoMaxValueBounds = IRECT::MakeXYWH(611.f, 489.f, 50.f, 20.f);
  const IRECT portamentoLabelBounds = IRECT::MakeXYWH(532.5f, 455.f, 70.f, 12.f);
  auto* pitchValueControl = new ICaptionControl(pitchValueBounds, globalModifierParamIdxs[1], compactValueText, COLOR_TRANSPARENT, true);
  auto* portamentoMinValueControl = new ICaptionControl(portamentoMinValueBounds, portamentoAtCC5MinParamIdx, portamentoValueText, COLOR_TRANSPARENT, true);
  auto* portamentoMaxValueControl = new ICaptionControl(portamentoMaxValueBounds, portamentoAtCC5MaxParamIdx, portamentoValueText, COLOR_TRANSPARENT, true);
  pitchValueControl->SetIgnoreMouse(true); pitchValueControl->DisablePrompt(true);
  portamentoMinValueControl->SetIgnoreMouse(true); portamentoMinValueControl->DisablePrompt(true);
  portamentoMaxValueControl->SetIgnoreMouse(true); portamentoMaxValueControl->DisablePrompt(true);
  pGraphics->AttachControl(pitchValueControl);
  pGraphics->AttachControl(portamentoMinValueControl);
  pGraphics->AttachControl(portamentoMaxValueControl);
  attachKnob(pitchKnobBounds, globalModifierParamIdxs[1]);
  pGraphics->AttachControl(new IVRangeSliderControl( portamentoSliderBounds, {portamentoAtCC5MinParamIdx, portamentoAtCC5MaxParamIdx}, "", knobStyle, EDirection::Horizontal, true, 9.f, 3.f));
  pGraphics->AttachControl(new ITextControl(pitchLabelBounds, "Pitch", compactLabelText));
  pGraphics->AttachControl(new ITextControl(portamentoLabelBounds, "Portamento", compactLabelText));

  // Blip guard panel
  // TODO: add controls for blip guard settings here once implemented

  // Variation panel: x=846, y=66, w=240, h=222
  const IRECT intVarAmtKnobBounds = IRECT::MakeMidXYWH(923.f, 141.5f, knob_size, knob_size);
  const IRECT intVarAmtValueBounds = IRECT::MakeXYWH(951.f, 134.f, 70.f, 12.f);
  const IRECT intVarRateKnobBounds = IRECT::MakeMidXYWH(1018.f, 141.5f, knob_size, knob_size);
  const IRECT intVarRateValueBounds = IRECT::MakeXYWH(1046.f, 134.f, 70.f, 12.f);
  const IRECT panVarAmtKnobBounds = IRECT::MakeMidXYWH(923.f, 195.5f, knob_size, knob_size);
  const IRECT panVarAmtValueBounds = IRECT::MakeXYWH(951.f, 188.f, 70.f, 12.f);
  const IRECT panVarRateKnobBounds = IRECT::MakeMidXYWH(1018.f, 195.5f, knob_size, knob_size);
  const IRECT panVarRateValueBounds = IRECT::MakeXYWH(1046.f, 188.f, 70.f, 12.f);
  const IRECT pitchVarAmtKnobBounds = IRECT::MakeMidXYWH(923.f, 249.5f, knob_size, knob_size);
  const IRECT pitchVarAmtValueBounds = IRECT::MakeXYWH(951.f, 242.f, 70.f, 12.f);
  const IRECT pitchVarRateKnobBounds = IRECT::MakeMidXYWH(1018.f, 249.5f, knob_size, knob_size);
  const IRECT pitchVarRateValueBounds = IRECT::MakeXYWH(1046.f, 242.f, 70.f, 12.f);
  auto* intVarAmtValueControl = new ICaptionControl(intVarAmtValueBounds, globalModifierParamIdxs[2], compactValueText, COLOR_TRANSPARENT, true);
  auto* intVarRateValueControl = new ICaptionControl(intVarRateValueBounds, globalModifierParamIdxs[7], compactValueText, COLOR_TRANSPARENT, true);
  auto* panVarAmtValueControl = new ICaptionControl(panVarAmtValueBounds, globalModifierParamIdxs[4], compactValueText, COLOR_TRANSPARENT, true);
  auto* panVarRateValueControl = new ICaptionControl(panVarRateValueBounds, globalModifierParamIdxs[9], compactValueText, COLOR_TRANSPARENT, true);
  auto* pitchVarAmtValueControl = new ICaptionControl(pitchVarAmtValueBounds, globalModifierParamIdxs[3], compactValueText, COLOR_TRANSPARENT, true);
  auto* pitchVarRateValueControl = new ICaptionControl(pitchVarRateValueBounds, globalModifierParamIdxs[8], compactValueText, COLOR_TRANSPARENT, true);
  intVarAmtValueControl->SetIgnoreMouse(true); intVarAmtValueControl->DisablePrompt(true);
  intVarRateValueControl->SetIgnoreMouse(true); intVarRateValueControl->DisablePrompt(true);
  panVarAmtValueControl->SetIgnoreMouse(true); panVarAmtValueControl->DisablePrompt(true);
  panVarRateValueControl->SetIgnoreMouse(true); panVarRateValueControl->DisablePrompt(true);
  pitchVarAmtValueControl->SetIgnoreMouse(true); pitchVarAmtValueControl->DisablePrompt(true);
  pitchVarRateValueControl->SetIgnoreMouse(true); pitchVarRateValueControl->DisablePrompt(true);
  pGraphics->AttachControl(intVarAmtValueControl);
  pGraphics->AttachControl(intVarRateValueControl);
  pGraphics->AttachControl(panVarAmtValueControl);
  pGraphics->AttachControl(panVarRateValueControl);
  pGraphics->AttachControl(pitchVarAmtValueControl);
  pGraphics->AttachControl(pitchVarRateValueControl);
  attachKnob(intVarAmtKnobBounds, globalModifierParamIdxs[2]);
  attachKnob(intVarRateKnobBounds, globalModifierParamIdxs[7]);
  attachKnob(panVarAmtKnobBounds, globalModifierParamIdxs[4]);
  attachKnob(panVarRateKnobBounds, globalModifierParamIdxs[9]);
  attachKnob(pitchVarAmtKnobBounds, globalModifierParamIdxs[3]);
  attachKnob(pitchVarRateKnobBounds, globalModifierParamIdxs[8]);

  // Output panel: x=846, y=290, w=240, h=84
  const IRECT panKnobBounds = IRECT::MakeMidXYWH(886.f, 342.5f, knob_size, knob_size);
  const IRECT panLabelBounds = IRECT::MakeXYWH(918.f, 330.f, 50.f, 12.f);
  const IRECT panValueBounds = IRECT::MakeXYWH(918.f, 343.f, 50.f, 12.f);
  const IRECT gainKnobBounds = IRECT::MakeMidXYWH(992.f, 342.5f, knob_size, knob_size);
  const IRECT gainLabelBounds = IRECT::MakeXYWH(1024.f, 330.f, 50.f, 12.f);
  const IRECT gainValueBounds = IRECT::MakeXYWH(1024.f, 343.f, 50.f, 12.f);
  auto* panValueControl = new ICaptionControl(panValueBounds, globalModifierParamIdxs[6], compactValueText, COLOR_TRANSPARENT, true);
  auto* gainValueControl = new ICaptionControl(gainValueBounds, gainParamIdx, compactValueText, COLOR_TRANSPARENT, true);
  panValueControl->SetIgnoreMouse(true); panValueControl->DisablePrompt(true);
  gainValueControl->SetIgnoreMouse(true); gainValueControl->DisablePrompt(true);
  pGraphics->AttachControl(gainValueControl);
  pGraphics->AttachControl(panValueControl);
  attachKnob(gainKnobBounds, gainParamIdx);
  attachKnob(panKnobBounds, globalModifierParamIdxs[6]);
  pGraphics->AttachControl(new ITextControl(gainLabelBounds, "Gain", compactLabelText));
  pGraphics->AttachControl(new ITextControl(panLabelBounds, "Pan", compactLabelText));

  // Effects panel
  // TODO: add controls for effects settings here once implemented

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

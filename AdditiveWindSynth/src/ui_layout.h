#pragma once

#include "IControls.h"
#include "colour.h"
#include "harmonic_visualizer_control.h"

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
  const IRECT harmonicVisualizerBounds = IRECT::MakeXYWH(50.f, 20.f, 900.f, 190.f);
  const IRECT wheelsBounds = IRECT::MakeXYWH(5.f, 420.f, 35.f, 120.f);
  const IRECT keyboardBounds = IRECT::MakeXYWH(45.f, 430.f, 945.f, 110.f);
  const IRECT globalKnobGridBounds = IRECT::MakeXYWH(50.f, 240.f, 400.f, 160.f);

  const IRECT portamentoGroupBounds = IRECT::MakeXYWH(480.f, 240.f, 160.f, 30.f);
  const IRECT portamentoSliderBounds = IRECT::MakeXYWH(480.f, 240.f, 160.f, 30.f);
  const IRECT portamentoMinValueBounds = IRECT::MakeXYWH(480.f, 260.f, 50.f, 20.f);
  const IRECT portamentoMaxValueBounds = IRECT::MakeXYWH(590.f, 260.f, 50.f, 20.f);

  const IRECT breathMeterBounds = IRECT::MakeXYWH(10.f, 8.f, 30.f, 200.f);
  const IRECT outMeterBounds = IRECT::MakeXYWH(960.f, 10.f, 30.f, 200.f);
  const IRECT gainKnobBounds = IRECT::MakeXYWH(900.f, 220.f, 90.f, 90.f);
  const IVStyle knobStyle = theme::BaseStyle();
  const IVStyle compactKnobStyle = knobStyle
    .WithLabelText(IText(10.5f, colour::ui::kLabelText, "Roboto-Regular", EAlign::Center, EVAlign::Top))
    .WithValueText(IText(10.f, colour::ui::kValueText, "Roboto-Regular", EAlign::Center, EVAlign::Bottom));
  const IText portamentoValueText = IText(12.f, colour::ui::kValueText, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
  const IVStyle groupStyle = theme::BaseStyle(true, false);
  const IVStyle meterStyle = theme::MeterStyle();
  constexpr int kGlobalKnobCols = 5;
  constexpr int kGlobalKnobRows = 2;
  constexpr std::array<const char*, 10> kGlobalKnobLabels{{
    "Attack",
    "Pitch",
    "Int Var Amt",
    "Pitch Var Amt",
    "Pan Var Amt",
    "Release",
    "Pan",
    "Int Var Rate",
    "Pitch Var Rate",
    "Pan Var Rate"
  }};

  pGraphics->AttachControl(new HarmonicVisualizerControl(harmonicVisualizerBounds), harmonicVisualizerTag);
  // Keep keyboard in standard white/black colors for readability.
  pGraphics->AttachControl(new IVKeyboardControl(keyboardBounds, 21, 108), keyboardTag);
  pGraphics->AttachControl(new IWheelControl(wheelsBounds), benderTag);
  pGraphics->AttachControl(
    new IVGroupControl(globalKnobGridBounds.GetPadded(8.f, 16.f, 8.f, 8.f), "Global settings", 6.f, groupStyle));
  pGraphics->AttachControl(
    new IVGroupControl(portamentoGroupBounds.GetPadded(8.f, 16.f, 8.f, 8.f), "Portamento", 6.f, groupStyle));
  const float globalCellWidth = globalKnobGridBounds.W() / static_cast<float>(kGlobalKnobCols);
  const float globalCellHeight = globalKnobGridBounds.H() / static_cast<float>(kGlobalKnobRows);
  for(size_t i = 0; i < globalModifierParamIdxs.size(); ++i)
  {
    const int col = static_cast<int>(i % kGlobalKnobCols);
    const int row = static_cast<int>(i / kGlobalKnobCols);
    const IRECT knobBounds = IRECT::MakeXYWH(
      globalKnobGridBounds.L + static_cast<float>(col) * globalCellWidth,
      globalKnobGridBounds.T + static_cast<float>(row) * globalCellHeight,
      globalCellWidth,
      globalCellHeight);
    pGraphics->AttachControl(new IVKnobControl(knobBounds, globalModifierParamIdxs[i], kGlobalKnobLabels[i], compactKnobStyle));
  }
  pGraphics->AttachControl(
    new IVRangeSliderControl(
      portamentoSliderBounds.GetPadded(8.f, 8.f, 8.f, 8.f),
      {portamentoAtCC5MinParamIdx, portamentoAtCC5MaxParamIdx},
      "",
      knobStyle,
      EDirection::Horizontal,
      true,
      9.f,
      3.f));
  auto* portamentoMinValueControl =
    new ICaptionControl(portamentoMinValueBounds, portamentoAtCC5MinParamIdx, portamentoValueText, COLOR_TRANSPARENT, true);
  portamentoMinValueControl->SetIgnoreMouse(true);
  portamentoMinValueControl->DisablePrompt(true);
  pGraphics->AttachControl(portamentoMinValueControl);

  auto* portamentoMaxValueControl =
    new ICaptionControl(portamentoMaxValueBounds, portamentoAtCC5MaxParamIdx, portamentoValueText, COLOR_TRANSPARENT, true);
  portamentoMaxValueControl->SetIgnoreMouse(true);
  portamentoMaxValueControl->DisablePrompt(true);
  pGraphics->AttachControl(portamentoMaxValueControl);
  pGraphics->AttachControl(new IVKnobControl(gainKnobBounds, gainParamIdx, "Gain", knobStyle));
  pGraphics->AttachControl(new IVMeterControl<1>(breathMeterBounds, "Breath", meterStyle), breathMeterTag);
  pGraphics->AttachControl(new IVLEDMeterControl<2>(outMeterBounds, "Out", meterStyle), outMeterTag);
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

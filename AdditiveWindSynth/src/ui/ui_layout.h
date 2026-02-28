#pragma once

#include "IControls.h"
#include "colour.h"
#include "../visualizer/harmonic_visualizer_control.h"

#include <algorithm>

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

inline void AttachSVGKnobControl(
  IGraphics* pGraphics,
  const IRECT& bounds,
  int paramIdx,
  const char* label,
  const ISVG& knobSVG,
  const IText& labelText,
  const IText& valueText)
{
  constexpr float kLabelHeight = 12.f;
  constexpr float kValueHeight = 12.f;
  constexpr float kKnobToLabelGap = -7.f;
  constexpr float kLabelToValueGap = -3.f;
  constexpr float kKnobHorizontalPadding = 6.f;
  constexpr float kTextBottomPadding = 1.f;

  const float textStackHeight = kKnobToLabelGap + kLabelHeight + kLabelToValueGap + kValueHeight + kTextBottomPadding;
  const IRECT knobRegion(bounds.L + kKnobHorizontalPadding, bounds.T, bounds.R - kKnobHorizontalPadding, bounds.B - textStackHeight);
  const float knobSide = std::min(knobRegion.W(), knobRegion.H());
  const IRECT knobBounds = knobRegion.GetCentredInside(knobSide, knobSide);
  const float labelTop = knobBounds.B + kKnobToLabelGap;
  const IRECT labelBounds(bounds.L, labelTop, bounds.R, labelTop + kLabelHeight);
  const IRECT valueBounds(bounds.L, labelBounds.B + kLabelToValueGap, bounds.R, labelBounds.B + kLabelToValueGap + kValueHeight);

  pGraphics->AttachControl(new ISVGKnobControl(knobBounds, knobSVG, paramIdx));

  auto* valueControl = new ICaptionControl(valueBounds, paramIdx, valueText, COLOR_TRANSPARENT, true);
  valueControl->SetIgnoreMouse(true);
  valueControl->DisablePrompt(true);
  pGraphics->AttachControl(valueControl);

  pGraphics->AttachControl(new ITextControl(labelBounds, label, labelText));
}

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
  const IRECT globalKnobGridBounds = IRECT::MakeXYWH(50.f, 240.f, 400.f, 160.f);

  const IRECT portamentoGroupBounds = IRECT::MakeXYWH(480.f, 240.f, 160.f, 30.f);
  const IRECT portamentoSliderBounds = IRECT::MakeXYWH(480.f, 240.f, 160.f, 30.f);
  const IRECT portamentoMinValueBounds = IRECT::MakeXYWH(480.f, 260.f, 50.f, 20.f);
  const IRECT portamentoMaxValueBounds = IRECT::MakeXYWH(590.f, 260.f, 50.f, 20.f);

  const IVStyle knobStyle = theme::BaseStyle();
  const IText compactLabelText = IText(10.5f, colour::ui::kLabelText, "Roboto-Black", EAlign::Center, EVAlign::Middle);
  const IText compactValueText = IText(10.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle);
  const IText gainLabelText = IText(13.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Center, EVAlign::Middle);
  const IText gainValueText = IText(12.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle);
  const IText portamentoValueText = IText(12.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle);
  const IVStyle groupStyle = theme::BaseStyle(true, false);
  const IVStyle meterStyle = theme::MeterStyle();
  const ISVG knobSVG = pGraphics->LoadSVG("knob.svg");
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

  // The top right panel has the output meter.
  const IRECT outMeterBounds = IRECT::MakeXYWH(853.f, 11.f, 226.f, 46.f);
  pGraphics->AttachControl(new IVLEDMeterControl<2>(outMeterBounds, "", meterStyle, EDirection::Horizontal), outMeterTag);

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

  // Envelope panel

  // Pitch panel
  // Pitch knob goes here: x=409, y=464, w=42, h=42

  // Blip guard panel

  // Variation panel

  // Output panel
  const IRECT gainKnobBounds = IRECT::MakeXYWH(982.f, 322.f, 90.f, 90.f);
  AttachSVGKnobControl(pGraphics, gainKnobBounds, gainParamIdx, "Gain", knobSVG, gainLabelText, gainValueText);

  // Effects panel



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
    AttachSVGKnobControl(
      pGraphics,
      knobBounds,
      globalModifierParamIdxs[i],
      kGlobalKnobLabels[i],
      knobSVG,
      compactLabelText,
      compactValueText);
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

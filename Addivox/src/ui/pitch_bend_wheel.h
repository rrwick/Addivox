#pragma once

#include "IControls.h"

#include <cmath>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace layout
{
class PitchBendWheelControl final : public ISliderControlBase
{
  static constexpr int kSpringAnimationTime = 50;
  static constexpr int kNumRungs = 10;

public:
  static constexpr int kMessageTagSetPitchBendRange = 0;

  explicit PitchBendWheelControl(const IRECT& bounds, int initBendRange = 12)
  : ISliderControlBase(bounds, kNoParameter, EDirection::Vertical, DEFAULT_GEARING, 40.f)
  , mPitchBendRange(initBendRange)
  {
    mMenu.AddItem("1 semitone");
    mMenu.AddItem("2 semitones");
    mMenu.AddItem("Fifth");
    mMenu.AddItem("Octave");

    SetValue(0.5);
    SetWantsMidi(true);
    SetActionFunction([](IControl* pControl) {
      IMidiMsg msg;
      msg.MakePitchWheelMsg((pControl->GetValue() * 2.) - 1.);
      pControl->GetDelegate()->SendMidiMsgFromUI(msg);
    });
  }

  void SetPitchBendRange(int pitchBendRange)
  {
    mPitchBendRange = pitchBendRange;
  }

  int GetPitchBendRange() const
  {
    return mPitchBendRange;
  }

  void Draw(IGraphics& g) override
  {
    IRECT handleBounds = mRECT.GetPadded(-10.f);
    const float stepSize = handleBounds.H() / static_cast<float>(kNumRungs);
    g.FillRoundRect(DEFAULT_SHCOLOR, mRECT.GetPadded(-5.f));

    if(!g.CheckLayer(mLayer))
    {
      const IRECT layerRect = handleBounds.GetMidVPadded(handleBounds.H() + stepSize);

      if(layerRect.W() > 0 && layerRect.H() > 0)
      {
        g.StartLayer(this, layerRect);
        g.DrawGrid(COLOR_BLACK.WithOpacity(0.5f), layerRect, 0.f, stepSize, nullptr, 2.f);
        mLayer = g.EndLayer();
      }
    }

    IRECT upperHalf = handleBounds.FracRectVertical(0.5, true);
    g.PathRect(upperHalf);
    g.PathFill(IPattern::CreateLinearGradient(
      upperHalf,
      EDirection::Vertical,
      {{COLOR_BLACK, 0.f}, {COLOR_MID_GRAY, 1.f}}));

    IRECT lowerHalf = handleBounds.FracRectVertical(0.51f, false);
    g.PathRect(lowerHalf);
    g.PathFill(IPattern::CreateLinearGradient(
      lowerHalf,
      EDirection::Vertical,
      {{COLOR_MID_GRAY, 0.f}, {COLOR_BLACK, 1.f}}));

    const float value = static_cast<float>(GetValue());
    const float y = (handleBounds.H() - stepSize) * value;
    const float triangleRamp = std::fabs(value - 0.5f) * 2.f;

    g.DrawBitmap(mLayer->GetBitmap(), handleBounds, 0, static_cast<int>(y));

    const IRECT cutoutBounds = handleBounds.GetFromBottom(stepSize).GetTranslated(0, -y);
    g.PathRect(cutoutBounds);
    g.PathFill(IPattern::CreateLinearGradient(
      cutoutBounds,
      EDirection::Vertical,
      {
        {COLOR_BLACK.WithContrast(iplug::Lerp(0.f, 0.5f, triangleRamp)), 0.f},
        {COLOR_BLACK.WithContrast(iplug::Lerp(0.5f, 0.f, triangleRamp)), 1.f}
      }));

    g.DrawVerticalLine(COLOR_BLACK, cutoutBounds, 0.f);
    g.DrawVerticalLine(COLOR_BLACK, cutoutBounds, 1.f);
    g.DrawRect(COLOR_BLACK, handleBounds);
  }

  void OnMidi(const IMidiMsg& msg) override
  {
    if(msg.StatusMsg() != IMidiMsg::kPitchWheel)
      return;

    SetValue((msg.PitchWheel() + 1.) * 0.5);
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    (void) x;
    (void) y;
    (void) mod;
    (void) d;
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    (void) valIdx;

    if(!pSelectedMenu)
      return;

    switch(pSelectedMenu->GetChosenItemIdx())
    {
      case 0: mPitchBendRange = 1; break;
      case 1: mPitchBendRange = 2; break;
      case 2: mPitchBendRange = 7; break;
      case 3:
      default:
        mPitchBendRange = 12;
        break;
    }

    GetDelegate()->SendArbitraryMsgFromUI(kMessageTagSetPitchBendRange, GetTag(), sizeof(int), &mPitchBendRange);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if(mod.R)
    {
      switch(mPitchBendRange)
      {
        case 1: mMenu.CheckItemAlone(0); break;
        case 2: mMenu.CheckItemAlone(1); break;
        case 7: mMenu.CheckItemAlone(2); break;
        case 12: mMenu.CheckItemAlone(3); break;
        default: break;
      }

      GetUI()->CreatePopupMenu(*this, mMenu, x, y);
      return;
    }

    ISliderControlBase::OnMouseDown(x, y, mod);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    const double startValue = GetValue();
    SetAnimation([startValue](IControl* pCaller) {
      pCaller->SetValue(iplug::Lerp(startValue, 0.5, Clip(pCaller->GetAnimationProgress(), 0., 1.)));
      if(pCaller->GetAnimationProgress() > 1.)
      {
        pCaller->SetDirty(true);
        pCaller->OnEndAnimation();
      }
    }, kSpringAnimationTime);

    ISliderControlBase::OnMouseUp(x, y, mod);
  }

private:
  IPopupMenu mMenu;
  int mPitchBendRange;
  ILayerPtr mLayer;
};
} // namespace layout
} // namespace plugin_ui

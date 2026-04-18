#pragma once

#include "IControls.h"
#include "colour.h"
#include "control_utils.h"
#include "help_text.h"
#include "theme.h"

#include <algorithm>

namespace plugin_ui
{
using namespace iplug;
using namespace igraphics;

namespace layout
{
struct KnobAssets
{
  ISVG fixed;
  ISVG rotating;
};

struct KnobValueSpec
{
  IRECT knobBounds;
  IRECT valueBounds;
  int paramIdx = kNoParameter;
};

class LayeredSVGKnobControl : public IKnobControlBase
{
public:
  LayeredSVGKnobControl(const IRECT& bounds,
                        const ISVG& fixedSVG,
                        const ISVG& rotatingSVG,
                        int paramIdx = kNoParameter,
                        float startAngle = -135.f,
                        float endAngle = 135.f,
                        EDirection direction = EDirection::Vertical,
                        double gearing = DEFAULT_GEARING)
  : IKnobControlBase(bounds, paramIdx, direction, gearing)
  , mFixedSVG(fixedSVG)
  , mRotatingSVG(rotatingSVG)
  , mStartAngle(startAngle)
  , mEndAngle(endAngle)
  {
  }

  void Draw(IGraphics& g) override
  {
    g.DrawSVG(mFixedSVG, mRECT, &mBlend);
    DrawValueArc(g);
    g.DrawRotatedSVG(mRotatingSVG, mRECT.MW(), mRECT.MH(), mRECT.W(), mRECT.H(), AngleForValue(), &mBlend);
  }

  void SetFixedSVG(const ISVG& fixedSVG)
  {
    mFixedSVG = fixedSVG;
    SetDirty(false);
  }

  void SetRotatingSVG(const ISVG& rotatingSVG)
  {
    mRotatingSVG = rotatingSVG;
    SetDirty(false);
  }

  void SetAngleRange(float startAngle, float endAngle)
  {
    mStartAngle = startAngle;
    mEndAngle = endAngle;
    SetDirty(false);
  }

  void SetValueArcThickness(float valueArcThickness)
  {
    mValueArcThickness = std::max(0.f, valueArcThickness);
    SetDirty(false);
  }

private:
  static constexpr float kArcRadiusRatio = 0.44f;

  float AngleForValue() const
  {
    return mStartAngle + static_cast<float>(GetValue()) * (mEndAngle - mStartAngle);
  }

  float AngleForNormalizedValue(double normalizedValue) const
  {
    return mStartAngle + static_cast<float>(normalizedValue) * (mEndAngle - mStartAngle);
  }

  double ArcStartNormalizedValue() const
  {
    if(const IParam* param = GetParam())
    {
      if(param->GetMin() <= 0.0 && param->GetMax() >= 0.0)
        return param->ToNormalized(0.0);

      return param->ToNormalized(param->GetMin());
    }

    return 0.0;
  }

  IColor ValueArcColor() const
  {
    return colour::visualizer::kKnob;
  }

  void DrawValueArc(IGraphics& g)
  {
    if(mValueArcThickness <= 0.f)
      return;

    const float startAngle = AngleForNormalizedValue(ArcStartNormalizedValue());
    const float endAngle = AngleForValue();
    const float angleMin = std::min(startAngle, endAngle);
    const float angleMax = std::max(startAngle, endAngle);

    if(angleMin == angleMax)
      return;

    IStrokeOptions options;
    options.mCapOption = ELineCap::Round;

    const float radius = std::min(mRECT.W(), mRECT.H()) * kArcRadiusRatio;
    g.PathClear();
    g.PathArc(mRECT.MW(), mRECT.MH(), radius, angleMin, angleMax);
    g.PathStroke(ValueArcColor(), mValueArcThickness, options, &mBlend);
  }

  ISVG mFixedSVG;
  ISVG mRotatingSVG;
  float mStartAngle = -135.f;
  float mEndAngle = 135.f;
  float mValueArcThickness = 4.8f;
};

class KnobReadoutControl final : public ITextControl
{
public:
  KnobReadoutControl(const IRECT& bounds, int paramIdx, const char* label, const IText& text)
  : ITextControl(bounds, label ? label : "", text, COLOR_TRANSPARENT)
  , mLabel(label ? label : "")
  {
    SetParamIdx(paramIdx);
    SetIgnoreMouse(true);
    DisablePrompt(true);
  }

  void ShowValueWhileInteracting()
  {
    mShowValueWhileInteracting = true;
    SetDirty(false);
  }

  void ShowValueTemporarily(int durationMs)
  {
    if(mShowValueWhileInteracting)
      return;

    mShowValueTemporarily = true;
    SetAnimation(DefaultAnimationFunc, durationMs);
    SetDirty(false);
  }

  void ShowLabel()
  {
    mShowValueWhileInteracting = false;
    mShowValueTemporarily = false;
    SetAnimation(nullptr);
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    if(ShouldShowValue())
    {
      if(const IParam* param = GetParam())
        param->GetDisplay(mStr);
      else
        mStr.Set(mLabel.Get());
    }
    else
    {
      mStr.Set(mLabel.Get());
    }

    ITextControl::Draw(g);
  }

  void OnEndAnimation() override
  {
    mShowValueTemporarily = false;
    IControl::OnEndAnimation();
    SetDirty(false);
  }

private:
  bool ShouldShowValue() const
  {
    return mShowValueWhileInteracting || mShowValueTemporarily;
  }

  WDL_String mLabel;
  bool mShowValueWhileInteracting = false;
  bool mShowValueTemporarily = false;
};

class InteractiveLayeredSVGKnobControl final : public LayeredSVGKnobControl
{
public:
  InteractiveLayeredSVGKnobControl(const IRECT& bounds,
                                   const ISVG& fixedSVG,
                                   const ISVG& rotatingSVG,
                                   int paramIdx = kNoParameter,
                                   float startAngle = -135.f,
                                   float endAngle = 135.f,
                                   EDirection direction = EDirection::Vertical,
                                   double gearing = DEFAULT_GEARING)
  : LayeredSVGKnobControl(bounds, fixedSVG, rotatingSVG, paramIdx, startAngle, endAngle, direction, gearing)
  {
  }

  void SetReadoutControl(KnobReadoutControl* readoutControl)
  {
    mReadoutControl = readoutControl;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    LayeredSVGKnobControl::OnMouseDown(x, y, mod);

    if(mod.L && mReadoutControl)
      mReadoutControl->ShowValueWhileInteracting();
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    LayeredSVGKnobControl::OnMouseUp(x, y, mod);

    if(mReadoutControl)
      mReadoutControl->ShowLabel();
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    LayeredSVGKnobControl::OnMouseWheel(x, y, mod, d);

    if(d != 0.f && mReadoutControl)
      mReadoutControl->ShowValueTemporarily(250);
  }

private:
  KnobReadoutControl* mReadoutControl = nullptr;
};

class LabelledKnob final : public IContainerBase
{
public:
  LabelledKnob(const IRECT& bounds,
               int paramIdx,
               const char* label,
               float knobTextGap = 6.f)
  : IContainerBase(bounds, kNoParameter)
  , mParamIdx(paramIdx)
  , mKnobTextGap(knobTextGap)
  {
    mLabel.Set(label ? label : "");
  }

  void SetTooltip(const char* tooltip)
  {
    IControl::SetTooltip(tooltip);
    mTooltip.Set(tooltip ? tooltip : "");

    if(mKnobControl)
      mKnobControl->SetTooltip(mTooltip.Get());
    if(mReadoutControl)
      mReadoutControl->SetTooltip(mTooltip.Get());
  }

  void OnAttached() override
  {
    auto* ui = GetUI();
    if(!ui)
      return;

    const ISVG fixedSVG = ui->LoadSVG("knob-fixed.svg");
    const ISVG rotatingSVG = ui->LoadSVG("knob-rotating.svg");

    mKnobControl = new InteractiveLayeredSVGKnobControl(IRECT(), fixedSVG, rotatingSVG, mParamIdx, -150.f, 150.f);
    mReadoutControl = new KnobReadoutControl(IRECT(), mParamIdx, mLabel.Get(), theme::CompactLabelText(EAlign::Center));
    mKnobControl->SetReadoutControl(mReadoutControl);

    AddChildControl(mKnobControl);
    AddChildControl(mReadoutControl);

    const char* const tooltip = help_text::main_ui::GetParam(mParamIdx);
    SetTooltip((mTooltip.GetLength() > 0) ? mTooltip.Get() : tooltip);
    OnResize();
  }

  void OnResize() override
  {
    if(!mKnobControl || !mReadoutControl)
      return;

    const IRECT knobBounds = GetKnobBounds();
    const IRECT readoutBounds = GetTextBounds();

    mKnobControl->SetTargetAndDrawRECTs(knobBounds);
    mReadoutControl->SetTargetAndDrawRECTs(readoutBounds);
  }

private:
  float GetKnobTextGap() const
  {
    return std::clamp(mKnobTextGap, 0.f, 2.f);
  }

  IRECT GetKnobBounds() const
  {
    constexpr float kTextHeight = 12.f;

    const float knobTextGap = GetKnobTextGap();
    const float textBlockHeight = kTextHeight;
    const float knobSize = std::max(0.f, std::min(mRECT.W(), mRECT.H() - textBlockHeight - knobTextGap));
    return IRECT::MakeXYWH(mRECT.MW() - (knobSize * 0.5f), mRECT.T, knobSize, knobSize);
  }

  IRECT GetTextBounds() const
  {
    constexpr float kTextHeight = 12.f;

    const IRECT knobBounds = GetKnobBounds();
    const float top = knobBounds.B + GetKnobTextGap();
    return IRECT::MakeXYWH(mRECT.L, top, mRECT.W(), kTextHeight);
  }

  int mParamIdx = kNoParameter;
  float mKnobTextGap = 6.f;
  WDL_String mLabel;
  WDL_String mTooltip;
  InteractiveLayeredSVGKnobControl* mKnobControl = nullptr;
  KnobReadoutControl* mReadoutControl = nullptr;
};

inline void SetTooltipIfPresent(IControl* control, const char* tooltip)
{
  if(control && tooltip && tooltip[0] != '\0')
    control->SetTooltip(tooltip);
}

inline void AttachKnob(IGraphics* pGraphics,
                       const KnobAssets& assets,
                       const IRECT& bounds,
                       int paramIdx,
                       const char* tooltip = nullptr)
{
  auto* control = new LayeredSVGKnobControl(bounds, assets.fixed, assets.rotating, paramIdx);
  SetTooltipIfPresent(control, tooltip);
  pGraphics->AttachControl(control);
}

inline void AttachKnobWithValue(IGraphics* pGraphics,
                                const KnobAssets& assets,
                                const KnobValueSpec& spec,
                                const IText& valueText)
{
  const char* const tooltip = help_text::main_ui::GetParam(spec.paramIdx);
  auto* valueControl = MakePassiveControl(
    new ICaptionControl(spec.valueBounds, spec.paramIdx, valueText, COLOR_TRANSPARENT, true));
  SetTooltipIfPresent(valueControl, tooltip);
  pGraphics->AttachControl(valueControl);
  AttachKnob(pGraphics, assets, spec.knobBounds, spec.paramIdx, tooltip);
}
} // namespace layout
} // namespace plugin_ui

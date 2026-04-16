#pragma once

#include "IControls.h"
#include "control_utils.h"
#include "help_text.h"
#include "layered_svg_knob_control.h"
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
    if(mValueControl)
      mValueControl->SetTooltip(mTooltip.Get());
    if(mLabelControl)
      mLabelControl->SetTooltip(mTooltip.Get());
  }

  void OnAttached() override
  {
    auto* ui = GetUI();
    if(!ui)
      return;

    const ISVG fixedSVG = ui->LoadSVG("knob-fixed.svg");
    const ISVG rotatingSVG = ui->LoadSVG("knob-rotating.svg");

    mKnobControl = new LayeredSVGKnobControl(IRECT(), fixedSVG, rotatingSVG, mParamIdx, -150.f, 150.f);
    mValueControl = MakePassiveControl(
      new ICaptionControl(IRECT(), mParamIdx, theme::CompactValueText(EAlign::Center), COLOR_TRANSPARENT, false));
    mLabelControl = MakePassiveControl(
      new ITextControl(IRECT(), mLabel.Get(), theme::CompactLabelText(EAlign::Center), COLOR_TRANSPARENT));

    AddChildControl(mKnobControl);
    AddChildControl(mValueControl);
    AddChildControl(mLabelControl);

    const char* const tooltip = help_text::main_ui::GetParam(mParamIdx);
    SetTooltip((mTooltip.GetLength() > 0) ? mTooltip.Get() : tooltip);
    OnResize();
  }

  void OnResize() override
  {
    if(!mKnobControl || !mValueControl || !mLabelControl)
      return;

    const IRECT knobBounds = GetKnobBounds();
    const IRECT labelBounds = GetTextLineBounds(-1);
    const IRECT valueBounds = GetTextLineBounds(+1);

    mKnobControl->SetTargetAndDrawRECTs(knobBounds);
    mLabelControl->SetTargetAndDrawRECTs(labelBounds);
    mValueControl->SetTargetAndDrawRECTs(valueBounds);
  }

private:
  float GetKnobTextGap() const
  {
    return std::clamp(mKnobTextGap, 0.f, 2.f);
  }

  IRECT GetKnobBounds() const
  {
    constexpr float kTextHeight = 12.f;
    constexpr float kTextLineGap = 0.f;

    const float knobTextGap = GetKnobTextGap();
    const float textBlockHeight = (2.f * kTextHeight) + kTextLineGap;
    const float knobSize = std::max(0.f, std::min(mRECT.W(), mRECT.H() - textBlockHeight - knobTextGap));
    return IRECT::MakeXYWH(mRECT.MW() - (knobSize * 0.5f), mRECT.T, knobSize, knobSize);
  }

  IRECT GetTextLineBounds(int lineDirection) const
  {
    constexpr float kTextHeight = 12.f;
    constexpr float kTextLineGap = 0.f;

    const IRECT knobBounds = GetKnobBounds();
    const float labelTop = knobBounds.B + GetKnobTextGap();
    const float valueTop = labelTop + kTextHeight + kTextLineGap;
    const float top = (lineDirection < 0) ? labelTop : valueTop;

    return IRECT::MakeXYWH(mRECT.L, top, mRECT.W(), kTextHeight);
  }

  int mParamIdx = kNoParameter;
  float mKnobTextGap = 6.f;
  WDL_String mLabel;
  WDL_String mTooltip;
  LayeredSVGKnobControl* mKnobControl = nullptr;
  ICaptionControl* mValueControl = nullptr;
  ITextControl* mLabelControl = nullptr;
};

inline void SetTooltipIfPresent(IControl* control, const char* tooltip)
{
  if(control && tooltip && tooltip[0] != '\0')
    control->SetTooltip(tooltip);
}

inline void AttachPassiveText(IGraphics* pGraphics,
                              const IRECT& bounds,
                              const char* text,
                              const IText& style,
                              const char* tooltip = nullptr)
{
  auto* control = MakePassiveControl(new ITextControl(bounds, text, style, COLOR_TRANSPARENT));
  SetTooltipIfPresent(control, tooltip);
  pGraphics->AttachControl(control);
}

inline void AttachPassiveCaption(IGraphics* pGraphics,
                                 const IRECT& bounds,
                                 int paramIdx,
                                 const IText& style,
                                 const char* tooltip = nullptr)
{
  auto* control = MakePassiveControl(new ICaptionControl(bounds, paramIdx, style, COLOR_TRANSPARENT, true));
  SetTooltipIfPresent(control, tooltip);
  pGraphics->AttachControl(control);
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
  AttachPassiveCaption(pGraphics, spec.valueBounds, spec.paramIdx, valueText, tooltip);
  AttachKnob(pGraphics, assets, spec.knobBounds, spec.paramIdx, tooltip);
}
} // namespace layout
} // namespace plugin_ui

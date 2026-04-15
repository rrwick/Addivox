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

    mKnobControl = new LayeredSVGKnobControl(IRECT(), fixedSVG, rotatingSVG, mParamIdx);
    mValueControl = MakePassiveControl(
      new ICaptionControl(IRECT(), mParamIdx, theme::CompactValueText(), COLOR_TRANSPARENT, true));
    mLabelControl = MakePassiveControl(
      new ITextControl(IRECT(), mLabel.Get(), theme::CompactLabelText(), COLOR_TRANSPARENT));

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
  IRECT GetKnobBounds() const
  {
    return IRECT::MakeXYWH(mRECT.L, mRECT.T, mRECT.H(), mRECT.H());
  }

  IRECT GetTextBlockBounds() const
  {
    const float textWidth = std::max(0.f, mRECT.W() - mRECT.H() - mKnobTextGap);
    return IRECT::MakeXYWH(mRECT.L + mRECT.H() + mKnobTextGap, mRECT.T, textWidth, mRECT.H());
  }

  IRECT GetTextLineBounds(int lineDirection) const
  {
    constexpr float kTextHeight = 12.f;
    constexpr float kLineCenterOffset = 6.5f;

    const IRECT textBounds = GetTextBlockBounds();
    const float centerYOffset = kLineCenterOffset * static_cast<float>(lineDirection);
    const float centerY = mRECT.MH() + centerYOffset;
    return IRECT::MakeXYWH(textBounds.L, centerY - (kTextHeight * 0.5f), textBounds.W(), kTextHeight);
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

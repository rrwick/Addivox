#pragma once

#include "IControls.h"
#include "colour.h"

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

inline IText CompactLabelText(EAlign align = EAlign::Near)
{
  return {14.f, colour::ui::kLabelText, "Roboto-Black", align};
}

inline IText SectionLabelText(float angle = -90.f)
{
  return {22.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Center, EVAlign::Middle, angle};
}

inline IText CompactValueText(EAlign align = EAlign::Near)
{
  return {14.f, colour::ui::kValueText, "Roboto-Black", align};
}

inline IText PortamentoValueText()
{
  return {12.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center};
}

inline IVStyle VizEditButtonStyle()
{
  return BaseStyle(true, false)
    .WithLabelText(IText(16.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle));
}

inline IVStyle PatchManagerStyle()
{
  return BaseStyle(true, false)
    .WithLabelText(IText(15.f, colour::ui::kValueText, "Roboto-Bold", EAlign::Center, EVAlign::Middle))
    .WithColor(kFG, colour::ui::kControlBody)
    .WithColor(kHL, colour::editor::kHoverOverlay);
}

inline IVStyle PatchActionButtonStyle()
{
  return BaseStyle(true, false)
    .WithLabelText(IText(14.f, colour::ui::kValueText, "Roboto-Bold", EAlign::Center, EVAlign::Middle))
    .WithColor(kFG, colour::ui::kControlBody)
    .WithColor(kHL, colour::editor::kHoverOverlay);
}

inline IVStyle AboutIconButtonStyle()
{
  return BaseStyle(true, false)
    .WithLabelText(IText(20.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle))
    .WithRoundness(1.f)
    .WithFrameThickness(1.4f)
    .WithColor(kFG, colour::ui::kControlBody)
    .WithColor(kHL, colour::ui::kControlHighlight);
}

inline IVStyle MainPanelModeSwitchStyle()
{
  return BaseStyle(false, false)
    .WithColor(kFG, colour::ui::kValueText)
    .WithColor(kHL, colour::ui::kValueText);
}

inline IVStyle PortamentoRangeSliderStyle()
{
  return BaseStyle(false, false)
    .WithColor(kFG, colour::ui::kValueText)
    .WithColor(kHL, colour::ui::kValueText);
}

inline IText AboutTitleText()
{
  return {40.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle};
}

inline IText AboutMetaText(float size = 22.f)
{
  return {size, colour::ui::kLabelText, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
}

inline IText AboutLinkText()
{
  return {22.f, colour::ui::kAccentPrimary, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
}

struct EditorStyles
{
  IColor darkSurface{colour::editor::kSurface};
  IColor darkTab{colour::editor::kTab};
  IColor barColor{colour::editor::kSliderBar};
  IVStyle tabsStyle = BaseStyle(false, false)
    .WithValueText(IText(13.f, colour::ui::kValueText, "Roboto-Bold", EAlign::Center, EVAlign::Middle))
    .WithColor(kFG, darkTab)
    .WithColor(kPR, darkSurface)
    .WithColor(kHL, colour::editor::kHoverOverlay);
  IVStyle sliderStyle = BaseStyle(false, false)
    .WithColor(kBG, darkSurface)
    .WithColor(kFG, barColor)
    .WithColor(kX1, barColor)
    .WithColor(kHL, colour::editor::kHoverOverlay);
  IText tabTitleText{16.f, colour::ui::kValueText, "Roboto-Bold", EAlign::Near, EVAlign::Top};
  IText descriptionText{13.f, colour::ui::kLabelText, "Roboto-Regular", EAlign::Near, EVAlign::Top};
  IVStyle restoreButtonStyle = BaseStyle(true, false)
    .WithLabelText(IText(13.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle))
    .WithColor(kFG, darkTab)
    .WithColor(kHL, colour::editor::kHoverOverlay);
  IVStyle utilityNumberBoxStyle = BaseStyle(false, false)
    .WithValueText(IText(13.f, colour::ui::kValueText, "Roboto-Regular", EAlign::Center, EVAlign::Middle))
    .WithColor(kBG, darkTab)
    .WithColor(kFG, colour::editor::kUtilityBody)
    .WithColor(kFR, colour::ui::kControlFrame)
    .WithColor(kHL, colour::editor::kHoverOverlay);
  IVStyle utilityToggleStyle = BaseStyle(false, true)
    .WithValueText(IText(14.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle))
    .WithColor(kBG, darkTab)
    .WithColor(kFG, colour::editor::kUtilityBody)
    .WithColor(kFR, colour::ui::kControlFrame)
    .WithColor(kHL, colour::editor::kHoverOverlay);
  IText utilityLabelText{13.f, colour::ui::kLabelText, "Roboto-Black", EAlign::Near, EVAlign::Middle};
  IText utilityDropdownText{13.f, colour::ui::kValueText, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
  IText utilityActionTitleText{13.f, colour::ui::kValueText, "Roboto-Black", EAlign::Center, EVAlign::Middle};
};
} // namespace theme
} // namespace plugin_ui

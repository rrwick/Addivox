#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline std::shared_ptr<EditorLevelTransform> GetAttackReleaseTransform(const std::shared_ptr<EditorContext>& context,
                                                                       OscillatorParameter parameter)
{
  return (parameter == OscillatorParameter::attack)
    ? context->attackReleaseTab.attackTransform
    : context->attackReleaseTab.releaseTransform;
}

inline double GetAttackReleaseMaxValue(OscillatorParameter parameter)
{
  return parameter == OscillatorParameter::release ? 0.1 : 1.0;
}

inline bool TryGetAttackReleaseShapeValue(OscillatorParameter parameter,
                                          const char* shapeName,
                                          int oscillatorIndex,
                                          double& value)
{
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);

  if(parameter == OscillatorParameter::attack)
  {
    if(std::strcmp(shapeName, "linear ramp up") == 0 ||
       std::strcmp(shapeName, "linear ramp up (slow)") == 0)
    {
      value = harmonicNumber / 100.0;
      return true;
    }

    if(std::strcmp(shapeName, "linear ramp up (fast)") == 0)
    {
      value = harmonicNumber / 1000.0;
      return true;
    }

    if(std::strcmp(shapeName, "square root ramp up") == 0 ||
       std::strcmp(shapeName, "sqrt ramp up (slow)") == 0)
    {
      value = (0.11 * std::sqrt(harmonicNumber)) - 0.1;
      return true;
    }

    if(std::strcmp(shapeName, "sqrt ramp up (fast)") == 0)
    {
      value = ((0.11 * std::sqrt(harmonicNumber)) - 0.1) / 10.0;
      return true;
    }

    if(std::strcmp(shapeName, "logarithmic ramp up (slow)") == 0 ||
       std::strcmp(shapeName, "exponential ramp up (slow)") == 0)
    {
      value = 0.01 + (0.99 * std::log(harmonicNumber) / std::log(100.0));
      return true;
    }

    if(std::strcmp(shapeName, "logarithmic ramp up (fast)") == 0 ||
       std::strcmp(shapeName, "exponential ramp up (fast)") == 0)
    {
      value = 0.001 + (0.099 * std::log(harmonicNumber) / std::log(100.0));
      return true;
    }

    if(std::strcmp(shapeName, "flat") == 0)
    {
      value = 0.1;
      return true;
    }
  }
  else if(parameter == OscillatorParameter::release)
  {
    if(std::strcmp(shapeName, "linear ramp down") == 0 ||
       std::strcmp(shapeName, "linear ramp down (slow)") == 0)
    {
      value = 0.1 - (static_cast<double>(oscillatorIndex) / 1000.0);
      return true;
    }

    if(std::strcmp(shapeName, "linear ramp down (fast)") == 0)
    {
      value = 0.01 - (static_cast<double>(oscillatorIndex) / 10000.0);
      return true;
    }

    if(std::strcmp(shapeName, "square ramp down (slow)") == 0)
    {
      const double distanceFromEnd = harmonicNumber - 101.0;
      value = ((distanceFromEnd * distanceFromEnd) / 101000.0) + (1.0 / 1010.0);
      return true;
    }

    if(std::strcmp(shapeName, "square ramp down (fast)") == 0)
    {
      const double distanceFromEnd = harmonicNumber - 101.0;
      value = ((((distanceFromEnd * distanceFromEnd) / 101000.0) + (1.0 / 1010.0)) / 10.0);
      return true;
    }

    if(std::strcmp(shapeName, "exponential ramp down (slow)") == 0)
    {
      value = 0.1 * std::pow(10.0, (-2.0 * static_cast<double>(oscillatorIndex)) / 99.0);
      return true;
    }

    if(std::strcmp(shapeName, "exponential ramp down (fast)") == 0)
    {
      value = 0.01 * std::pow(10.0, (-2.0 * static_cast<double>(oscillatorIndex)) / 99.0);
      return true;
    }

    if(std::strcmp(shapeName, "flat") == 0)
    {
      value = 0.1;
      return true;
    }
  }

  return false;
}

inline bool ApplyAttackReleaseShape(SimplePreset& preset, OscillatorParameter parameter, const char* shapeName)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    double value = 0.0;
    if(!TryGetAttackReleaseShapeValue(parameter, shapeName, oscillatorIndex, value))
      return false;

    preset.SetOscillatorParameter(
      oscillatorIndex,
      parameter,
      std::clamp(value, 0.0, GetAttackReleaseMaxValue(parameter)));
  }

  return true;
}

inline bool ApplyAttackReleaseAction(SimplePreset& preset, OscillatorParameter parameter, const char* actionName)
{
  const double maxValue = GetAttackReleaseMaxValue(parameter);

  if(std::strcmp(actionName, "scale up all") == 0)
    return preset.ScaleOscillatorParameterAll(parameter, 1.111111111111111, 0.0, maxValue);
  if(std::strcmp(actionName, "scale down all") == 0)
    return preset.ScaleOscillatorParameterAll(parameter, 0.9, 0.0, maxValue);
  if(std::strcmp(actionName, "scale up even") == 0)
    return preset.ScaleOscillatorParameterEven(parameter, 1.111111111111111, 0.0, maxValue);
  if(std::strcmp(actionName, "scale down even") == 0)
    return preset.ScaleOscillatorParameterEven(parameter, 0.9, 0.0, maxValue);
  if(std::strcmp(actionName, "scale up odd") == 0)
    return preset.ScaleOscillatorParameterOdd(parameter, 1.111111111111111, 0.0, maxValue);
  if(std::strcmp(actionName, "scale down odd") == 0)
    return preset.ScaleOscillatorParameterOdd(parameter, 0.9, 0.0, maxValue);

  return false;
}

inline void AppendAttackReleaseTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[2],
    "Attack time",
    OscillatorParameter::attack,
    {0.0, 1.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::attack)
  });
  descriptors.push_back({
    kOscillatorTabTitles[3],
    "Release time",
    OscillatorParameter::release,
    {0.0, 0.1},
    help_text::oscillator_tabs::Get(OscillatorParameter::release)
  });
}

inline void ResizeAttackReleaseTabPage(IContainerBase* pTab, const IRECT& r)
{
  if(pTab->NChildren() < 14)
    return;

  constexpr float kLeftInset = 104.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = 24.f;
  constexpr float kDescriptionHeight = 64.f;
  constexpr float kButtonHeight = 24.f;
  constexpr float kGap = 8.f;
  constexpr float kTightGap = 4.f;
  constexpr float kBottomPad = 8.f;
  constexpr float kHalfGap = 6.f;
  constexpr float kToggleLabelGap = 8.f;
  constexpr float kControlsBlockHeight =
    (kLabelHeight * 4.f) + (kControlHeight * 5.f) + (kGap * 4.f) + (kTightGap * 4.f);

  auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
  auto leftColumnBounds = innerBounds.GetFromLeft(kLeftInset);
  const auto sliderBounds = GetOscillatorSliderBounds(pTab, r, kLeftInset);

  const float restoreTop = leftColumnBounds.B - (kButtonHeight + kBottomPad);
  auto restoreButtonBounds = IRECT(
    leftColumnBounds.L + 8.f,
    restoreTop,
    leftColumnBounds.R - 8.f,
    leftColumnBounds.B - kBottomPad);

  auto descriptionBounds = IRECT(
    leftColumnBounds.L + 4.f,
    leftColumnBounds.T + 2.f,
    leftColumnBounds.R - 4.f,
    leftColumnBounds.T + 2.f + kDescriptionHeight);

  const float controlsTop = std::min(descriptionBounds.B, restoreTop - kGap - kControlsBlockHeight);
  float y = controlsTop;
  const float rowL = leftColumnBounds.L + 8.f;
  const float rowR = leftColumnBounds.R - 8.f;
  const float rowMid = (rowL + rowR) * 0.5f;
  auto xRangeLabelBounds = IRECT(rowL, y, rowR, y + kLabelHeight);
  y += kLabelHeight + kTightGap;
  auto xRangeMinBounds = IRECT(rowL, y, rowMid - kHalfGap * 0.5f, y + kControlHeight);
  auto xRangeMaxBounds = IRECT(rowMid + kHalfGap * 0.5f, y, rowR, y + kControlHeight);
  y += kControlHeight + kGap;
  auto yTransformLabelBounds = IRECT(rowL, y, rowR, y + kLabelHeight);
  y += kLabelHeight + kTightGap;
  auto yTransformBounds = IRECT(rowL, y, rowR, y + kControlHeight);
  y += kControlHeight + kGap;
  auto setShapeLabelBounds = IRECT(rowL, y, rowR, y + kLabelHeight);
  y += kLabelHeight + kTightGap;
  auto setShapeBounds = IRECT(rowL, y, rowR, y + kControlHeight);
  y += kControlHeight + kGap;
  auto actionsLabelBounds = IRECT(rowL, y, rowR, y + kLabelHeight);
  y += kLabelHeight + kTightGap;
  auto actionsBounds = IRECT(rowL, y, rowR, y + kControlHeight);
  auto allKeyNotesToggleBounds = IRECT(rowL, actionsBounds.B + kGap, rowL + kControlHeight, actionsBounds.B + kGap + kControlHeight);
  auto allKeyNotesLabelBounds = IRECT(
    allKeyNotesToggleBounds.R + kToggleLabelGap,
    allKeyNotesToggleBounds.T,
    rowR,
    allKeyNotesToggleBounds.B);

  pTab->GetChild(0)->SetTargetAndDrawRECTs(descriptionBounds);
  pTab->GetChild(1)->SetTargetAndDrawRECTs(xRangeLabelBounds);
  pTab->GetChild(2)->SetTargetAndDrawRECTs(xRangeMinBounds);
  pTab->GetChild(3)->SetTargetAndDrawRECTs(xRangeMaxBounds);
  pTab->GetChild(4)->SetTargetAndDrawRECTs(yTransformLabelBounds);
  pTab->GetChild(5)->SetTargetAndDrawRECTs(yTransformBounds);
  pTab->GetChild(6)->SetTargetAndDrawRECTs(setShapeLabelBounds);
  pTab->GetChild(7)->SetTargetAndDrawRECTs(setShapeBounds);
  pTab->GetChild(8)->SetTargetAndDrawRECTs(actionsLabelBounds);
  pTab->GetChild(9)->SetTargetAndDrawRECTs(actionsBounds);
  pTab->GetChild(10)->SetTargetAndDrawRECTs(allKeyNotesToggleBounds);
  pTab->GetChild(11)->SetTargetAndDrawRECTs(allKeyNotesLabelBounds);
  pTab->GetChild(12)->SetTargetAndDrawRECTs(restoreButtonBounds);
  pTab->GetChild(13)->SetTargetAndDrawRECTs(sliderBounds);
}

inline void AttachAttackReleaseTabChildren(IVTabPage* page,
                                           const std::shared_ptr<EditorContext>& context,
                                           const EditorStyles& styles,
                                           const OscillatorTabDescriptor& descriptor,
                                           IVButtonControl* restoreButton,
                                           OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);
  const auto attackReleaseIndex = GetAttackReleaseTabIndex(descriptor.parameter);
  auto* yTransformControl = CreateYTransformControl(
    GetAttackReleaseTransform(context, descriptor.parameter),
    sliderControl,
    styles);
  auto* setShapeControl = new ActionSelectionControl(
    IRECT(),
    "choose shape",
    descriptor.parameter == OscillatorParameter::attack
      ? std::initializer_list<const char*>{
          "linear ramp up (slow)",
          "linear ramp up (fast)",
          "sqrt ramp up (slow)",
          "sqrt ramp up (fast)",
          "logarithmic ramp up (slow)",
          "logarithmic ramp up (fast)",
          "flat"}
      : std::initializer_list<const char*>{
          "linear ramp down (slow)",
          "linear ramp down (fast)",
          "square ramp down (slow)",
          "square ramp down (fast)",
          "exponential ramp down (slow)",
          "exponential ramp down (fast)",
          "flat"},
    styles.utilityDropdownText,
    styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl, parameter = descriptor.parameter](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      parameter,
      [selectedText, parameter](SimplePreset& preset) {
        return ApplyAttackReleaseShape(preset, parameter, selectedText);
      });
  });

  auto* actionsControl = new ActionSelectionControl(
    IRECT(),
    "run action",
    {"scale up all", "scale down all", "scale up even", "scale down even", "scale up odd", "scale down odd"},
    styles.utilityDropdownText,
    styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl, parameter = descriptor.parameter](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      parameter,
      [selectedText, parameter](SimplePreset& preset) {
        return ApplyAttackReleaseAction(preset, parameter, selectedText);
      });
  });

  (*context->attackReleaseTab.setShapeControls)[attackReleaseIndex] = setShapeControl;
  (*context->attackReleaseTab.actionsControls)[attackReleaseIndex] = actionsControl;

  page->AddChildControl(CreateTabTitleControl(descriptor, styles));
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "X range:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(xRangeControls.minControl);
  page->AddChildControl(xRangeControls.maxControl);
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Y transform:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(yTransformControl);
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Set shape:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(setShapeControl);
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Actions:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(actionsControl);
  page->AddChildControl(allKeyNotesControls.toggleControl);
  page->AddChildControl(allKeyNotesControls.labelControl);
  page->AddChildControl(restoreButton);
  page->AddChildControl(sliderControl);
}
} // namespace editor
} // namespace plugin_ui

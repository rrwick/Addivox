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
  return ApplyScaleAction(preset, parameter, actionName, 0.0, GetAttackReleaseMaxValue(parameter));
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
  if(pTab->NChildren() < 15)
    return;

  constexpr float kLeftInset = 104.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = kEditorControlHeight;
  constexpr float kButtonHeight = kEditorControlHeight;
  constexpr float kGap = 8.f;
  constexpr float kTightGap = 0.f;
  constexpr float kBottomPad = 8.f;
  constexpr float kHalfGap = 6.f;
  constexpr float kToggleLabelGap = 8.f;

  auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
  auto leftColumnBounds = innerBounds.GetFromLeft(kLeftInset);
  const auto sliderBounds = GetOscillatorSliderBounds(pTab, r, kLeftInset);

  const float restoreTop = leftColumnBounds.B - (kButtonHeight + kBottomPad);
  auto restoreButtonBounds = IRECT(
    leftColumnBounds.L + 8.f,
    restoreTop,
    leftColumnBounds.R - 8.f,
    leftColumnBounds.B - kBottomPad);

  const float rowL = leftColumnBounds.L + 8.f;
  const float rowR = leftColumnBounds.R - 8.f;
  const float rowMid = (rowL + rowR) * 0.5f;
  const float allKeyNotesTop = restoreButtonBounds.T - kGap - kControlHeight;
  auto allKeyNotesToggleBounds = IRECT(rowL, allKeyNotesTop, rowL + kControlHeight, allKeyNotesTop + kControlHeight);
  auto allKeyNotesLabelBounds = IRECT(
    allKeyNotesToggleBounds.R + kToggleLabelGap,
    allKeyNotesToggleBounds.T,
    rowR,
    allKeyNotesToggleBounds.B);
  const float actionsTop = allKeyNotesToggleBounds.T - kGap - kControlHeight;
  auto actionsBounds = IRECT(rowL, actionsTop, rowR, actionsTop + kControlHeight);
  auto actionsLabelBounds = IRECT(rowL, actionsBounds.T - kTightGap - kLabelHeight, rowR, actionsBounds.T - kTightGap);
  const float setShapeTop = actionsLabelBounds.T - kGap - kControlHeight;
  auto setShapeBounds = IRECT(rowL, setShapeTop, rowR, setShapeTop + kControlHeight);
  auto setShapeLabelBounds = IRECT(rowL, setShapeBounds.T - kTightGap - kLabelHeight, rowR, setShapeBounds.T - kTightGap);
  const float editModeTop = setShapeLabelBounds.T - kGap - kControlHeight;
  auto editModeBounds = IRECT(rowL, editModeTop, rowR, editModeTop + kControlHeight);
  auto editModeLabelBounds = IRECT(rowL, editModeBounds.T - kTightGap - kLabelHeight, rowR, editModeBounds.T - kTightGap);
  const float yTransformTop = editModeLabelBounds.T - kGap - kControlHeight;
  auto yTransformBounds = IRECT(rowL, yTransformTop, rowR, yTransformTop + kControlHeight);
  auto yTransformLabelBounds = IRECT(rowL, yTransformBounds.T - kTightGap - kLabelHeight, rowR, yTransformBounds.T - kTightGap);
  const float xRangeTop = yTransformLabelBounds.T - kGap - kControlHeight;
  auto xRangeMinBounds = IRECT(rowL, xRangeTop, rowMid - kHalfGap * 0.5f, xRangeTop + kControlHeight);
  auto xRangeMaxBounds = IRECT(rowMid + kHalfGap * 0.5f, xRangeMinBounds.T, rowR, xRangeMinBounds.B);
  auto xRangeLabelBounds = IRECT(rowL, xRangeMinBounds.T - kTightGap - kLabelHeight, rowR, xRangeMinBounds.T - kTightGap);

  pTab->GetChild(0)->SetTargetAndDrawRECTs(xRangeLabelBounds);
  pTab->GetChild(1)->SetTargetAndDrawRECTs(xRangeMinBounds);
  pTab->GetChild(2)->SetTargetAndDrawRECTs(xRangeMaxBounds);
  pTab->GetChild(3)->SetTargetAndDrawRECTs(yTransformLabelBounds);
  pTab->GetChild(4)->SetTargetAndDrawRECTs(yTransformBounds);
  pTab->GetChild(5)->SetTargetAndDrawRECTs(editModeLabelBounds);
  pTab->GetChild(6)->SetTargetAndDrawRECTs(editModeBounds);
  pTab->GetChild(7)->SetTargetAndDrawRECTs(setShapeLabelBounds);
  pTab->GetChild(8)->SetTargetAndDrawRECTs(setShapeBounds);
  pTab->GetChild(9)->SetTargetAndDrawRECTs(actionsLabelBounds);
  pTab->GetChild(10)->SetTargetAndDrawRECTs(actionsBounds);
  pTab->GetChild(11)->SetTargetAndDrawRECTs(allKeyNotesToggleBounds);
  pTab->GetChild(12)->SetTargetAndDrawRECTs(allKeyNotesLabelBounds);
  pTab->GetChild(13)->SetTargetAndDrawRECTs(restoreButtonBounds);
  pTab->GetChild(14)->SetTargetAndDrawRECTs(sliderBounds);
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
    {"scale up", "scale down"},
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

  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "X range:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(xRangeControls.minControl);
  page->AddChildControl(xRangeControls.maxControl);
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Y transform:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(yTransformControl);
  page->AddChildControl(CreateEditModeLabelControl(styles));
  page->AddChildControl(CreateEditModeControl(context->model.oscillatorEditModes, descriptor, styles));
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

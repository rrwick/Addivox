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

inline bool TryGetAttackReleaseShapeValue(OscillatorParameter parameter,
                                          const char* shapeName,
                                          int oscillatorIndex,
                                          double& value)
{
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);

  if(parameter == OscillatorParameter::attack)
  {
    if(std::strcmp(shapeName, "linear ramp up") == 0)
    {
      value = harmonicNumber / 100.0;
      return true;
    }

    if(std::strcmp(shapeName, "square root ramp up") == 0)
    {
      value = std::sqrt(harmonicNumber) / 10.0;
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
    if(std::strcmp(shapeName, "linear ramp down") == 0)
    {
      value = 1.0 - (harmonicNumber / 100.0);
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

    preset.SetOscillatorParameter(oscillatorIndex, parameter, std::clamp(value, 0.0, 1.0));
  }

  return true;
}

inline bool ApplyAttackReleaseAction(SimplePreset& preset, OscillatorParameter parameter, const char* actionName)
{
  if(std::strcmp(actionName, "scale up all") == 0)
    return preset.ScaleOscillatorParameterAll(parameter, 1.1, 0.0, 1.0);
  if(std::strcmp(actionName, "scale down all") == 0)
    return preset.ScaleOscillatorParameterAll(parameter, 0.9, 0.0, 1.0);
  if(std::strcmp(actionName, "scale up even") == 0)
    return preset.ScaleOscillatorParameterEven(parameter, 1.1, 0.0, 1.0);
  if(std::strcmp(actionName, "scale down even") == 0)
    return preset.ScaleOscillatorParameterEven(parameter, 0.9, 0.0, 1.0);
  if(std::strcmp(actionName, "scale up odd") == 0)
    return preset.ScaleOscillatorParameterOdd(parameter, 1.1, 0.0, 1.0);
  if(std::strcmp(actionName, "scale down odd") == 0)
    return preset.ScaleOscillatorParameterOdd(parameter, 0.9, 0.0, 1.0);

  return false;
}

inline void AppendAttackReleaseTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[2],
    OscillatorParameter::attack,
    {0.0, 1.0},
    "Controls how\nquickly each\nharmonic ramps\nup at note start."
  });
  descriptors.push_back({
    kOscillatorTabTitles[3],
    OscillatorParameter::release,
    {0.0, 1.0},
    "Controls how\nquickly each\nharmonic fades\nafter note release."
  });
}

inline void ResizeAttackReleaseTabPage(IContainerBase* pTab, const IRECT& r)
{
  if(pTab->NChildren() < 12)
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
  constexpr float kControlsBlockHeight =
    (kLabelHeight * 4.f) + (kControlHeight * 4.f) + (kGap * 3.f) + (kTightGap * 4.f);

  auto innerBounds = r.GetPadded(-static_cast<float>(pTab->As<IVTabPage>()->GetPadding()));
  auto leftColumnBounds = innerBounds.GetFromLeft(kLeftInset);
  auto sliderBounds = innerBounds;
  sliderBounds.L += kLeftInset;

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

  const float controlsTop = std::min(descriptionBounds.B, restoreTop - kControlsBlockHeight);
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
  pTab->GetChild(10)->SetTargetAndDrawRECTs(restoreButtonBounds);
  pTab->GetChild(11)->SetTargetAndDrawRECTs(sliderBounds);
}

inline void AttachAttackReleaseTabChildren(IVTabPage* page,
                                           const std::shared_ptr<EditorContext>& context,
                                           const EditorStyles& styles,
                                           const OscillatorTabDescriptor& descriptor,
                                           IVButtonControl* restoreButton,
                                           OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto attackReleaseIndex = GetAttackReleaseTabIndex(descriptor.parameter);
  auto* yTransformControl = CreateYTransformControl(
    GetAttackReleaseTransform(context, descriptor.parameter),
    sliderControl,
    styles);
  auto* setShapeControl = new ActionSelectionControl(
    IRECT(),
    "choose shape",
    descriptor.parameter == OscillatorParameter::attack
      ? std::initializer_list<const char*>{"linear ramp up", "square root ramp up", "flat"}
      : std::initializer_list<const char*>{"linear ramp down", "flat"},
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

  page->AddChildControl(MakePassiveControl(new IMultiLineTextControl(IRECT(), descriptor.description, styles.descriptionText, COLOR_TRANSPARENT)));
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "X range:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(xRangeControls.minControl);
  page->AddChildControl(xRangeControls.maxControl);
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Y transform:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(yTransformControl);
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Set shape:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(setShapeControl);
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "Actions:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(actionsControl);
  page->AddChildControl(restoreButton);
  page->AddChildControl(sliderControl);
}
} // namespace editor
} // namespace plugin_ui

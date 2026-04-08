#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline bool TryGetBreathShapeValue(const char* shapeName, int oscillatorIndex, double& value)
{
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);

  if(std::strcmp(shapeName, "flat") == 0)
  {
    value = 1.0;
    return true;
  }

  if(std::strcmp(shapeName, "linear ramp") == 0)
  {
    value = harmonicNumber;
    return true;
  }

  if(std::strcmp(shapeName, "square ramp") == 0)
  {
    value = 1.0 + ((harmonicNumber * harmonicNumber) / 101.01010101010101);
    return true;
  }

  if(std::strcmp(shapeName, "cube ramp") == 0)
  {
    value = 1.0 + ((harmonicNumber * harmonicNumber * harmonicNumber) / 10101.010101010101);
    return true;
  }

  return false;
}

inline bool ApplyBreathShape(SimplePreset& preset, const char* shapeName)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    double value = 0.0;
    if(!TryGetBreathShapeValue(shapeName, oscillatorIndex, value))
      return false;

    preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::breath_power, value);
  }

  return true;
}

inline bool ApplyBreathAction(SimplePreset& preset, const char* actionName)
{
  return ApplyScaleAction(preset, OscillatorParameter::breath_power, actionName, 0.0, 100.0);
}

inline void AppendBreathTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[1],
    "Breath power",
    OscillatorParameter::breath_power,
    {0.0, 100.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::breath_power)
  });
}

inline void ResizeBreathTabPage(IContainerBase* pTab, const IRECT& r)
{
  if(pTab->NChildren() < 15)
    return;

  constexpr float kLeftInset = 104.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = 24.f;
  constexpr float kButtonHeight = 24.f;
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

inline void AttachBreathTabChildren(IVTabPage* page,
                                    const std::shared_ptr<EditorContext>& context,
                                    const EditorStyles& styles,
                                    const OscillatorTabDescriptor& descriptor,
                                    IVButtonControl* restoreButton,
                                    OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);

  auto* yTransformControl = CreateYTransformControl(context->breathTab.breathTransform, sliderControl, styles);

  auto* setShapeControl = new ActionSelectionControl(
    IRECT(),
    "choose shape",
    {"flat", "linear ramp", "square ramp", "cube ramp"},
    styles.utilityDropdownText,
    styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::breath_power,
      [selectedText](SimplePreset& preset) {
        return ApplyBreathShape(preset, selectedText);
      });
  });

  auto* actionsControl = new ActionSelectionControl(
    IRECT(),
    "run action",
    {"scale up", "scale down"},
    styles.utilityDropdownText,
    styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::breath_power,
      [selectedText](SimplePreset& preset) {
        return ApplyBreathAction(preset, selectedText);
      });
  });

  *context->breathTab.setShapeControl = setShapeControl;
  *context->breathTab.actionsControl = actionsControl;

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

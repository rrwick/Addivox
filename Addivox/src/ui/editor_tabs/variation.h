#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline bool TryGetVariationShapeValue(const char* shapeName, int oscillatorIndex, double& value)
{
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);

  if(std::strcmp(shapeName, "zero") == 0)
  {
    value = 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "flat") == 0)
  {
    value = 1.0;
    return true;
  }

  if(std::strcmp(shapeName, "linear ramp up") == 0)
  {
    value = harmonicNumber / 10.0;
    return true;
  }

  return false;
}

inline bool ApplyVariationShape(SimplePreset& preset, OscillatorParameter parameter, const char* shapeName)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    double value = 0.0;
    if(!TryGetVariationShapeValue(shapeName, oscillatorIndex, value))
      return false;

    preset.SetOscillatorParameter(oscillatorIndex, parameter, std::clamp(value, 0.0, 10.0));
  }

  return true;
}

inline bool ApplyVariationAction(SimplePreset& preset, OscillatorParameter parameter, const char* actionName)
{
  return ApplyScaleAction(preset, parameter, actionName, 0.0, 10.0);
}

inline void AppendVariationTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[6],
    "Level variation amount",
    OscillatorParameter::intensity_variation_amplitude,
    {0.0, 10.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::intensity_variation_amplitude)
  });
  descriptors.push_back({
    kOscillatorTabTitles[7],
    "Level variation rate",
    OscillatorParameter::intensity_variation_rate,
    {0.0, 10.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::intensity_variation_rate)
  });
  descriptors.push_back({
    kOscillatorTabTitles[8],
    "Pitch variation amount",
    OscillatorParameter::pitch_variation_amplitude,
    {0.0, 10.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::pitch_variation_amplitude)
  });
  descriptors.push_back({
    kOscillatorTabTitles[9],
    "Pitch variation rate",
    OscillatorParameter::pitch_variation_rate,
    {0.0, 10.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::pitch_variation_rate)
  });
  descriptors.push_back({
    kOscillatorTabTitles[10],
    "Pan variation amount",
    OscillatorParameter::pan_variation_amplitude,
    {0.0, 10.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::pan_variation_amplitude)
  });
  descriptors.push_back({
    kOscillatorTabTitles[11],
    "Pan variation rate",
    OscillatorParameter::pan_variation_rate,
    {0.0, 10.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::pan_variation_rate)
  });
}

inline void ResizeVariationTabPage(IContainerBase* pTab, const IRECT& r)
{
  if(pTab->NChildren() < 15)
    return;

  constexpr float kLeftInset = 104.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = 24.f;
  constexpr float kDescriptionHeight = 44.f;
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
  auto editModeLabelBounds = IRECT(rowL, descriptionBounds.T, rowR, descriptionBounds.T + kLabelHeight);
  auto editModeBounds = IRECT(rowL, editModeLabelBounds.B + kTightGap, rowR, editModeLabelBounds.B + kTightGap + kControlHeight);
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

  pTab->GetChild(0)->SetTargetAndDrawRECTs(editModeLabelBounds);
  pTab->GetChild(1)->SetTargetAndDrawRECTs(editModeBounds);
  pTab->GetChild(2)->SetTargetAndDrawRECTs(xRangeLabelBounds);
  pTab->GetChild(3)->SetTargetAndDrawRECTs(xRangeMinBounds);
  pTab->GetChild(4)->SetTargetAndDrawRECTs(xRangeMaxBounds);
  pTab->GetChild(5)->SetTargetAndDrawRECTs(yTransformLabelBounds);
  pTab->GetChild(6)->SetTargetAndDrawRECTs(yTransformBounds);
  pTab->GetChild(7)->SetTargetAndDrawRECTs(setShapeLabelBounds);
  pTab->GetChild(8)->SetTargetAndDrawRECTs(setShapeBounds);
  pTab->GetChild(9)->SetTargetAndDrawRECTs(actionsLabelBounds);
  pTab->GetChild(10)->SetTargetAndDrawRECTs(actionsBounds);
  pTab->GetChild(11)->SetTargetAndDrawRECTs(allKeyNotesToggleBounds);
  pTab->GetChild(12)->SetTargetAndDrawRECTs(allKeyNotesLabelBounds);
  pTab->GetChild(13)->SetTargetAndDrawRECTs(restoreButtonBounds);
  pTab->GetChild(14)->SetTargetAndDrawRECTs(sliderBounds);
}

inline void AttachVariationTabChildren(IVTabPage* page,
                                       const std::shared_ptr<EditorContext>& context,
                                       const EditorStyles& styles,
                                       const OscillatorTabDescriptor& descriptor,
                                       IVButtonControl* restoreButton,
                                       OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);
  const auto variationIndex = GetVariationTabIndex(descriptor.parameter);
  auto* yTransformControl = CreateYTransformControl(
    std::shared_ptr<EditorLevelTransform>(context->variationTab.transforms, &(*context->variationTab.transforms)[variationIndex]),
    sliderControl,
    styles);
  auto* setShapeControl = new ActionSelectionControl(
    IRECT(),
    "choose shape",
    {"zero", "flat", "linear ramp up"},
    styles.utilityDropdownText,
    styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl, parameter = descriptor.parameter](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      parameter,
      [selectedText, parameter](SimplePreset& preset) {
        return ApplyVariationShape(preset, parameter, selectedText);
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
        return ApplyVariationAction(preset, parameter, selectedText);
      });
  });

  (*context->variationTab.setShapeControls)[variationIndex] = setShapeControl;
  (*context->variationTab.actionsControls)[variationIndex] = actionsControl;

  page->AddChildControl(CreateEditModeLabelControl(styles));
  page->AddChildControl(CreateEditModeControl(context->model.oscillatorEditModes, descriptor, styles));
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

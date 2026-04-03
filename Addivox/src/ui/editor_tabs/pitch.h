#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline bool TryGetPitchShapeValue(const char* shapeName, int oscillatorIndex, double& value)
{
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);
  const double centsOffset = harmonicNumber - 1.0;

  if(std::strcmp(shapeName, "zero") == 0)
  {
    value = 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "alternating") == 0)
  {
    value = (oscillatorIndex % 2) == 0 ? -10.0 : 10.0;
    return true;
  }

  if(std::strcmp(shapeName, "ramp sharp") == 0)
  {
    value = centsOffset;
    return true;
  }

  if(std::strcmp(shapeName, "ramp flat") == 0)
  {
    value = -centsOffset;
    return true;
  }

  if(std::strcmp(shapeName, "ramp alternating") == 0)
  {
    value = ((oscillatorIndex % 2) == 1 ? 1.0 : -1.0) * centsOffset;
    return true;
  }

  return false;
}

inline bool ApplyPitchShape(SimplePreset& preset, const char* shapeName)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    double value = 0.0;
    if(!TryGetPitchShapeValue(shapeName, oscillatorIndex, value))
      return false;

    preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pitch, value);
  }

  return true;
}

inline bool ApplyPitchAction(SimplePreset& preset, const char* actionName)
{
  constexpr double kPitchMin = -100.0;
  constexpr double kPitchMax = 100.0;

  if(ApplyScaleAction(preset, OscillatorParameter::pitch, actionName, kPitchMin, kPitchMax))
    return true;
  if(std::strcmp(actionName, "zero even") == 0)
  {
    for(int oscillatorIndex = 1; oscillatorIndex < SimplePreset::kNumOscillators; oscillatorIndex += 2)
      preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pitch, 0.0);
    return true;
  }
  if(std::strcmp(actionName, "zero odd") == 0)
  {
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; oscillatorIndex += 2)
      preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pitch, 0.0);
    return true;
  }
  if(std::strcmp(actionName, "flip sign") == 0)
  {
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
    {
      const double pitch = preset.GetOscillatorSettings(oscillatorIndex).pitch;
      preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pitch, -pitch);
    }
    return true;
  }

  return false;
}

inline void AppendPitchTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[4],
    "Pitch offset",
    OscillatorParameter::pitch,
    {-100.0, 100.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::pitch)
  });
}

inline void ResizePitchTabPage(IContainerBase* pTab, const IRECT& r)
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

inline void AttachPitchTabChildren(IVTabPage* page,
                                   const std::shared_ptr<EditorContext>& context,
                                   const EditorStyles& styles,
                                   const OscillatorTabDescriptor& descriptor,
                                   IVButtonControl* restoreButton,
                                   OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);
  auto* yTransformControl = CreateYTransformControl(context->pitchTab.pitchTransform, sliderControl, styles);
  auto* setShapeControl = new ActionSelectionControl(
    IRECT(),
    "choose shape",
    {"zero", "alternating", "ramp sharp", "ramp flat", "ramp alternating"},
    styles.utilityDropdownText,
    styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::pitch,
      [selectedText](SimplePreset& preset) {
        return ApplyPitchShape(preset, selectedText);
      });
  });
  auto* actionsControl = new ActionSelectionControl(
    IRECT(),
    "run action",
    {"scale up all", "scale down all", "scale up even", "scale down even", "scale up odd", "scale down odd", "zero even", "zero odd", "flip sign"},
    styles.utilityDropdownText,
    styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::pitch,
      [selectedText](SimplePreset& preset) {
        return ApplyPitchAction(preset, selectedText);
      });
  });
  *context->pitchTab.setShapeControl = setShapeControl;
  *context->pitchTab.actionsControl = actionsControl;

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

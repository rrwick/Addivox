#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline void AppendLevelTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[0],
    OscillatorParameter::intensity,
    {0.0, 1.0},
    "Controls the\nintensity of each\nharmonic at full\nbreath."
  });
}

inline void ResizeLevelTabPage(IContainerBase* pTab, const IRECT& r)
{
  if(pTab->NChildren() < 12)
    return;

  constexpr float kLeftInset = 104.f;
  constexpr float kLabelHeight = 14.f;
  constexpr float kControlHeight = 24.f;
  constexpr float kDescriptionHeight = 96.f;
  constexpr float kButtonHeight = 24.f;
  constexpr float kGap = 8.f;
  constexpr float kTightGap = 4.f;
  constexpr float kBottomPad = 8.f;
  constexpr float kHalfGap = 6.f;

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
    std::min(leftColumnBounds.T + kDescriptionHeight, restoreTop - (kLabelHeight * 4.f + kControlHeight * 4.f + kGap * 5.f + kTightGap * 4.f)));

  float y = descriptionBounds.B + kGap;
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

inline void AttachLevelTabChildren(IVTabPage* page,
                                   const std::shared_ptr<EditorContext>& context,
                                   const EditorStyles& styles,
                                   const OscillatorTabDescriptor& descriptor,
                                   IVButtonControl* restoreButton,
                                   OscillatorSliderControl* sliderControl)
{
  auto* xRangeMinControl = new IVNumberBoxControl(
    IRECT(), kNoParameter, nullptr, "", styles.utilityNumberBoxStyle, false,
    static_cast<double>(*context->levelTab.levelXRangeMin), 1.0, 100.0, "%0.0f", false);
  auto* xRangeMaxControl = new IVNumberBoxControl(
    IRECT(), kNoParameter, nullptr, "", styles.utilityNumberBoxStyle, false,
    static_cast<double>(*context->levelTab.levelXRangeMax), 1.0, 100.0, "%0.0f", false);
  xRangeMinControl->SetDrawTriangle(false);
  xRangeMaxControl->SetDrawTriangle(false);

  auto xRangeConstraintGuard = std::make_shared<bool>(false);
  const auto updateVisibleOscillatorRange = [context, sliderControl, xRangeMinControl, xRangeMaxControl]() {
    *context->levelTab.levelXRangeMin = static_cast<int>(xRangeMinControl->GetRealValue());
    *context->levelTab.levelXRangeMax = static_cast<int>(xRangeMaxControl->GetRealValue());
    sliderControl->SetVisibleOscillatorRange(*context->levelTab.levelXRangeMin, *context->levelTab.levelXRangeMax);
  };
  const auto clampEditedXRange = [xRangeConstraintGuard, xRangeMinControl, xRangeMaxControl](IVNumberBoxControl* editedControl) {
    if(*xRangeConstraintGuard)
      return;

    const double minValue = xRangeMinControl->GetRealValue();
    const double maxValue = xRangeMaxControl->GetRealValue();
    if(minValue <= maxValue)
      return;

    *xRangeConstraintGuard = true;
    WDL_String textValue;
    textValue.SetFormatted(32, "%0.0f", editedControl == xRangeMinControl ? maxValue : minValue);
    editedControl->OnTextEntryCompletion(textValue.Get(), 0);
    *xRangeConstraintGuard = false;
  };
  xRangeMinControl->SetActionFunction([clampEditedXRange, updateVisibleOscillatorRange, xRangeMinControl](IControl*) {
    clampEditedXRange(xRangeMinControl);
    updateVisibleOscillatorRange();
  });
  xRangeMaxControl->SetActionFunction([clampEditedXRange, updateVisibleOscillatorRange, xRangeMaxControl](IControl*) {
    clampEditedXRange(xRangeMaxControl);
    updateVisibleOscillatorRange();
  });
  updateVisibleOscillatorRange();

  auto* yTransformControl = new ActionSelectionControl(
    IRECT(),
    GetLevelTransformLabel(*context->levelTab.levelTransform),
    {"linear", "square root", "pseudo-log"},
    styles.utilityDropdownText,
    styles.darkTab,
    true);
  yTransformControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    if(std::strcmp(selectedText, "square root") == 0)
      *context->levelTab.levelTransform = EditorLevelTransform::SquareRoot;
    else if(std::strcmp(selectedText, "pseudo-log") == 0)
      *context->levelTab.levelTransform = EditorLevelTransform::PseudoLog;
    else
      *context->levelTab.levelTransform = EditorLevelTransform::Linear;

    auto config = sliderControl->GetConfig();
    config.transform = GetSliderValueTransform(*context->levelTab.levelTransform);
    sliderControl->SetConfig(config);
  });

  auto* setShapeControl = new ActionSelectionControl(
    IRECT(), "choose shape", {"saw", "square", "triangle", "sine"}, styles.utilityDropdownText, styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyLevelActionToSelectedKeyNote(sliderControl, [selectedText](SimplePreset& preset) {
      for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
      {
        const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);
        double intensity = 0.0;

        if(std::strcmp(selectedText, "saw") == 0)
          intensity = 1.0 / harmonicNumber;
        else if(std::strcmp(selectedText, "square") == 0)
          intensity = (oscillatorIndex % 2 == 0) ? (1.0 / harmonicNumber) : 0.0;
        else if(std::strcmp(selectedText, "triangle") == 0)
          intensity = (oscillatorIndex % 2 == 0) ? (1.0 / (harmonicNumber * harmonicNumber)) : 0.0;
        else if(std::strcmp(selectedText, "sine") == 0)
          intensity = (oscillatorIndex == 0) ? 1.0 : 0.0;
        else
          return false;

        preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::intensity, intensity);
      }

      preset.ApplyIntensityTopTaper();
      return preset.NormalizeIntensityWaveformRms();
    });
  });

  auto* actionsControl = new ActionSelectionControl(
    IRECT(),
    "run action",
    {"normalise", "taper top", "smooth", "scale up all", "scale down all", "scale up even", "scale down even", "scale up odd", "scale down odd", "zero even", "zero odd"},
    styles.utilityDropdownText,
    styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyLevelActionToSelectedKeyNote(sliderControl, [selectedText](SimplePreset& preset) {
      if(std::strcmp(selectedText, "normalise") == 0)
        return preset.NormalizeIntensityWaveformRms();
      if(std::strcmp(selectedText, "taper top") == 0)
        return preset.ApplyIntensityTopTaper();
      if(std::strcmp(selectedText, "smooth") == 0)
        return preset.SmoothIntensity();
      if(std::strcmp(selectedText, "scale up all") == 0)
        return preset.ScaleIntensityUp();
      if(std::strcmp(selectedText, "scale down all") == 0)
        return preset.ScaleIntensityDown();
      if(std::strcmp(selectedText, "scale up even") == 0)
        return preset.ScaleIntensityUpEven();
      if(std::strcmp(selectedText, "scale down even") == 0)
        return preset.ScaleIntensityDownEven();
      if(std::strcmp(selectedText, "scale up odd") == 0)
        return preset.ScaleIntensityUpOdd();
      if(std::strcmp(selectedText, "scale down odd") == 0)
        return preset.ScaleIntensityDownOdd();
      if(std::strcmp(selectedText, "zero even") == 0)
        return preset.ZeroEvenIntensities();
      if(std::strcmp(selectedText, "zero odd") == 0)
        return preset.ZeroOddIntensities();
      return false;
    });
  });

  *context->levelTab.setShapeControl = setShapeControl;
  *context->levelTab.actionsControl = actionsControl;

  page->AddChildControl(MakePassiveControl(new IMultiLineTextControl(IRECT(), descriptor.description, styles.descriptionText, COLOR_TRANSPARENT)));
  page->AddChildControl(MakePassiveControl(new ITextControl(IRECT(), "X range:", styles.utilityLabelText, COLOR_TRANSPARENT)));
  page->AddChildControl(xRangeMinControl);
  page->AddChildControl(xRangeMaxControl);
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

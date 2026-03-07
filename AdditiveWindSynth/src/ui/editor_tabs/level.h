#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline double GetLevelPseudoLogRolloffIntensity(int oscillatorIndex)
{
  const double displayRampValue = 1.0 - (static_cast<double>(oscillatorIndex) / static_cast<double>(SimplePreset::kNumOscillators));
  return transformations::NormalizedExp(displayRampValue, transformations::GetGlobalPseudoLogShapeValue());
}

inline bool IsPowerOfTwo(int value)
{
  return value > 0 && (value & (value - 1)) == 0;
}

template <std::size_t N>
inline bool IsListedHarmonic(int harmonicIndex, const std::array<int, N>& harmonics)
{
  return std::binary_search(harmonics.begin(), harmonics.end(), harmonicIndex);
}

inline bool IsOctavesAndFifthsHarmonic(int harmonicIndex)
{
  static constexpr std::array<int, 13> kHarmonics{{1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96}};
  return IsListedHarmonic(harmonicIndex, kHarmonics);
}

inline bool IsOctavesFifthsAndThirdsHarmonic(int harmonicIndex)
{
  static constexpr std::array<int, 18> kHarmonics{{1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64, 80, 96}};
  return IsListedHarmonic(harmonicIndex, kHarmonics);
}

inline bool ShouldApplyLevelShapeTopTaper(const char* shapeName)
{
  return std::strcmp(shapeName, "saw") == 0
      || std::strcmp(shapeName, "square") == 0
      || std::strcmp(shapeName, "triangle") == 0
      || std::strcmp(shapeName, "flat") == 0;
}

inline bool TryGetLevelShapeIntensity(const char* shapeName, int oscillatorIndex, double& intensity)
{
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);
  const int harmonicIndex = oscillatorIndex + 1;

  if(std::strcmp(shapeName, "sine") == 0)
  {
    intensity = (oscillatorIndex == 0) ? 1.0 : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "saw") == 0)
  {
    intensity = 1.0 / harmonicNumber;
    return true;
  }

  if(std::strcmp(shapeName, "square") == 0)
  {
    intensity = (oscillatorIndex % 2 == 0) ? (1.0 / harmonicNumber) : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "triangle") == 0)
  {
    intensity = (oscillatorIndex % 2 == 0) ? (1.0 / (harmonicNumber * harmonicNumber)) : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "flat") == 0)
  {
    intensity = 1.0;
    return true;
  }

  if(std::strcmp(shapeName, "rolloff") == 0)
  {
    intensity = GetLevelPseudoLogRolloffIntensity(oscillatorIndex);
    return true;
  }

  if(std::strcmp(shapeName, "rolloff odd") == 0)
  {
    intensity = (oscillatorIndex % 2 == 0) ? GetLevelPseudoLogRolloffIntensity(oscillatorIndex) : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "octaves") == 0)
  {
    intensity = IsPowerOfTwo(harmonicIndex) ? 1.0 : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "octaves+fifths") == 0)
  {
    intensity = IsOctavesAndFifthsHarmonic(harmonicIndex) ? 1.0 : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "octaves+fifths+thirds") == 0)
  {
    intensity = IsOctavesFifthsAndThirdsHarmonic(harmonicIndex) ? 1.0 : 0.0;
    return true;
  }

  return false;
}

inline bool ApplyLevelShape(SimplePreset& preset, const char* shapeName)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    double intensity = 0.0;
    if(!TryGetLevelShapeIntensity(shapeName, oscillatorIndex, intensity))
      return false;

    preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::intensity, intensity);
  }

  if(ShouldApplyLevelShapeTopTaper(shapeName))
    preset.ApplyIntensityTopTaper();

  return preset.NormalizeIntensityWaveformRms();
}

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

inline void AttachLevelTabChildren(IVTabPage* page,
                                   const std::shared_ptr<EditorContext>& context,
                                   const EditorStyles& styles,
                                   const OscillatorTabDescriptor& descriptor,
                                   IVButtonControl* restoreButton,
                                   OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);

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
    IRECT(), "choose shape", {"sine", "saw", "square", "triangle", "flat", "rolloff", "rolloff odd", "octaves", "octaves+fifths", "octaves+fifths+thirds"}, styles.utilityDropdownText, styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::intensity,
      [selectedText](SimplePreset& preset) {
        return ApplyLevelShape(preset, selectedText);
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

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::intensity,
      [selectedText](SimplePreset& preset) {
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

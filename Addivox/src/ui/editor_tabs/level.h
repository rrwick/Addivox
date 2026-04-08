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

template <std::size_t N>
inline bool IsListedHarmonic(int harmonicIndex, const std::array<int, N>& harmonics)
{
  return std::binary_search(harmonics.begin(), harmonics.end(), harmonicIndex);
}

inline bool IsOctavesHarmonic(int harmonicIndex)
{
  return harmonicIndex > 0 && (harmonicIndex & (harmonicIndex - 1)) == 0;
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
    intensity = IsOctavesHarmonic(harmonicIndex) ? 1.0 : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "octaves+fifths") == 0)
  {
    if (IsOctavesHarmonic(harmonicIndex))
      intensity = 1.0;
    else if (IsOctavesAndFifthsHarmonic(harmonicIndex))
      intensity = 0.5;
    else
      intensity = 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "octaves+fifths+thirds") == 0)
  {
    if (IsOctavesHarmonic(harmonicIndex))
      intensity = 1.0;
    else if (IsOctavesAndFifthsHarmonic(harmonicIndex))
      intensity = 0.5;
    else if (IsOctavesFifthsAndThirdsHarmonic(harmonicIndex))
      intensity = 0.25;
    else
      intensity = 0.0;
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
    "Level",
    OscillatorParameter::intensity,
    {0.0, 1.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::intensity)
  });
}

inline void ResizeLevelTabPage(IContainerBase* pTab, const IRECT& r)
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

inline void AttachLevelTabChildren(IVTabPage* page,
                                   const std::shared_ptr<EditorContext>& context,
                                   const EditorStyles& styles,
                                   const OscillatorTabDescriptor& descriptor,
                                   IVButtonControl* restoreButton,
                                   OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);

  auto* yTransformControl = CreateYTransformControl(context->levelTab.levelTransform, sliderControl, styles);

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
    {"normalise", "taper top", "smooth", "scale up", "scale down", "zero"},
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
        if(ApplyScaleAction(preset, OscillatorParameter::intensity, selectedText, 0.0, 1.0))
          return true;
        if(std::strcmp(selectedText, "zero") == 0)
        {
          for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
            preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::intensity, 0.0);
          return true;
        }
        return false;
      });
  });

  *context->levelTab.setShapeControl = setShapeControl;
  *context->levelTab.actionsControl = actionsControl;

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

#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
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

inline bool TryGetLevelShapeValue(const char* shapeName, int oscillatorIndex, double& level)
{
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);
  const int harmonicIndex = oscillatorIndex + 1;

  if(std::strcmp(shapeName, "sine") == 0)
  {
    level = (oscillatorIndex == 0) ? 1.0 : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "saw") == 0)
  {
    level = 1.0 / harmonicNumber;
    return true;
  }

  if(std::strcmp(shapeName, "square") == 0)
  {
    level = (oscillatorIndex % 2 == 0) ? (1.0 / harmonicNumber) : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "triangle") == 0)
  {
    level = (oscillatorIndex % 2 == 0) ? (1.0 / (harmonicNumber * harmonicNumber)) : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "flat") == 0)
  {
    level = 1.0;
    return true;
  }

  if(std::strcmp(shapeName, "octaves") == 0)
  {
    level = IsOctavesHarmonic(harmonicIndex) ? 1.0 : 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "octaves+fifths") == 0)
  {
    if (IsOctavesHarmonic(harmonicIndex))
      level = 1.0;
    else if (IsOctavesAndFifthsHarmonic(harmonicIndex))
      level = 0.5;
    else
      level = 0.0;
    return true;
  }

  if(std::strcmp(shapeName, "octaves+fifths+thirds") == 0)
  {
    if (IsOctavesHarmonic(harmonicIndex))
      level = 1.0;
    else if (IsOctavesAndFifthsHarmonic(harmonicIndex))
      level = 0.5;
    else if (IsOctavesFifthsAndThirdsHarmonic(harmonicIndex))
      level = 0.25;
    else
      level = 0.0;
    return true;
  }

  return false;
}

inline bool ApplyLevelShape(SimplePatch& patch, const char* shapeName)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex)
  {
    double level = 0.0;
    if(!TryGetLevelShapeValue(shapeName, oscillatorIndex, level))
      return false;

    patch.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::level, level);
  }

  return patch.NormalizeLevelWaveformRms();
}

inline bool ApplyLevelAction(SimplePatch& patch, const char* actionName, EditorOscillatorEditScope editScope)
{
  if(MatchesActionLabel(actionName, kActionNormalize))
    return patch.NormalizeLevelWaveformRms();

  return ApplyStandardHarmonicAction(patch, OscillatorParameter::level, actionName, 0.0, 1.0, editScope);
}

inline void AppendLevelTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[0],
    "Level",
    OscillatorParameter::level,
    {0.0, 1.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::level)
  });
}

inline void AttachLevelTabChildren(IVTabPage* page,
                                   const std::shared_ptr<EditorContext>& context,
                                   const EditorStyles& styles,
                                   const OscillatorTabDescriptor& descriptor,
                                   IVButtonControl* restoreButton,
                                   IVButtonControl* addButton,
                                   IVButtonControl* deleteButton,
                                   OscillatorSliderControl* sliderControl)
{
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);

  auto* yTransformControl = CreateYTransformControl(context->levelTab.levelTransform, sliderControl, styles);

  auto* setShapeControl = new ActionSelectionControl(
    IRECT(), "choose shape", {"sine", "saw", "square", "triangle", "flat", "octaves", "octaves+fifths", "octaves+fifths+thirds"}, styles.utilityDropdownText, styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::level,
      [selectedText](SimplePatch& patch) {
        return ApplyLevelShape(patch, selectedText);
      });
  });

  auto* actionsControl = new ActionSelectionControl(
    IRECT(),
    "run action",
    {
      kActionScaleUpMenuLabel,
      kActionScaleDownMenuLabel,
      kActionTowardMaxMenuLabel,
      kActionAwayFromMaxMenuLabel,
      kActionBendUpMenuLabel,
      kActionBendDownMenuLabel,
      kActionNormalizeMenuLabel},
    styles.utilityDropdownText,
    styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    const bool applyEditScope = !MatchesActionLabel(selectedText, kActionNormalize);
    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::level,
      [selectedText, context](SimplePatch& patch) {
        return ApplyLevelAction(
          patch,
          selectedText,
          context->GetOscillatorEditScope(OscillatorParameter::level));
      },
      applyEditScope);
  });

  *context->levelTab.setShapeControl = setShapeControl;
  *context->levelTab.actionsControl = actionsControl;

  AttachHarmonicTabChildren(
    page,
    context,
    styles,
    descriptor,
    xRangeControls,
    yTransformControl,
    setShapeControl,
    actionsControl,
    allKeyNotesControls,
    restoreButton,
    addButton,
    deleteButton,
    sliderControl);
}
} // namespace editor
} // namespace plugin_ui

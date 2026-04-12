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

  return preset.NormalizeIntensityWaveformRms();
}

inline bool ApplyLevelAction(SimplePreset& preset, const char* actionName, EditorOscillatorEditScope editScope)
{
  if(MatchesActionLabel(actionName, kActionNormalize))
    return preset.NormalizeIntensityWaveformRms();

  return ApplyStandardHarmonicAction(preset, OscillatorParameter::intensity, actionName, 0.0, 1.0, editScope);
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
    IRECT(), "choose shape", {"sine", "saw", "square", "triangle", "flat", "octaves", "octaves+fifths", "octaves+fifths+thirds"}, styles.utilityDropdownText, styles.darkTab);
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

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::intensity,
      [selectedText, context](SimplePreset& preset) {
        return ApplyLevelAction(
          preset,
          selectedText,
          context->GetOscillatorEditScope(OscillatorParameter::intensity));
      });
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
    sliderControl);
}
} // namespace editor
} // namespace plugin_ui

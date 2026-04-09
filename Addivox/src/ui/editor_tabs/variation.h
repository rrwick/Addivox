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

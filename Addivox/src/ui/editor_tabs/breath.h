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

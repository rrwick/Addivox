#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline double ApplyLinearScale(double value, double scale, double minValue, double maxValue)
{
  return std::clamp(value * scale, minValue, maxValue);
}

inline double ApplyTowardMaxScale(double value, double factor, double minValue, double maxValue)
{
  return std::clamp(maxValue - ((maxValue - value) * factor), minValue, maxValue);
}

inline double ApplyCurveWarp(double value, double exponent, double minValue, double maxValue)
{
  if(!std::isfinite(exponent) || exponent <= 0.0 || maxValue <= minValue)
    return std::clamp(value, minValue, maxValue);

  const double normalizedValue = std::clamp((value - minValue) / (maxValue - minValue), 0.0, 1.0);
  const double warpedValue = std::pow(normalizedValue, exponent);
  return minValue + ((maxValue - minValue) * warpedValue);
}

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
  constexpr double kMinValue = 0.0;
  constexpr double kMaxValue = 100.0;
  constexpr double kScaleUp = 1.01010101010101;
  constexpr double kScaleDown = 0.99;
  constexpr double kTowardMaxFactor = 0.999;
  constexpr double kAwayFromMaxFactor = 1.001001001001001;
  constexpr double kCurveExponent = 1.05;
  double currentMinValue = kMaxValue;
  double currentMaxValue = kMinValue;

  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    const double value = preset.GetOscillatorSettings(oscillatorIndex).breath_power;
    currentMinValue = std::min(currentMinValue, value);
    currentMaxValue = std::max(currentMaxValue, value);
  }

  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    const double value = preset.GetOscillatorSettings(oscillatorIndex).breath_power;
    double updatedValue = value;

    if(std::strcmp(actionName, "scale up") == 0)
      updatedValue = ApplyLinearScale(value, kScaleUp, kMinValue, kMaxValue);
    else if(std::strcmp(actionName, "scale down") == 0)
      updatedValue = ApplyLinearScale(value, kScaleDown, kMinValue, kMaxValue);
    else if(std::strcmp(actionName, "toward max") == 0)
      updatedValue = ApplyTowardMaxScale(value, kTowardMaxFactor, kMinValue, kMaxValue);
    else if(std::strcmp(actionName, "away from max") == 0)
      updatedValue = ApplyTowardMaxScale(value, kAwayFromMaxFactor, kMinValue, kMaxValue);
    else if(std::strcmp(actionName, "curve earlier") == 0)
      updatedValue = ApplyCurveWarp(value, 1.0 / kCurveExponent, currentMinValue, currentMaxValue);
    else if(std::strcmp(actionName, "curve later") == 0)
      updatedValue = ApplyCurveWarp(value, kCurveExponent, currentMinValue, currentMaxValue);
    else
      return false;

    preset.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::breath_power, updatedValue);
  }

  return true;
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
    {"scale up", "scale down", "toward max", "away from max", "curve earlier", "curve later"},
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

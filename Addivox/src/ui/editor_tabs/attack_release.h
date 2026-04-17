#pragma once

#include "common.h"

namespace plugin_ui
{
namespace editor
{
inline std::shared_ptr<EditorLevelTransform> GetAttackReleaseTransform(const std::shared_ptr<EditorContext>& context,
                                                                       OscillatorParameter parameter)
{
  return (parameter == OscillatorParameter::attack)
    ? context->attackReleaseTab.attackTransform
    : context->attackReleaseTab.releaseTransform;
}

inline double GetAttackReleaseMaxValue(OscillatorParameter parameter)
{
  return parameter == OscillatorParameter::release ? 0.1 : 1.0;
}

inline bool TryGetAttackReleaseShapeValue(OscillatorParameter parameter,
                                          const char* shapeName,
                                          int oscillatorIndex,
                                          double& value)
{
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);

  if(parameter == OscillatorParameter::attack)
  {
    if(std::strcmp(shapeName, "linear ramp up") == 0 ||
       std::strcmp(shapeName, "linear ramp up (slow)") == 0)
    {
      value = harmonicNumber / 100.0;
      return true;
    }

    if(std::strcmp(shapeName, "linear ramp up (fast)") == 0)
    {
      value = harmonicNumber / 1000.0;
      return true;
    }

    if(std::strcmp(shapeName, "square root ramp up") == 0 ||
       std::strcmp(shapeName, "sqrt ramp up (slow)") == 0)
    {
      value = (0.11 * std::sqrt(harmonicNumber)) - 0.1;
      return true;
    }

    if(std::strcmp(shapeName, "sqrt ramp up (fast)") == 0)
    {
      value = ((0.11 * std::sqrt(harmonicNumber)) - 0.1) / 10.0;
      return true;
    }

    if(std::strcmp(shapeName, "logarithmic ramp up (slow)") == 0 ||
       std::strcmp(shapeName, "exponential ramp up (slow)") == 0)
    {
      value = 0.01 + (0.99 * std::log(harmonicNumber) / std::log(100.0));
      return true;
    }

    if(std::strcmp(shapeName, "logarithmic ramp up (fast)") == 0 ||
       std::strcmp(shapeName, "exponential ramp up (fast)") == 0)
    {
      value = 0.001 + (0.099 * std::log(harmonicNumber) / std::log(100.0));
      return true;
    }

    if(std::strcmp(shapeName, "flat") == 0)
    {
      value = 0.1;
      return true;
    }
  }
  else if(parameter == OscillatorParameter::release)
  {
    if(std::strcmp(shapeName, "linear ramp down") == 0 ||
       std::strcmp(shapeName, "linear ramp down (slow)") == 0)
    {
      value = 0.1 - (static_cast<double>(oscillatorIndex) / 1000.0);
      return true;
    }

    if(std::strcmp(shapeName, "linear ramp down (fast)") == 0)
    {
      value = 0.01 - (static_cast<double>(oscillatorIndex) / 10000.0);
      return true;
    }

    if(std::strcmp(shapeName, "square ramp down (slow)") == 0)
    {
      const double distanceFromEnd = harmonicNumber - 101.0;
      value = ((distanceFromEnd * distanceFromEnd) / 101000.0) + (1.0 / 1010.0);
      return true;
    }

    if(std::strcmp(shapeName, "square ramp down (fast)") == 0)
    {
      const double distanceFromEnd = harmonicNumber - 101.0;
      value = ((((distanceFromEnd * distanceFromEnd) / 101000.0) + (1.0 / 1010.0)) / 10.0);
      return true;
    }

    if(std::strcmp(shapeName, "exponential ramp down (slow)") == 0)
    {
      value = 0.1 * std::pow(10.0, (-2.0 * static_cast<double>(oscillatorIndex)) / 99.0);
      return true;
    }

    if(std::strcmp(shapeName, "exponential ramp down (fast)") == 0)
    {
      value = 0.01 * std::pow(10.0, (-2.0 * static_cast<double>(oscillatorIndex)) / 99.0);
      return true;
    }

    if(std::strcmp(shapeName, "flat") == 0)
    {
      value = 0.1;
      return true;
    }
  }

  return false;
}

inline bool ApplyAttackReleaseShape(SimplePreset& preset, OscillatorParameter parameter, const char* shapeName)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    double value = 0.0;
    if(!TryGetAttackReleaseShapeValue(parameter, shapeName, oscillatorIndex, value))
      return false;

    preset.SetOscillatorParameter(
      oscillatorIndex,
      parameter,
      std::clamp(value, 0.0, GetAttackReleaseMaxValue(parameter)));
  }

  return true;
}

inline bool ApplyAttackReleaseAction(SimplePreset& preset,
                                     OscillatorParameter parameter,
                                     const char* actionName,
                                     EditorOscillatorEditScope editScope)
{
  return ApplyStandardHarmonicAction(
    preset,
    parameter,
    actionName,
    0.0,
    GetAttackReleaseMaxValue(parameter),
    editScope);
}

inline void AppendAttackReleaseTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors)
{
  descriptors.push_back({
    kOscillatorTabTitles[2],
    "Attack time",
    OscillatorParameter::attack,
    {0.0, 1.0},
    help_text::oscillator_tabs::Get(OscillatorParameter::attack)
  });
  descriptors.push_back({
    kOscillatorTabTitles[3],
    "Release time",
    OscillatorParameter::release,
    {0.0, 0.1},
    help_text::oscillator_tabs::Get(OscillatorParameter::release)
  });
}

inline void AttachAttackReleaseTabChildren(IVTabPage* page,
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
  const auto attackReleaseIndex = GetAttackReleaseTabIndex(descriptor.parameter);
  auto* yTransformControl = CreateYTransformControl(
    GetAttackReleaseTransform(context, descriptor.parameter),
    sliderControl,
    styles);
  auto* setShapeControl = new ActionSelectionControl(
    IRECT(),
    "choose shape",
    descriptor.parameter == OscillatorParameter::attack
      ? std::initializer_list<const char*>{
          "linear ramp up (slow)",
          "linear ramp up (fast)",
          "sqrt ramp up (slow)",
          "sqrt ramp up (fast)",
          "logarithmic ramp up (slow)",
          "logarithmic ramp up (fast)",
          "flat"}
      : std::initializer_list<const char*>{
          "linear ramp down (slow)",
          "linear ramp down (fast)",
          "square ramp down (slow)",
          "square ramp down (fast)",
          "exponential ramp down (slow)",
          "exponential ramp down (fast)",
          "flat"},
    styles.utilityDropdownText,
    styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl, parameter = descriptor.parameter](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      parameter,
      [selectedText, parameter](SimplePreset& preset) {
        return ApplyAttackReleaseShape(preset, parameter, selectedText);
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
      kActionBendDownMenuLabel},
    styles.utilityDropdownText,
    styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl, parameter = descriptor.parameter](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      parameter,
      [selectedText, parameter, context](SimplePreset& preset) {
        return ApplyAttackReleaseAction(
          preset,
          parameter,
          selectedText,
          context->GetOscillatorEditScope(parameter));
      });
  });

  (*context->attackReleaseTab.setShapeControls)[attackReleaseIndex] = setShapeControl;
  (*context->attackReleaseTab.actionsControls)[attackReleaseIndex] = actionsControl;

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

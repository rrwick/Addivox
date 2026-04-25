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

inline bool ApplyPitchShape(SimplePatch& patch, const char* shapeName)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex)
  {
    double value = 0.0;
    if(!TryGetPitchShapeValue(shapeName, oscillatorIndex, value))
      return false;

    patch.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pitch, value);
  }

  return true;
}

inline bool ApplyPitchAction(SimplePatch& patch, const char* actionName, EditorOscillatorEditScope editScope)
{
  constexpr double kPitchMin = -100.0;
  constexpr double kPitchMax = 100.0;
  constexpr double kPitchShiftAmount = 1.0;

  if(MatchesActionLabel(actionName, kActionScaleUp) || MatchesActionLabel(actionName, kActionScaleDown))
    return ApplyStandardHarmonicAction(patch, OscillatorParameter::pitch, actionName, kPitchMin, kPitchMax, editScope);
  if(MatchesActionLabel(actionName, kActionShiftUp) || MatchesActionLabel(actionName, kActionShiftDown))
  {
    const double shiftAmount = MatchesActionLabel(actionName, kActionShiftUp) ? kPitchShiftAmount : -kPitchShiftAmount;
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex)
    {
      if(!MatchesOscillatorEditScope(editScope, oscillatorIndex))
        continue;

      const double pitch = patch.GetOscillatorSettings(oscillatorIndex).pitch;
      patch.SetOscillatorParameter(
        oscillatorIndex,
        OscillatorParameter::pitch,
        ApplyShiftOffset(pitch, shiftAmount, kPitchMin, kPitchMax));
    }
    return true;
  }
  if(MatchesActionLabel(actionName, kActionInvert))
  {
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex)
    {
      if(!MatchesOscillatorEditScope(editScope, oscillatorIndex))
        continue;

      const double pitch = patch.GetOscillatorSettings(oscillatorIndex).pitch;
      patch.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pitch, -pitch);
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

inline void AttachPitchTabChildren(IVTabPage* page,
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
      [selectedText](SimplePatch& patch) {
        return ApplyPitchShape(patch, selectedText);
      });
  });
  auto* actionsControl = new ActionSelectionControl(
    IRECT(),
    "run action",
    {
      kActionScaleUpMenuLabel,
      kActionScaleDownMenuLabel,
      kActionShiftUpMenuLabel,
      kActionShiftDownMenuLabel,
      kActionInvertMenuLabel},
    styles.utilityDropdownText,
    styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if(!selectedText)
      return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
      sliderControl,
      OscillatorParameter::pitch,
      [selectedText, context](SimplePatch& patch) {
        return ApplyPitchAction(
          patch,
          selectedText,
          context->GetOscillatorEditScope(OscillatorParameter::pitch));
      });
  });
  *context->pitchTab.setShapeControl = setShapeControl;
  *context->pitchTab.actionsControl = actionsControl;

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

#pragma once

#include "common.h"

namespace plugin_ui {
namespace editor {
inline double GetPanRampValue(int oscillatorIndex) { return static_cast<double>(oscillatorIndex) / static_cast<double>(SimplePatch::kNumOscillators - 1); }

inline bool TryGetPanShapeValue(const char* shapeName, int oscillatorIndex, double& value) {
  const double rampValue = GetPanRampValue(oscillatorIndex);

  if (std::strcmp(shapeName, "zero") == 0) {
    value = 0.0;
    return true;
  }

  if (std::strcmp(shapeName, "ramp right") == 0) {
    value = rampValue;
    return true;
  }

  if (std::strcmp(shapeName, "ramp left") == 0) {
    value = -rampValue;
    return true;
  }

  if (std::strcmp(shapeName, "ramp alternating") == 0) {
    value = ((oscillatorIndex % 2) == 1 ? 1.0 : -1.0) * rampValue;
    return true;
  }

  if (std::strcmp(shapeName, "full alternating") == 0) {
    value = (oscillatorIndex % 2) == 0 ? 1.0 : -1.0;
    return true;
  }

  return false;
}

inline bool ApplyPanShape(SimplePatch& patch, const char* shapeName) {
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    double value = 0.0;
    if (!TryGetPanShapeValue(shapeName, oscillatorIndex, value)) return false;

    patch.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pan, value);
  }

  return true;
}

inline bool ApplyPanAction(SimplePatch& patch, const char* actionName, EditorOscillatorEditScope editScope) {
  constexpr double kPanMin = -1.0;
  constexpr double kPanMax = 1.0;
  constexpr double kPanShiftAmount = 0.01;

  if (MatchesActionLabel(actionName, kActionScaleUp) || MatchesActionLabel(actionName, kActionScaleDown))
    return ApplyStandardHarmonicAction(patch, OscillatorParameter::pan, actionName, kPanMin, kPanMax, editScope);
  if (MatchesActionLabel(actionName, kActionShiftUp) || MatchesActionLabel(actionName, kActionShiftDown)) {
    const double shiftAmount = MatchesActionLabel(actionName, kActionShiftUp) ? kPanShiftAmount : -kPanShiftAmount;
    for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
      if (!MatchesOscillatorEditScope(editScope, oscillatorIndex)) continue;

      const double pan = patch.GetOscillatorSettings(oscillatorIndex).pan;
      patch.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pan, ApplyShiftOffset(pan, shiftAmount, kPanMin, kPanMax));
    }
    return true;
  }
  if (MatchesActionLabel(actionName, kActionInvert)) {
    for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
      if (!MatchesOscillatorEditScope(editScope, oscillatorIndex)) continue;

      const double pan = patch.GetOscillatorSettings(oscillatorIndex).pan;
      patch.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::pan, -pan);
    }
    return true;
  }

  return false;
}

inline void AppendPanTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors) {
  descriptors.push_back(
      {kOscillatorTabTitles[5], "Pan offset", OscillatorParameter::pan, {-1.0, 1.0}, help_text::oscillator_tabs::Get(OscillatorParameter::pan)});
}

inline void AttachPanTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                 const OscillatorTabDescriptor& descriptor, IVButtonControl* restoreButton, IVButtonControl* addButton,
                                 IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);
  auto* yTransformControl = CreateYTransformControl(context->panTab.panTransform, sliderControl, styles);
  auto* setShapeControl = new ActionSelectionControl(IRECT(), "choose shape", {"zero", "ramp right", "ramp left", "ramp alternating", "full alternating"},
                                                     styles.utilityDropdownText, styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if (!selectedText) return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(sliderControl, OscillatorParameter::pan,
                                                             [selectedText](SimplePatch& patch) { return ApplyPanShape(patch, selectedText); });
  });
  auto* actionsControl = new ActionSelectionControl(
      IRECT(), "run action", {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionShiftUpMenuLabel, kActionShiftDownMenuLabel, kActionInvertMenuLabel},
      styles.utilityDropdownText, styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if (!selectedText) return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(sliderControl, OscillatorParameter::pan, [selectedText, context](SimplePatch& patch) {
      return ApplyPanAction(patch, selectedText, context->GetOscillatorEditScope(OscillatorParameter::pan));
    });
  });
  *context->panTab.setShapeControl = setShapeControl;
  *context->panTab.actionsControl = actionsControl;

  AttachHarmonicTabChildren(page, context, styles, descriptor, xRangeControls, yTransformControl, setShapeControl, actionsControl, allKeyNotesControls,
                            restoreButton, addButton, deleteButton, sliderControl);
}
} // namespace editor
} // namespace plugin_ui

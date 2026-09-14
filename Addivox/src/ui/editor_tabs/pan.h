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
  return ApplyBipolarHarmonicAction(patch, OscillatorParameter::pan, actionName, 1.0, 0.01, editScope);
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
  auto* yTransformControl = CreateYTransformControl(context->GetTransformRef(descriptor.parameter), sliderControl, styles);

  auto* setShapeControl = CreateHarmonicShapeControl(context, descriptor.parameter, sliderControl, styles,
                                                     {"zero", "ramp right", "ramp left", "ramp alternating", "full alternating"}, ApplyPanShape);
  auto* actionsControl = CreateHarmonicActionsControl(
      context, descriptor.parameter, sliderControl, styles,
      {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionShiftUpMenuLabel, kActionShiftDownMenuLabel, kActionInvertMenuLabel}, ApplyPanAction);

  *context->panTab.setShapeControl = setShapeControl;
  *context->panTab.actionsControl = actionsControl;

  AttachHarmonicTabChildren(page, context, styles, descriptor, xRangeControls, yTransformControl, setShapeControl, actionsControl, allKeyNotesControls,
                            restoreButton, addButton, deleteButton, sliderControl);
}
} // namespace editor
} // namespace plugin_ui

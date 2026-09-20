#pragma once

#include "common.h"

namespace plugin_ui {
namespace editor {
inline bool TryGetPitchShapeValue(const char* shapeName, int oscillatorIndex, double& value) {
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);
  const double centsOffset = harmonicNumber - 1.0;

  if (std::strcmp(shapeName, "zero") == 0) {
    value = 0.0;
  } else if (std::strcmp(shapeName, "alternating") == 0) {
    value = (oscillatorIndex % 2) == 0 ? -10.0 : 10.0;
  } else if (std::strcmp(shapeName, "ramp sharp") == 0) {
    value = centsOffset;
  } else if (std::strcmp(shapeName, "ramp flat") == 0) {
    value = -centsOffset;
  } else if (std::strcmp(shapeName, "ramp alternating") == 0) {
    value = ((oscillatorIndex % 2) == 1 ? 1.0 : -1.0) * centsOffset;
  } else {
    return false;
  }

  return true;
}

inline bool ApplyPitchShape(SimplePatch& patch, const char* shapeName) {
  return ApplyHarmonicShape(patch, OscillatorParameter::pitch, shapeName, TryGetPitchShapeValue);
}

inline bool ApplyPitchAction(SimplePatch& patch, const char* actionName, EditorOscillatorEditScope editScope) {
  return ApplyBipolarHarmonicAction(patch, OscillatorParameter::pitch, actionName, 2400.0, 1.0, editScope);
}

inline void AppendPitchTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors) {
  descriptors.push_back(
      {kOscillatorTabTitles[4], OscillatorParameter::pitch, {-2400.0, 2400.0}, help_text::oscillator_tabs::Get(OscillatorParameter::pitch)});
}

inline void AttachPitchTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                   const OscillatorTabDescriptor& descriptor, IVButtonControl* restoreButton, IVButtonControl* addButton,
                                   IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  auto* setShapeControl = CreateHarmonicShapeControl(context, descriptor.parameter, sliderControl, styles,
                                                     {"zero", "alternating", "ramp sharp", "ramp flat", "ramp alternating"}, ApplyPitchShape);
  auto* actionsControl = CreateHarmonicActionsControl(
      context, descriptor.parameter, sliderControl, styles,
      {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionShiftUpMenuLabel, kActionShiftDownMenuLabel, kActionInvertMenuLabel}, ApplyPitchAction);

  AttachHarmonicTabChildren(page, context, styles, descriptor, setShapeControl, actionsControl, restoreButton, addButton, deleteButton,
                            sliderControl);
}
} // namespace editor
} // namespace plugin_ui

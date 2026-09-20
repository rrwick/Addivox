#pragma once

#include "common.h"

namespace plugin_ui {
namespace editor {
inline bool TryGetVariationShapeValue(const char* shapeName, int oscillatorIndex, double& value) {
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);

  if (std::strcmp(shapeName, "zero") == 0) {
    value = 0.0;
    return true;
  }

  if (std::strcmp(shapeName, "flat") == 0) {
    value = 1.0;
    return true;
  }

  if (std::strcmp(shapeName, "linear ramp up") == 0) {
    value = harmonicNumber / 10.0;
    return true;
  }

  return false;
}

inline bool ApplyVariationShape(SimplePatch& patch, OscillatorParameter parameter, const char* shapeName) {
  return ApplyHarmonicShape(patch, parameter, shapeName, [](const char* shape, int index, double& value) {
    if (!TryGetVariationShapeValue(shape, index, value)) return false;
    value = std::clamp(value, 0.0, 10.0);
    return true;
  });
}

inline bool ApplyVariationAction(SimplePatch& patch, OscillatorParameter parameter, const char* actionName, EditorOscillatorEditScope editScope) {
  return ApplyStandardHarmonicAction(patch, parameter, actionName, 0.0, 10.0, editScope);
}

inline void AppendVariationTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors) {
  static constexpr std::array<OscillatorParameter, 6> kParameters{{
      OscillatorParameter::level_variation_amplitude,
      OscillatorParameter::level_variation_rate,
      OscillatorParameter::pitch_variation_amplitude,
      OscillatorParameter::pitch_variation_rate,
      OscillatorParameter::pan_variation_amplitude,
      OscillatorParameter::pan_variation_rate,
  }};
  for (std::size_t i = 0; i < kParameters.size(); ++i)
    descriptors.push_back({kOscillatorTabTitles[6 + i], kParameters[i], {0.0, 10.0}, help_text::oscillator_tabs::Get(kParameters[i])});
}

inline void AttachVariationTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                       const OscillatorTabDescriptor& descriptor, IVButtonControl* restoreButton, IVButtonControl* addButton,
                                       IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  auto* setShapeControl = CreateHarmonicShapeControl(
      context, descriptor.parameter, sliderControl, styles, {"zero", "flat", "linear ramp up"},
      [parameter = descriptor.parameter](SimplePatch& patch, const char* shape) { return ApplyVariationShape(patch, parameter, shape); });
  auto* actionsControl =
      CreateHarmonicActionsControl(context, descriptor.parameter, sliderControl, styles,
                                   {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionTowardMaxMenuLabel, kActionAwayFromMaxMenuLabel,
                                    kActionBendUpMenuLabel, kActionBendDownMenuLabel},
                                   [parameter = descriptor.parameter](SimplePatch& patch, const char* action, EditorOscillatorEditScope scope) {
                                     return ApplyVariationAction(patch, parameter, action, scope);
                                   });

  AttachHarmonicTabChildren(page, context, styles, descriptor, setShapeControl, actionsControl, restoreButton, addButton, deleteButton,
                            sliderControl);
}
} // namespace editor
} // namespace plugin_ui

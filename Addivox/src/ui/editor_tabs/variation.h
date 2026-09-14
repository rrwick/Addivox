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
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    double value = 0.0;
    if (!TryGetVariationShapeValue(shapeName, oscillatorIndex, value)) return false;

    patch.SetOscillatorParameter(oscillatorIndex, parameter, std::clamp(value, 0.0, 10.0));
  }

  return true;
}

inline bool ApplyVariationAction(SimplePatch& patch, OscillatorParameter parameter, const char* actionName, EditorOscillatorEditScope editScope) {
  return ApplyStandardHarmonicAction(patch, parameter, actionName, 0.0, 10.0, editScope);
}

inline void AppendVariationTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors) {
  static constexpr std::array<std::pair<const char*, OscillatorParameter>, 6> kEntries{{
      {"Level variation amount", OscillatorParameter::level_variation_amplitude},
      {"Level variation rate",   OscillatorParameter::level_variation_rate},
      {"Pitch variation amount", OscillatorParameter::pitch_variation_amplitude},
      {"Pitch variation rate",   OscillatorParameter::pitch_variation_rate},
      {"Pan variation amount",   OscillatorParameter::pan_variation_amplitude},
      {"Pan variation rate",     OscillatorParameter::pan_variation_rate},
  }};
  for (std::size_t i = 0; i < kEntries.size(); ++i)
    descriptors.push_back({kOscillatorTabTitles[6 + i], kEntries[i].first, kEntries[i].second, {0.0, 10.0},
                           help_text::oscillator_tabs::Get(kEntries[i].second)});
}

inline void AttachVariationTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                       const OscillatorTabDescriptor& descriptor, IVButtonControl* restoreButton, IVButtonControl* addButton,
                                       IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);
  const auto variationIndex = GetVariationTabIndex(descriptor.parameter);
  auto* yTransformControl = CreateYTransformControl(context->GetTransformRef(descriptor.parameter), sliderControl, styles);

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

  (*context->variationTab.setShapeControls)[variationIndex] = setShapeControl;
  (*context->variationTab.actionsControls)[variationIndex] = actionsControl;

  AttachHarmonicTabChildren(page, context, styles, descriptor, xRangeControls, yTransformControl, setShapeControl, actionsControl, allKeyNotesControls,
                            restoreButton, addButton, deleteButton, sliderControl);
}
} // namespace editor
} // namespace plugin_ui

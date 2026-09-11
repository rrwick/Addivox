#pragma once

#include "common.h"
#include "macro_fit.h"

#include <limits>

namespace plugin_ui {
namespace editor {
inline bool TryGetBreathShapeValue(const char* shapeName, int oscillatorIndex, double& value) {
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);

  if (std::strcmp(shapeName, "flat") == 0) {
    value = 1.0;
    return true;
  }

  if (std::strcmp(shapeName, "linear ramp") == 0) {
    value = harmonicNumber;
    return true;
  }

  if (std::strcmp(shapeName, "square ramp") == 0) {
    value = 1.0 + ((harmonicNumber * harmonicNumber) / 101.01010101010101);
    return true;
  }

  if (std::strcmp(shapeName, "cube ramp") == 0) {
    value = 1.0 + ((harmonicNumber * harmonicNumber * harmonicNumber) / 10101.010101010101);
    return true;
  }

  return false;
}

inline bool ApplyBreathShape(SimplePatch& patch, const char* shapeName) {
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    double value = 0.0;
    if (!TryGetBreathShapeValue(shapeName, oscillatorIndex, value)) return false;

    patch.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::breath_power, value);
  }

  return true;
}

inline bool ApplyBreathAction(SimplePatch& patch, const char* actionName, EditorOscillatorEditScope editScope) {
  return ApplyStandardHarmonicAction(patch, OscillatorParameter::breath_power, actionName, 0.0, 100.0, editScope);
}

// Breath macros interpolate actual breath powers from harmonic 1 to harmonic 100.
inline constexpr double kBreathBaseMin = 0.5;
inline constexpr double kBreathBaseMax = 3.0;
inline constexpr double kBreathPowerMax = 100.0;
inline constexpr double kBreathOddEvenAdditionMax = 10.0;
inline constexpr double kBreathOddEvenTravelExponent = 2.32;
inline constexpr double kBreathFitRelativeFloor = 0.01;

// Smooth offset exponentials put the intended values at minimum, half and maximum travel.
// Base: 0.5 -> 1.5 -> 3. Shape exponent: 4 -> 1.5 -> 0.75.
inline double GetBreathMacroBase(double travel) {
  return 2.0 * std::pow(2.25, std::clamp(travel, 0.0, 1.0)) - 1.5;
}

inline double GetBreathMacroBaseTravel(double base) {
  return std::log((std::clamp(base, kBreathBaseMin, kBreathBaseMax) + 1.5) / 2.0) / std::log(2.25);
}

inline double GetBreathMacroShapeExponent(double travel) {
  return (3.0 + 25.0 * std::pow(0.09, std::clamp(travel, 0.0, 1.0))) / 7.0;
}

inline double GetBreathOddEvenAddition(double travel) {
  const double distance = 2.0 * std::clamp(travel, 0.0, 1.0) - 1.0;
  return std::copysign(kBreathOddEvenAdditionMax * std::pow(std::abs(distance), kBreathOddEvenTravelExponent), distance);
}

inline double GetBreathOddEvenTravel(double addition) {
  const double distance = std::pow(std::clamp(std::abs(addition) / kBreathOddEvenAdditionMax, 0.0, 1.0), 1.0 / kBreathOddEvenTravelExponent);
  return 0.5 + 0.5 * std::copysign(distance, addition);
}

// Normalised positions in descriptor/storage order. Defaults are also the knobs' double-click targets.
struct BreathMacroKnobs {
  double base{0.5};
  double top{1.0};
  double shape{0.5};
  double oddEven{0.5};
};

inline OscillatorParameterValues MakeBreathMacroBasis(double shape) {
  const double exponent = GetBreathMacroShapeExponent(shape);
  OscillatorParameterValues basis{};
  for (std::size_t i = 0; i < basis.size(); ++i) basis[i] = std::pow(i / static_cast<double>(basis.size() - 1), exponent);
  return basis;
}

inline OscillatorParameterValues GenerateBreathMacroCurve(const BreathMacroKnobs& knobs) {
  auto values = MakeBreathMacroBasis(knobs.shape);
  const double base = GetBreathMacroBase(knobs.base);
  const double spread = std::clamp(knobs.top, 0.0, 1.0) * (kBreathPowerMax - base);
  const double addition = GetBreathOddEvenAddition(knobs.oddEven);
  for (std::size_t i = 0; i < values.size(); ++i) {
    const bool oddHarmonic = (i % 2 == 0);
    const double offset = (oddHarmonic ? addition < 0.0 : addition > 0.0) ? std::abs(addition) : 0.0;
    values[i] = std::min(kBreathPowerMax, base + spread * values[i] + offset);
  }
  return values;
}

inline BreathMacroKnobs FitBreathMacroRamp(const OscillatorParameterValues& values, int parity) {
  if (values.front() <= kBreathBaseMax && std::all_of(values.begin(), values.end(), [&](double value) { return value == values.front(); })) {
    return {GetBreathMacroBaseTravel(values.front()), 0.0, 0.5};
  }
  auto target = MakeMacroFitTarget(values, kBreathFitRelativeFloor);
  for (std::size_t i = 0; i < values.size(); ++i)
    if (static_cast<int>(i % 2) != parity) target.weights[i] = 0.0;
  BreathMacroKnobs best;
  double bestResidual = std::numeric_limits<double>::max();
  MacroFitPoint<1> bestPoint{};

  // Search only Shape. For each candidate, solve Base and Top by weighted least squares,
  // constrained to 0.5 <= Base <= 3 and Base <= Top <= 100.
  const auto score = [&](const MacroFitPoint<1>& point) {
    const double shape = std::clamp(point[0], 0.0, 1.0);
    const auto basis = MakeBreathMacroBasis(shape);
    double aa = 0.0, ab = 0.0, bb = 0.0, ay = 0.0, by = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
      const double a = 1.0 - basis[i], b = basis[i], weight = target.weights[i];
      aa += weight * a * a;
      ab += weight * a * b;
      bb += weight * b * b;
      ay += weight * a * values[i];
      by += weight * b * values[i];
    }

    double residual = std::numeric_limits<double>::max();
    const auto evaluate = [&](double base, double top) {
      if (base < kBreathBaseMin || base > kBreathBaseMax || top < base || top > kBreathPowerMax) return;
      double error = 0.0;
      for (std::size_t i = 0; i < values.size(); ++i) {
        const double difference = base + (top - base) * basis[i] - values[i];
        error += target.weights[i] * difference * difference;
      }
      residual = std::min(residual, error);
      if (error >= bestResidual) return;
      bestResidual = error;
      bestPoint = {shape};
      best = {GetBreathMacroBaseTravel(base), (top - base) / (kBreathPowerMax - base), shape};
    };

    const double determinant = aa * bb - ab * ab;
    if (determinant > kMacroEpsilon * aa * bb) evaluate((ay * bb - by * ab) / determinant, (by * aa - ay * ab) / determinant);
    // The constrained optimum is either inside the region or on one of its four edges.
    for (const double base : {kBreathBaseMin, kBreathBaseMax})
      evaluate(base, std::clamp((by - base * ab) / bb, base, kBreathPowerMax));
    evaluate(std::clamp((ay - kBreathPowerMax * ab) / aa, kBreathBaseMin, kBreathBaseMax), kBreathPowerMax);
    const double flat = std::clamp((ay + by) / (aa + 2.0 * ab + bb), kBreathBaseMin, kBreathBaseMax);
    evaluate(flat, flat);
    return residual;
  };

  // Refine each sampled local minimum, including the endpoints, then polish the best result.
  constexpr int kShapeSteps = 32;
  std::array<double, kShapeSteps + 1> residuals{};
  for (int i = 0; i <= kShapeSteps; ++i) residuals[i] = score({i / static_cast<double>(kShapeSteps)});
  for (int i = 0; i <= kShapeSteps; ++i) {
    if (i > 0 && residuals[i] > residuals[i - 1]) continue;
    if (i < kShapeSteps && residuals[i] > residuals[i + 1]) continue;
    SearchMacroFitSimplex(MacroFitPoint<1>{i / static_cast<double>(kShapeSteps)}, MacroFitPoint<1>{1.0 / kShapeSteps}, score, 1.0e-8);
  }
  SearchMacroFitSimplex(bestPoint, MacroFitPoint<1>{0.01}, score, 1.0e-8);
  return best;
}

inline BreathMacroKnobs FitBreathMacroKnobs(const OscillatorParameterValues& values) {
  const auto target = MakeMacroFitTarget(values, kBreathFitRelativeFloor);
  BreathMacroKnobs best;
  double bestResidual = std::numeric_limits<double>::max();

  // The unaffected parity still describes the original ramp, even when the other parity clips at 100.
  // Try both directions, then refine against the complete, capped curve. Search addition linearly so
  // tuning the knob's centre resolution cannot change the fitter's accuracy.
  for (int parity = 0; parity < 2; ++parity) {
    const double direction = (parity == 0) ? 1.0 : -1.0;
    const auto ramp = FitBreathMacroRamp(values, parity);
    const auto baseline = GenerateBreathMacroCurve(ramp);
    double weightedOffset = 0.0, weightSum = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (static_cast<int>(i % 2) == parity || values[i] >= kBreathPowerMax) continue;
      weightedOffset += target.weights[i] * (values[i] - baseline[i]);
      weightSum += target.weights[i];
    }
    const double offset = (weightSum > 0.0) ? std::clamp(weightedOffset / weightSum, 0.0, kBreathOddEvenAdditionMax) : kBreathOddEvenAdditionMax;
    MacroFitPoint<4> bestPoint{ramp.base, ramp.top, ramp.shape, offset / kBreathOddEvenAdditionMax};
    double directionResidual = std::numeric_limits<double>::max();
    const auto score = [&](MacroFitPoint<4> point) {
      for (double& value : point) value = std::clamp(value, 0.0, 1.0);
      const BreathMacroKnobs knobs{point[0], point[1], point[2], GetBreathOddEvenTravel(direction * point[3] * kBreathOddEvenAdditionMax)};
      const auto curve = GenerateBreathMacroCurve(knobs);
      double error = 0.0;
      for (std::size_t i = 0; i < values.size(); ++i) {
        const double difference = curve[i] - values[i];
        error += target.weights[i] * difference * difference;
      }
      if (error < directionResidual) {
        directionResidual = error;
        bestPoint = point;
      }
      if (error < bestResidual) {
        bestResidual = error;
        best = knobs;
      }
      return error;
    };
    score(bestPoint);
    if (bestResidual < 1.0e-20) return best;
    for (int restart = 0; restart < 3; ++restart)
      SearchMacroFitSimplex(bestPoint, MacroFitPoint<4>{0.05, 0.05, 0.05, 0.05}, score, 1.0e-8);
  }
  return best;
}

inline std::vector<MacroKnobDescriptor> GetBreathMacroKnobDescriptors() {
  const BreathMacroKnobs defaults;
  return {{"Base", help_text::oscillator_tabs::kMacroBreathBase, defaults.base, false},
          {"Top", help_text::oscillator_tabs::kMacroBreathTop, defaults.top, false},
          {"Shape", help_text::oscillator_tabs::kMacroBreathShape, defaults.shape, false},
          {"Odd/Even", help_text::oscillator_tabs::kMacroBreathOddEven, defaults.oddEven, true}};
}

inline void RegisterBreathMacroFunctions(const std::shared_ptr<EditorContext>& context, const std::vector<layout::LabelledKnob*>& knobs) {
  if (knobs.size() != 4) return;
  auto& functions = (*context->oscillatorTabControls.macroFunctions)[static_cast<std::size_t>(OscillatorParameter::breath_power)];
  functions.version = 6; // Adds Odd/Even breath-power offsets, capped at 100.
  functions.knobs = knobs;
  functions.generateValues = [knobs]() {
    return GenerateBreathMacroCurve({knobs[0]->GetNormalizedValue(), knobs[1]->GetNormalizedValue(), knobs[2]->GetNormalizedValue(),
                                     knobs[3]->GetNormalizedValue()});
  };
  functions.fitKnobsToValues = [knobs](const OscillatorParameterValues& values) {
    const auto fitted = FitBreathMacroKnobs(values);
    knobs[0]->SetNormalizedValueSilently(fitted.base);
    knobs[1]->SetNormalizedValueSilently(fitted.top);
    knobs[2]->SetNormalizedValueSilently(fitted.shape);
    knobs[3]->SetNormalizedValueSilently(fitted.oddEven);
  };
}

inline void AppendBreathTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors) {
  descriptors.push_back({kOscillatorTabTitles[1],
                         "Breath power",
                         OscillatorParameter::breath_power,
                         {0.0, 100.0},
                         help_text::oscillator_tabs::Get(OscillatorParameter::breath_power)});
}

inline void AttachBreathTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                    const OscillatorTabDescriptor& descriptor, IVButtonControl* restoreButton, IVButtonControl* addButton,
                                    IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);

  auto* yTransformControl = CreateYTransformControl(context->GetTransformRef(descriptor.parameter), sliderControl, styles);

  auto* setShapeControl =
      new ActionSelectionControl(IRECT(), "choose shape", {"flat", "linear ramp", "square ramp", "cube ramp"}, styles.utilityDropdownText, styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if (!selectedText) return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(sliderControl, OscillatorParameter::breath_power,
                                                             [selectedText](SimplePatch& patch) { return ApplyBreathShape(patch, selectedText); });
  });

  auto* actionsControl = new ActionSelectionControl(IRECT(), "run action",
                                                    {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionTowardMaxMenuLabel, kActionAwayFromMaxMenuLabel,
                                                     kActionBendUpMenuLabel, kActionBendDownMenuLabel},
                                                    styles.utilityDropdownText, styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if (!selectedText) return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(sliderControl, OscillatorParameter::breath_power, [selectedText, context](SimplePatch& patch) {
      return ApplyBreathAction(patch, selectedText, context->GetOscillatorEditScope(OscillatorParameter::breath_power));
    });
  });

  *context->breathTab.setShapeControl = setShapeControl;
  *context->breathTab.actionsControl = actionsControl;

  AttachHarmonicTabChildren(page, context, styles, descriptor, xRangeControls, yTransformControl, setShapeControl, actionsControl, allKeyNotesControls,
                            restoreButton, addButton, deleteButton, sliderControl);

  RegisterBreathMacroFunctions(context, AttachMacroKnobChildren(page, context, descriptor, GetBreathMacroKnobDescriptors()));
}
} // namespace editor
} // namespace plugin_ui

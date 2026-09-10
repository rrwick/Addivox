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

// Breath macros interpolate actual breath powers, using the same distance scale on both sides of Position.
// The farther endpoint reaches Outer; the nearer endpoint follows the same slope without being stretched.
inline constexpr double kBreathPowerMax = 100.0;
inline constexpr double kBreathPowerTravelExponent = 12.0;

// The breath power at half travel, which is where Base's double-tap lands. The linear coefficient is solved
// from it rather than chosen: with power(t) = linear * t + (max - linear) * t^exponent, requiring
// power(0.5) = centre has exactly one solution, so moving the centre carries the whole lower travel with it
// and half travel keeps meaning exactly this power. The exponent still decides how abruptly the top of the
// travel runs away to max.
inline constexpr double kBreathPowerTravelCentre = 1.5;
inline const double kBreathPowerTravelLinear = [] {
  const double half = std::pow(0.5, kBreathPowerTravelExponent);
  return (kBreathPowerTravelCentre - (kBreathPowerMax * half)) / (0.5 - half);
}();
// Shape runs geometrically from 16 through 2 at half travel to 0.25.
inline constexpr double kBreathShapeExponentMax = 16.0;
inline constexpr double kBreathShapeExponentCentre = 2.0;
inline constexpr double kBreathPositionMax = static_cast<double>(SimplePatch::kNumOscillators);
inline constexpr double kBreathFitRelativeFloor = 0.01;

inline double GetBreathMacroPower(double travel) {
  const double t = std::clamp(travel, 0.0, 1.0);
  return kBreathPowerTravelLinear * t + (kBreathPowerMax - kBreathPowerTravelLinear) * std::pow(t, kBreathPowerTravelExponent);
}

inline double GetBreathMacroPowerTravel(double power) {
  if (power <= 0.0) return 0.0;
  if (power >= kBreathPowerMax) return 1.0;

  double low = 0.0, high = 1.0;
  for (int iteration = 0; iteration < 48; ++iteration) {
    const double middle = (low + high) * 0.5;
    if (GetBreathMacroPower(middle) < power) low = middle;
    else
      high = middle;
  }
  return (low + high) * 0.5;
}

// Normalised positions in descriptor/storage order. Defaults are also the knobs' double-click targets.
struct BreathMacroKnobs {
  double base{0.5}; // Half travel, which the mapping above pins to kBreathPowerTravelCentre.
  double outer{1.0};
  double shape{0.5};
  double position{0.0};
};

inline OscillatorParameterValues MakeBreathMacroBasis(double shape, double position) {
  const double exponent = kBreathShapeExponentCentre * std::pow(kBreathShapeExponentMax / kBreathShapeExponentCentre, 1.0 - 2.0 * std::clamp(shape, 0.0, 1.0));
  const double distance = std::max(position - 1.0, kBreathPositionMax - position);
  OscillatorParameterValues basis{};
  for (std::size_t i = 0; i < basis.size(); ++i) basis[i] = std::pow(std::fabs(i + 1.0 - position) / distance, exponent);
  return basis;
}

inline OscillatorParameterValues GenerateBreathMacroCurve(const BreathMacroKnobs& knobs) {
  auto values = MakeBreathMacroBasis(knobs.shape, std::pow(kBreathPositionMax, std::clamp(knobs.position, 0.0, 1.0)));
  const double base = GetBreathMacroPower(knobs.base), outer = GetBreathMacroPower(knobs.outer);
  for (double& value : values) value = std::clamp(base + (outer - base) * value, 0.0, kBreathPowerMax);
  return values;
}

inline BreathMacroKnobs FitBreathMacroKnobs(const OscillatorParameterValues& values) {
  if (std::all_of(values.begin(), values.end(), [&](double value) { return value == values.front(); })) {
    const double travel = GetBreathMacroPowerTravel(values.front());
    return {travel, travel};
  }
  const auto target = MakeMacroFitTarget(values, kBreathFitRelativeFloor);
  BreathMacroKnobs best;
  double bestResidual = std::numeric_limits<double>::max();
  MacroFitPoint<2> bestPoint{};

  // Search Shape and physical Position, independently of their knob mappings. Base and Outer are a bounded
  // linear least-squares solve for each candidate. Relative weights keep the low breath powers significant.
  const auto score = [&](const MacroFitPoint<2>& point) {
    const double shape = std::clamp(point[0], 0.0, 1.0), position = std::clamp(point[1], 0.0, 1.0);
    const auto basis = MakeBreathMacroBasis(shape, 1.0 + (kBreathPositionMax - 1.0) * position);
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
    const auto evaluate = [&](double base, double outer) {
      if (base < 0.0 || base > kBreathPowerMax || outer < 0.0 || outer > kBreathPowerMax) return;
      double error = 0.0;
      for (std::size_t i = 0; i < values.size(); ++i) {
        const double difference = base + (outer - base) * basis[i] - values[i];
        error += target.weights[i] * difference * difference;
      }
      residual = std::min(residual, error);
      if (error >= bestResidual) return;
      bestResidual = error;
      bestPoint = {shape, position};
      best = {GetBreathMacroPowerTravel(base), GetBreathMacroPowerTravel(outer), shape,
              std::log1p((kBreathPositionMax - 1.0) * position) / std::log(kBreathPositionMax)};
    };

    const double determinant = aa * bb - ab * ab;
    if (determinant > kMacroEpsilon * aa * bb) evaluate((ay * bb - by * ab) / determinant, (by * aa - ay * ab) / determinant);
    // A constrained optimum is either the unconstrained solution or lies on one of the four box edges.
    for (const double edge : {0.0, kBreathPowerMax}) {
      evaluate(edge, std::clamp((by - edge * ab) / bb, 0.0, kBreathPowerMax));
      evaluate(std::clamp((ay - edge * ab) / aa, 0.0, kBreathPowerMax), edge);
    }
    return residual;
  };

  // Keep a seed at each integer anchor, then refine the best few. A valley near one endpoint can otherwise
  // lose to a reversed hill at the other, and fractional anchors have sharp minima when the shape exponent is low.
  std::array<std::pair<double, MacroFitPoint<2>>, SimplePatch::kNumOscillators> seeds;
  for (int harmonic = 0; harmonic < SimplePatch::kNumOscillators; ++harmonic) {
    auto& seed = seeds[harmonic];
    seed.first = std::numeric_limits<double>::max();
    for (int shape = 0; shape <= 8; ++shape) {
      const MacroFitPoint<2> point{shape / 8.0, harmonic / (kBreathPositionMax - 1.0)};
      const double residual = score(point);
      if (residual < seed.first) seed = {residual, point};
    }
  }
  std::sort(seeds.begin(), seeds.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  for (int seed = 0; seed < 8; ++seed) SearchMacroFitSimplex(seeds[seed].second, MacroFitPoint<2>{0.1, 0.01}, score, 1.0e-8);
  // Seed near each sampled extremum at several distances, including very close to h1/h100. Match the
  // simplex step to that distance so it can resolve a sharp fractional cusp without jumping past it.
  const auto extrema = std::minmax_element(values.begin(), values.end());
  for (const auto extremum : {extrema.first, extrema.second}) {
    for (const double offset : {-0.5, -0.1, -0.01, 0.01, 0.1, 0.5}) {
      const double position = std::clamp((std::distance(values.begin(), extremum) + offset) / (kBreathPositionMax - 1.0), 0.0, 1.0);
      for (const double shape : {2.0 / 3.0, 1.0}) // Linear and sharp starting curves.
        SearchMacroFitSimplex(MacroFitPoint<2>{shape, position}, MacroFitPoint<2>{0.1, 0.5 * std::fabs(offset) / (kBreathPositionMax - 1.0)}, score, 1.0e-8);
    }
  }
  SearchMacroFitSimplex(bestPoint, MacroFitPoint<2>{0.02, 0.002}, score, 1.0e-8);
  return best;
}

inline std::vector<MacroKnobDescriptor> GetBreathMacroKnobDescriptors() {
  const BreathMacroKnobs defaults;
  return {{"Base", help_text::oscillator_tabs::kMacroBreathBase, defaults.base, false},
          {"Outer", help_text::oscillator_tabs::kMacroBreathOuter, defaults.outer, false},
          {"Shape", help_text::oscillator_tabs::kMacroBreathShape, defaults.shape, false},
          {"Position", help_text::oscillator_tabs::kMacroBreathPosition, defaults.position, false}};
}

inline void RegisterBreathMacroFunctions(const std::shared_ptr<EditorContext>& context, const std::vector<layout::LabelledKnob*>& knobs) {
  if (knobs.size() != 4) return;
  auto& functions = (*context->oscillatorTabControls.macroFunctions)[static_cast<std::size_t>(OscillatorParameter::breath_power)];
  functions.version = 3; // 3: Shape runs from 16 through 2 to 0.25; 2 introduced the Base/Outer midpoint of 1.5.
  functions.knobs = knobs;
  functions.generateValues = [knobs]() {
    return GenerateBreathMacroCurve(
        {knobs[0]->GetNormalizedValue(), knobs[1]->GetNormalizedValue(), knobs[2]->GetNormalizedValue(), knobs[3]->GetNormalizedValue()});
  };
  functions.fitKnobsToValues = [knobs](const OscillatorParameterValues& values) {
    const auto fitted = FitBreathMacroKnobs(values);
    knobs[0]->SetNormalizedValueSilently(fitted.base);
    knobs[1]->SetNormalizedValueSilently(fitted.outer);
    knobs[2]->SetNormalizedValueSilently(fitted.shape);
    knobs[3]->SetNormalizedValueSilently(fitted.position);
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

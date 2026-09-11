#pragma once

#include "common.h"
#include "macro_fit.h"

#include <limits>

namespace plugin_ui {
namespace editor {
// Attack macros interpolate seconds, independently of the chart and global Attack multiplier.
// Changing these mappings or Shape also requires incrementing the registered macro version below.
inline constexpr double kAttackShapeExponent = 2.0;
inline constexpr double kAttackTimeTravelExponent = 3.321928094887362;    // Half travel = 0.1 seconds.
inline constexpr double kAttackOddEvenTravelExponent = 4.321928094887362; // Half strength = 0.05 seconds.
inline constexpr double kAttackFitRelativeFloor = 0.01;

inline double GetAttackMacroTime(double travel) { return BendMacroTravel(travel, kAttackTimeTravelExponent); }
inline double GetAttackMacroTimeTravel(double seconds) { return BendMacroTravel(seconds, 1.0 / kAttackTimeTravelExponent); }
inline double GetAttackMacroPosition(double travel) { return std::pow(100.0, std::clamp(travel, 0.0, 1.0)); }
inline double GetAttackMacroPositionTravel(double harmonic) { return std::log(std::clamp(harmonic, 1.0, 100.0)) / std::log(100.0); }

inline double GetAttackOddEvenAddition(double travel) {
  const double distance = 2.0 * std::clamp(travel, 0.0, 1.0) - 1.0;
  return std::copysign(std::pow(std::abs(distance), kAttackOddEvenTravelExponent), distance);
}

inline double GetAttackOddEvenTravel(double addition) {
  return 0.5 + 0.5 * std::copysign(BendMacroTravel(std::abs(addition), 1.0 / kAttackOddEvenTravelExponent), addition);
}

// Normalised positions in descriptor/storage order; also the double-click targets.
struct AttackMacroKnobs {
  double base{0.0};
  double outer{0.0};
  double position{0.0};
  double oddEven{0.5};
};

inline OscillatorParameterValues MakeAttackMacroBasis(double position) {
  const double distance = std::max(position - 1.0, 100.0 - position);
  OscillatorParameterValues basis{};
  for (std::size_t i = 0; i < basis.size(); ++i) basis[i] = std::pow(std::abs(i + 1.0 - position) / distance, kAttackShapeExponent);
  return basis;
}

inline OscillatorParameterValues GenerateAttackMacroCurve(const AttackMacroKnobs& knobs) {
  auto values = MakeAttackMacroBasis(GetAttackMacroPosition(knobs.position));
  const double base = GetAttackMacroTime(knobs.base), outer = GetAttackMacroTime(knobs.outer);
  const double addition = GetAttackOddEvenAddition(knobs.oddEven);
  for (std::size_t i = 0; i < values.size(); ++i) {
    const bool affected = (i % 2 == 0) ? addition < 0.0 : addition > 0.0;
    values[i] = std::clamp(base + (outer - base) * values[i] + (affected ? std::abs(addition) : 0.0), 0.0, 1.0);
  }
  return values;
}

inline AttackMacroKnobs FitAttackMacroDistanceCurve(const OscillatorParameterValues& values, int parity) {
  auto target = MakeMacroFitTarget(values, kAttackFitRelativeFloor);
  for (std::size_t i = 0; i < values.size(); ++i)
    if (static_cast<int>(i % 2) != parity) target.weights[i] = 0.0;
  AttackMacroKnobs best;
  double bestResidual = std::numeric_limits<double>::max();
  MacroFitPoint<1> bestPoint{};

  // Search physical Position; solve independent bounded Base/Outer values at each candidate.
  const auto score = [&](const MacroFitPoint<1>& point) {
    const double position = std::clamp(point[0], 0.0, 1.0);
    const auto basis = MakeAttackMacroBasis(1.0 + 99.0 * position);
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
      if (base < 0.0 || base > 1.0 || outer < 0.0 || outer > 1.0) return;
      double error = 0.0;
      for (std::size_t i = 0; i < values.size(); ++i) {
        const double difference = base + (outer - base) * basis[i] - values[i];
        error += target.weights[i] * difference * difference;
      }
      residual = std::min(residual, error);
      if (error >= bestResidual) return;
      bestResidual = error;
      bestPoint = {position};
      best = {GetAttackMacroTimeTravel(base), GetAttackMacroTimeTravel(outer), GetAttackMacroPositionTravel(1.0 + 99.0 * position)};
    };

    const double determinant = aa * bb - ab * ab;
    if (determinant > kMacroEpsilon * aa * bb) evaluate((ay * bb - by * ab) / determinant, (by * aa - ay * ab) / determinant);
    // The constrained optimum is either inside the region or on one of its four edges.
    for (const double base : {0.0, 1.0}) evaluate(base, std::clamp((by - base * ab) / bb, 0.0, 1.0));
    for (const double outer : {0.0, 1.0}) evaluate(std::clamp((ay - outer * ab) / aa, 0.0, 1.0), outer);
    const double flat = std::clamp((ay + by) / (aa + 2.0 * ab + bb), 0.0, 1.0);
    evaluate(flat, flat);
    return residual;
  };

  // Refine each sampled local minimum, including the endpoints, then polish the best result.
  constexpr int kPositionSteps = 99;
  std::array<double, kPositionSteps + 1> residuals{};
  for (int i = 0; i <= kPositionSteps; ++i) residuals[i] = score({i / static_cast<double>(kPositionSteps)});
  for (int i = 0; i <= kPositionSteps; ++i) {
    if (i > 0 && residuals[i] > residuals[i - 1]) continue;
    if (i < kPositionSteps && residuals[i] > residuals[i + 1]) continue;
    SearchMacroFitSimplex(MacroFitPoint<1>{i / static_cast<double>(kPositionSteps)}, MacroFitPoint<1>{1.0 / kPositionSteps}, score, 1.0e-8);
  }
  SearchMacroFitSimplex(bestPoint, MacroFitPoint<1>{0.01}, score, 1.0e-8);
  return best;
}

inline AttackMacroKnobs FitAttackMacroKnobs(const OscillatorParameterValues& values) {
  if (std::all_of(values.begin(), values.end(), [&](double value) { return value == values.front(); })) {
    const double travel = GetAttackMacroTimeTravel(values.front());
    return {travel, travel, 0.0, 0.5};
  }
  const auto target = MakeMacroFitTarget(values, kAttackFitRelativeFloor);
  AttackMacroKnobs best;
  double bestResidual = std::numeric_limits<double>::max();

  // The unaffected parity still describes the original ramp, even when the other parity clips at 1.
  // Try both directions, then refine against the complete, capped curve. Search addition linearly so
  // tuning the knob's centre resolution cannot change the fitter's accuracy.
  for (int parity = 0; parity < 2; ++parity) {
    const double direction = (parity == 0) ? 1.0 : -1.0;
    const auto ramp = FitAttackMacroDistanceCurve(values, parity);
    const auto baseline = GenerateAttackMacroCurve(ramp);
    double weightedOffset = 0.0, weightSum = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (static_cast<int>(i % 2) == parity || values[i] >= 1.0) continue;
      weightedOffset += target.weights[i] * (values[i] - baseline[i]);
      weightSum += target.weights[i];
    }
    const double offset = (weightSum > 0.0) ? std::clamp(weightedOffset / weightSum, 0.0, 1.0) : 1.0;
    MacroFitPoint<4> bestPoint{GetAttackMacroTime(ramp.base), GetAttackMacroTime(ramp.outer), (GetAttackMacroPosition(ramp.position) - 1.0) / 99.0, offset};
    double directionResidual = std::numeric_limits<double>::max();
    const auto score = [&](MacroFitPoint<4> point) {
      for (double& value : point) value = std::clamp(value, 0.0, 1.0);
      const AttackMacroKnobs knobs{GetAttackMacroTimeTravel(point[0]), GetAttackMacroTimeTravel(point[1]), GetAttackMacroPositionTravel(1.0 + 99.0 * point[2]),
                                   GetAttackOddEvenTravel(direction * point[3])};
      const auto curve = GenerateAttackMacroCurve(knobs);
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
    for (int restart = 0; restart < 3; ++restart) SearchMacroFitSimplex(bestPoint, MacroFitPoint<4>{0.05, 0.05, 0.05, 0.05}, score, 1.0e-8);
  }
  return best;
}

inline std::vector<MacroKnobDescriptor> GetAttackMacroKnobDescriptors() {
  const AttackMacroKnobs defaults;
  return {{"Base", help_text::oscillator_tabs::kMacroAttackBase, defaults.base, false},
          {"Outer", help_text::oscillator_tabs::kMacroAttackOuter, defaults.outer, false},
          {"Position", help_text::oscillator_tabs::kMacroAttackPosition, defaults.position, false},
          {"Odd/Even", help_text::oscillator_tabs::kMacroAttackOddEven, defaults.oddEven, true}};
}

inline void RegisterAttackMacroFunctions(const std::shared_ptr<EditorContext>& context, const std::vector<layout::LabelledKnob*>& knobs) {
  if (knobs.size() != 4) return;
  auto& functions = (*context->oscillatorTabControls.macroFunctions)[static_cast<std::size_t>(OscillatorParameter::attack)];
  functions.version = 1;
  functions.knobs = knobs;
  functions.generateValues = [knobs]() {
    return GenerateAttackMacroCurve(
        {knobs[0]->GetNormalizedValue(), knobs[1]->GetNormalizedValue(), knobs[2]->GetNormalizedValue(), knobs[3]->GetNormalizedValue()});
  };
  functions.fitKnobsToValues = [knobs](const OscillatorParameterValues& values) {
    const auto fitted = FitAttackMacroKnobs(values);
    knobs[0]->SetNormalizedValueSilently(fitted.base);
    knobs[1]->SetNormalizedValueSilently(fitted.outer);
    knobs[2]->SetNormalizedValueSilently(fitted.position);
    knobs[3]->SetNormalizedValueSilently(fitted.oddEven);
  };
}

} // namespace editor
} // namespace plugin_ui

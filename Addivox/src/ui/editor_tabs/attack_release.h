#pragma once

#include "common.h"
#include "macro_fit.h"

#include <limits>

namespace plugin_ui {
namespace editor {
inline double GetAttackReleaseMaxValue(OscillatorParameter parameter) { return parameter == OscillatorParameter::release ? 0.1 : 1.0; }

inline bool TryGetAttackReleaseShapeValue(OscillatorParameter parameter, const char* shapeName, int oscillatorIndex, double& value) {
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);

  if (parameter == OscillatorParameter::attack) {
    if (std::strcmp(shapeName, "linear ramp up") == 0 || std::strcmp(shapeName, "linear ramp up (slow)") == 0) {
      value = harmonicNumber / 100.0;
      return true;
    }

    if (std::strcmp(shapeName, "linear ramp up (fast)") == 0) {
      value = harmonicNumber / 1000.0;
      return true;
    }

    if (std::strcmp(shapeName, "square root ramp up") == 0 || std::strcmp(shapeName, "sqrt ramp up (slow)") == 0) {
      value = (0.11 * std::sqrt(harmonicNumber)) - 0.1;
      return true;
    }

    if (std::strcmp(shapeName, "sqrt ramp up (fast)") == 0) {
      value = ((0.11 * std::sqrt(harmonicNumber)) - 0.1) / 10.0;
      return true;
    }

    if (std::strcmp(shapeName, "logarithmic ramp up (slow)") == 0 || std::strcmp(shapeName, "exponential ramp up (slow)") == 0) {
      value = 0.01 + (0.99 * std::log(harmonicNumber) / std::log(100.0));
      return true;
    }

    if (std::strcmp(shapeName, "logarithmic ramp up (fast)") == 0 || std::strcmp(shapeName, "exponential ramp up (fast)") == 0) {
      value = 0.001 + (0.099 * std::log(harmonicNumber) / std::log(100.0));
      return true;
    }

    if (std::strcmp(shapeName, "flat") == 0) {
      value = 0.1;
      return true;
    }
  } else if (parameter == OscillatorParameter::release) {
    if (std::strcmp(shapeName, "linear ramp down") == 0 || std::strcmp(shapeName, "linear ramp down (slow)") == 0) {
      value = 0.1 - (static_cast<double>(oscillatorIndex) / 1000.0);
      return true;
    }

    if (std::strcmp(shapeName, "linear ramp down (fast)") == 0) {
      value = 0.01 - (static_cast<double>(oscillatorIndex) / 10000.0);
      return true;
    }

    if (std::strcmp(shapeName, "square ramp down (slow)") == 0) {
      const double distanceFromEnd = harmonicNumber - 101.0;
      value = ((distanceFromEnd * distanceFromEnd) / 101000.0) + (1.0 / 1010.0);
      return true;
    }

    if (std::strcmp(shapeName, "square ramp down (fast)") == 0) {
      const double distanceFromEnd = harmonicNumber - 101.0;
      value = ((((distanceFromEnd * distanceFromEnd) / 101000.0) + (1.0 / 1010.0)) / 10.0);
      return true;
    }

    if (std::strcmp(shapeName, "exponential ramp down (slow)") == 0) {
      value = 0.1 * std::pow(10.0, (-2.0 * static_cast<double>(oscillatorIndex)) / 99.0);
      return true;
    }

    if (std::strcmp(shapeName, "exponential ramp down (fast)") == 0) {
      value = 0.01 * std::pow(10.0, (-2.0 * static_cast<double>(oscillatorIndex)) / 99.0);
      return true;
    }

    if (std::strcmp(shapeName, "flat") == 0) {
      value = 0.1;
      return true;
    }
  }

  return false;
}

inline bool ApplyAttackReleaseShape(SimplePatch& patch, OscillatorParameter parameter, const char* shapeName) {
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    double value = 0.0;
    if (!TryGetAttackReleaseShapeValue(parameter, shapeName, oscillatorIndex, value)) return false;

    patch.SetOscillatorParameter(oscillatorIndex, parameter, std::clamp(value, 0.0, GetAttackReleaseMaxValue(parameter)));
  }

  return true;
}

inline bool ApplyAttackReleaseAction(SimplePatch& patch, OscillatorParameter parameter, const char* actionName, EditorOscillatorEditScope editScope) {
  return ApplyStandardHarmonicAction(patch, parameter, actionName, 0.0, GetAttackReleaseMaxValue(parameter), editScope);
}

// Attack macros draw straight ramps in square-root display space, then add Odd/Even in seconds.
// Keep version 1 during unreleased development; bump only when changing a released macro definition.
inline constexpr double kAttackSlopeMax = 30.0;            // Chart-height change over 99 harmonics, independent of Position.
inline constexpr double kAttackSlopeQuarterStrength = 1.0; // Magnitude one quarter of the way from centre to either end.
static_assert(kAttackSlopeQuarterStrength > 0.0 && kAttackSlopeQuarterStrength < kAttackSlopeMax, "Quarter strength must be inside the slope range");
inline const double kAttackSlopeTravelExponent = std::log(kAttackSlopeMax / kAttackSlopeQuarterStrength) / std::log(4.0);
inline constexpr double kAttackTimeTravelExponent = 3.321928094887362;    // Half travel = 0.1 seconds.
inline constexpr double kAttackOddEvenTravelExponent = 4.321928094887362; // Half strength = 0.05 seconds.
inline constexpr double kAttackFitRelativeFloor = 0.01;

inline double GetAttackMacroTime(double travel) { return BendMacroTravel(travel, kAttackTimeTravelExponent); }
inline double GetAttackMacroTimeTravel(double seconds) { return BendMacroTravel(seconds, 1.0 / kAttackTimeTravelExponent); }
inline double GetAttackMacroPosition(double travel) { return std::pow(100.0, std::clamp(travel, 0.0, 1.0)); }
inline double GetAttackMacroPositionTravel(double harmonic) { return std::log(std::clamp(harmonic, 1.0, 100.0)) / std::log(100.0); }

inline double GetAttackMacroSlope(double travel) {
  const double distance = 2.0 * std::clamp(travel, 0.0, 1.0) - 1.0;
  return std::copysign(kAttackSlopeMax * std::pow(std::abs(distance), kAttackSlopeTravelExponent), distance);
}
inline double GetAttackMacroSlopeTravel(double slope) {
  return 0.5 + 0.5 * std::copysign(BendMacroTravel(std::abs(slope) / kAttackSlopeMax, 1.0 / kAttackSlopeTravelExponent), slope);
}

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
  double slope{0.625}; // +1 with the current mapping.
  double position{0.0};
  double oddEven{0.5};
};

inline OscillatorParameterValues MakeAttackMacroBasis(double position) {
  OscillatorParameterValues basis{};
  for (std::size_t i = 0; i < basis.size(); ++i) basis[i] = std::abs(i + 1.0 - position) / 99.0;
  return basis;
}

inline OscillatorParameterValues GenerateAttackMacroCurve(const AttackMacroKnobs& knobs) {
  auto values = MakeAttackMacroBasis(GetAttackMacroPosition(knobs.position));
  const double baseHeight = std::sqrt(GetAttackMacroTime(knobs.base)), slope = GetAttackMacroSlope(knobs.slope);
  const double addition = GetAttackOddEvenAddition(knobs.oddEven);
  for (std::size_t i = 0; i < values.size(); ++i) {
    const bool affected = (i % 2 == 0) ? addition < 0.0 : addition > 0.0;
    const double height = std::clamp(baseHeight + slope * values[i], 0.0, 1.0);
    values[i] = std::min(1.0, height * height + (affected ? std::abs(addition) : 0.0));
  }
  return values;
}

inline AttackMacroKnobs FitAttackMacroDistanceCurve(const OscillatorParameterValues& values, int parity, double fixedPosition = -1.0) {
  auto heights = values;
  for (double& height : heights) height = std::sqrt(height);
  auto target = MakeMacroFitTarget(heights, kAttackFitRelativeFloor);
  for (std::size_t i = 0; i < values.size(); ++i)
    if (static_cast<int>(i % 2) != parity) target.weights[i] = 0.0;
  AttackMacroKnobs best;
  double bestResidual = std::numeric_limits<double>::max();
  MacroFitPoint<1> bestPoint{};

  // Unclipped samples describe a straight line in display space. Solve its intercept and slope,
  // then score against every sample, including the clipped plateaus.
  const auto score = [&](const MacroFitPoint<1>& point) {
    const double position = std::clamp(point[0], 0.0, 1.0);
    const auto basis = MakeAttackMacroBasis(1.0 + 99.0 * position);
    double aa = 0.0, ab = 0.0, bb = 0.0, ay = 0.0, by = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (values[i] <= 0.0 || values[i] >= 1.0) continue;
      const double a = 1.0, b = basis[i], weight = target.weights[i];
      aa += weight * a * a;
      ab += weight * a * b;
      bb += weight * b * b;
      ay += weight * a * heights[i];
      by += weight * b * heights[i];
    }

    double residual = std::numeric_limits<double>::max();
    const auto evaluate = [&](double base, double slope) {
      if (base < 0.0 || base > 1.0 || slope < -kAttackSlopeMax || slope > kAttackSlopeMax) return;
      double error = 0.0;
      for (std::size_t i = 0; i < values.size(); ++i) {
        const double difference = std::clamp(base + slope * basis[i], 0.0, 1.0) - heights[i];
        error += target.weights[i] * difference * difference;
      }
      residual = std::min(residual, error);
      if (error >= bestResidual) return;
      bestResidual = error;
      bestPoint = {position};
      best = {GetAttackMacroTimeTravel(base * base), GetAttackMacroSlopeTravel(slope), GetAttackMacroPositionTravel(1.0 + 99.0 * position)};
    };

    const double determinant = aa * bb - ab * ab;
    if (determinant > kMacroEpsilon * aa * bb) evaluate((ay * bb - by * ab) / determinant, (by * aa - ay * ab) / determinant);
    if (aa > 0.0 && bb > 0.0) {
      for (const double base : {0.0, 1.0}) evaluate(base, std::clamp((by - base * ab) / bb, -kAttackSlopeMax, kAttackSlopeMax));
      for (const double slope : {-kAttackSlopeMax, kAttackSlopeMax}) evaluate(std::clamp((ay - slope * ab) / aa, 0.0, 1.0), slope);
      evaluate(std::clamp(ay / aa, 0.0, 1.0), 0.0);
    }
    // Also seed fully clipped curves, for which no unconstrained line can be measured.
    evaluate(0.0, 0.0);
    evaluate(1.0, 0.0);
    return residual;
  };

  if (fixedPosition >= 0.0) {
    score({fixedPosition});
    return best;
  }

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
    return {travel, 0.5, 0.0, 0.5};
  }
  const auto target = MakeMacroFitTarget(values, kAttackFitRelativeFloor);
  AttackMacroKnobs best;
  double bestResidual = std::numeric_limits<double>::max();

  // The unaffected parity still describes the original ramp, even when the other parity clips at 1.
  // Try both directions, then refine against the complete, capped curve. Search addition linearly so
  // tuning the knob's centre resolution cannot change the fitter's accuracy.
  for (int parity = 0; parity < 2; ++parity) {
    // A peak entirely between unaffected harmonics is invisible to that parity. Recover a
    // possible single-harmonic peak from the other parity's excess over its constant offset.
    bool silentParity = true;
    double offset = 1.0;
    std::size_t peak = 1 - parity;
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (static_cast<int>(i % 2) == parity) silentParity = silentParity && values[i] == 0.0;
      else {
        offset = std::min(offset, values[i]);
        if (values[i] > values[peak]) peak = i;
      }
    }
    if (silentParity) {
      const AttackMacroKnobs candidate{GetAttackMacroTimeTravel(values[peak] - offset), 0.0, GetAttackMacroPositionTravel(peak + 1.0),
                                       GetAttackOddEvenTravel(parity == 0 ? offset : -offset)};
      const auto curve = GenerateAttackMacroCurve(candidate);
      double error = 0.0;
      for (std::size_t i = 0; i < values.size(); ++i) error = std::max(error, std::abs(values[i] - curve[i]));
      if (error < 1.0e-12) return candidate;
    }
    std::vector<double> seedPositions{-1.0, 0.0, 0.005, 0.01, 0.99, 0.995, 1.0};
    // Narrow clipped peaks may expose only one sample. Seed around the parity's extrema,
    // as well as the endpoint intervals where the two parities cannot locate the same vertex.
    std::size_t lowest = parity, highest = parity;
    for (std::size_t i = parity; i < values.size(); i += 2) {
      if (values[i] < values[lowest]) lowest = i;
      if (values[i] > values[highest]) highest = i;
    }
    for (const auto index : {lowest, highest})
      for (const double offset : {-0.5, 0.0, 0.5}) seedPositions.push_back(std::clamp((index + offset) / 99.0, 0.0, 1.0));
    for (const double seedPosition : seedPositions) {
      const double direction = (parity == 0) ? 1.0 : -1.0;
      const auto ramp = FitAttackMacroDistanceCurve(values, parity, seedPosition);
      const auto baseline = GenerateAttackMacroCurve(ramp);
      double weightedOffset = 0.0, weightSum = 0.0;
      for (std::size_t i = 0; i < values.size(); ++i) {
        if (static_cast<int>(i % 2) == parity || values[i] >= 1.0) continue;
        weightedOffset += target.weights[i] * (values[i] - baseline[i]);
        weightSum += target.weights[i];
      }
      const double offset = (weightSum > 0.0) ? std::clamp(weightedOffset / weightSum, 0.0, 1.0) : 1.0;
      MacroFitPoint<4> bestPoint{std::sqrt(GetAttackMacroTime(ramp.base)), 0.5 + 0.5 * GetAttackMacroSlope(ramp.slope) / kAttackSlopeMax,
                                 (GetAttackMacroPosition(ramp.position) - 1.0) / 99.0, offset};
      double directionResidual = std::numeric_limits<double>::max();
      const auto score = [&](MacroFitPoint<4> point) {
        for (double& value : point) value = std::clamp(value, 0.0, 1.0);
        const AttackMacroKnobs knobs{GetAttackMacroTimeTravel(point[0] * point[0]), GetAttackMacroSlopeTravel(kAttackSlopeMax * (2.0 * point[1] - 1.0)),
                                     GetAttackMacroPositionTravel(1.0 + 99.0 * point[2]), GetAttackOddEvenTravel(direction * point[3])};
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
      // Steep clipped ramps need small steps; keep that search independent of the broader
      // search so a wide step cannot erase a promising narrow-peak seed.
      const auto seed = bestPoint;
      for (const double stepScale : {1.0 / kAttackSlopeMax, 1.0}) {
        bestPoint = seed;
        directionResidual = std::numeric_limits<double>::max();
        for (int restart = 0; restart < 8; ++restart)
          SearchMacroFitSimplex(bestPoint, MacroFitPoint<4>{0.02, 0.02 * stepScale, 0.002 * stepScale, 0.02}, score, 1.0e-9);
      }
    }
  }
  return best;
}

inline std::vector<MacroKnobDescriptor> GetAttackMacroKnobDescriptors() {
  const AttackMacroKnobs defaults;
  return {{"Base", help_text::oscillator_tabs::kMacroAttackBase, defaults.base, false},
          {"Slope", help_text::oscillator_tabs::kMacroAttackSlope, defaults.slope, true},
          {"Position", help_text::oscillator_tabs::kMacroAttackPosition, defaults.position, false},
          {"Odd/Even", help_text::oscillator_tabs::kMacroAttackOddEven, defaults.oddEven, true}};
}

inline void RegisterAttackMacroFunctions(const std::shared_ptr<EditorContext>& context, const std::vector<layout::LabelledKnob*>& knobs) {
  if (knobs.size() != 4) return;
  auto& functions = (*context->oscillatorTabControls.macroFunctions)[static_cast<std::size_t>(OscillatorParameter::attack)];
  functions.version = 1; // Slope uses a fixed harmonic distance, independent of Position.
  functions.knobs = knobs;
  functions.generateValues = [knobs]() {
    return GenerateAttackMacroCurve(
        {knobs[0]->GetNormalizedValue(), knobs[1]->GetNormalizedValue(), knobs[2]->GetNormalizedValue(), knobs[3]->GetNormalizedValue()});
  };
  functions.fitKnobsToValues = [knobs](const OscillatorParameterValues& values) {
    const auto fitted = FitAttackMacroKnobs(values);
    knobs[0]->SetNormalizedValueSilently(fitted.base);
    knobs[1]->SetNormalizedValueSilently(fitted.slope);
    knobs[2]->SetNormalizedValueSilently(fitted.position);
    knobs[3]->SetNormalizedValueSilently(fitted.oddEven);
  };
}

inline void AppendAttackReleaseTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors) {
  descriptors.push_back(
      {kOscillatorTabTitles[2], "Attack time", OscillatorParameter::attack, {0.0, 1.0}, help_text::oscillator_tabs::Get(OscillatorParameter::attack)});
  descriptors.push_back(
      {kOscillatorTabTitles[3], "Release time", OscillatorParameter::release, {0.0, 0.1}, help_text::oscillator_tabs::Get(OscillatorParameter::release)});
}

inline void AttachAttackReleaseTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                           const OscillatorTabDescriptor& descriptor, IVButtonControl* restoreButton, IVButtonControl* addButton,
                                           IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);
  const auto attackReleaseIndex = GetAttackReleaseTabIndex(descriptor.parameter);
  auto* yTransformControl = CreateYTransformControl(context->GetTransformRef(descriptor.parameter), sliderControl, styles);
  auto* setShapeControl = new ActionSelectionControl(
      IRECT(), "choose shape",
      descriptor.parameter == OscillatorParameter::attack
          ? std::initializer_list<const char*>{"linear ramp up (slow)", "linear ramp up (fast)", "sqrt ramp up (slow)", "sqrt ramp up (fast)",
                                               "logarithmic ramp up (slow)", "logarithmic ramp up (fast)", "flat"}
          : std::initializer_list<const char*>{"linear ramp down (slow)", "linear ramp down (fast)", "square ramp down (slow)", "square ramp down (fast)",
                                               "exponential ramp down (slow)", "exponential ramp down (fast)", "flat"},
      styles.utilityDropdownText, styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl, parameter = descriptor.parameter](const char* selectedText) {
    if (!selectedText) return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(
        sliderControl, parameter, [selectedText, parameter](SimplePatch& patch) { return ApplyAttackReleaseShape(patch, parameter, selectedText); });
  });

  auto* actionsControl = new ActionSelectionControl(IRECT(), "run action",
                                                    {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionTowardMaxMenuLabel, kActionAwayFromMaxMenuLabel,
                                                     kActionBendUpMenuLabel, kActionBendDownMenuLabel},
                                                    styles.utilityDropdownText, styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl, parameter = descriptor.parameter](const char* selectedText) {
    if (!selectedText) return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(sliderControl, parameter, [selectedText, parameter, context](SimplePatch& patch) {
      return ApplyAttackReleaseAction(patch, parameter, selectedText, context->GetOscillatorEditScope(parameter));
    });
  });

  (*context->attackReleaseTab.setShapeControls)[attackReleaseIndex] = setShapeControl;
  (*context->attackReleaseTab.actionsControls)[attackReleaseIndex] = actionsControl;

  AttachHarmonicTabChildren(page, context, styles, descriptor, xRangeControls, yTransformControl, setShapeControl, actionsControl, allKeyNotesControls,
                            restoreButton, addButton, deleteButton, sliderControl);

  if (descriptor.parameter == OscillatorParameter::attack)
    RegisterAttackMacroFunctions(context, AttachMacroKnobChildren(page, context, descriptor, GetAttackMacroKnobDescriptors()));
}
} // namespace editor
} // namespace plugin_ui

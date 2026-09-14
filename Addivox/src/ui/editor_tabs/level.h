#pragma once

#include "common.h"
#include "macro_fit.h"

#include <limits>

namespace plugin_ui {
namespace editor {
template <std::size_t N> inline bool IsListedHarmonic(int harmonicIndex, const std::array<int, N>& harmonics) {
  return std::binary_search(harmonics.begin(), harmonics.end(), harmonicIndex);
}

inline bool IsOctavesHarmonic(int harmonicIndex) { return harmonicIndex > 0 && (harmonicIndex & (harmonicIndex - 1)) == 0; }

inline bool IsOctavesAndFifthsHarmonic(int harmonicIndex) {
  static constexpr std::array<int, 13> kHarmonics{{1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96}};
  return IsListedHarmonic(harmonicIndex, kHarmonics);
}

inline bool IsOctavesFifthsAndThirdsHarmonic(int harmonicIndex) {
  static constexpr std::array<int, 18> kHarmonics{{1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64, 80, 96}};
  return IsListedHarmonic(harmonicIndex, kHarmonics);
}

inline bool TryGetLevelShapeValue(const char* shapeName, int oscillatorIndex, double& level) {
  const double harmonicNumber = static_cast<double>(oscillatorIndex + 1);
  const int harmonicIndex = oscillatorIndex + 1;

  if (std::strcmp(shapeName, "sine") == 0) {
    level = (oscillatorIndex == 0) ? 1.0 : 0.0;
    return true;
  }

  if (std::strcmp(shapeName, "saw") == 0) {
    level = 1.0 / harmonicNumber;
    return true;
  }

  if (std::strcmp(shapeName, "square") == 0) {
    level = (oscillatorIndex % 2 == 0) ? (1.0 / harmonicNumber) : 0.0;
    return true;
  }

  if (std::strcmp(shapeName, "triangle") == 0) {
    level = (oscillatorIndex % 2 == 0) ? (1.0 / (harmonicNumber * harmonicNumber)) : 0.0;
    return true;
  }

  if (std::strcmp(shapeName, "flat") == 0) {
    level = 1.0;
    return true;
  }

  if (std::strcmp(shapeName, "octaves") == 0) {
    level = IsOctavesHarmonic(harmonicIndex) ? 1.0 : 0.0;
    return true;
  }

  if (std::strcmp(shapeName, "octaves+fifths") == 0) {
    if (IsOctavesHarmonic(harmonicIndex)) level = 1.0;
    else if (IsOctavesAndFifthsHarmonic(harmonicIndex))
      level = 0.5;
    else
      level = 0.0;
    return true;
  }

  if (std::strcmp(shapeName, "octaves+fifths+thirds") == 0) {
    if (IsOctavesHarmonic(harmonicIndex)) level = 1.0;
    else if (IsOctavesAndFifthsHarmonic(harmonicIndex))
      level = 0.5;
    else if (IsOctavesFifthsAndThirdsHarmonic(harmonicIndex))
      level = 0.25;
    else
      level = 0.0;
    return true;
  }

  return false;
}

inline bool ApplyLevelShape(SimplePatch& patch, const char* shapeName) {
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    double level = 0.0;
    if (!TryGetLevelShapeValue(shapeName, oscillatorIndex, level)) return false;

    patch.SetOscillatorParameter(oscillatorIndex, OscillatorParameter::level, level);
  }

  return patch.NormalizeLevelWaveformRms();
}

inline bool ApplyLevelAction(SimplePatch& patch, const char* actionName, EditorOscillatorEditScope editScope) {
  if (MatchesActionLabel(actionName, kActionNormalize)) return patch.NormalizeLevelWaveformRms();

  return ApplyStandardHarmonicAction(patch, OscillatorParameter::level, actionName, 0.0, 1.0, editScope);
}

// Level macros shape a spectral envelope in pseudo-log display space, then normalize its nominal waveform RMS.

// Descriptor/storage order. Odd/Even is solved analytically; the other knobs are searched.
enum LevelMacroKnobIndex { kLevelWidthKnob, kLevelShapeKnob, kLevelFundKnob, kLevelOddEvenKnob, kNumLevelMacroKnobs };

inline constexpr std::size_t kNumLevelMacroSearchKnobs = kLevelOddEvenKnob;

inline constexpr double   kLevelWidthDefault = 0.50;
inline constexpr double   kLevelShapeDefault = 0.50;
inline constexpr double    kLevelFundDefault = 0.50; // Centred is no lift at all, which is what a double-tap should give.
inline constexpr double kLevelOddEvenDefault = 0.50;

// Width is the last audible harmonic; the curve reaches zero one harmonic later.
inline constexpr double kLevelWidthHarmonicMin =   1.0;
inline constexpr double kLevelWidthHarmonicMax = static_cast<double>(SimplePatch::kNumOscillators);

// Geometric exponent range centred on a straight line in display space.
inline constexpr double kLevelShapeExponentMax = 4.0;

// Fund lift: 1 + lift * exp(-(offset / reach)^decay), with reach = fraction * widthHarmonic^power.
inline constexpr double kLevelFundReachFraction = 0.40;
inline constexpr double kLevelFundReachWidthPower = 0.55;
inline constexpr double kLevelFundDecayExponent = 1.20;
inline constexpr double kLevelFundBoostMax = 1.00;

inline constexpr double kLevelWidthTravelExponent = 1.0;
inline constexpr double kLevelShapeTravelExponent = 2.0;
inline constexpr double kLevelFundTravelExponent = 1.25;

// Where the fit stops caring: harmonics more than 60 dB below the peak no longer pull on it.
inline constexpr double kLevelFitRelativeFloor = 1.0e-3;

using LevelMacroCurve = std::array<double, SimplePatch::kNumOscillators>;

// Normalised 0..1 knob positions, in the same order as the saved Level macro settings.
struct LevelMacroKnobs {
  double width{kLevelWidthDefault};
  double shape{kLevelShapeDefault};
  double fund{kLevelFundDefault};
  double oddEven{kLevelOddEvenDefault};
};

// The same values in the units the curve is drawn in.
struct LevelMacroModel {
  double widthHarmonic{0.0};  // Last audible harmonic; the falling curve reaches zero at widthHarmonic + 1
  double shapeExponent{1.0};  // Bows the fall: below 1 it holds up then plunges, 1 straight, above 1 the reverse
  double fundLift{0.0};       // What the fundamental gains, as a fraction of what the fall alone gives it: -1 silences it
  double oddEvenWeight{0.0};  // -1 all even, 0 balanced, +1 all odd
};

inline LevelMacroModel GetLevelMacroModel(const LevelMacroKnobs& knobs) {
  const double widthTravel = BendMacroTravel(knobs.width, kLevelWidthTravelExponent);
  const double shapeTravel = BendMacroTravelAboutCentre(knobs.shape, kLevelShapeTravelExponent);

  // Fund is bipolar: the two halves share one expression but not one range, since cutting ends at silence and
  // boosting ends wherever the boost maximum is put.
  const double fundTravel = (2.0 * BendMacroTravelAboutCentre(knobs.fund, kLevelFundTravelExponent)) - 1.0;

  LevelMacroModel model;
  model.widthHarmonic = kLevelWidthHarmonicMin + (widthTravel * (kLevelWidthHarmonicMax - kLevelWidthHarmonicMin));
  model.shapeExponent = std::pow(kLevelShapeExponentMax, 1.0 - (2.0 * shapeTravel));
  model.fundLift = (fundTravel < 0.0) ? fundTravel : (fundTravel * kLevelFundBoostMax);
  model.oddEvenWeight = 1.0 - (2.0 * std::clamp(knobs.oddEven, 0.0, 1.0));
  return model;
}

inline double GetLevelOddEvenSign(int oscillatorIndex) { return IsOddHarmonic(oscillatorIndex) ? 1.0 : -1.0; }

inline int GetLevelHarmonicParity(int oscillatorIndex) { return IsOddHarmonic(oscillatorIndex) ? 1 : 0; }

// Multiply the Width/Shape fall by the Fund lift in display space.
inline void FillLevelMacroDisplayCurve(const LevelMacroModel& model, LevelMacroCurve& display) {
  const double reach = kLevelFundReachFraction * std::pow(model.widthHarmonic, kLevelFundReachWidthPower);

  for (std::size_t index = 0; index < display.size(); ++index) {
    const double offset = static_cast<double>(index);
    const double fall = std::pow(std::max(0.0, 1.0 - (offset / model.widthHarmonic)), model.shapeExponent);
    const double scoop = std::exp(-std::pow(offset / reach, kLevelFundDecayExponent));

    display[index] = fall * (1.0 + (model.fundLift * scoop));
  }

  // A unit apex gives the height solver a known upper bound.
  double apex = 0.0;
  for (const double value : display) apex = std::max(apex, value);
  if (apex <= kMacroEpsilon) return;

  for (double& value : display) value /= apex;
}

// Solve the display height for the shared nominal RMS target. Scaling levels afterwards would bend a
// straight line on the pseudo-log chart, whereas solving the height preserves the drawn shape.
// With exponent = displayShape * height, solve sum(expm1(exponent * s)^2) = 2 * (RMS * expm1(displayShape))^2.
// This sum is increasing and convex. Starting where the apex alone reaches the target gives an overshoot,
// so Newton converges from above without needing a bracket.
inline double SolveLevelMacroDisplayHeight(const LevelMacroCurve& display) {
  constexpr int kMaxIterations = 16;
  constexpr double kRelativeTolerance = 1.0e-13;

  const double displayShape = transformations::GetGlobalPseudoLogShapeValue();
  const double scaledRms = SimplePatch::kReferenceLevelWaveformRms * std::expm1(displayShape);
  const double target = 2.0 * scaledRms * scaledRms;

  double exponent = std::log1p(std::sqrt(target));
  for (int iteration = 0; iteration < kMaxIterations; ++iteration) {
    double sum = 0.0;
    double slope = 0.0;
    for (const double value : display) {
      const double term = std::expm1(exponent * value);
      sum += term * term;
      slope += 2.0 * term * (term + 1.0) * value;
    }

    if (slope <= kMacroEpsilon) return 0.0;
    if (std::fabs(sum - target) <= kRelativeTolerance * target) break;
    exponent -= (sum - target) / slope;
  }

  return exponent / displayShape;
}

// Convert from the fixed pseudo-log macro model, independently of the tab's current Y transform.
inline void FillLevelMacroBasis(const LevelMacroCurve& display, OscillatorParameterValues& basis) {
  const double displayShape = transformations::GetGlobalPseudoLogShapeValue();
  const double height = SolveLevelMacroDisplayHeight(display);

  for (std::size_t index = 0; index < basis.size(); ++index) basis[index] = transformations::NormalizedExp(height * display[index], displayShape);
}

// Apply the shared RMS target and nominal peak ceiling after odd/even weighting. The fit's free parity
// scales absorb this uniform gain; fitting candidates do not need to scan the waveform.
inline OscillatorParameterValues GenerateLevelMacroCurve(const LevelMacroKnobs& knobs) {
  const LevelMacroModel model = GetLevelMacroModel(knobs);

  LevelMacroCurve display{};
  FillLevelMacroDisplayCurve(model, display);

  OscillatorParameterValues values{};
  FillLevelMacroBasis(display, values);

  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    double& value = values[static_cast<std::size_t>(oscillatorIndex)];
    value = std::max(0.0, value * (1.0 + (model.oddEvenWeight * GetLevelOddEvenSign(oscillatorIndex))));
  }

  if (!SimplePatch::NormalizeLevels(values)) {
    // A degenerate curve still has to be audible; normalise the fallback sine by the same rules.
    values.fill(0.0);
    values[0] = 1.0;
    SimplePatch::NormalizeLevels(values);
  }

  return values;
}

struct LevelMacroFitResult {
  LevelMacroKnobs knobs{};
  double residual{std::numeric_limits<double>::max()};
};

// Solve independent weighted parity scales: their ratio gives Odd/Even, while their common gain absorbs loudness.
// A joint 2x2 solve loses precision on narrow curves because its determinant subtracts nearly equal products.
inline void AccumulateLevelMacroFit(const OscillatorParameterValues& basis, const MacroFitTarget& target, const LevelMacroKnobs& knobs,
                                    LevelMacroFitResult& best) {
  double basisSquared[2] = {0.0, 0.0}; // Indexed by GetLevelHarmonicParity: even harmonics, then odd.
  double targetDotBasis[2] = {0.0, 0.0};
  double targetSquared = 0.0;

  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    const auto index = static_cast<std::size_t>(oscillatorIndex);
    const double weightedBasis = target.weights[index] * basis[index];
    const int parity = GetLevelHarmonicParity(oscillatorIndex);

    basisSquared[parity] += weightedBasis * basis[index];
    targetDotBasis[parity] += weightedBasis * target.values[index];
    targetSquared += target.weights[index] * target.values[index] * target.values[index];
  }

  // Missing parity data cannot determine Odd/Even.
  const double totalBasisSquared = basisSquared[0] + basisSquared[1];
  if (std::min(basisSquared[0], basisSquared[1]) <= kMacroEpsilon * totalBasisSquared) return;

  const double evenScale = std::max(0.0, targetDotBasis[0] / basisSquared[0]);
  const double oddScale = std::max(0.0, targetDotBasis[1] / basisSquared[1]);
  const double totalScale = evenScale + oddScale;
  if (totalScale <= kMacroEpsilon) return;

  double residual = targetSquared;
  residual += (evenScale * ((evenScale * basisSquared[0]) - (2.0 * targetDotBasis[0])));
  residual += (oddScale * ((oddScale * basisSquared[1]) - (2.0 * targetDotBasis[1])));
  residual = std::max(0.0, residual);
  if (residual >= best.residual) return;

  best.residual = residual;
  best.knobs = LevelMacroKnobs{knobs.width, knobs.shape, knobs.fund, evenScale / totalScale};
}

inline void EvaluateLevelMacroCandidate(const MacroFitTarget& target, const LevelMacroKnobs& knobs, LevelMacroCurve& display,
                                        OscillatorParameterValues& basis, LevelMacroFitResult& best) {
  FillLevelMacroDisplayCurve(GetLevelMacroModel(knobs), display);
  FillLevelMacroBasis(display, basis);
  AccumulateLevelMacroFit(basis, target, knobs, best);
}

// Seed Width from the last audible harmonic: crossing the silent tail produces a sharp residual cliff.
inline double MeasureLevelMacroWidthSeed(const OscillatorParameterValues& values) {
  double peak = 0.0;
  for (const double value : values) peak = std::max(peak, value);

  const double audibleFloor = peak * kLevelFitRelativeFloor;
  std::size_t topIndex = 0;
  for (std::size_t index = values.size(); index-- > 0;) {
    if (values[index] <= audibleFloor) continue;

    topIndex = index;
    break;
  }

  const double span = std::clamp(static_cast<double>(topIndex) / (kLevelWidthHarmonicMax - kLevelWidthHarmonicMin), 0.0, 1.0);
  return (kLevelWidthTravelExponent == 1.0) ? span : std::pow(span, 1.0 / kLevelWidthTravelExponent);
}

// Search Shape and Fund over their full travel, and Width near its measured endpoint.
inline constexpr int    kLevelMacroGridSteps = 8;    // Nine samples an axis, so the grid lands within a sixteenth of the travel.
inline constexpr double kLevelMacroWidthBand = 0.08; // Eight harmonics either side of the measurement.

inline std::array<MacroFitAxis, kNumLevelMacroSearchKnobs> GetLevelMacroFitAxes(double widthSeed) {
  return {{{std::max(0.0, widthSeed - kLevelMacroWidthBand), std::min(1.0, widthSeed + kLevelMacroWidthBand)}, {0.0, 1.0}, {0.0, 1.0}}};
}

// The searched knobs as a point, in the order the knob enum lists them.
using LevelMacroPoint = MacroFitPoint<kNumLevelMacroSearchKnobs>;

inline LevelMacroKnobs MakeLevelMacroKnobs(const LevelMacroPoint& point) {
  return {std::clamp(point[0], 0.0, 1.0), std::clamp(point[1], 0.0, 1.0), std::clamp(point[2], 0.0, 1.0)};
}

inline LevelMacroPoint MakeLevelMacroPoint(const LevelMacroKnobs& knobs) { return {knobs.width, knobs.shape, knobs.fund}; }

// Width needs smaller simplex steps than Shape/Fund. Restarts escape stalls in narrow residual valleys.
inline constexpr LevelMacroPoint kLevelMacroSimplexSteps{{0.04, 0.18, 0.18}};
inline constexpr int kLevelMacroSimplexRuns = 4;

// Grid search followed by simplex restarts; the scorer retains the best knobs, including solved Odd/Even.
inline LevelMacroKnobs FitLevelMacroKnobs(const OscillatorParameterValues& values) {
  const MacroFitTarget target = MakeMacroFitTarget(values, kLevelFitRelativeFloor);

  LevelMacroCurve display{};
  OscillatorParameterValues basis{};
  LevelMacroFitResult best;

  const auto score = [&](const LevelMacroPoint& point) {
    LevelMacroFitResult candidate;
    EvaluateLevelMacroCandidate(target, MakeLevelMacroKnobs(point), display, basis, candidate);

    if (candidate.residual < best.residual) best = candidate;
    return candidate.residual;
  };

  SweepMacroFitGrid(GetLevelMacroFitAxes(MeasureLevelMacroWidthSeed(values)), kLevelMacroGridSteps, score);
  if (best.residual == std::numeric_limits<double>::max()) return LevelMacroKnobs{};

  for (int run = 0; run < kLevelMacroSimplexRuns; ++run) SearchMacroFitSimplex(MakeLevelMacroPoint(best.knobs), kLevelMacroSimplexSteps, score);

  return best.knobs;
}

inline std::vector<MacroKnobDescriptor> GetLevelMacroKnobDescriptors() {
  return {{"Width", help_text::oscillator_tabs::kMacroLevelWidth, kLevelWidthDefault, false},
          {"Shape", help_text::oscillator_tabs::kMacroLevelShape, kLevelShapeDefault, false},
          {"Fund", help_text::oscillator_tabs::kMacroLevelFund, kLevelFundDefault, false},
          {"Odd/Even", help_text::oscillator_tabs::kMacroLevelOddEven, kLevelOddEvenDefault, true}};
}

inline LevelMacroKnobs ReadLevelMacroKnobs(const std::vector<layout::LabelledKnob*>& knobControls) {
  return {knobControls[kLevelWidthKnob]->GetNormalizedValue(), knobControls[kLevelShapeKnob]->GetNormalizedValue(),
          knobControls[kLevelFundKnob]->GetNormalizedValue(), knobControls[kLevelOddEvenKnob]->GetNormalizedValue()};
}

inline void RegisterLevelMacroFunctions(const std::shared_ptr<EditorContext>& context, const std::vector<layout::LabelledKnob*>& knobControls) {
  if (knobControls.size() != static_cast<std::size_t>(kNumLevelMacroKnobs)) return;

  auto& functions = (*context->oscillatorTabControls.macroFunctions)[static_cast<std::size_t>(OscillatorParameter::level)];

  functions.version = 1;
  functions.knobs = knobControls;
  functions.generateValues = [knobControls]() { return GenerateLevelMacroCurve(ReadLevelMacroKnobs(knobControls)); };

  functions.fitKnobsToValues = [knobControls](const OscillatorParameterValues& values) {
    const LevelMacroKnobs knobs = FitLevelMacroKnobs(values);

    knobControls[kLevelWidthKnob]->SetNormalizedValueSilently(knobs.width);
    knobControls[kLevelShapeKnob]->SetNormalizedValueSilently(knobs.shape);
    knobControls[kLevelFundKnob]->SetNormalizedValueSilently(knobs.fund);
    knobControls[kLevelOddEvenKnob]->SetNormalizedValueSilently(knobs.oddEven);
  };
}

inline void AppendLevelTabDescriptors(std::vector<OscillatorTabDescriptor>& descriptors) {
  descriptors.push_back(
      {kOscillatorTabTitles[0], "Level", OscillatorParameter::level, {0.0, 1.0}, help_text::oscillator_tabs::Get(OscillatorParameter::level)});
}

inline void AttachLevelTabChildren(IVTabPage* page, const std::shared_ptr<EditorContext>& context, const EditorStyles& styles,
                                   const OscillatorTabDescriptor& descriptor, IVButtonControl* restoreButton, IVButtonControl* addButton,
                                   IVButtonControl* deleteButton, OscillatorSliderControl* sliderControl) {
  const auto xRangeControls = CreateXRangeControls(context, descriptor, styles);
  const auto allKeyNotesControls = CreateAllKeyNotesControls(context, descriptor, styles);

  auto* yTransformControl = CreateYTransformControl(context->GetTransformRef(descriptor.parameter), sliderControl, styles);

  auto* setShapeControl =
      CreateHarmonicShapeControl(context, descriptor.parameter, sliderControl, styles,
                                 {"sine", "saw", "square", "triangle", "flat", "octaves", "octaves+fifths", "octaves+fifths+thirds"}, ApplyLevelShape);

  auto* actionsControl =
      CreateHarmonicActionsControl(context, descriptor.parameter, sliderControl, styles,
                                   {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionTowardMaxMenuLabel, kActionAwayFromMaxMenuLabel,
                                    kActionBendUpMenuLabel, kActionBendDownMenuLabel, kActionNormalizeMenuLabel},
                                   ApplyLevelAction);

  *context->levelTab.setShapeControl = setShapeControl;
  *context->levelTab.actionsControl = actionsControl;

  AttachHarmonicTabChildren(page, context, styles, descriptor, xRangeControls, yTransformControl, setShapeControl, actionsControl, allKeyNotesControls,
                            restoreButton, addButton, deleteButton, sliderControl);

  RegisterLevelMacroFunctions(context, AttachMacroKnobChildren(page, context, descriptor, GetLevelMacroKnobDescriptors()));
}
} // namespace editor
} // namespace plugin_ui

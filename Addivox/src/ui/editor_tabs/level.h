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

// ---------------------------------------------------------------------------------------------------------
// Level macros
//
// The curve is drawn in the tab's pseudo-log display space: a fall from the fundamental to silence just past
// the width harmonic, thinned at the bottom end by Fund. Width says how far up the series the fall reaches,
// Shape bows it, Fund scoops out the low harmonics, and odd/even weighting is multiplied in afterwards with
// the result normalised to the shared nominal waveform RMS target. There is no rounding control because
// both pieces are smooth, so their product has no corner in it at any setting.
//
// Working in display space is what makes the knobs feel even: a straight line on screen is a constant number
// of decibels per harmonic, so every knob moves the curve by roughly as much per degree of rotation wherever
// it happens to be.
//
// Nothing says where the peak goes, because nothing needs to: the fall wants h1 and Fund pushes down on it, so
// the strongest harmonic ends up whereever the two stop arguing. Turning Fund down moves it up the series.

// The knob controls come back in descriptor order, and these name the positions so that the descriptor table,
// the reader and the writer cannot drift apart. The knobs the search moves come first, so that a search point
// is just the leading part of the row -- Odd/Even is last because the fit solves for it exactly rather than
// searching for it.
enum LevelMacroKnobIndex { kLevelWidthKnob, kLevelShapeKnob, kLevelFundKnob, kLevelOddEvenKnob, kNumLevelMacroKnobs };

inline constexpr std::size_t kNumLevelMacroSearchKnobs = kLevelOddEvenKnob;

inline constexpr double   kLevelWidthDefault = 0.50;
inline constexpr double   kLevelShapeDefault = 0.50;
inline constexpr double    kLevelFundDefault = 0.50; // Centred is no lift at all, which is what a double-tap should give.
inline constexpr double kLevelOddEvenDefault = 0.50;

// ---------------------------------------------------------------------------------------------------------
// Tuning
//
// Every constant here is a dial rather than a decision: changing one moves the curves the knobs draw, or moves
// where along a knob's travel a given curve sits, but none of them changes how the macros work. They are kept
// together because they are meant to be tuned together, against how well the macros can match sounds worth
// matching and against how the knobs feel under the hand.

// What Width spans. It is the last audible harmonic, so the falling curve reaches zero one harmonic past it,
// and the top of the travel is the oscillator count -- a curve that only just fades out by the end of the
// series. Width is not merely an endpoint: it sets the slope of the whole fall, so every harmonic moves with
// it, and it sets how far Fund's scoop reaches as well.
inline constexpr double kLevelWidthHarmonicMin =   1.0;
inline constexpr double kLevelWidthHarmonicMax = static_cast<double>(SimplePatch::kNumOscillators);

// What Shape spans: the exponent of the fall, geometrically from 1/max through 1 -- a straight line on the
// chart -- to max. Measured against the factory patches this one is close to inert, anything from 3 upwards
// fitting them to the same four decimal places, so it is set for feel rather than for fit.
inline constexpr double kLevelShapeExponentMax = 4.0;

// The lift Fund applies to the bottom of the series, which is  1 + lift * exp(-(offset / reach)^decay). The
// lift runs -1 at the bottom of the knob, through 0 at the centre, up to the boost maximum at the top: -1
// silences the fundamental outright, and the same expression run the other way lets it stand above the fall.
//
// Reach is where the lift fades out, in harmonics, as  fraction * widthHarmonic^power. A power of 1 keeps it
// the same size relative to the curve it works on, so Fund reads as the same gesture on a narrow shape as on a
// wide one; lowering it holds the lift tight on wide shapes while still opening it out on narrow ones, which
// is the trade between what a bright sound wants and what a dark one does.
//
// Decay is the lift's profile. At 1 it is a plain exponential; below 1 it digs a deeper, narrower notch that
// climbs back steeply, and above 1 it spreads into something broader and flatter-topped. Above 1 also means
// the lift is flat at the fundamental rather than sloping, so it makes a shoulder just above h1 instead of
// steepening the curve's start -- which is what keeps it from doing Shape's job over again.
//
// The boost maximum is how far the other half of the knob reaches, as a multiple of the height the fall alone
// gives the fundamental. There is no principled ceiling the way silence is a principled floor, so this is the
// one end of Fund's travel that is chosen rather than derived.
inline constexpr double kLevelFundReachFraction = 0.40;
inline constexpr double kLevelFundReachWidthPower = 0.55;
inline constexpr double kLevelFundDecayExponent = 1.20;
inline constexpr double kLevelFundBoostMax = 1.00;

// Where along each knob's travel those curves sit. These cannot change which curves the macros can draw, only
// which part of a rotation draws them, so they answer to feel alone: they are set so that the sounds worth
// reaching are spread across the travel rather than bunched into a corner of it.
//
// Width bends its travel end to end -- above 1 gives the bottom of the knob more room, below 1 the top. Shape
// and Fund bend about their centres instead, so that half travel stays exactly what it is: a straight fall,
// and no lift at all.
//
// These were set by fitting the twenty-one brass and reed curves and looking at where their knobs landed.
// Width came out spread evenly across its whole travel already and is left alone. Shape bunched towards its
// centre, so that centre is stretched out. Fund's natural sounds all sit in its cutting half, because a
// natural spectrum does not stand its fundamental above the rest -- the sounds its boosting half exists for
// simply are not in the factory set, so its bend is a moderate one rather than one fitted to that lopsidedness.
inline constexpr double kLevelWidthTravelExponent = 1.0;
inline constexpr double kLevelShapeTravelExponent = 2.0;
inline constexpr double kLevelFundTravelExponent = 1.25;

// Where the fit stops caring: harmonics more than 60 dB below the peak no longer pull on it.
inline constexpr double kLevelFitRelativeFloor = 1.0e-3;

// ---------------------------------------------------------------------------------------------------------

using LevelMacroCurve = std::array<double, SimplePatch::kNumOscillators>;

// Normalised 0..1 knob positions: exactly what the knob controls hold, and the only place macro values live.
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
  model.oddEvenWeight = (std::clamp(knobs.oddEven, 0.0, 1.0) * 2.0) - 1.0;
  return model;
}

inline double GetLevelOddEvenSign(int oscillatorIndex) { return IsOddHarmonic(oscillatorIndex) ? 1.0 : -1.0; }

inline int GetLevelHarmonicParity(int oscillatorIndex) { return IsOddHarmonic(oscillatorIndex) ? 1 : 0; }

// The curve in display units, running 1 at the fundamental down to 0 (silent). Two pieces multiplied together:
//
//   fall(h)  = (1 - u)^q,  u running 0 at the fundamental to 1 one harmonic past the width harmonic
//   lift(h)  = 1 + fundLift * exp(-((h - 1) / reach)^decay)
//
// The fall is the shape Width and Shape draw between them: q below 1 leaves the fundamental gently and turns
// down hard at the end, above 1 drops away immediately and then trails, and 1 is a straight line on the chart.
// It arrives at zero rather than stopping short of it.
//
// The lift is Fund. It is at full strength on the fundamental and fades back to nothing as it goes up the
// series, so Fund is the fundamental's height against the fall and nothing else has to be arranged for it.
// Below the centre it cuts, reaching silence at the bottom of the travel because the factor is then 1 - 1;
// above the centre the same expression boosts instead, and being a factor rather than a subtraction it can
// never take a harmonic below silence at either end.
//
// It reaches beyond h1 either way, because a fundamental moved on its own reads as a notch or a spike rather
// than a voice: strongest on h2, weaker on h3, weaker again on h4, and never quite nothing, so there is no
// harmonic where the lift stops and the curve creases.
//
// Boosting and cutting are the same gesture here, not opposites. The height solved for below is what sets the
// curve's loudness, so standing the fundamental above the fall is arithmetically the same as pressing the rest
// of the series down beneath it -- which is why the top of the knob thins the tone out rather than filling it.
inline void FillLevelMacroDisplayCurve(const LevelMacroModel& model, LevelMacroCurve& display) {
  const double reach = kLevelFundReachFraction * std::pow(model.widthHarmonic, kLevelFundReachWidthPower);

  for (std::size_t index = 0; index < display.size(); ++index) {
    const double offset = static_cast<double>(index);
    const double fall = std::pow(std::max(0.0, 1.0 - (offset / model.widthHarmonic)), model.shapeExponent);
    const double scoop = std::exp(-std::pow(offset / reach, kLevelFundDecayExponent));

    display[index] = fall * (1.0 + (model.fundLift * scoop));
  }

  // Scaled so the apex is exactly 1. That is not a change of shape -- the height solved for below absorbs it --
  // but it is what lets that solve start from a known overshoot.
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

// The curve before odd/even weighting and normalisation: the drawn shape, read out of display space as levels.
// It uses the same pseudo-log shape the chart draws with, but as a constant of its own rather than a read of
// the tab's Y transform, so the generator stays a pure function of the knobs and the dropdown stays
// display-only.
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

inline double GetLevelMacroSearchKnob(const LevelMacroKnobs& knobs, std::size_t axis) {
  switch (axis) {
  case kLevelWidthKnob: return knobs.width;
  case kLevelShapeKnob: return knobs.shape;
  default:              return knobs.fund;
  }
}

inline void SetLevelMacroSearchKnob(LevelMacroKnobs& knobs, std::size_t axis, double value) {
  switch (axis) {
  case kLevelWidthKnob: knobs.width = value; break;
  case kLevelShapeKnob: knobs.shape = value; break;
  default:              knobs.fund = value; break;
  }
}

// With the other three fixed, the target is  basis * (1 + weight * sign) * scale. Because the sign is exactly
// +/-1, the odd harmonics and the even harmonics never appear in each other's normal equations: each half is
// just a scale on the basis, and weighted least squares gives it in one division. Leaving both scales free
// means the fit reads the curve's shape and ignores its loudness, which is what the knobs describe, and their
// ratio is precisely what Odd/Even means -- so that knob is solved rather than searched for.
//
// Splitting by parity is also what keeps the solve honest. As one symmetric 2x2 it is the same arithmetic, but
// its determinant is a difference of two nearly equal products, and the weights here span eight orders of
// magnitude: a curve reaching only a harmonic or two above the fundamental lost every significant digit of it
// and came back claiming a residual of zero, which then beat every real candidate in the search.
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

  // A basis with nothing on one parity says nothing about the balance between them, and a curve that thin is
  // not one the knobs are trying to draw.
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
  best.knobs = LevelMacroKnobs{knobs.width, knobs.shape, knobs.fund, oddScale / totalScale};
}

inline void EvaluateLevelMacroCandidate(const MacroFitTarget& target, const LevelMacroKnobs& knobs, LevelMacroCurve& display,
                                        OscillatorParameterValues& basis, LevelMacroFitResult& best) {
  FillLevelMacroDisplayCurve(GetLevelMacroModel(knobs), display);
  FillLevelMacroBasis(display, basis);
  AccumulateLevelMacroFit(basis, target, knobs, best);
}

// Where the search starts on Width. The end of the curve is the one feature that can be read straight off the
// target instead of searched for -- it is just the last harmonic still audible -- and it has to be, because the
// residual is far sharper in Width than in the other two. Everything past the end is exactly silent, so a
// candidate reaching even one harmonic too far puts sound where the target has none and the relative weighting
// charges full price for it. That is a cliff rather than a slope, and a blind grid samples across it instead of
// walking down it.
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

// Shape and Fund are searched blind over their whole travel, because neither has a cliff in it: both only bend
// harmonics that are already there. Width is searched in a band around its measurement, wide enough that the
// measurement can be several harmonics out without the answer falling outside it.
inline constexpr int    kLevelMacroGridSteps = 8;    // Nine samples an axis, so the grid lands within a sixteenth of the travel.
inline constexpr double kLevelMacroWidthBand = 0.08; // Eight harmonics either side of the measurement.

inline std::array<MacroFitAxis, kNumLevelMacroSearchKnobs> GetLevelMacroFitAxes(double widthSeed) {
  return {{{std::max(0.0, widthSeed - kLevelMacroWidthBand), std::min(1.0, widthSeed + kLevelMacroWidthBand)}, {0.0, 1.0}, {0.0, 1.0}}};
}

// The searched knobs as a point, in the order the knob enum lists them.
using LevelMacroPoint = MacroFitPoint<kNumLevelMacroSearchKnobs>;

inline LevelMacroKnobs MakeLevelMacroKnobs(const LevelMacroPoint& point) {
  LevelMacroKnobs knobs;
  for (std::size_t axis = 0; axis < kNumLevelMacroSearchKnobs; ++axis) SetLevelMacroSearchKnob(knobs, axis, std::clamp(point[axis], 0.0, 1.0));
  return knobs;
}

inline LevelMacroPoint MakeLevelMacroPoint(const LevelMacroKnobs& knobs) {
  LevelMacroPoint point{};
  for (std::size_t axis = 0; axis < kNumLevelMacroSearchKnobs; ++axis) point[axis] = GetLevelMacroSearchKnob(knobs, axis);
  return point;
}

// Width starts a few harmonics wide because a step of it moves harmonics on and off the end of the series, where
// Shape and Fund only bend what is already there; those two start wide enough to cross a grid cell and find a
// neighbouring basin. The restarts matter as much as the step sizes: a single simplex stalls in the steep valley
// and left the worst factory patch at 0.168 where restarting from where it stopped gets 0.080.
inline constexpr LevelMacroPoint kLevelMacroSimplexSteps{{0.04, 0.18, 0.18}};
inline constexpr int kLevelMacroSimplexRuns = 4;

// A coarse grid to find the basin, then a simplex to sharpen inside it. Scoring is one lambda shared by both,
// and it holds everything Level-specific about the fit: a point becomes knobs, the knobs a curve, and the curve
// a residual with the parity scales solved out. It keeps its own best rather than handing it back, because the
// solved Odd/Even arrives with the winning candidate and a search that knows nothing of knobs has nowhere to
// put it.
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

// The knob controls are the only storage the macro values have, so the generator and the fit are closures over
// them: read the four positions out, or push four positions in.
inline void RegisterLevelMacroFunctions(const std::shared_ptr<EditorContext>& context, const std::vector<layout::LabelledKnob*>& knobControls) {
  if (knobControls.size() != static_cast<std::size_t>(kNumLevelMacroKnobs)) return;

  auto& functions = (*context->oscillatorTabControls.macroFunctions)[static_cast<std::size_t>(OscillatorParameter::level)];

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

  auto* yTransformControl = CreateYTransformControl(context->levelTab.levelTransform, sliderControl, styles);

  auto* setShapeControl =
      new ActionSelectionControl(IRECT(), "choose shape", {"sine", "saw", "square", "triangle", "flat", "octaves", "octaves+fifths", "octaves+fifths+thirds"},
                                 styles.utilityDropdownText, styles.darkTab);
  setShapeControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if (!selectedText) return;

    context->ApplyOscillatorParameterActionToSelectedKeyNote(sliderControl, OscillatorParameter::level,
                                                             [selectedText](SimplePatch& patch) { return ApplyLevelShape(patch, selectedText); });
  });

  auto* actionsControl = new ActionSelectionControl(IRECT(), "run action",
                                                    {kActionScaleUpMenuLabel, kActionScaleDownMenuLabel, kActionTowardMaxMenuLabel, kActionAwayFromMaxMenuLabel,
                                                     kActionBendUpMenuLabel, kActionBendDownMenuLabel, kActionNormalizeMenuLabel},
                                                    styles.utilityDropdownText, styles.darkTab);
  actionsControl->SetOnSelection([context, sliderControl](const char* selectedText) {
    if (!selectedText) return;

    const bool applyEditScope = !MatchesActionLabel(selectedText, kActionNormalize);
    context->ApplyOscillatorParameterActionToSelectedKeyNote(
        sliderControl, OscillatorParameter::level,
        [selectedText, context](SimplePatch& patch) {
          return ApplyLevelAction(patch, selectedText, context->GetOscillatorEditScope(OscillatorParameter::level));
        },
        applyEditScope);
  });

  *context->levelTab.setShapeControl = setShapeControl;
  *context->levelTab.actionsControl = actionsControl;

  AttachHarmonicTabChildren(page, context, styles, descriptor, xRangeControls, yTransformControl, setShapeControl, actionsControl, allKeyNotesControls,
                            restoreButton, addButton, deleteButton, sliderControl);

  RegisterLevelMacroFunctions(context, AttachMacroKnobChildren(page, context, descriptor, GetLevelMacroKnobDescriptors()));
}
} // namespace editor
} // namespace plugin_ui

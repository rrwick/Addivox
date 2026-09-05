#pragma once

#include "../../dsp/shared.h"
#include "common.h"

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
// The curve is  h^a * exp(-b*h)  weighted for odd/even balance and tapered to silence at the top, then
// normalised. See the knob mappings below for what each knob drives.

// The knob controls come back in descriptor order, and these name the positions so that the descriptor table,
// the reader and the writer cannot drift apart.
enum LevelMacroKnobIndex { kLevelBrightnessKnob, kLevelRolloffKnob, kLevelOddEvenKnob, kLevelTaperKnob, kNumLevelMacroKnobs };

inline constexpr double kLevelBrightnessDefault = 0.50;
inline constexpr double    kLevelRolloffDefault = 0.60;
inline constexpr double    kLevelOddEvenDefault = 0.50;
inline constexpr double      kLevelTaperDefault = 0.15;

// Brightness is the exponent directly. The factory patches fit between -1.00 (Simple Saw) and 2.20 (Bright
// Brass C1), so the range covers them with a little headroom at the top.
inline constexpr double kLevelExponentMin = -1.0;
inline constexpr double kLevelExponentMax =  3.0;

// Rolloff is dialled as reach -- the harmonic at which the exponential has decayed to 1/e -- because that is
// what spreads the factory patches across the knob. Decaying it geometrically keeps the knob's feel even.
inline constexpr double kLevelReachHarmonicsMin =   1.5;
inline constexpr double kLevelReachHarmonicsMax = 150.0;

// Taper onset, in harmonics. At the top of the range the taper only touches h100, so the knob has a genuine
// "off" position; at the bottom it shapes most of the series.
inline constexpr double kLevelTaperOnsetMin =  10.0;
inline constexpr double kLevelTaperOnsetMax = 100.0;

inline constexpr double kLevelMacroEpsilon = 1.0e-12;

// Normalised 0..1 knob positions: exactly what the knob controls hold, and the only place macro values live.
struct LevelMacroKnobs {
  double brightness{kLevelBrightnessDefault};
  double rolloff{kLevelRolloffDefault};
  double oddEven{kLevelOddEvenDefault};
  double taper{kLevelTaperDefault};
};

// The same four values in the units the formula is written in.
struct LevelMacroModel {
  double exponent{0.0};      // a
  double decay{0.0};         // b
  double oddEvenWeight{0.0}; // -1 all even, 0 balanced, +1 all odd
  double taperOnset{0.0};    // First harmonic the taper touches
};

inline double GetLevelMacroTaperOnset(double taperKnob) {
  return kLevelTaperOnsetMax - (std::clamp(taperKnob, 0.0, 1.0) * (kLevelTaperOnsetMax - kLevelTaperOnsetMin));
}

inline LevelMacroModel GetLevelMacroModel(const LevelMacroKnobs& knobs) {
  const double reach = kLevelReachHarmonicsMax * std::pow(kLevelReachHarmonicsMin / kLevelReachHarmonicsMax, std::clamp(knobs.rolloff, 0.0, 1.0));

  LevelMacroModel model;
  model.exponent = kLevelExponentMin + (std::clamp(knobs.brightness, 0.0, 1.0) * (kLevelExponentMax - kLevelExponentMin));
  model.decay = 1.0 / reach;
  model.oddEvenWeight = (std::clamp(knobs.oddEven, 0.0, 1.0) * 2.0) - 1.0;
  model.taperOnset = GetLevelMacroTaperOnset(knobs.taper);
  return model;
}

// Harmonic numbers and their logarithms, so that h^a * exp(-b*h) is one exp per harmonic rather than a pow and
// an exp. The fit builds the basis some twenty thousand times per call, which is where that matters.
struct LevelMacroHarmonics {
  std::array<double, SimplePatch::kNumOscillators> number{};
  std::array<double, SimplePatch::kNumOscillators> logNumber{};

  LevelMacroHarmonics() {
    for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
      const auto index = static_cast<std::size_t>(oscillatorIndex);
      number[index] = static_cast<double>(oscillatorIndex + 1);
      logNumber[index] = std::log(number[index]);
    }
  }
};

inline const LevelMacroHarmonics& GetLevelMacroHarmonics() {
  static const LevelMacroHarmonics harmonics;
  return harmonics;
}

// A raised cosine falling from 1 at the onset to 0 one harmonic past the top of the series. Tabulated rather
// than evaluated per harmonic because the fit varies the onset far less often than the other two parameters.
inline void FillLevelMacroTaper(double taperOnset, std::array<double, SimplePatch::kNumOscillators>& taper) {
  const auto& harmonics = GetLevelMacroHarmonics();
  const double taperEnd = static_cast<double>(SimplePatch::kNumOscillators) + 1.0;

  for (std::size_t index = 0; index < taper.size(); ++index) {
    const double harmonicNumber = harmonics.number[index];
    taper[index] = (harmonicNumber <= taperOnset || taperOnset >= taperEnd)
                       ? 1.0
                       : (0.5 * (1.0 + std::cos(dsp::kPi * (harmonicNumber - taperOnset) / (taperEnd - taperOnset))));
  }
}

// The curve before odd/even weighting and normalisation. Kept separate because the fit holds this fixed while
// it solves for the weighting.
inline void FillLevelMacroBasis(const LevelMacroModel& model, const std::array<double, SimplePatch::kNumOscillators>& taper,
                                OscillatorParameterValues& basis) {
  const auto& harmonics = GetLevelMacroHarmonics();

  for (std::size_t index = 0; index < basis.size(); ++index)
    basis[index] = std::exp((model.exponent * harmonics.logNumber[index]) - (model.decay * harmonics.number[index])) * taper[index];
}

inline double GetLevelOddEvenSign(int oscillatorIndex) { return IsOddHarmonic(oscillatorIndex) ? 1.0 : -1.0; }

// Every harmonic in phase makes the rendered waveform peak at the sum of the levels, so the sum is what gets
// pinned to 1: at full breath the worst case then reaches full scale and no further. This is a quieter
// convention than the RMS normalisation the shape presets and the factory patches use, which can clip there.
inline OscillatorParameterValues GenerateLevelMacroCurve(const LevelMacroKnobs& knobs) {
  const LevelMacroModel model = GetLevelMacroModel(knobs);

  std::array<double, SimplePatch::kNumOscillators> taper{};
  FillLevelMacroTaper(model.taperOnset, taper);

  OscillatorParameterValues values{};
  FillLevelMacroBasis(model, taper, values);

  double total = 0.0;
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    double& value = values[static_cast<std::size_t>(oscillatorIndex)];
    value = std::max(0.0, value * (1.0 + (model.oddEvenWeight * GetLevelOddEvenSign(oscillatorIndex))));
    total += value;
  }

  if (total <= kLevelMacroEpsilon) {
    values.fill(0.0);
    values[0] = 1.0; // A degenerate curve still has to be audible, and a sine is the honest fallback.
    return values;
  }

  for (auto& value : values) value /= total;

  return values;
}

// The curve to fit, plus a per-harmonic weight of 1 / (value + floor)^2. Unweighted least squares is blind to
// the top of the series -- the harmonics up there are a thousandth of the fundamental, so a taper that is
// wildly wrong costs almost nothing -- yet those are exactly the harmonics the taper knob exists to control,
// and pseudo-log display makes them half the chart. Relative error weights every harmonic about equally.
struct LevelMacroFitTarget {
  OscillatorParameterValues values{};
  OscillatorParameterValues weights{};
};

inline LevelMacroFitTarget MakeLevelMacroFitTarget(const OscillatorParameterValues& values) {
  constexpr double kRelativeFloor = 1.0e-3; // Harmonics more than 60 dB below the peak stop pulling on the fit.

  double peak = 0.0;
  for (const double value : values) peak = std::max(peak, value);

  const double floorValue = std::max(peak * kRelativeFloor, kLevelMacroEpsilon);

  LevelMacroFitTarget target;
  target.values = values;
  for (std::size_t i = 0; i < values.size(); ++i) {
    const double scale = 1.0 / (values[i] + floorValue);
    target.weights[i] = scale * scale;
  }

  return target;
}

struct LevelMacroFitResult {
  LevelMacroKnobs knobs{};
  double residual{std::numeric_limits<double>::max()};
};

// One axis of the fit search. steps is the number of intervals, so it takes steps + 1 samples.
struct LevelMacroFitRange {
  double min{0.0};
  double max{1.0};
  int steps{1};

  double At(int step) const { return min + ((max - min) * (static_cast<double>(step) / static_cast<double>(std::max(steps, 1)))); }
};

// With brightness, rolloff and taper fixed, the target is  scale * basis * (1 + weight * sign), which is
// weighted linear least squares in (scale, scale * weight) and solves in closed form. Leaving the scale free
// means the fit reads the curve's shape and ignores its loudness, which is what the knobs describe.
inline void AccumulateLevelMacroFit(const OscillatorParameterValues& basis, const LevelMacroFitTarget& target, const LevelMacroKnobs& knobs,
                                    LevelMacroFitResult& best) {
  double basisSquared = 0.0;
  double signedBasisSquared = 0.0;
  double targetDotBasis = 0.0;
  double targetDotSignedBasis = 0.0;
  double targetSquared = 0.0;

  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    const auto index = static_cast<std::size_t>(oscillatorIndex);
    const double basisValue = basis[index];
    const double targetValue = target.values[index];
    const double weight = target.weights[index];
    const double sign = GetLevelOddEvenSign(oscillatorIndex);

    // The signs are +/-1, so the weighted basis and the weighted signed basis share the same squared sum and
    // the normal equations stay a symmetric 2x2 that solves in closed form.
    basisSquared += weight * basisValue * basisValue;
    signedBasisSquared += weight * basisValue * basisValue * sign;
    targetDotBasis += weight * targetValue * basisValue;
    targetDotSignedBasis += weight * targetValue * basisValue * sign;
    targetSquared += weight * targetValue * targetValue;
  }

  const double determinant = (basisSquared * basisSquared) - (signedBasisSquared * signedBasisSquared);
  if (basisSquared <= kLevelMacroEpsilon || std::fabs(determinant) <= kLevelMacroEpsilon) return;

  const double scale = ((basisSquared * targetDotBasis) - (signedBasisSquared * targetDotSignedBasis)) / determinant;
  const double scaledWeight = ((basisSquared * targetDotSignedBasis) - (signedBasisSquared * targetDotBasis)) / determinant;
  if (scale <= kLevelMacroEpsilon) return;

  const double residual = std::max(0.0, targetSquared - ((scale * targetDotBasis) + (scaledWeight * targetDotSignedBasis)));
  if (residual >= best.residual) return;

  const double oddEvenWeight = std::clamp(scaledWeight / scale, -1.0, 1.0);
  best.residual = residual;
  best.knobs = LevelMacroKnobs{knobs.brightness, knobs.rolloff, (oddEvenWeight + 1.0) * 0.5, knobs.taper};
}

inline void SweepLevelMacroFit(const LevelMacroFitTarget& target, const LevelMacroFitRange& brightnessRange, const LevelMacroFitRange& rolloffRange,
                               const LevelMacroFitRange& taperRange, LevelMacroFitResult& best) {
  std::array<double, SimplePatch::kNumOscillators> taper{};
  OscillatorParameterValues basis{};

  // Taper outermost, because it is the only parameter the taper table depends on: this builds one table per
  // taper step rather than one per candidate, which is the difference between a handful of cosine passes and
  // twenty thousand.
  for (int taperStep = 0; taperStep <= taperRange.steps; ++taperStep) {
    const double taperValue = taperRange.At(taperStep);
    FillLevelMacroTaper(GetLevelMacroTaperOnset(taperValue), taper);

    for (int brightnessStep = 0; brightnessStep <= brightnessRange.steps; ++brightnessStep) {
      for (int rolloffStep = 0; rolloffStep <= rolloffRange.steps; ++rolloffStep) {
        const LevelMacroKnobs knobs{brightnessRange.At(brightnessStep), rolloffRange.At(rolloffStep), kLevelOddEvenDefault, taperValue};

        FillLevelMacroBasis(GetLevelMacroModel(knobs), taper, basis);
        AccumulateLevelMacroFit(basis, target, knobs, best);
      }
    }
  }
}

inline LevelMacroFitRange NarrowLevelMacroFitRange(const LevelMacroFitRange& range, double value, int steps) {
  const double halfWidth = (range.max - range.min) / static_cast<double>(std::max(range.steps, 1));
  return {std::max(0.0, value - halfWidth), std::min(1.0, value + halfWidth), steps};
}

// A coarse sweep followed by one pass over the winning cell. This runs on entering Macro mode and on changing
// key note, never per frame, so a search is cheaper to understand than a gradient method and no less accurate.
inline LevelMacroKnobs FitLevelMacroKnobs(const OscillatorParameterValues& values) {
  constexpr LevelMacroFitRange kCoarseBrightnessRange{0.0, 1.0, 40};
  constexpr LevelMacroFitRange    kCoarseRolloffRange{0.0, 1.0, 40};
  constexpr LevelMacroFitRange      kCoarseTaperRange{0.0, 1.0, 10};
  constexpr int kRefineSteps = 8;

  const LevelMacroFitTarget target = MakeLevelMacroFitTarget(values);

  LevelMacroFitResult best;
  SweepLevelMacroFit(target, kCoarseBrightnessRange, kCoarseRolloffRange, kCoarseTaperRange, best);

  if (best.residual == std::numeric_limits<double>::max()) return LevelMacroKnobs{};

  SweepLevelMacroFit(target, NarrowLevelMacroFitRange(kCoarseBrightnessRange, best.knobs.brightness, kRefineSteps),
                     NarrowLevelMacroFitRange(kCoarseRolloffRange, best.knobs.rolloff, kRefineSteps),
                     NarrowLevelMacroFitRange(kCoarseTaperRange, best.knobs.taper, kRefineSteps), best);

  return best.knobs;
}

inline std::vector<MacroKnobDescriptor> GetLevelMacroKnobDescriptors() {
  return {{"Bright", help_text::oscillator_tabs::kMacroLevelBrightness, kLevelBrightnessDefault, false},
          {"Rolloff", help_text::oscillator_tabs::kMacroLevelRolloff, kLevelRolloffDefault, false},
          {"Odd/Even", help_text::oscillator_tabs::kMacroLevelOddEven, kLevelOddEvenDefault, true},
          {"Taper", help_text::oscillator_tabs::kMacroLevelTaper, kLevelTaperDefault, false}};
}

// The knob controls are the only storage the macro values have, so the generator and the fit are closures over
// them: read four positions out, or push four positions in.
inline void RegisterLevelMacroFunctions(const std::shared_ptr<EditorContext>& context, const std::vector<layout::LabelledKnob*>& knobControls) {
  if (knobControls.size() != static_cast<std::size_t>(kNumLevelMacroKnobs)) return;

  auto& functions = (*context->oscillatorTabControls.macroFunctions)[static_cast<std::size_t>(OscillatorParameter::level)];

  functions.generateValues = [knobControls]() {
    return GenerateLevelMacroCurve({knobControls[kLevelBrightnessKnob]->GetNormalizedValue(), knobControls[kLevelRolloffKnob]->GetNormalizedValue(),
                                    knobControls[kLevelOddEvenKnob]->GetNormalizedValue(), knobControls[kLevelTaperKnob]->GetNormalizedValue()});
  };

  functions.fitKnobsToValues = [knobControls](const OscillatorParameterValues& values) {
    const LevelMacroKnobs knobs = FitLevelMacroKnobs(values);

    knobControls[kLevelBrightnessKnob]->SetNormalizedValueSilently(knobs.brightness);
    knobControls[kLevelRolloffKnob]->SetNormalizedValueSilently(knobs.rolloff);
    knobControls[kLevelOddEvenKnob]->SetNormalizedValueSilently(knobs.oddEven);
    knobControls[kLevelTaperKnob]->SetNormalizedValueSilently(knobs.taper);
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

#include "oscillator.h"

#include <algorithm>
#include <cmath>

namespace {
using Parameter = OscillatorSettings::Parameter;

struct ParameterDescriptor {
  double OscillatorSettings::* member;
  double min;
  double max;
};

// Bounds mirror the ranges the UI sliders for these parameters allow (see
// ui/editor_tabs/{level,breath,attack_release,pitch,pan,variation}.h).
constexpr std::array<ParameterDescriptor, OscillatorSettings::kNumParameters> kParameterDescriptors{{
    {&OscillatorSettings::level,                        0.0,    1.0},
    {&OscillatorSettings::breath_power,                 0.0,  100.0},
    {&OscillatorSettings::attack,                       0.0,    1.0},
    {&OscillatorSettings::release,                      0.0,    1.0},
    {&OscillatorSettings::pitch,                    -2400.0, 2400.0},
    {&OscillatorSettings::pan,                         -1.0,    1.0},
    {&OscillatorSettings::level_variation_amplitude,    0.0,   10.0},
    {&OscillatorSettings::level_variation_rate,         0.0,   10.0},
    {&OscillatorSettings::pitch_variation_amplitude,    0.0,   10.0},
    {&OscillatorSettings::pitch_variation_rate,         0.0,   10.0},
    {&OscillatorSettings::pan_variation_amplitude,      0.0,   10.0},
    {&OscillatorSettings::pan_variation_rate,           0.0,   10.0},
}};

const ParameterDescriptor* GetDescriptor(Parameter parameter) {
  const int index = static_cast<int>(parameter);
  if (index < 0 || index >= OscillatorSettings::kNumParameters) return nullptr;

  return &kParameterDescriptors[static_cast<std::size_t>(index)];
}

// Same fixed pseudo-log curve as the Level editor; the midpoint between 0 and 1 is 0.01.
constexpr double kLevelCurveShape = 9.19023970026918;
constexpr double kLevelCurveScale = 9800.0;
constexpr int kLevelTableIntervals = 4096;
// Built before playback. Linear lookup error is below 6.4e-7 relative to (level + 1/9800).
const auto kLevelTable = [] {
  std::array<double, kLevelTableIntervals + 1> table{};
  for (int i = 0; i <= kLevelTableIntervals; ++i)
    table[i] = std::expm1(kLevelCurveShape * i / kLevelTableIntervals) / kLevelCurveScale;
  table.back() = 1.0;
  return table;
}();

double Lerp(double lo, double hi, double t) { return lo + (hi - lo) * t; }

using OscillatorParameterValues = CompoundPatch::OscillatorParameterValues;

OscillatorParameterValues GetParameterValues(const SimplePatch& patch, Parameter parameter) {
  OscillatorParameterValues values{};
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    values[static_cast<std::size_t>(oscillatorIndex)] = patch.GetOscillatorSettings(oscillatorIndex).GetParameter(parameter);
  }

  return values;
}

OscillatorParameterValues SanitizeParameterValues(Parameter parameter, const OscillatorParameterValues& values) {
  OscillatorParameterValues sanitized{};
  for (std::size_t i = 0; i < values.size(); ++i) sanitized[i] = OscillatorSettings::SanitizeParameter(parameter, values[i]);

  return sanitized;
}

void SetParameterValues(SimplePatch& patch, OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values) {
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    patch.SetOscillatorParameter(oscillatorIndex, parameter, values[static_cast<std::size_t>(oscillatorIndex)]);
  }
}

constexpr double kLevelWaveformRmsEpsilon = 1.0e-12;

// A sine sum is antisymmetric, so half a cycle contains its full absolute peak. The table is built once,
// off the audio path; harmonic-major storage makes each waveform accumulation contiguous.
constexpr int kLevelPeakCycleSamples = 4096;
constexpr double kLevelPeakPhaseStep = 6.28318530717958647692 / kLevelPeakCycleSamples;
using LevelPeakWaveform = std::array<double, kLevelPeakCycleSamples / 2 + 1>;

const auto& GetLevelPeakSines() {
  static const auto sines = [] {
    std::array<LevelPeakWaveform, SimplePatch::kNumOscillators> table{};
    for (int harmonic = 0; harmonic < SimplePatch::kNumOscillators; ++harmonic) {
      for (std::size_t sample = 0; sample < table[harmonic].size(); ++sample)
        table[harmonic][sample] = std::sin(kLevelPeakPhaseStep * static_cast<double>(sample) * (harmonic + 1));
    }
    return table;
  }();
  return sines;
}

double GetLevelWaveformPeakBound(const SimplePatch::LevelArray& levels) {
  const auto& sines = GetLevelPeakSines();
  LevelPeakWaveform waveform{};
  double curvatureBound = 0.0;
  for (int harmonic = 0; harmonic < SimplePatch::kNumOscillators; ++harmonic) {
    const double level = levels[harmonic];
    if (level == 0.0) continue;
    const double frequency = harmonic + 1;
    curvatureBound += std::abs(level) * frequency * frequency;
    for (std::size_t sample = 0; sample < waveform.size(); ++sample) waveform[sample] += level * sines[harmonic][sample];
  }

  double peak = 0.0;
  for (const double sample : waveform) peak = std::max(peak, std::abs(sample));
  // Linear interpolation differs from the continuous waveform by at most max|f''| * step^2 / 8.
  // Including that bound protects peaks between samples without an iterative peak search.
  return peak + curvatureBound * kLevelPeakPhaseStep * kLevelPeakPhaseStep / 8.0;
}

const SimplePatch& GetDefaultPatch() {
  static const SimplePatch patch = [] {
    SimplePatch::OscillatorArray oscillatorSettings{};
    oscillatorSettings.fill(OscillatorSettings{0.0});
    oscillatorSettings[0] = OscillatorSettings{1.0};
    return SimplePatch{oscillatorSettings};
  }();

  return patch;
}

const EqCurve& GetDefaultEqCurve() {
  static const EqCurve curve{};
  return curve;
}
} // namespace

double OscillatorSettings::GetParameter(Parameter parameter) const {
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? this->*(descriptor->member) : 0.0;
}

void OscillatorSettings::SetParameter(Parameter parameter, double value) {
  const auto* descriptor = GetDescriptor(parameter);
  if (descriptor) this->*(descriptor->member) = SanitizeParameter(parameter, value);
}

double OscillatorSettings::SanitizeParameter(Parameter parameter, double value) {
  const auto* descriptor = GetDescriptor(parameter);
  if (!descriptor || !std::isfinite(value)) return 0.0;

  return std::clamp(value, descriptor->min, descriptor->max);
}

OscillatorSettings SimplePatch::InterpolateOscillatorSettings(const SimplePatch& hi, int oscillatorIndex, double t) const {
  const int index = ClampOscillatorIndex(oscillatorIndex);
  const auto& lower = mOscillatorSettings[index];
  const auto& upper = hi.mOscillatorSettings[index];
  if (t <= 0.0) return lower;
  if (t >= 1.0) return upper;
  OscillatorSettings out{};
  for (const auto& descriptor : kParameterDescriptors)
    out.*(descriptor.member) = Lerp(lower.*(descriptor.member), upper.*(descriptor.member), t);

  // Preserve identical levels exactly, including silence and "All notes".
  if (lower.level == upper.level) out.level = lower.level;
  else {
    const double position = std::clamp(Lerp(mLevelCoordinates[index], hi.mLevelCoordinates[index], t), 0.0, 1.0) * kLevelTableIntervals;
    const int cell = std::min(static_cast<int>(position), kLevelTableIntervals - 1);
    out.level = std::clamp(Lerp(kLevelTable[cell], kLevelTable[cell + 1], position - cell),
                           std::min(lower.level, upper.level), std::max(lower.level, upper.level));
  }
  return out;
}

SimplePatch::SimplePatch(const OscillatorArray& oscillatorSettings) : mOscillatorSettings(oscillatorSettings) { UpdateLevelCoordinates(); }

void SimplePatch::UpdateLevelCoordinates() {
  for (int harmonic = 0; harmonic < kNumOscillators; ++harmonic)
    mLevelCoordinates[harmonic] = std::log1p(kLevelCurveScale * mOscillatorSettings[harmonic].level) / kLevelCurveShape;
}

int SimplePatch::ClampOscillatorIndex(int oscillatorIndex) { return std::clamp(oscillatorIndex, 0, kNumOscillators - 1); }

const OscillatorSettings& SimplePatch::GetOscillatorSettings(int oscillatorIndex) const { return mOscillatorSettings[ClampOscillatorIndex(oscillatorIndex)]; }

const SimplePatch::OscillatorArray& SimplePatch::GetOscillatorSettingsArray() const { return mOscillatorSettings; }

void SimplePatch::SetOscillatorParameter(int oscillatorIndex, OscillatorSettings::Parameter parameter, double value) {
  const int index = ClampOscillatorIndex(oscillatorIndex);
  mOscillatorSettings[index].SetParameter(parameter, value);
  if (parameter == Parameter::level)
    mLevelCoordinates[index] = std::log1p(kLevelCurveScale * mOscillatorSettings[index].level) / kLevelCurveShape;
}

bool SimplePatch::NormalizeLevels(LevelArray& levels) {
  double sumSquares = 0.0;
  double sumAbsolute = 0.0;
  for (const double level : levels) {
    sumSquares += level * level;
    sumAbsolute += std::abs(level);
  }
  const double rms = std::sqrt(sumSquares * 0.5);
  if (rms <= kLevelWaveformRmsEpsilon) return false;

  double scale = kReferenceLevelWaveformRms / rms;
  // Sparse, quiet spectra can be proven safe without scanning the waveform.
  if (sumAbsolute * scale > kLevelWaveformPeak) scale = std::min(scale, kLevelWaveformPeak / GetLevelWaveformPeakBound(levels));
  for (double& level : levels) level *= scale;
  return true;
}

bool SimplePatch::NormalizeLevelWaveformRms() {
  LevelArray levels{};
  for (int harmonic = 0; harmonic < kNumOscillators; ++harmonic) levels[harmonic] = mOscillatorSettings[harmonic].level;
  if (!NormalizeLevels(levels)) return false;
  for (int harmonic = 0; harmonic < kNumOscillators; ++harmonic) mOscillatorSettings[harmonic].level = levels[harmonic];
  UpdateLevelCoordinates();
  return true;
}

SimplePatch SimplePatch::Interpolate(const SimplePatch& lo, const SimplePatch& hi, double t) {
  OscillatorArray out{};
  for (int oscillator = 0; oscillator < kNumOscillators; ++oscillator) {
    out[oscillator] = lo.InterpolateOscillatorSettings(hi, oscillator, t);
  }
  return SimplePatch{out};
}

int CompoundPatch::ClampMidiNote(int midiNote) { return std::clamp(midiNote, kMinMidiNote, kMaxMidiNote); }

int CompoundPatch::RoundAndClampMidiNote(double midiNote) { return ClampMidiNote(static_cast<int>(std::lround(midiNote))); }

std::size_t CompoundPatch::ParameterIndex(OscillatorSettings::Parameter parameter) { return static_cast<std::size_t>(parameter); }

CompoundPatch::CompoundPatch() : CompoundPatch({}) {}

CompoundPatch::CompoundPatch(std::initializer_list<KeyNotePatch> keyNotePatches) {
  for (const auto& [midiNote, patch] : keyNotePatches) {
    mKeyNotePatches[ClampMidiNote(midiNote)] = patch;
    mKeyNoteEqCurves[ClampMidiNote(midiNote)] = GetDefaultEqCurve();
  }
}

SimplePatch CompoundPatch::GetPatchForMidiNote(double midiNote) const {
  const ResolvedNoteSpan span = ResolveNoteSpan(midiNote);
  if (span.t <= 0.0 || span.lowerPatch == span.upperPatch) return *span.lowerPatch;

  return SimplePatch::Interpolate(*span.lowerPatch, *span.upperPatch, span.t);
}

const SimplePatch* CompoundPatch::GetKeyNotePatch(double midiNote) const {
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  const auto keyNoteIt = mKeyNotePatches.find(clampedNote);
  if (keyNoteIt == mKeyNotePatches.end()) return nullptr;

  return &keyNoteIt->second;
}

EqCurve CompoundPatch::GetEqCurveForMidiNote(double midiNote) const {
  const ResolvedNoteSpan span = ResolveNoteSpan(midiNote);
  if (span.t <= 0.0 || span.lowerEqCurve == span.upperEqCurve) return *span.lowerEqCurve;

  return EqCurve::FromResponseLut(EqCurve::InterpolateResponseLut(span.lowerEqCurve->BuildResponseLut(), span.upperEqCurve->BuildResponseLut(), span.t));
}

const EqCurve* CompoundPatch::GetKeyNoteEqCurve(double midiNote) const {
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  const auto keyNoteIt = mKeyNoteEqCurves.find(clampedNote);
  if (keyNoteIt == mKeyNoteEqCurves.end()) return nullptr;

  return &keyNoteIt->second;
}

const std::map<int, SimplePatch>& CompoundPatch::GetKeyNotePatches() const { return mKeyNotePatches; }

bool CompoundPatch::IsAllKeyNotesEnabled(OscillatorSettings::Parameter parameter) const { return mAllKeyNotesEnabled[ParameterIndex(parameter)]; }

const CompoundPatch::OscillatorParameterValues& CompoundPatch::GetAllKeyNotesValues(OscillatorSettings::Parameter parameter) const {
  return mAllKeyNotesValues[ParameterIndex(parameter)];
}

bool CompoundPatch::IsAllKeyNotesEqEnabled() const { return mAllKeyNotesEqEnabled; }

const EqCurve& CompoundPatch::GetAllKeyNotesEqCurve() const { return mAllKeyNotesEqCurve; }

CompoundPatch::ResolvedNoteSpan CompoundPatch::ResolveNoteSpan(double midiNote) const {
  const EqCurve& defaultEqCurve = IsAllKeyNotesEqEnabled() ? GetAllKeyNotesEqCurve() : GetDefaultEqCurve();
  if (mKeyNotePatches.empty()) {
    return ResolvedNoteSpan{&GetDefaultPatch(), &GetDefaultPatch(), &defaultEqCurve, &defaultEqCurve, 0.0};
  }

  const double clampedMidiNote = std::clamp(midiNote, static_cast<double>(kMinMidiNote), static_cast<double>(kMaxMidiNote));
  auto makeExactSpan = [&](const std::map<int, SimplePatch>::const_iterator& it) {
    const EqCurve& eqCurve = GetKeyNoteEqCurveOrDefault(it->first);
    return ResolvedNoteSpan{&it->second, &it->second, &eqCurve, &eqCurve, 0.0};
  };

  auto upper = mKeyNotePatches.lower_bound(static_cast<int>(std::ceil(clampedMidiNote)));
  if (upper == mKeyNotePatches.begin()) return makeExactSpan(upper);

  if (upper == mKeyNotePatches.end()) return makeExactSpan(std::prev(mKeyNotePatches.end()));

  if (static_cast<double>(upper->first) == clampedMidiNote) return makeExactSpan(upper);

  auto lower = std::prev(upper);
  const double interval = static_cast<double>(upper->first - lower->first);

  const EqCurve& lowerEqCurve = GetKeyNoteEqCurveOrDefault(lower->first);
  const EqCurve& upperEqCurve = GetKeyNoteEqCurveOrDefault(upper->first);
  return ResolvedNoteSpan{&lower->second, &upper->second, &lowerEqCurve, &upperEqCurve, (clampedMidiNote - static_cast<double>(lower->first)) / interval};
}

OscillatorSettings CompoundPatch::InterpolateOscillatorSettings(const ResolvedNoteSpan& span, int oscillatorIndex) const {
  const OscillatorSettings& lowerSettings = span.lowerPatch->GetOscillatorSettings(oscillatorIndex);
  if (span.t <= 0.0 || span.lowerPatch == span.upperPatch) return lowerSettings;

  return span.lowerPatch->InterpolateOscillatorSettings(*span.upperPatch, oscillatorIndex, span.t);
}

double CompoundPatch::EvaluateEqGain(const ResolvedNoteSpan& span, double frequencyHz) const {
  const double lowerGain = EqCurve::DbToGain(span.lowerEqCurve->EvaluateDb(frequencyHz));
  if (span.t <= 0.0 || span.lowerEqCurve == span.upperEqCurve) return lowerGain;

  return Lerp(lowerGain, EqCurve::DbToGain(span.upperEqCurve->EvaluateDb(frequencyHz)), span.t);
}

bool CompoundPatch::HasKeyNotePatch(double midiNote) const { return GetKeyNotePatch(midiNote) != nullptr; }

int CompoundPatch::GetNumKeyNotePatches() const { return static_cast<int>(mKeyNotePatches.size()); }

bool CompoundPatch::AddKeyNotePatch(double midiNote) {
  const int note = RoundAndClampMidiNote(midiNote);
  if (HasKeyNotePatch(note)) return false;

  TabMacroSettings macros{};
  if (!mKeyNotePatches.empty()) {
    auto upper = mKeyNotePatches.lower_bound(note);
    auto lower = upper == mKeyNotePatches.begin() ? upper : std::prev(upper);
    if (upper == mKeyNotePatches.end()) upper = lower;
    for (auto parameter : OscillatorSettings::AllParameters()) {
      const auto& settings = GetMacroSettings(lower->first, parameter);
      if (settings == GetMacroSettings(upper->first, parameter)) macros[ParameterIndex(parameter)] = settings;
    }
  }

  SetKeyNotePatch(note, GetPatchForMidiNote(midiNote));
  for (auto parameter : OscillatorSettings::AllParameters())
    if (!macros[ParameterIndex(parameter)].positions.empty()) SetMacroSettings(note, parameter, macros[ParameterIndex(parameter)]);
  return true;
}

void CompoundPatch::SetKeyNotePatch(int midiNote, const SimplePatch& patch) {
  const int clampedMidiNote = ClampMidiNote(midiNote);
  const EqCurve* keyNoteEqCurve = GetKeyNoteEqCurve(clampedMidiNote);
  EqCurve eqCurve = keyNoteEqCurve ? *keyNoteEqCurve : GetEqCurveForMidiNote(clampedMidiNote);
  if (IsAllKeyNotesEqEnabled()) eqCurve = GetAllKeyNotesEqCurve();

  SimplePatch updatedPatch = patch;
  ApplyAllKeyNotesValues(updatedPatch);
  mKeyNotePatches[clampedMidiNote] = updatedPatch;
  mKeyNoteMacros.erase(clampedMidiNote);
  mKeyNoteEqCurves[clampedMidiNote] = std::move(eqCurve);
}

bool CompoundPatch::SetKeyNoteOscillatorParameter(double midiNote, int oscillatorIndex, OscillatorSettings::Parameter parameter, double value) {
  const auto keyNote = mKeyNotePatches.find(RoundAndClampMidiNote(midiNote));
  if (keyNote == mKeyNotePatches.end()) return false;

  // SimplePatch::SetOscillatorParameter() clamps internally, but mAllKeyNotesValues below is indexed directly, so clamp here too.
  const int clampedOscillatorIndex = std::clamp(oscillatorIndex, 0, SimplePatch::kNumOscillators - 1);

  const double previous = keyNote->second.GetOscillatorSettings(clampedOscillatorIndex).GetParameter(parameter);
  if (previous != OscillatorSettings::SanitizeParameter(parameter, value)) SetMacroSettings(midiNote, parameter, {});

  if (IsAllKeyNotesEnabled(parameter)) {
    auto& sharedValues = mAllKeyNotesValues[ParameterIndex(parameter)];
    sharedValues[static_cast<std::size_t>(clampedOscillatorIndex)] = OscillatorSettings::SanitizeParameter(parameter, value);
    for (auto& [_, patch] : mKeyNotePatches) patch.SetOscillatorParameter(clampedOscillatorIndex, parameter, value);
  } else
    keyNote->second.SetOscillatorParameter(clampedOscillatorIndex, parameter, value);

  return true;
}

bool CompoundPatch::SetKeyNoteOscillatorParameterValues(double midiNote, OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values,
                                                        const MacroSettings& macros) {
  const auto keyNote = mKeyNotePatches.find(RoundAndClampMidiNote(midiNote));
  if (keyNote == mKeyNotePatches.end()) return false;

  if (IsAllKeyNotesEnabled(parameter)) {
    mAllKeyNotesValues[ParameterIndex(parameter)] = SanitizeParameterValues(parameter, values);
    for (auto& [_, patch] : mKeyNotePatches) SetParameterValues(patch, parameter, values);
  } else
    SetParameterValues(keyNote->second, parameter, values);

  SetMacroSettings(midiNote, parameter, macros);
  return true;
}

const MacroSettings& CompoundPatch::GetMacroSettings(double midiNote, OscillatorSettings::Parameter parameter) const {
  static const MacroSettings empty;
  if (!HasKeyNotePatch(midiNote)) return empty;
  if (IsAllKeyNotesEnabled(parameter)) return mAllKeyNotesMacros[ParameterIndex(parameter)];
  const auto it = mKeyNoteMacros.find(RoundAndClampMidiNote(midiNote));
  return it == mKeyNoteMacros.end() ? empty : it->second[ParameterIndex(parameter)];
}

void CompoundPatch::SetMacroSettings(double midiNote, OscillatorSettings::Parameter parameter, const MacroSettings& macros) {
  if (!HasKeyNotePatch(midiNote)) return;
  if (IsAllKeyNotesEnabled(parameter)) mAllKeyNotesMacros[ParameterIndex(parameter)] = macros;
  else if (!macros.positions.empty() || mKeyNoteMacros.count(RoundAndClampMidiNote(midiNote)))
    mKeyNoteMacros[RoundAndClampMidiNote(midiNote)][ParameterIndex(parameter)] = macros;
}

bool CompoundPatch::SetKeyNoteEqCurve(double midiNote, const EqCurve& curve) {
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  if (mKeyNotePatches.find(clampedNote) == mKeyNotePatches.end()) return false;

  if (IsAllKeyNotesEqEnabled()) {
    mAllKeyNotesEqCurve = curve;
    SetAllKeyNoteEqCurves(mAllKeyNotesEqCurve);
  } else
    mKeyNoteEqCurves[clampedNote] = curve;
  return true;
}

void CompoundPatch::EnableAllKeyNotes(OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values, const MacroSettings& macros) {
  const auto parameterIndex = ParameterIndex(parameter);
  mAllKeyNotesValues[parameterIndex] = SanitizeParameterValues(parameter, values);
  mAllKeyNotesEnabled[parameterIndex] = true;
  mAllKeyNotesMacros[parameterIndex] = macros;

  for (auto& [_, patch] : mKeyNotePatches) SetParameterValues(patch, parameter, values);
}

void CompoundPatch::SetAllKeyNotesEnabled(OscillatorSettings::Parameter parameter, bool enabled, double sourceMidiNote) {
  if (IsAllKeyNotesEnabled(parameter) == enabled) return;
  const auto index = ParameterIndex(parameter);
  if (!enabled) {
    mAllKeyNotesEnabled[index] = false;
    for (const auto& [note, _] : mKeyNotePatches) SetMacroSettings(note, parameter, mAllKeyNotesMacros[index]);
    mAllKeyNotesMacros[index] = {};
    return;
  }

  const int sourceNote =
      HasKeyNotePatch(sourceMidiNote) ? RoundAndClampMidiNote(sourceMidiNote) : (mKeyNotePatches.empty() ? kMinMidiNote : mKeyNotePatches.begin()->first);
  if (const auto* sourcePatch = GetKeyNotePatch(sourceNote))
    EnableAllKeyNotes(parameter, GetParameterValues(*sourcePatch, parameter), GetMacroSettings(sourceNote, parameter));
  else
    mAllKeyNotesEnabled[index] = true;
}

void CompoundPatch::EnableAllKeyNotesEq(const EqCurve& curve) {
  mAllKeyNotesEqCurve = curve;
  mAllKeyNotesEqEnabled = true;
  SetAllKeyNoteEqCurves(mAllKeyNotesEqCurve);
}

void CompoundPatch::SetAllKeyNotesEqEnabled(bool enabled) {
  mAllKeyNotesEqEnabled = enabled;
  if (enabled) {
    if (!mKeyNoteEqCurves.empty()) mAllKeyNotesEqCurve = mKeyNoteEqCurves.begin()->second;
    SetAllKeyNoteEqCurves(mAllKeyNotesEqCurve);
  }
}

bool CompoundPatch::RemoveKeyNotePatch(int midiNote) {
  if (mKeyNotePatches.size() <= 1) return false;

  const int clampedMidiNote = ClampMidiNote(midiNote);
  const size_t numRemoved = mKeyNotePatches.erase(clampedMidiNote);
  mKeyNoteEqCurves.erase(clampedMidiNote);
  mKeyNoteMacros.erase(clampedMidiNote);
  return numRemoved > 0;
}

void CompoundPatch::ClearKeyNotePatches() {
  mKeyNotePatches.clear();
  mKeyNoteEqCurves.clear();
  mKeyNoteMacros.clear();
  mAllKeyNotesMacros = {};
}

void CompoundPatch::ApplyAllKeyNotesValues(SimplePatch& patch) const {
  for (auto parameter : OscillatorSettings::AllParameters()) {
    if (IsAllKeyNotesEnabled(parameter)) SetParameterValues(patch, parameter, GetAllKeyNotesValues(parameter));
  }
}

const EqCurve& CompoundPatch::GetKeyNoteEqCurveOrDefault(int midiNote) const {
  if (IsAllKeyNotesEqEnabled()) return GetAllKeyNotesEqCurve();

  if (const auto eqCurveIt = mKeyNoteEqCurves.find(midiNote); eqCurveIt != mKeyNoteEqCurves.end()) return eqCurveIt->second;

  return GetDefaultEqCurve();
}

void CompoundPatch::SetAllKeyNoteEqCurves(const EqCurve& curve) {
  for (auto& [_, keyNoteCurve] : mKeyNoteEqCurves) keyNoteCurve = curve;
}

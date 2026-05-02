#include "oscillator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
using Parameter = OscillatorSettings::Parameter;
using MemberPtr = double OscillatorSettings::*;

struct ParameterDescriptor {
  const char* name;
  const char* unit;
  MemberPtr member;
};

constexpr std::array<ParameterDescriptor, OscillatorSettings::kNumParameters> kParameterDescriptors{
    {{"Level", "", &OscillatorSettings::level},
     {"Breath power", "", &OscillatorSettings::breath_power},
     {"Attack", "s", &OscillatorSettings::attack},
     {"Release", "s", &OscillatorSettings::release},
     {"Pitch", "cents", &OscillatorSettings::pitch},
     {"Pan", "", &OscillatorSettings::pan},
     {"Level variation amount", "", &OscillatorSettings::level_variation_amplitude},
     {"Level variation rate", "Hz", &OscillatorSettings::level_variation_rate},
     {"Pitch variation amount", "cents", &OscillatorSettings::pitch_variation_amplitude},
     {"Pitch variation rate", "Hz", &OscillatorSettings::pitch_variation_rate},
     {"Pan variation amount", "", &OscillatorSettings::pan_variation_amplitude},
     {"Pan variation rate", "Hz", &OscillatorSettings::pan_variation_rate}}};

const ParameterDescriptor* GetDescriptor(Parameter parameter) {
  const int index = static_cast<int>(parameter);
  if (index < 0 || index >= OscillatorSettings::kNumParameters) return nullptr;

  return &kParameterDescriptors[static_cast<std::size_t>(index)];
}

double Lerp(double lo, double hi, double t) { return lo + (hi - lo) * t; }

using OscillatorParameterValues = CompoundPatch::OscillatorParameterValues;

OscillatorParameterValues GetParameterValues(const SimplePatch& patch, Parameter parameter) {
  OscillatorParameterValues values{};
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    values[static_cast<std::size_t>(oscillatorIndex)] = patch.GetOscillatorSettings(oscillatorIndex).GetParameter(parameter);
  }

  return values;
}

// Existing patches are normalized so the sum of squared harmonic levels is 1.
// For a harmonic sine sum at full breath, that corresponds to a waveform RMS of
// 1/sqrt(2).
constexpr double kReferenceLevelWaveformRms = 0.70710678118654752440;
constexpr double kLevelWaveformRmsEpsilon = 1.0e-12;

enum class HarmonicParity { All, Even, Odd };

bool MatchesParity(int oscillatorIndex, HarmonicParity parity) {
  switch (parity) {
  case HarmonicParity::All:  return true;
  case HarmonicParity::Even: return (oscillatorIndex % 2) == 1;
  case HarmonicParity::Odd:  return (oscillatorIndex % 2) == 0;
  }

  return false;
}

bool IsMirroredBipolarRange(double minValue, double maxValue) {
  if (minValue >= 0.0 || maxValue <= 0.0) return false;

  const double minMagnitude = std::fabs(minValue);
  const double maxMagnitude = std::fabs(maxValue);
  const double tolerance = std::max({1.0, minMagnitude, maxMagnitude}) * 1.0e-9;
  return std::fabs(minMagnitude - maxMagnitude) <= tolerance;
}

// Scale the odds of a normalized value so reciprocal scale factors undo each
// other while the result remains inside the finite range.
double ScaleNormalizedBoundedValue(double normalizedValue, double scale) {
  const double clampedValue = std::clamp(normalizedValue, 0.0, 1.0);
  if (!std::isfinite(scale) || scale <= 0.0 || clampedValue <= 0.0 || clampedValue >= 1.0) return clampedValue;

  const double denominator = 1.0 + ((scale - 1.0) * clampedValue);
  if (denominator <= 0.0) return (scale >= 1.0) ? 1.0 : 0.0;

  return std::clamp((clampedValue * scale) / denominator, 0.0, 1.0);
}

double ScaleParameterValue(double value, double scale, double minValue, double maxValue) {
  if (std::isfinite(minValue) && std::isfinite(maxValue) && maxValue > minValue) {
    if (IsMirroredBipolarRange(minValue, maxValue)) {
      const double maxMagnitude = std::max(std::fabs(minValue), std::fabs(maxValue));
      if (maxMagnitude <= 0.0) return 0.0;

      const double sign = (value < 0.0) ? -1.0 : 1.0;
      const double normalizedMagnitude = std::clamp(std::fabs(value) / maxMagnitude, 0.0, 1.0);
      const double scaledMagnitude = ScaleNormalizedBoundedValue(normalizedMagnitude, scale);
      return std::clamp(sign * scaledMagnitude * maxMagnitude, minValue, maxValue);
    }

    const double range = maxValue - minValue;
    const double normalizedValue = std::clamp((value - minValue) / range, 0.0, 1.0);
    const double scaledNormalizedValue = ScaleNormalizedBoundedValue(normalizedValue, scale);
    return minValue + (scaledNormalizedValue * range);
  }

  return std::clamp(value * scale, minValue, maxValue);
}

bool ScaleParameters(SimplePatch::OscillatorArray& oscillatorSettings, MemberPtr member, double scale, HarmonicParity parity,
                     double minValue = -std::numeric_limits<double>::infinity(), double maxValue = std::numeric_limits<double>::infinity()) {
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    if (MatchesParity(oscillatorIndex, parity)) {
      auto& value = oscillatorSettings[oscillatorIndex].*member;
      value = ScaleParameterValue(value, scale, minValue, maxValue);
    }
  }

  return true;
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

const char* OscillatorSettings::GetParameterName(Parameter parameter) {
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? descriptor->name : "";
}

const char* OscillatorSettings::GetParameterUnit(Parameter parameter) {
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? descriptor->unit : "";
}

double OscillatorSettings::GetParameter(Parameter parameter) const {
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? this->*(descriptor->member) : 0.0;
}

void OscillatorSettings::SetParameter(Parameter parameter, double value) {
  const auto* descriptor = GetDescriptor(parameter);
  if (descriptor) this->*(descriptor->member) = value;
}

OscillatorSettings OscillatorSettings::Interpolate(const OscillatorSettings& lo, const OscillatorSettings& hi, double t) {
  const double clampedT = std::clamp(t, 0.0, 1.0);
  OscillatorSettings out{};
  for (const auto& descriptor : kParameterDescriptors) {
    out.*(descriptor.member) = Lerp(lo.*(descriptor.member), hi.*(descriptor.member), clampedT);
  }
  return out;
}

SimplePatch::SimplePatch(const OscillatorArray& oscillatorSettings) : mOscillatorSettings(oscillatorSettings) {}

int SimplePatch::ClampOscillatorIndex(int oscillatorIndex) { return std::clamp(oscillatorIndex, 0, kNumOscillators - 1); }

const OscillatorSettings& SimplePatch::GetOscillatorSettings(int oscillatorIndex) const { return mOscillatorSettings[ClampOscillatorIndex(oscillatorIndex)]; }

const SimplePatch::OscillatorArray& SimplePatch::GetOscillatorSettingsArray() const { return mOscillatorSettings; }

void SimplePatch::SetOscillatorSettings(int oscillatorIndex, const OscillatorSettings& settings) {
  mOscillatorSettings[ClampOscillatorIndex(oscillatorIndex)] = settings;
}

void SimplePatch::SetOscillatorParameter(int oscillatorIndex, OscillatorSettings::Parameter parameter, double value) {
  mOscillatorSettings[ClampOscillatorIndex(oscillatorIndex)].SetParameter(parameter, value);
}

double SimplePatch::GetLevelWaveformRms() const {
  double sumSquares = 0.0;
  for (const auto& settings : mOscillatorSettings) sumSquares += settings.level * settings.level;

  return std::sqrt(sumSquares * 0.5);
}

bool SimplePatch::ScaleOscillatorParameterAll(OscillatorSettings::Parameter parameter, double scale, double minValue, double maxValue) {
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? ScaleParameters(mOscillatorSettings, descriptor->member, scale, HarmonicParity::All, minValue, maxValue) : false;
}

bool SimplePatch::ScaleOscillatorParameterEven(OscillatorSettings::Parameter parameter, double scale, double minValue, double maxValue) {
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? ScaleParameters(mOscillatorSettings, descriptor->member, scale, HarmonicParity::Even, minValue, maxValue) : false;
}

bool SimplePatch::ScaleOscillatorParameterOdd(OscillatorSettings::Parameter parameter, double scale, double minValue, double maxValue) {
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? ScaleParameters(mOscillatorSettings, descriptor->member, scale, HarmonicParity::Odd, minValue, maxValue) : false;
}

bool SimplePatch::ZeroEvenLevels() {
  for (int oscillatorIndex = 1; oscillatorIndex < kNumOscillators; oscillatorIndex += 2) mOscillatorSettings[oscillatorIndex].level = 0.0;

  return true;
}

bool SimplePatch::ZeroOddLevels() {
  for (int oscillatorIndex = 0; oscillatorIndex < kNumOscillators; oscillatorIndex += 2) mOscillatorSettings[oscillatorIndex].level = 0.0;

  return true;
}

bool SimplePatch::NormalizeLevelWaveformRms() {
  const double currentRms = GetLevelWaveformRms();
  if (currentRms <= kLevelWaveformRmsEpsilon) return false;

  const double scale = kReferenceLevelWaveformRms / currentRms;
  for (auto& settings : mOscillatorSettings) settings.level *= scale;

  return true;
}

SimplePatch SimplePatch::Interpolate(const SimplePatch& lo, const SimplePatch& hi, double t) {
  OscillatorArray out{};
  for (int oscillator = 0; oscillator < kNumOscillators; ++oscillator) {
    out[oscillator] = OscillatorSettings::Interpolate(lo.mOscillatorSettings[oscillator], hi.mOscillatorSettings[oscillator], t);
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

OscillatorSettings CompoundPatch::GetOscillatorSettings(double midiNote, int oscillatorIndex) const {
  return InterpolateOscillatorSettings(ResolveNoteSpan(midiNote), oscillatorIndex);
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
  if (interval <= 0.0) return makeExactSpan(lower);

  const EqCurve& lowerEqCurve = GetKeyNoteEqCurveOrDefault(lower->first);
  const EqCurve& upperEqCurve = GetKeyNoteEqCurveOrDefault(upper->first);
  return ResolvedNoteSpan{&lower->second, &upper->second, &lowerEqCurve, &upperEqCurve, (clampedMidiNote - static_cast<double>(lower->first)) / interval};
}

OscillatorSettings CompoundPatch::InterpolateOscillatorSettings(const ResolvedNoteSpan& span, int oscillatorIndex) const {
  const OscillatorSettings& lowerSettings = span.lowerPatch->GetOscillatorSettings(oscillatorIndex);
  if (span.t <= 0.0 || span.lowerPatch == span.upperPatch) return lowerSettings;

  return OscillatorSettings::Interpolate(lowerSettings, span.upperPatch->GetOscillatorSettings(oscillatorIndex), span.t);
}

double CompoundPatch::EvaluateEqGain(const ResolvedNoteSpan& span, double frequencyHz) const {
  const double lowerGain = EqCurve::DbToGain(span.lowerEqCurve->EvaluateDb(frequencyHz));
  if (span.t <= 0.0 || span.lowerEqCurve == span.upperEqCurve) return lowerGain;

  return Lerp(lowerGain, EqCurve::DbToGain(span.upperEqCurve->EvaluateDb(frequencyHz)), span.t);
}

bool CompoundPatch::HasKeyNotePatch(double midiNote) const { return GetKeyNotePatch(midiNote) != nullptr; }

int CompoundPatch::GetNumKeyNotePatches() const { return static_cast<int>(mKeyNotePatches.size()); }

void CompoundPatch::SetKeyNotePatch(int midiNote, const SimplePatch& patch) {
  const int clampedMidiNote = ClampMidiNote(midiNote);
  const EqCurve* keyNoteEqCurve = GetKeyNoteEqCurve(clampedMidiNote);
  EqCurve eqCurve = keyNoteEqCurve ? *keyNoteEqCurve : GetEqCurveForMidiNote(clampedMidiNote);
  if (IsAllKeyNotesEqEnabled()) eqCurve = GetAllKeyNotesEqCurve();

  SimplePatch updatedPatch = patch;
  ApplyAllKeyNotesValues(updatedPatch);
  mKeyNotePatches[clampedMidiNote] = updatedPatch;
  mKeyNoteEqCurves[clampedMidiNote] = eqCurve;
}

bool CompoundPatch::SetKeyNoteOscillatorParameter(double midiNote, int oscillatorIndex, OscillatorSettings::Parameter parameter, double value) {
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  if (mKeyNotePatches.find(clampedNote) == mKeyNotePatches.end()) return false;

  if (IsAllKeyNotesEnabled(parameter)) {
    auto& sharedValues = mAllKeyNotesValues[ParameterIndex(parameter)];
    sharedValues[static_cast<std::size_t>(oscillatorIndex)] = value;
    for (auto& [_, patch] : mKeyNotePatches) patch.SetOscillatorParameter(oscillatorIndex, parameter, value);
  } else
    mKeyNotePatches[clampedNote].SetOscillatorParameter(oscillatorIndex, parameter, value);

  return true;
}

bool CompoundPatch::SetKeyNoteOscillatorParameterValues(double midiNote, OscillatorSettings::Parameter parameter,
                                                        const std::array<double, SimplePatch::kNumOscillators>& values) {
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  if (mKeyNotePatches.find(clampedNote) == mKeyNotePatches.end()) return false;

  if (IsAllKeyNotesEnabled(parameter)) {
    mAllKeyNotesValues[ParameterIndex(parameter)] = values;
    for (auto& [_, patch] : mKeyNotePatches) ApplyAllKeyNotesValues(patch, parameter, values);
  } else
    ApplyAllKeyNotesValues(mKeyNotePatches[clampedNote], parameter, values);

  return true;
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

void CompoundPatch::EnableAllKeyNotes(OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values) {
  const auto parameterIndex = ParameterIndex(parameter);
  mAllKeyNotesValues[parameterIndex] = values;
  mAllKeyNotesEnabled[parameterIndex] = true;

  for (auto& [_, patch] : mKeyNotePatches) ApplyAllKeyNotesValues(patch, parameter, values);
}

void CompoundPatch::SetAllKeyNotesEnabled(OscillatorSettings::Parameter parameter, bool enabled, double sourceMidiNote) {
  mAllKeyNotesEnabled[ParameterIndex(parameter)] = enabled;

  if (enabled) {
    if (const SimplePatch* sourcePatch = GetKeyNotePatch(sourceMidiNote))
      mAllKeyNotesValues[ParameterIndex(parameter)] = GetParameterValues(*sourcePatch, parameter);
    else if (!mKeyNotePatches.empty())
      mAllKeyNotesValues[ParameterIndex(parameter)] = GetParameterValues(mKeyNotePatches.begin()->second, parameter);

    for (auto& [_, patch] : mKeyNotePatches) ApplyAllKeyNotesValues(patch, parameter, GetAllKeyNotesValues(parameter));
  }
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
  return numRemoved > 0;
}

void CompoundPatch::ClearKeyNotePatches() {
  mKeyNotePatches.clear();
  mKeyNoteEqCurves.clear();
}

void CompoundPatch::ApplyAllKeyNotesValues(SimplePatch& patch) const {
  for (auto parameter : OscillatorSettings::AllParameters()) {
    if (IsAllKeyNotesEnabled(parameter)) ApplyAllKeyNotesValues(patch, parameter, GetAllKeyNotesValues(parameter));
  }
}

void CompoundPatch::ApplyAllKeyNotesValues(SimplePatch& patch, OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values) const {
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    patch.SetOscillatorParameter(oscillatorIndex, parameter, values[static_cast<std::size_t>(oscillatorIndex)]);
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

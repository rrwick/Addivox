#include "oscillator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
using Parameter = OscillatorSettings::Parameter;
using MemberPtr = double OscillatorSettings::*;

struct ParameterDescriptor
{
  const char* name;
  const char* unit;
  MemberPtr member;
};

constexpr std::array<ParameterDescriptor, OscillatorSettings::kNumParameters> kParameterDescriptors{{
  {"Intensity", "", &OscillatorSettings::intensity},
  {"Breath power", "", &OscillatorSettings::breath_power},
  {"Attack", "s", &OscillatorSettings::attack},
  {"Release", "s", &OscillatorSettings::release},
  {"Pitch", "cents", &OscillatorSettings::pitch},
  {"Pan", "", &OscillatorSettings::pan},
  {"Intensity variation amount", "", &OscillatorSettings::intensity_variation_amplitude},
  {"Intensity variation rate", "Hz", &OscillatorSettings::intensity_variation_rate},
  {"Pitch variation amount", "cents", &OscillatorSettings::pitch_variation_amplitude},
  {"Pitch variation rate", "Hz", &OscillatorSettings::pitch_variation_rate},
  {"Pan variation amount", "", &OscillatorSettings::pan_variation_amplitude},
  {"Pan variation rate", "Hz", &OscillatorSettings::pan_variation_rate}
}};

const ParameterDescriptor* GetDescriptor(Parameter parameter)
{
  const int index = static_cast<int>(parameter);
  if(index < 0 || index >= OscillatorSettings::kNumParameters)
    return nullptr;

  return &kParameterDescriptors[static_cast<std::size_t>(index)];
}

double Lerp(double lo, double hi, double t)
{
  return lo + (hi - lo) * t;
}

using OscillatorParameterValues = CompoundPreset::OscillatorParameterValues;

OscillatorParameterValues GetParameterValues(const SimplePreset& preset, Parameter parameter)
{
  OscillatorParameterValues values{};
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    values[static_cast<std::size_t>(oscillatorIndex)] =
      preset.GetOscillatorSettings(oscillatorIndex).GetParameter(parameter);
  }

  return values;
}

// Existing presets are normalized so the sum of squared harmonic levels is 1.
// For a harmonic sine sum at full breath, that corresponds to a waveform RMS of 1/sqrt(2).
constexpr double kReferenceIntensityWaveformRms = 0.70710678118654752440;
constexpr double kIntensityWaveformRmsEpsilon = 1.0e-12;
constexpr int kIntensityTopTaperStartOscillatorIndex = 80;
constexpr int kIntensityTopTaperZeroAtOscillatorIndex = SimplePreset::kNumOscillators;

enum class HarmonicParity
{
  All,
  Even,
  Odd
};

double GetIntensityTopTaperScale(int oscillatorIndex)
{
  if(oscillatorIndex < kIntensityTopTaperStartOscillatorIndex)
    return 1.0;

  const double taperPosition = static_cast<double>(oscillatorIndex - kIntensityTopTaperStartOscillatorIndex);
  const double taperLength = static_cast<double>(kIntensityTopTaperZeroAtOscillatorIndex - kIntensityTopTaperStartOscillatorIndex);
  return std::clamp(1.0 - (taperPosition / taperLength), 0.0, 1.0);
}

bool MatchesParity(int oscillatorIndex, HarmonicParity parity)
{
  switch(parity)
  {
    case HarmonicParity::All:
      return true;
    case HarmonicParity::Even:
      return (oscillatorIndex % 2) == 1;
    case HarmonicParity::Odd:
      return (oscillatorIndex % 2) == 0;
  }

  return false;
}

bool IsMirroredBipolarRange(double minValue, double maxValue)
{
  if(minValue >= 0.0 || maxValue <= 0.0)
    return false;

  const double minMagnitude = std::fabs(minValue);
  const double maxMagnitude = std::fabs(maxValue);
  const double tolerance = std::max({1.0, minMagnitude, maxMagnitude}) * 1.0e-9;
  return std::fabs(minMagnitude - maxMagnitude) <= tolerance;
}

// Scale the odds of a normalized value so reciprocal scale factors undo each
// other while the result remains inside the finite range.
double ScaleNormalizedBoundedValue(double normalizedValue, double scale)
{
  const double clampedValue = std::clamp(normalizedValue, 0.0, 1.0);
  if(!std::isfinite(scale) || scale <= 0.0 || clampedValue <= 0.0 || clampedValue >= 1.0)
    return clampedValue;

  const double denominator = 1.0 + ((scale - 1.0) * clampedValue);
  if(denominator <= 0.0)
    return (scale >= 1.0) ? 1.0 : 0.0;

  return std::clamp((clampedValue * scale) / denominator, 0.0, 1.0);
}

double ScaleParameterValue(double value, double scale, double minValue, double maxValue)
{
  if(std::isfinite(minValue) && std::isfinite(maxValue) && maxValue > minValue)
  {
    if(IsMirroredBipolarRange(minValue, maxValue))
    {
      const double maxMagnitude = std::max(std::fabs(minValue), std::fabs(maxValue));
      if(maxMagnitude <= 0.0)
        return 0.0;

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

bool ScaleParameters(SimplePreset::OscillatorArray& oscillatorSettings,
                     MemberPtr member,
                     double scale,
                     HarmonicParity parity,
                     double minValue = -std::numeric_limits<double>::infinity(),
                     double maxValue = std::numeric_limits<double>::infinity())
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    if(MatchesParity(oscillatorIndex, parity))
    {
      auto& value = oscillatorSettings[oscillatorIndex].*member;
      value = ScaleParameterValue(value, scale, minValue, maxValue);
    }
  }

  return true;
}

const SimplePreset& GetDefaultPreset()
{
  static const SimplePreset preset = [] {
    SimplePreset::OscillatorArray oscillatorSettings{};
    oscillatorSettings.fill(OscillatorSettings{0.0});
    oscillatorSettings[0] = OscillatorSettings{1.0};
    return SimplePreset{oscillatorSettings};
  }();

  return preset;
}

const EqCurve& GetDefaultEqCurve()
{
  static const EqCurve curve{};
  return curve;
}
} // namespace

const char* OscillatorSettings::GetParameterName(Parameter parameter)
{
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? descriptor->name : "";
}

const char* OscillatorSettings::GetParameterUnit(Parameter parameter)
{
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? descriptor->unit : "";
}

double OscillatorSettings::GetParameter(Parameter parameter) const
{
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor ? this->*(descriptor->member) : 0.0;
}

void OscillatorSettings::SetParameter(Parameter parameter, double value)
{
  const auto* descriptor = GetDescriptor(parameter);
  if(descriptor)
    this->*(descriptor->member) = value;
}

OscillatorSettings OscillatorSettings::Interpolate(const OscillatorSettings& lo, const OscillatorSettings& hi, double t)
{
  const double clampedT = std::clamp(t, 0.0, 1.0);
  OscillatorSettings out{};
  for(const auto& descriptor : kParameterDescriptors)
  {
    out.*(descriptor.member) = Lerp(lo.*(descriptor.member), hi.*(descriptor.member), clampedT);
  }
  return out;
}

SimplePreset::SimplePreset(const OscillatorArray& oscillatorSettings)
: mOscillatorSettings(oscillatorSettings)
{
}

int SimplePreset::ClampOscillatorIndex(int oscillatorIndex)
{
  return std::clamp(oscillatorIndex, 0, kNumOscillators - 1);
}

const OscillatorSettings& SimplePreset::GetOscillatorSettings(int oscillatorIndex) const
{
  return mOscillatorSettings[ClampOscillatorIndex(oscillatorIndex)];
}

const SimplePreset::OscillatorArray& SimplePreset::GetOscillatorSettingsArray() const
{
  return mOscillatorSettings;
}

void SimplePreset::SetOscillatorSettings(int oscillatorIndex, const OscillatorSettings& settings)
{
  mOscillatorSettings[ClampOscillatorIndex(oscillatorIndex)] = settings;
}

void SimplePreset::SetOscillatorParameter(int oscillatorIndex, OscillatorSettings::Parameter parameter, double value)
{
  mOscillatorSettings[ClampOscillatorIndex(oscillatorIndex)].SetParameter(parameter, value);
}

double SimplePreset::GetIntensityWaveformRms() const
{
  double sumSquares = 0.0;
  for(const auto& settings : mOscillatorSettings)
    sumSquares += settings.intensity * settings.intensity;

  return std::sqrt(sumSquares * 0.5);
}

bool SimplePreset::ApplyIntensityTopTaper()
{
  for(int oscillatorIndex = 0; oscillatorIndex < kNumOscillators; ++oscillatorIndex)
    mOscillatorSettings[oscillatorIndex].intensity *= GetIntensityTopTaperScale(oscillatorIndex);

  return true;
}

bool SimplePreset::ScaleOscillatorParameterAll(OscillatorSettings::Parameter parameter,
                                               double scale,
                                               double minValue,
                                               double maxValue)
{
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor
    ? ScaleParameters(mOscillatorSettings, descriptor->member, scale, HarmonicParity::All, minValue, maxValue)
    : false;
}

bool SimplePreset::ScaleOscillatorParameterEven(OscillatorSettings::Parameter parameter,
                                                double scale,
                                                double minValue,
                                                double maxValue)
{
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor
    ? ScaleParameters(mOscillatorSettings, descriptor->member, scale, HarmonicParity::Even, minValue, maxValue)
    : false;
}

bool SimplePreset::ScaleOscillatorParameterOdd(OscillatorSettings::Parameter parameter,
                                               double scale,
                                               double minValue,
                                               double maxValue)
{
  const auto* descriptor = GetDescriptor(parameter);
  return descriptor
    ? ScaleParameters(mOscillatorSettings, descriptor->member, scale, HarmonicParity::Odd, minValue, maxValue)
    : false;
}

bool SimplePreset::ZeroEvenIntensities()
{
  for(int oscillatorIndex = 1; oscillatorIndex < kNumOscillators; oscillatorIndex += 2)
    mOscillatorSettings[oscillatorIndex].intensity = 0.0;

  return true;
}

bool SimplePreset::ZeroOddIntensities()
{
  for(int oscillatorIndex = 0; oscillatorIndex < kNumOscillators; oscillatorIndex += 2)
    mOscillatorSettings[oscillatorIndex].intensity = 0.0;

  return true;
}

bool SimplePreset::NormalizeIntensityWaveformRms()
{
  const double currentRms = GetIntensityWaveformRms();
  if(currentRms <= kIntensityWaveformRmsEpsilon)
    return false;

  const double scale = kReferenceIntensityWaveformRms / currentRms;
  for(auto& settings : mOscillatorSettings)
    settings.intensity *= scale;

  return true;
}

SimplePreset SimplePreset::Interpolate(const SimplePreset& lo, const SimplePreset& hi, double t)
{
  OscillatorArray out{};
  for(int oscillator = 0; oscillator < kNumOscillators; ++oscillator)
  {
    out[oscillator] = OscillatorSettings::Interpolate(
      lo.mOscillatorSettings[oscillator],
      hi.mOscillatorSettings[oscillator],
      t);
  }
  return SimplePreset{out};
}

int CompoundPreset::ClampMidiNote(int midiNote)
{
  return std::clamp(midiNote, kMinMidiNote, kMaxMidiNote);
}

int CompoundPreset::RoundAndClampMidiNote(double midiNote)
{
  return ClampMidiNote(static_cast<int>(std::lround(midiNote)));
}

std::size_t CompoundPreset::ParameterIndex(OscillatorSettings::Parameter parameter)
{
  return static_cast<std::size_t>(parameter);
}

CompoundPreset::CompoundPreset()
: CompoundPreset({})
{
}

CompoundPreset::CompoundPreset(std::initializer_list<KeyNotePreset> keyNotePresets)
{
  for(const auto& [midiNote, preset] : keyNotePresets)
  {
    mKeyNotePresets[ClampMidiNote(midiNote)] = preset;
    mKeyNoteEqCurves[ClampMidiNote(midiNote)] = GetDefaultEqCurve();
  }
}

OscillatorSettings CompoundPreset::GetOscillatorSettings(double midiNote, int oscillatorIndex) const
{
  return InterpolateOscillatorSettings(ResolveNoteSpan(midiNote), oscillatorIndex);
}

SimplePreset CompoundPreset::GetPresetForMidiNote(double midiNote) const
{
  const ResolvedNoteSpan span = ResolveNoteSpan(midiNote);
  if(span.t <= 0.0 || span.lowerPreset == span.upperPreset)
    return *span.lowerPreset;

  return SimplePreset::Interpolate(*span.lowerPreset, *span.upperPreset, span.t);
}

const SimplePreset* CompoundPreset::GetKeyNotePreset(double midiNote) const
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  const auto keyNoteIt = mKeyNotePresets.find(clampedNote);
  if(keyNoteIt == mKeyNotePresets.end())
    return nullptr;

  return &keyNoteIt->second;
}

EqCurve CompoundPreset::GetEqCurveForMidiNote(double midiNote) const
{
  const ResolvedNoteSpan span = ResolveNoteSpan(midiNote);
  if(span.t <= 0.0 || span.lowerEqCurve == span.upperEqCurve)
    return *span.lowerEqCurve;

  return EqCurve::FromResponseLut(
    EqCurve::InterpolateResponseLut(
      span.lowerEqCurve->BuildResponseLut(),
      span.upperEqCurve->BuildResponseLut(),
      span.t));
}

const EqCurve* CompoundPreset::GetKeyNoteEqCurve(double midiNote) const
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  const auto keyNoteIt = mKeyNoteEqCurves.find(clampedNote);
  if(keyNoteIt == mKeyNoteEqCurves.end())
    return nullptr;

  return &keyNoteIt->second;
}

const std::map<int, SimplePreset>& CompoundPreset::GetKeyNotePresets() const
{
  return mKeyNotePresets;
}

bool CompoundPreset::IsAllKeyNotesEnabled(OscillatorSettings::Parameter parameter) const
{
  return mAllKeyNotesEnabled[ParameterIndex(parameter)];
}

const CompoundPreset::OscillatorParameterValues& CompoundPreset::GetAllKeyNotesValues(
  OscillatorSettings::Parameter parameter) const
{
  return mAllKeyNotesValues[ParameterIndex(parameter)];
}

bool CompoundPreset::IsAllKeyNotesEqEnabled() const
{
  return mAllKeyNotesEqEnabled;
}

const EqCurve& CompoundPreset::GetAllKeyNotesEqCurve() const
{
  return mAllKeyNotesEqCurve;
}

CompoundPreset::ResolvedNoteSpan CompoundPreset::ResolveNoteSpan(double midiNote) const
{
  const EqCurve& defaultEqCurve = IsAllKeyNotesEqEnabled() ? GetAllKeyNotesEqCurve() : GetDefaultEqCurve();
  if(mKeyNotePresets.empty())
  {
    return ResolvedNoteSpan{
      &GetDefaultPreset(),
      &GetDefaultPreset(),
      &defaultEqCurve,
      &defaultEqCurve,
      0.0};
  }

  const double clampedMidiNote = std::clamp(midiNote, static_cast<double>(kMinMidiNote), static_cast<double>(kMaxMidiNote));
  auto makeExactSpan = [&](const std::map<int, SimplePreset>::const_iterator& it) {
    const EqCurve& eqCurve = GetKeyNoteEqCurveOrDefault(it->first);
    return ResolvedNoteSpan{
      &it->second,
      &it->second,
      &eqCurve,
      &eqCurve,
      0.0};
  };

  auto upper = mKeyNotePresets.lower_bound(static_cast<int>(std::ceil(clampedMidiNote)));
  if(upper == mKeyNotePresets.begin())
    return makeExactSpan(upper);

  if(upper == mKeyNotePresets.end())
    return makeExactSpan(std::prev(mKeyNotePresets.end()));

  if(static_cast<double>(upper->first) == clampedMidiNote)
    return makeExactSpan(upper);

  auto lower = std::prev(upper);
  const double interval = static_cast<double>(upper->first - lower->first);
  if(interval <= 0.0)
    return makeExactSpan(lower);

  const EqCurve& lowerEqCurve = GetKeyNoteEqCurveOrDefault(lower->first);
  const EqCurve& upperEqCurve = GetKeyNoteEqCurveOrDefault(upper->first);
  return ResolvedNoteSpan{
    &lower->second,
    &upper->second,
    &lowerEqCurve,
    &upperEqCurve,
    (clampedMidiNote - static_cast<double>(lower->first)) / interval};
}

OscillatorSettings CompoundPreset::InterpolateOscillatorSettings(const ResolvedNoteSpan& span, int oscillatorIndex) const
{
  const OscillatorSettings& lowerSettings = span.lowerPreset->GetOscillatorSettings(oscillatorIndex);
  if(span.t <= 0.0 || span.lowerPreset == span.upperPreset)
    return lowerSettings;

  return OscillatorSettings::Interpolate(
    lowerSettings,
    span.upperPreset->GetOscillatorSettings(oscillatorIndex),
    span.t);
}

double CompoundPreset::EvaluateEqGain(const ResolvedNoteSpan& span, double frequencyHz) const
{
  const double lowerGain = EqCurve::DbToGain(span.lowerEqCurve->EvaluateDb(frequencyHz));
  if(span.t <= 0.0 || span.lowerEqCurve == span.upperEqCurve)
    return lowerGain;

  return Lerp(
    lowerGain,
    EqCurve::DbToGain(span.upperEqCurve->EvaluateDb(frequencyHz)),
    span.t);
}

bool CompoundPreset::HasKeyNotePreset(double midiNote) const
{
  return GetKeyNotePreset(midiNote) != nullptr;
}

int CompoundPreset::GetNumKeyNotePresets() const
{
  return static_cast<int>(mKeyNotePresets.size());
}

void CompoundPreset::SetKeyNotePreset(int midiNote, const SimplePreset& preset)
{
  const int clampedMidiNote = ClampMidiNote(midiNote);
  const EqCurve* keyNoteEqCurve = GetKeyNoteEqCurve(clampedMidiNote);
  EqCurve eqCurve = keyNoteEqCurve ? *keyNoteEqCurve : GetEqCurveForMidiNote(clampedMidiNote);
  if(IsAllKeyNotesEqEnabled())
    eqCurve = GetAllKeyNotesEqCurve();

  SimplePreset updatedPreset = preset;
  ApplyAllKeyNotesValues(updatedPreset);
  mKeyNotePresets[clampedMidiNote] = updatedPreset;
  mKeyNoteEqCurves[clampedMidiNote] = eqCurve;
}

bool CompoundPreset::SetKeyNoteOscillatorParameter(double midiNote,
                                                   int oscillatorIndex,
                                                   OscillatorSettings::Parameter parameter,
                                                   double value)
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  if(mKeyNotePresets.find(clampedNote) == mKeyNotePresets.end())
    return false;

  if(IsAllKeyNotesEnabled(parameter))
  {
    auto& sharedValues = mAllKeyNotesValues[ParameterIndex(parameter)];
    sharedValues[static_cast<std::size_t>(oscillatorIndex)] = value;
    for(auto& [_, preset] : mKeyNotePresets)
      preset.SetOscillatorParameter(oscillatorIndex, parameter, value);
  }
  else
    mKeyNotePresets[clampedNote].SetOscillatorParameter(oscillatorIndex, parameter, value);

  return true;
}

bool CompoundPreset::SetKeyNoteOscillatorParameterValues(
  double midiNote,
  OscillatorSettings::Parameter parameter,
  const std::array<double, SimplePreset::kNumOscillators>& values)
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  if(mKeyNotePresets.find(clampedNote) == mKeyNotePresets.end())
    return false;

  if(IsAllKeyNotesEnabled(parameter))
  {
    mAllKeyNotesValues[ParameterIndex(parameter)] = values;
    for(auto& [_, preset] : mKeyNotePresets)
      ApplyAllKeyNotesValues(preset, parameter, values);
  }
  else
    ApplyAllKeyNotesValues(mKeyNotePresets[clampedNote], parameter, values);

  return true;
}

bool CompoundPreset::SetKeyNoteEqCurve(double midiNote, const EqCurve& curve)
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  if(mKeyNotePresets.find(clampedNote) == mKeyNotePresets.end())
    return false;

  if(IsAllKeyNotesEqEnabled())
  {
    mAllKeyNotesEqCurve = curve;
    SetAllKeyNoteEqCurves(mAllKeyNotesEqCurve);
  }
  else
    mKeyNoteEqCurves[clampedNote] = curve;
  return true;
}

void CompoundPreset::EnableAllKeyNotes(OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values)
{
  const auto parameterIndex = ParameterIndex(parameter);
  mAllKeyNotesValues[parameterIndex] = values;
  mAllKeyNotesEnabled[parameterIndex] = true;

  for(auto& [_, preset] : mKeyNotePresets)
    ApplyAllKeyNotesValues(preset, parameter, values);
}

void CompoundPreset::SetAllKeyNotesEnabled(OscillatorSettings::Parameter parameter, bool enabled, double sourceMidiNote)
{
  mAllKeyNotesEnabled[ParameterIndex(parameter)] = enabled;

  if(enabled)
  {
    if(const SimplePreset* sourcePreset = GetKeyNotePreset(sourceMidiNote))
      mAllKeyNotesValues[ParameterIndex(parameter)] = GetParameterValues(*sourcePreset, parameter);
    else if(!mKeyNotePresets.empty())
      mAllKeyNotesValues[ParameterIndex(parameter)] = GetParameterValues(mKeyNotePresets.begin()->second, parameter);

    for(auto& [_, preset] : mKeyNotePresets)
      ApplyAllKeyNotesValues(preset, parameter, GetAllKeyNotesValues(parameter));
  }
}

void CompoundPreset::EnableAllKeyNotesEq(const EqCurve& curve)
{
  mAllKeyNotesEqCurve = curve;
  mAllKeyNotesEqEnabled = true;
  SetAllKeyNoteEqCurves(mAllKeyNotesEqCurve);
}

void CompoundPreset::SetAllKeyNotesEqEnabled(bool enabled)
{
  mAllKeyNotesEqEnabled = enabled;
  if(enabled)
  {
    if(!mKeyNoteEqCurves.empty())
      mAllKeyNotesEqCurve = mKeyNoteEqCurves.begin()->second;
    SetAllKeyNoteEqCurves(mAllKeyNotesEqCurve);
  }
}

bool CompoundPreset::RemoveKeyNotePreset(int midiNote)
{
  if(mKeyNotePresets.size() <= 1)
    return false;

  const int clampedMidiNote = ClampMidiNote(midiNote);
  const size_t numRemoved = mKeyNotePresets.erase(clampedMidiNote);
  mKeyNoteEqCurves.erase(clampedMidiNote);
  return numRemoved > 0;
}

void CompoundPreset::ClearKeyNotePresets()
{
  mKeyNotePresets.clear();
  mKeyNoteEqCurves.clear();
}

void CompoundPreset::ApplyAllKeyNotesValues(SimplePreset& preset) const
{
  for(auto parameter : OscillatorSettings::AllParameters())
  {
    if(IsAllKeyNotesEnabled(parameter))
      ApplyAllKeyNotesValues(preset, parameter, GetAllKeyNotesValues(parameter));
  }
}

void CompoundPreset::ApplyAllKeyNotesValues(SimplePreset& preset,
                                            OscillatorSettings::Parameter parameter,
                                            const OscillatorParameterValues& values) const
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    preset.SetOscillatorParameter(
      oscillatorIndex,
      parameter,
      values[static_cast<std::size_t>(oscillatorIndex)]);
  }
}

const EqCurve& CompoundPreset::GetKeyNoteEqCurveOrDefault(int midiNote) const
{
  if(IsAllKeyNotesEqEnabled())
    return GetAllKeyNotesEqCurve();

  if(const auto eqCurveIt = mKeyNoteEqCurves.find(midiNote); eqCurveIt != mKeyNoteEqCurves.end())
    return eqCurveIt->second;

  return GetDefaultEqCurve();
}

void CompoundPreset::SetAllKeyNoteEqCurves(const EqCurve& curve)
{
  for(auto& [_, keyNoteCurve] : mKeyNoteEqCurves)
    keyNoteCurve = curve;
}

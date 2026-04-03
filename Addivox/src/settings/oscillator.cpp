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
constexpr double kBreathPowerMin = 0.0;
constexpr double kBreathPowerMax = 100.0;

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

bool SimplePreset::ScaleIntensityUp()
{
  return ScaleOscillatorParameterAll(OscillatorSettings::Parameter::intensity, 1.111111111111111, 0.0, 1.0);
}

bool SimplePreset::ScaleIntensityDown()
{
  return ScaleOscillatorParameterAll(OscillatorSettings::Parameter::intensity, 0.9, 0.0, 1.0);
}

bool SimplePreset::ScaleIntensityUpEven()
{
  return ScaleOscillatorParameterEven(OscillatorSettings::Parameter::intensity, 1.111111111111111, 0.0, 1.0);
}

bool SimplePreset::ScaleIntensityDownEven()
{
  return ScaleOscillatorParameterEven(OscillatorSettings::Parameter::intensity, 0.9, 0.0, 1.0);
}

bool SimplePreset::ScaleIntensityUpOdd()
{
  return ScaleOscillatorParameterOdd(OscillatorSettings::Parameter::intensity, 1.111111111111111, 0.0, 1.0);
}

bool SimplePreset::ScaleIntensityDownOdd()
{
  return ScaleOscillatorParameterOdd(OscillatorSettings::Parameter::intensity, 0.9, 0.0, 1.0);
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

bool SimplePreset::ScaleBreathPowerUp()
{
  return ScaleOscillatorParameterAll(OscillatorSettings::Parameter::breath_power, 1.111111111111111, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerDown()
{
  return ScaleOscillatorParameterAll(OscillatorSettings::Parameter::breath_power, 0.9, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerUpEven()
{
  return ScaleOscillatorParameterEven(OscillatorSettings::Parameter::breath_power, 1.111111111111111, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerDownEven()
{
  return ScaleOscillatorParameterEven(OscillatorSettings::Parameter::breath_power, 0.9, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerUpOdd()
{
  return ScaleOscillatorParameterOdd(OscillatorSettings::Parameter::breath_power, 1.111111111111111, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerDownOdd()
{
  return ScaleOscillatorParameterOdd(OscillatorSettings::Parameter::breath_power, 0.9, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::SmoothIntensity()
{
  std::array<double, kNumOscillators> smoothedIntensities{};

  for(int oscillatorIndex = 0; oscillatorIndex < kNumOscillators; ++oscillatorIndex)
  {
    double sum = mOscillatorSettings[oscillatorIndex].intensity;
    int count = 1;

    if(oscillatorIndex > 0)
    {
      sum += mOscillatorSettings[oscillatorIndex - 1].intensity;
      ++count;
    }

    if(oscillatorIndex + 1 < kNumOscillators)
    {
      sum += mOscillatorSettings[oscillatorIndex + 1].intensity;
      ++count;
    }

    smoothedIntensities[static_cast<std::size_t>(oscillatorIndex)] = sum / static_cast<double>(count);
  }

  for(int oscillatorIndex = 0; oscillatorIndex < kNumOscillators; ++oscillatorIndex)
    mOscillatorSettings[oscillatorIndex].intensity = smoothedIntensities[static_cast<std::size_t>(oscillatorIndex)];

  return true;
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

  RebuildInterpolatedPresets();
  RebuildInterpolatedEqCurves();
}

const OscillatorSettings& CompoundPreset::GetOscillatorSettings(double midiNote, int oscillatorIndex) const
{
  return GetPresetForMidiNote(midiNote).GetOscillatorSettings(oscillatorIndex);
}

const SimplePreset& CompoundPreset::GetPresetForMidiNote(double midiNote) const
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  return mInterpolatedPresets[clampedNote - kMinMidiNote];
}

const SimplePreset* CompoundPreset::GetKeyNotePreset(double midiNote) const
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  const auto keyNoteIt = mKeyNotePresets.find(clampedNote);
  if(keyNoteIt == mKeyNotePresets.end())
    return nullptr;

  return &keyNoteIt->second;
}

const EqCurve& CompoundPreset::GetEqCurveForMidiNote(double midiNote) const
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  return mInterpolatedEqCurves[clampedNote - kMinMidiNote];
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
  RebuildInterpolatedPresets();
  RebuildInterpolatedEqCurves();
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

    SetInterpolatedOscillatorParameter(parameter, oscillatorIndex, value);
  }
  else
  {
    mKeyNotePresets[clampedNote].SetOscillatorParameter(oscillatorIndex, parameter, value);
    RebuildInterpolatedPresets();
  }
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

    SetInterpolatedOscillatorParameterValues(parameter, values);
  }
  else
  {
    ApplyAllKeyNotesValues(mKeyNotePresets[clampedNote], parameter, values);
    RebuildInterpolatedPresets();
  }
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

  RebuildInterpolatedEqCurves();
  return true;
}

void CompoundPreset::EnableAllKeyNotes(OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values)
{
  const auto parameterIndex = ParameterIndex(parameter);
  mAllKeyNotesValues[parameterIndex] = values;
  mAllKeyNotesEnabled[parameterIndex] = true;

  for(auto& [_, preset] : mKeyNotePresets)
    ApplyAllKeyNotesValues(preset, parameter, values);

  SetInterpolatedOscillatorParameterValues(parameter, values);
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

    SetInterpolatedOscillatorParameterValues(parameter, GetAllKeyNotesValues(parameter));
  }
}

void CompoundPreset::EnableAllKeyNotesEq(const EqCurve& curve)
{
  mAllKeyNotesEqCurve = curve;
  mAllKeyNotesEqEnabled = true;
  SetAllKeyNoteEqCurves(mAllKeyNotesEqCurve);
  RebuildInterpolatedEqCurves();
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

  RebuildInterpolatedEqCurves();
}

bool CompoundPreset::RemoveKeyNotePreset(int midiNote)
{
  if(mKeyNotePresets.size() <= 1)
    return false;

  const int clampedMidiNote = ClampMidiNote(midiNote);
  const size_t numRemoved = mKeyNotePresets.erase(clampedMidiNote);
  mKeyNoteEqCurves.erase(clampedMidiNote);
  if(numRemoved > 0)
  {
    RebuildInterpolatedPresets();
    RebuildInterpolatedEqCurves();
  }

  return numRemoved > 0;
}

void CompoundPreset::ClearKeyNotePresets()
{
  mKeyNotePresets.clear();
  mKeyNoteEqCurves.clear();
  RebuildInterpolatedPresets();
  RebuildInterpolatedEqCurves();
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

void CompoundPreset::SetInterpolatedOscillatorParameter(OscillatorSettings::Parameter parameter,
                                                        int oscillatorIndex,
                                                        double value)
{
  for(auto& preset : mInterpolatedPresets)
    preset.SetOscillatorParameter(oscillatorIndex, parameter, value);
}

void CompoundPreset::SetInterpolatedOscillatorParameterValues(
  OscillatorSettings::Parameter parameter,
  const OscillatorParameterValues& values)
{
  for(auto& preset : mInterpolatedPresets)
    ApplyAllKeyNotesValues(preset, parameter, values);
}

const EqCurve& CompoundPreset::GetKeyNoteEqCurveOrDefault(int midiNote) const
{
  if(const auto eqCurveIt = mKeyNoteEqCurves.find(midiNote); eqCurveIt != mKeyNoteEqCurves.end())
    return eqCurveIt->second;

  return GetDefaultEqCurve();
}

void CompoundPreset::EnsureKeyNoteEqCurves()
{
  const EqCurve& defaultEqCurve = IsAllKeyNotesEqEnabled() ? GetAllKeyNotesEqCurve() : GetDefaultEqCurve();
  for(const auto& [midiNote, _] : mKeyNotePresets)
  {
    if(mKeyNoteEqCurves.find(midiNote) == mKeyNoteEqCurves.end())
      mKeyNoteEqCurves[midiNote] = defaultEqCurve;
  }
}

void CompoundPreset::SetAllKeyNoteEqCurves(const EqCurve& curve)
{
  for(auto& [_, keyNoteCurve] : mKeyNoteEqCurves)
    keyNoteCurve = curve;
}

void CompoundPreset::RebuildInterpolatedEqCurves()
{
  EnsureKeyNoteEqCurves();

  if(mKeyNotePresets.empty())
  {
    mInterpolatedEqCurves.fill(GetDefaultEqCurve());
    return;
  }

  if(IsAllKeyNotesEqEnabled())
  {
    mInterpolatedEqCurves.fill(mAllKeyNotesEqCurve);
    return;
  }

  if(mKeyNotePresets.size() == 1)
  {
    mInterpolatedEqCurves.fill(GetKeyNoteEqCurveOrDefault(mKeyNotePresets.begin()->first));
    return;
  }

  const auto first = mKeyNotePresets.begin();
  const auto last = std::prev(mKeyNotePresets.end());
  std::fill(
    mInterpolatedEqCurves.begin(),
    mInterpolatedEqCurves.begin() + (first->first - kMinMidiNote + 1),
    GetKeyNoteEqCurveOrDefault(first->first));
  std::fill(
    mInterpolatedEqCurves.begin() + (last->first - kMinMidiNote),
    mInterpolatedEqCurves.end(),
    GetKeyNoteEqCurveOrDefault(last->first));

  auto lower = first;
  auto upper = std::next(lower);
  while(upper != mKeyNotePresets.end())
  {
    const int lowerMidiNote = lower->first;
    const int upperMidiNote = upper->first;
    const EqCurve& lowerEqCurve = GetKeyNoteEqCurveOrDefault(lowerMidiNote);
    const EqCurve& upperEqCurve = GetKeyNoteEqCurveOrDefault(upperMidiNote);
    mInterpolatedEqCurves[lowerMidiNote - kMinMidiNote] = lowerEqCurve;

    if((upperMidiNote - lowerMidiNote) > 1)
    {
      const EqCurve::ResponseLut lowerResponseLut = lowerEqCurve.BuildResponseLut();
      const EqCurve::ResponseLut upperResponseLut = upperEqCurve.BuildResponseLut();
      const double interval = static_cast<double>(upperMidiNote - lowerMidiNote);
      for(int midiNote = lowerMidiNote + 1; midiNote < upperMidiNote; ++midiNote)
      {
        const double t = static_cast<double>(midiNote - lowerMidiNote) / interval;
        mInterpolatedEqCurves[midiNote - kMinMidiNote] = EqCurve::FromResponseLut(
          EqCurve::InterpolateResponseLut(lowerResponseLut, upperResponseLut, t));
      }
    }

    lower = upper;
    ++upper;
  }
}

void CompoundPreset::RebuildInterpolatedPresets()
{
  if(mKeyNotePresets.empty())
  {
    mInterpolatedPresets.fill(GetDefaultPreset());
  }
  else if(mKeyNotePresets.size() == 1)
  {
    mInterpolatedPresets.fill(mKeyNotePresets.begin()->second);
  }
  else
  {
    const auto first = mKeyNotePresets.begin();
    const auto last = std::prev(mKeyNotePresets.end());

    for(int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
      const int index = midiNote - kMinMidiNote;

      if(midiNote <= first->first)
      {
        mInterpolatedPresets[index] = first->second;
        continue;
      }

      if(midiNote >= last->first)
      {
        mInterpolatedPresets[index] = last->second;
        continue;
      }

      auto upper = mKeyNotePresets.lower_bound(midiNote);
      if(upper != mKeyNotePresets.end() && upper->first == midiNote)
      {
        mInterpolatedPresets[index] = upper->second;
        continue;
      }

      auto lower = std::prev(upper);
      const double t = static_cast<double>(midiNote - lower->first) / static_cast<double>(upper->first - lower->first);
      mInterpolatedPresets[index] = SimplePreset::Interpolate(lower->second, upper->second, t);
    }
  }
}

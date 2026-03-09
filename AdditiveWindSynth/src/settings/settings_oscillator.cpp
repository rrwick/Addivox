#include "settings_oscillator.h"

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
  {"Breath Power", "", &OscillatorSettings::breath_power},
  {"Attack", "s", &OscillatorSettings::attack},
  {"Release", "s", &OscillatorSettings::release},
  {"Pitch", "cents", &OscillatorSettings::pitch},
  {"Pan", "", &OscillatorSettings::pan},
  {"Intensity Variation Amount", "", &OscillatorSettings::intensity_variation_amplitude},
  {"Intensity Variation Rate", "Hz", &OscillatorSettings::intensity_variation_rate},
  {"Pitch Variation Amount", "cents", &OscillatorSettings::pitch_variation_amplitude},
  {"Pitch Variation Rate", "Hz", &OscillatorSettings::pitch_variation_rate},
  {"Pan Variation Amount", "", &OscillatorSettings::pan_variation_amplitude},
  {"Pan Variation Rate", "Hz", &OscillatorSettings::pan_variation_rate}
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
      value = std::clamp(value * scale, minValue, maxValue);
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
  return ScaleOscillatorParameterAll(OscillatorSettings::Parameter::intensity, 1.1, 0.0, std::numeric_limits<double>::infinity());
}

bool SimplePreset::ScaleIntensityDown()
{
  return ScaleOscillatorParameterAll(OscillatorSettings::Parameter::intensity, 0.9, 0.0, std::numeric_limits<double>::infinity());
}

bool SimplePreset::ScaleIntensityUpEven()
{
  return ScaleOscillatorParameterEven(OscillatorSettings::Parameter::intensity, 1.1, 0.0, std::numeric_limits<double>::infinity());
}

bool SimplePreset::ScaleIntensityDownEven()
{
  return ScaleOscillatorParameterEven(OscillatorSettings::Parameter::intensity, 0.9, 0.0, std::numeric_limits<double>::infinity());
}

bool SimplePreset::ScaleIntensityUpOdd()
{
  return ScaleOscillatorParameterOdd(OscillatorSettings::Parameter::intensity, 1.1, 0.0, std::numeric_limits<double>::infinity());
}

bool SimplePreset::ScaleIntensityDownOdd()
{
  return ScaleOscillatorParameterOdd(OscillatorSettings::Parameter::intensity, 0.9, 0.0, std::numeric_limits<double>::infinity());
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
  return ScaleOscillatorParameterAll(OscillatorSettings::Parameter::breath_power, 1.1, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerDown()
{
  return ScaleOscillatorParameterAll(OscillatorSettings::Parameter::breath_power, 0.9, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerUpEven()
{
  return ScaleOscillatorParameterEven(OscillatorSettings::Parameter::breath_power, 1.1, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerDownEven()
{
  return ScaleOscillatorParameterEven(OscillatorSettings::Parameter::breath_power, 0.9, kBreathPowerMin, kBreathPowerMax);
}

bool SimplePreset::ScaleBreathPowerUpOdd()
{
  return ScaleOscillatorParameterOdd(OscillatorSettings::Parameter::breath_power, 1.1, kBreathPowerMin, kBreathPowerMax);
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

CompoundPreset::CompoundPreset()
: CompoundPreset({})
{
}

CompoundPreset::CompoundPreset(std::initializer_list<KeyNotePreset> keyNotePresets)
{
  for(const auto& [midiNote, preset] : keyNotePresets)
    mKeyNotePresets[ClampMidiNote(midiNote)] = preset;

  RebuildInterpolatedPresets();
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

const std::map<int, SimplePreset>& CompoundPreset::GetKeyNotePresets() const
{
  return mKeyNotePresets;
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
  mKeyNotePresets[ClampMidiNote(midiNote)] = preset;
  RebuildInterpolatedPresets();
}

bool CompoundPreset::SetKeyNoteOscillatorParameter(double midiNote,
                                                   int oscillatorIndex,
                                                   OscillatorSettings::Parameter parameter,
                                                   double value)
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  const auto keyNoteIt = mKeyNotePresets.find(clampedNote);
  if(keyNoteIt == mKeyNotePresets.end())
    return false;

  keyNoteIt->second.SetOscillatorParameter(oscillatorIndex, parameter, value);
  RebuildInterpolatedPresets();
  return true;
}

bool CompoundPreset::SetKeyNoteOscillatorParameterValues(
  double midiNote,
  OscillatorSettings::Parameter parameter,
  const std::array<double, SimplePreset::kNumOscillators>& values)
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  const auto keyNoteIt = mKeyNotePresets.find(clampedNote);
  if(keyNoteIt == mKeyNotePresets.end())
    return false;

  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
    keyNoteIt->second.SetOscillatorParameter(oscillatorIndex, parameter, values[static_cast<std::size_t>(oscillatorIndex)]);

  RebuildInterpolatedPresets();
  return true;
}

bool CompoundPreset::RemoveKeyNotePreset(int midiNote)
{
  if(mKeyNotePresets.size() <= 1)
    return false;

  const size_t numRemoved = mKeyNotePresets.erase(ClampMidiNote(midiNote));
  if(numRemoved > 0)
    RebuildInterpolatedPresets();

  return numRemoved > 0;
}

void CompoundPreset::ClearKeyNotePresets()
{
  mKeyNotePresets.clear();
  RebuildInterpolatedPresets();
}

void CompoundPreset::RebuildInterpolatedPresets()
{
  if(mKeyNotePresets.empty())
  {
    mInterpolatedPresets.fill(GetDefaultPreset());
    return;
  }

  if(mKeyNotePresets.size() == 1)
  {
    mInterpolatedPresets.fill(mKeyNotePresets.begin()->second);
    return;
  }

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

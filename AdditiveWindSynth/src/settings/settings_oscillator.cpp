#include "settings_oscillator.h"

#include "presets.h"

#include <algorithm>
#include <cmath>

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

bool CompoundPreset::HasKeyNotePreset(double midiNote) const
{
  return GetKeyNotePreset(midiNote) != nullptr;
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
    mInterpolatedPresets.fill(presets::sine);
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

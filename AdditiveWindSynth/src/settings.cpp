#include "settings.h"

#include "presets.h"

#include <algorithm>
#include <cmath>

OscillatorSettings OscillatorSettings::Interpolate(const OscillatorSettings& lo, const OscillatorSettings& hi, float t)
{
  const float clampedT = std::clamp(t, 0.f, 1.f);
  OscillatorSettings out{};
  out.intensity = lo.intensity + (hi.intensity - lo.intensity) * clampedT;
  out.attack = lo.attack + (hi.attack - lo.attack) * clampedT;
  out.release = lo.release + (hi.release - lo.release) * clampedT;
  out.pitch = lo.pitch + (hi.pitch - lo.pitch) * clampedT;
  out.pan = lo.pan + (hi.pan - lo.pan) * clampedT;
  out.intensity_variation_amplitude =
    lo.intensity_variation_amplitude +
    (hi.intensity_variation_amplitude - lo.intensity_variation_amplitude) * clampedT;
  out.intensity_variation_rate =
    lo.intensity_variation_rate +
    (hi.intensity_variation_rate - lo.intensity_variation_rate) * clampedT;
  out.pitch_variation_amplitude =
    lo.pitch_variation_amplitude +
    (hi.pitch_variation_amplitude - lo.pitch_variation_amplitude) * clampedT;
  out.pitch_variation_rate =
    lo.pitch_variation_rate +
    (hi.pitch_variation_rate - lo.pitch_variation_rate) * clampedT;
  out.pan_variation_amplitude =
    lo.pan_variation_amplitude +
    (hi.pan_variation_amplitude - lo.pan_variation_amplitude) * clampedT;
  out.pan_variation_rate =
    lo.pan_variation_rate +
    (hi.pan_variation_rate - lo.pan_variation_rate) * clampedT;
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

SimplePreset SimplePreset::Interpolate(const SimplePreset& lo, const SimplePreset& hi, float t)
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

int CompoundPreset::RoundAndClampMidiNote(float midiNote)
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

const OscillatorSettings& CompoundPreset::GetOscillatorSettings(float midiNote, int oscillatorIndex) const
{
  return GetPresetForMidiNote(midiNote).GetOscillatorSettings(oscillatorIndex);
}

const SimplePreset& CompoundPreset::GetPresetForMidiNote(float midiNote) const
{
  const int clampedNote = RoundAndClampMidiNote(midiNote);
  return mInterpolatedPresets[clampedNote - kMinMidiNote];
}

void CompoundPreset::SetKeyNotePreset(int midiNote, const SimplePreset& preset)
{
  mKeyNotePresets[ClampMidiNote(midiNote)] = preset;
  RebuildInterpolatedPresets();
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
    const float t = static_cast<float>(midiNote - lower->first) / static_cast<float>(upper->first - lower->first);
    mInterpolatedPresets[index] = SimplePreset::Interpolate(lower->second, upper->second, t);
  }
}

#pragma once

#include <array>
#include <initializer_list>
#include <map>
#include <utility>

class OscillatorSettings
{
public:
  OscillatorSettings() = default;
  constexpr OscillatorSettings(float intensityIn, float attackIn, float releaseIn, float panIn)
  : intensity(intensityIn), attack(attackIn), release(releaseIn), pan(panIn)
  {
  }

  float intensity{0.f};
  float attack{0.f};
  float release{0.f};
  float pan{0.f};

  static OscillatorSettings Interpolate(const OscillatorSettings& lo, const OscillatorSettings& hi, float t);
};

class SimplePreset
{
public:
  static constexpr int kNumOscillators = 100;
  using OscillatorArray = std::array<OscillatorSettings, kNumOscillators>;

  SimplePreset() = default;
  explicit SimplePreset(const OscillatorArray& oscillatorSettings);

  const OscillatorSettings& GetOscillatorSettings(int oscillatorIndex) const;
  void SetOscillatorSettings(int oscillatorIndex, const OscillatorSettings& settings);

  static SimplePreset Interpolate(const SimplePreset& lo, const SimplePreset& hi, float t);

private:
  static int ClampOscillatorIndex(int oscillatorIndex);

  OscillatorArray mOscillatorSettings{};
};

class CompoundPreset
{
public:
  using KeyNotePreset = std::pair<int, SimplePreset>;

  static constexpr int kMinMidiNote = 0;
  static constexpr int kMaxMidiNote = 127;
  static constexpr int kNumMidiNotes = kMaxMidiNote - kMinMidiNote + 1;

  CompoundPreset();
  explicit CompoundPreset(std::initializer_list<KeyNotePreset> keyNotePresets);

  const OscillatorSettings& GetOscillatorSettings(float midiNote, int oscillatorIndex) const;
  const SimplePreset& GetPresetForMidiNote(float midiNote) const;

  void SetKeyNotePreset(int midiNote, const SimplePreset& preset);
  bool RemoveKeyNotePreset(int midiNote);
  void ClearKeyNotePresets();

private:
  static int ClampMidiNote(int midiNote);
  static int RoundAndClampMidiNote(float midiNote);
  void RebuildInterpolatedPresets();

  std::map<int, SimplePreset> mKeyNotePresets{};
  std::array<SimplePreset, kNumMidiNotes> mInterpolatedPresets{};
};

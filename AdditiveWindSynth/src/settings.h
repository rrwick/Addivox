#pragma once

#include <array>
#include <initializer_list>
#include <map>
#include <utility>

class OscillatorSettings
{
public:
  enum class Parameter : int
  {
    intensity = 0,
    attack,
    release,
    pitch,
    pan,
    intensity_variation_amplitude,
    intensity_variation_rate,
    pitch_variation_amplitude,
    pitch_variation_rate,
    pan_variation_amplitude,
    pan_variation_rate,
    count
  };

  static constexpr int kNumParameters = static_cast<int>(Parameter::count);

  OscillatorSettings() = default;
  constexpr OscillatorSettings(float intensityIn,
                               float attackIn = 0.005f,
                               float releaseIn = 0.01f,
                               float pitchIn = 0.f,
                               float panIn = 0.f,
                               float intensityVariationAmplitudeIn = 0.25f,
                               float intensityVariationRateIn = 1.f,
                               float pitchVariationAmplitudeIn = 5.f,
                               float pitchVariationRateIn = 1.f,
                               float panVariationAmplitudeIn = 0.25f,
                               float panVariationRateIn = 1.f)
  : intensity(intensityIn)
  , attack(attackIn)
  , release(releaseIn)
  , pitch(pitchIn)
  , pan(panIn)
  , intensity_variation_amplitude(intensityVariationAmplitudeIn)
  , intensity_variation_rate(intensityVariationRateIn)
  , pitch_variation_amplitude(pitchVariationAmplitudeIn)
  , pitch_variation_rate(pitchVariationRateIn)
  , pan_variation_amplitude(panVariationAmplitudeIn)
  , pan_variation_rate(panVariationRateIn)
  {
  }

  float intensity{0.f};
  float attack{0.f};
  float release{0.f};
  float pitch{0.f};
  float pan{0.f};
  float intensity_variation_amplitude{0.f};
  float intensity_variation_rate{0.f};
  float pitch_variation_amplitude{0.f};
  float pitch_variation_rate{0.f};
  float pan_variation_amplitude{0.f};
  float pan_variation_rate{0.f};

  static constexpr std::array<Parameter, kNumParameters> AllParameters()
  {
    std::array<Parameter, kNumParameters> parameters{};
    for(int i = 0; i < kNumParameters; ++i)
      parameters[static_cast<std::size_t>(i)] = static_cast<Parameter>(i);

    return parameters;
  }

  static const char* GetParameterName(Parameter parameter);
  static const char* GetParameterUnit(Parameter parameter);
  float GetParameter(Parameter parameter) const;
  void SetParameter(Parameter parameter, float value);

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
  void SetOscillatorParameter(int oscillatorIndex, OscillatorSettings::Parameter parameter, float value);

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

// Oscillator-level settings are stored in three levels:
// - OscillatorSettings: settings for a single oscillator.
// - SimplePreset: settings for all 100 oscillators in a preset.
// - CompoundPreset: a collection of SimplePresets for different MIDI notes, with interpolation
//       between them.

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
    breath_power,
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
  constexpr OscillatorSettings(double intensityIn,
                               double breathPowerIn = 1.0,
                               double attackIn = 0.005,
                               double releaseIn = 0.01,
                               double pitchIn = 0.0,
                               double panIn = 0.0,
                               double intensityVariationAmplitudeIn = 0.25,
                               double intensityVariationRateIn = 1.0,
                               double pitchVariationAmplitudeIn = 5.0,
                               double pitchVariationRateIn = 1.0,
                               double panVariationAmplitudeIn = 0.25,
                               double panVariationRateIn = 1.0)
  : intensity(intensityIn)
  , breath_power(breathPowerIn)
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

  double intensity{0.0};
  double breath_power{1.0};
  double attack{0.0};
  double release{0.0};
  double pitch{0.0};
  double pan{0.0};
  double intensity_variation_amplitude{0.0};
  double intensity_variation_rate{0.0};
  double pitch_variation_amplitude{0.0};
  double pitch_variation_rate{0.0};
  double pan_variation_amplitude{0.0};
  double pan_variation_rate{0.0};

  static constexpr std::array<Parameter, kNumParameters> AllParameters()
  {
    std::array<Parameter, kNumParameters> parameters{};
    for(int i = 0; i < kNumParameters; ++i)
      parameters[static_cast<std::size_t>(i)] = static_cast<Parameter>(i);

    return parameters;
  }

  static const char* GetParameterName(Parameter parameter);
  static const char* GetParameterUnit(Parameter parameter);
  double GetParameter(Parameter parameter) const;
  void SetParameter(Parameter parameter, double value);

  static OscillatorSettings Interpolate(const OscillatorSettings& lo, const OscillatorSettings& hi, double t);
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
  void SetOscillatorParameter(int oscillatorIndex, OscillatorSettings::Parameter parameter, double value);
  double GetIntensityWaveformRms() const;
  bool ApplyIntensityTopTaper();
  bool ScaleIntensityUp();
  bool ScaleIntensityDown();
  bool ScaleIntensityUpEven();
  bool ScaleIntensityDownEven();
  bool ScaleIntensityUpOdd();
  bool ScaleIntensityDownOdd();
  bool SmoothIntensity();
  bool ZeroEvenIntensities();
  bool ZeroOddIntensities();
  bool NormalizeIntensityWaveformRms();

  static SimplePreset Interpolate(const SimplePreset& lo, const SimplePreset& hi, double t);

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

  const OscillatorSettings& GetOscillatorSettings(double midiNote, int oscillatorIndex) const;
  const SimplePreset& GetPresetForMidiNote(double midiNote) const;
  const SimplePreset* GetKeyNotePreset(double midiNote) const;
  bool HasKeyNotePreset(double midiNote) const;
  int GetNumKeyNotePresets() const;

  void SetKeyNotePreset(int midiNote, const SimplePreset& preset);
  bool SetKeyNoteOscillatorParameter(double midiNote,
                                     int oscillatorIndex,
                                     OscillatorSettings::Parameter parameter,
                                     double value);
  bool SetKeyNoteOscillatorParameterValues(
    double midiNote,
    OscillatorSettings::Parameter parameter,
    const std::array<double, SimplePreset::kNumOscillators>& values);
  bool RemoveKeyNotePreset(int midiNote);
  void ClearKeyNotePresets();

private:
  static int ClampMidiNote(int midiNote);
  static int RoundAndClampMidiNote(double midiNote);
  void RebuildInterpolatedPresets();

  std::map<int, SimplePreset> mKeyNotePresets{};
  std::array<SimplePreset, kNumMidiNotes> mInterpolatedPresets{};
};

// Oscillator-level settings are stored in three levels:
// - OscillatorSettings: settings for a single oscillator.
// - SimplePreset: settings for all 100 oscillators in a preset.
// - CompoundPreset: a collection of SimplePresets for different MIDI notes, with interpolation
//       between them.

#pragma once

#include "eq.h"

#include <array>
#include <cmath>
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
  const OscillatorArray& GetOscillatorSettingsArray() const;
  void SetOscillatorSettings(int oscillatorIndex, const OscillatorSettings& settings);
  void SetOscillatorParameter(int oscillatorIndex, OscillatorSettings::Parameter parameter, double value);
  double GetIntensityWaveformRms() const;
  bool ScaleOscillatorParameterAll(OscillatorSettings::Parameter parameter, double scale, double minValue, double maxValue);
  bool ScaleOscillatorParameterEven(OscillatorSettings::Parameter parameter, double scale, double minValue, double maxValue);
  bool ScaleOscillatorParameterOdd(OscillatorSettings::Parameter parameter, double scale, double minValue, double maxValue);
  bool ZeroEvenIntensities();
  bool ZeroOddIntensities();
  bool NormalizeIntensityWaveformRms();

  static SimplePreset Interpolate(const SimplePreset& lo, const SimplePreset& hi, double t);

private:
  static int ClampOscillatorIndex(int oscillatorIndex);

  OscillatorArray mOscillatorSettings{};
};

class NoiseBandProfile
{
public:
  static constexpr int kNumBands = 24;
  static constexpr int kNumBandEdges = kNumBands + 1;
  using BandValues = std::array<double, kNumBands>;
  using BandEdgeFrequencies = std::array<double, kNumBandEdges>;

  inline static constexpr BandEdgeFrequencies kBandEdgesHz{{
    20.00, 26.67, 35.57, 47.43, 63.25, 84.34, 112.5, 150.0, 200.0, 266.7, 355.7, 474.3,
    632.5, 843.4, 1124.7, 1500.0, 2000.0, 2667.0, 3557.0, 4743.0, 6325.0, 8434.0, 11250.0,
    15000.0, 20000.0
  }};

  NoiseBandProfile() = default;

  explicit constexpr NoiseBandProfile(const BandValues& values)
  : mValues(values)
  {
  }

  const BandValues& GetValues() const
  {
    return mValues;
  }

  double GetBandValue(int bandIndex) const
  {
    return mValues[ClampBandIndex(bandIndex)];
  }

  void SetBandValue(int bandIndex, double value)
  {
    mValues[ClampBandIndex(bandIndex)] = ClampBandValue(value);
  }

  void SetValues(BandValues values)
  {
    for(auto& value : values)
      value = ClampBandValue(value);

    mValues = values;
  }

  bool Empty() const
  {
    for(const double value : mValues)
    {
      if(value > 0.0)
        return false;
    }

    return true;
  }

  static constexpr int ClampBandIndex(int bandIndex)
  {
    return std::clamp(bandIndex, 0, kNumBands - 1);
  }

  static double ClampBandValue(double value)
  {
    if(!std::isfinite(value))
      return 0.0;

    return std::clamp(value, 0.0, 1.0);
  }

  static constexpr double GetBandEdgeFrequencyHz(int edgeIndex)
  {
    return kBandEdgesHz[edgeIndex < 0 ? 0 : (edgeIndex >= kNumBandEdges ? kNumBandEdges - 1 : edgeIndex)];
  }

  static constexpr double GetBandLowerFrequencyHz(int bandIndex)
  {
    return GetBandEdgeFrequencyHz(ClampBandIndex(bandIndex));
  }

  static constexpr double GetBandUpperFrequencyHz(int bandIndex)
  {
    return GetBandEdgeFrequencyHz(ClampBandIndex(bandIndex) + 1);
  }

  static double GetBandCenterFrequencyHz(int bandIndex)
  {
    return std::sqrt(GetBandLowerFrequencyHz(bandIndex) * GetBandUpperFrequencyHz(bandIndex));
  }

  static NoiseBandProfile Interpolate(const NoiseBandProfile& lo, const NoiseBandProfile& hi, double t)
  {
    const double clampedT = std::clamp(t, 0.0, 1.0);
    BandValues values{};
    for(int bandIndex = 0; bandIndex < kNumBands; ++bandIndex)
    {
      const auto index = static_cast<std::size_t>(bandIndex);
      values[index] =
        ClampBandValue(lo.mValues[index] + ((hi.mValues[index] - lo.mValues[index]) * clampedT));
    }

    return NoiseBandProfile{values};
  }

private:
  BandValues mValues{};
};

class CompoundPreset
{
public:
  using KeyNotePreset = std::pair<int, SimplePreset>;
  using OscillatorParameterValues = std::array<double, SimplePreset::kNumOscillators>;

  struct ResolvedNoteSpan
  {
    const SimplePreset* lowerPreset{nullptr};
    const SimplePreset* upperPreset{nullptr};
    const EqCurve* lowerEqCurve{nullptr};
    const EqCurve* upperEqCurve{nullptr};
    const NoiseBandProfile* lowerNoiseSustainProfile{nullptr};
    const NoiseBandProfile* upperNoiseSustainProfile{nullptr};
    double t{0.0};
  };

  static constexpr int kMinMidiNote = 0;
  static constexpr int kMaxMidiNote = 127;

  CompoundPreset();
  explicit CompoundPreset(std::initializer_list<KeyNotePreset> keyNotePresets);

  OscillatorSettings GetOscillatorSettings(double midiNote, int oscillatorIndex) const;
  SimplePreset GetPresetForMidiNote(double midiNote) const;
  const SimplePreset* GetKeyNotePreset(double midiNote) const;
  EqCurve GetEqCurveForMidiNote(double midiNote) const;
  const EqCurve* GetKeyNoteEqCurve(double midiNote) const;
  NoiseBandProfile GetNoiseSustainProfileForMidiNote(double midiNote) const;
  const NoiseBandProfile* GetKeyNoteNoiseSustainProfile(double midiNote) const;
  const std::map<int, SimplePreset>& GetKeyNotePresets() const;
  bool HasKeyNotePreset(double midiNote) const;
  int GetNumKeyNotePresets() const;
  bool IsAllKeyNotesEnabled(OscillatorSettings::Parameter parameter) const;
  const OscillatorParameterValues& GetAllKeyNotesValues(OscillatorSettings::Parameter parameter) const;
  bool IsAllKeyNotesEqEnabled() const;
  const EqCurve& GetAllKeyNotesEqCurve() const;
  bool IsAllKeyNotesNoiseSustainEnabled() const;
  const NoiseBandProfile& GetAllKeyNotesNoiseSustainProfile() const;
  ResolvedNoteSpan ResolveNoteSpan(double midiNote) const;
  OscillatorSettings InterpolateOscillatorSettings(const ResolvedNoteSpan& span, int oscillatorIndex) const;
  double EvaluateEqGain(const ResolvedNoteSpan& span, double frequencyHz) const;
  NoiseBandProfile InterpolateNoiseSustainProfile(const ResolvedNoteSpan& span) const;

  void SetKeyNotePreset(int midiNote, const SimplePreset& preset);
  bool SetKeyNoteOscillatorParameter(double midiNote,
                                     int oscillatorIndex,
                                     OscillatorSettings::Parameter parameter,
                                     double value);
  bool SetKeyNoteOscillatorParameterValues(
    double midiNote,
    OscillatorSettings::Parameter parameter,
    const std::array<double, SimplePreset::kNumOscillators>& values);
  bool SetKeyNoteEqCurve(double midiNote, const EqCurve& curve);
  bool SetKeyNoteNoiseSustainProfile(double midiNote, const NoiseBandProfile& profile);
  void EnableAllKeyNotes(OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values);
  void SetAllKeyNotesEnabled(OscillatorSettings::Parameter parameter,
                             bool enabled,
                             double sourceMidiNote = kMinMidiNote);
  void EnableAllKeyNotesEq(const EqCurve& curve);
  void SetAllKeyNotesEqEnabled(bool enabled);
  void EnableAllKeyNotesNoiseSustain(const NoiseBandProfile& profile);
  void SetAllKeyNotesNoiseSustainEnabled(bool enabled);
  bool RemoveKeyNotePreset(int midiNote);
  void ClearKeyNotePresets();

private:
  static int ClampMidiNote(int midiNote);
  static int RoundAndClampMidiNote(double midiNote);
  static std::size_t ParameterIndex(OscillatorSettings::Parameter parameter);
  void ApplyAllKeyNotesValues(SimplePreset& preset) const;
  void ApplyAllKeyNotesValues(SimplePreset& preset,
                              OscillatorSettings::Parameter parameter,
                              const OscillatorParameterValues& values) const;
  const EqCurve& GetKeyNoteEqCurveOrDefault(int midiNote) const;
  const NoiseBandProfile& GetKeyNoteNoiseSustainProfileOrDefault(int midiNote) const;
  void SetAllKeyNoteEqCurves(const EqCurve& curve);
  void SetAllKeyNoteNoiseSustainProfiles(const NoiseBandProfile& profile);

  std::map<int, SimplePreset> mKeyNotePresets{};
  std::map<int, EqCurve> mKeyNoteEqCurves{};
  std::map<int, NoiseBandProfile> mKeyNoteNoiseSustainProfiles{};
  std::array<bool, OscillatorSettings::kNumParameters> mAllKeyNotesEnabled{};
  std::array<OscillatorParameterValues, OscillatorSettings::kNumParameters> mAllKeyNotesValues{};
  bool mAllKeyNotesEqEnabled{false};
  EqCurve mAllKeyNotesEqCurve{};
  bool mAllKeyNotesNoiseSustainEnabled{false};
  NoiseBandProfile mAllKeyNotesNoiseSustainProfile{};
};

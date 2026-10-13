#pragma once

#include "eq.h"
#include "resonance.h"

#include <array>
#include <cmath>
#include <initializer_list>
#include <map>
#include <utility>
#include <vector>

class OscillatorSettings {
public:
  enum class Parameter : int {
    level = 0,
    breath_power,
    attack,
    release,
    pitch,
    pan,
    level_variation_amplitude,
    level_variation_rate,
    pitch_variation_amplitude,
    pitch_variation_rate,
    pan_variation_amplitude,
    pan_variation_rate,
    count
  };

  static constexpr int kNumParameters = static_cast<int>(Parameter::count);

  OscillatorSettings() = default;
  constexpr OscillatorSettings(double levelIn, double breathPowerIn = 1.0, double attackIn = 0.005, double releaseIn = 0.01, double pitchIn = 0.0,
                               double panIn = 0.0, double levelVariationAmplitudeIn = 0.25, double levelVariationRateIn = 1.0,
                               double pitchVariationAmplitudeIn = 5.0, double pitchVariationRateIn = 1.0, double panVariationAmplitudeIn = 0.25,
                               double panVariationRateIn = 1.0)
      : level(levelIn), breath_power(breathPowerIn), attack(attackIn), release(releaseIn), pitch(pitchIn), pan(panIn),
        level_variation_amplitude(levelVariationAmplitudeIn), level_variation_rate(levelVariationRateIn), pitch_variation_amplitude(pitchVariationAmplitudeIn),
        pitch_variation_rate(pitchVariationRateIn), pan_variation_amplitude(panVariationAmplitudeIn), pan_variation_rate(panVariationRateIn) {}

  double                       level {0.0};
  double                breath_power {1.0};
  double                      attack {0.0};
  double                     release {0.0};
  double                       pitch {0.0};
  double                         pan {0.0};
  double   level_variation_amplitude {0.0};
  double        level_variation_rate {0.0};
  double   pitch_variation_amplitude {0.0};
  double        pitch_variation_rate {0.0};
  double     pan_variation_amplitude {0.0};
  double          pan_variation_rate {0.0};

  static constexpr std::array<Parameter, kNumParameters> AllParameters() {
    std::array<Parameter, kNumParameters> parameters{};
    for (int i = 0; i < kNumParameters; ++i) parameters[static_cast<std::size_t>(i)] = static_cast<Parameter>(i);

    return parameters;
  }

  double GetParameter(Parameter parameter) const;
  void SetParameter(Parameter parameter, double value);

  // Clamps to the parameter range; non-finite values become zero.
  static double SanitizeParameter(Parameter parameter, double value);
};

class SimplePatch {
public:
  static constexpr int kNumOscillators = 100;
  // Tune these dBFS targets for Normalise, Set shape and Level macros, then rebuild.
  // Nominal sine-sum RMS before pan, modulation, EQ and global gain.
  static constexpr double kReferenceLevelWaveformRmsDbFS = -10.0;
  static constexpr double kLevelWaveformPeakDbFS = -0.25;
  inline static const double kLevelWaveformPeak = std::pow(10.0, kLevelWaveformPeakDbFS / 20.0);
  inline static const double kReferenceLevelWaveformRms = std::pow(10.0, kReferenceLevelWaveformRmsDbFS / 20.0);
  using OscillatorParameterValues = std::array<double, kNumOscillators>;
  using LevelArray = OscillatorParameterValues;
  using OscillatorArray = std::array<OscillatorSettings, kNumOscillators>;

  SimplePatch() = default;
  explicit SimplePatch(const OscillatorArray& oscillatorSettings);

  const OscillatorSettings& GetOscillatorSettings(int oscillatorIndex) const;
  const OscillatorArray& GetOscillatorSettingsArray() const;
  OscillatorParameterValues GetParameterValues(OscillatorSettings::Parameter parameter) const;
  void SetOscillatorParameter(int oscillatorIndex, OscillatorSettings::Parameter parameter, double value);
  bool NormalizeLevelWaveformRms();
  // Shared nominal waveform normalisation; leaves silence unchanged.
  static bool NormalizeLevels(LevelArray& levels);

  OscillatorSettings InterpolateOscillatorSettings(const SimplePatch& hi, int oscillatorIndex, double t) const;
  static SimplePatch Interpolate(const SimplePatch& lo, const SimplePatch& hi, double t);

private:
  static int ClampOscillatorIndex(int oscillatorIndex);

  void UpdateLevelCoordinates();

  OscillatorArray mOscillatorSettings{};
  LevelArray mLevelCoordinates{}; // Cached pseudo-log coordinates, updated only when levels change.
};

// Advisory editor metadata. Harmonic arrays remain the complete sound definition.
struct MacroSettings {
  int version{0};
  std::vector<double> positions;

  bool IsValid() const {
    if (version <= 0 || positions.empty()) return false;
    for (double value : positions)
      if (!std::isfinite(value) || value < 0.0 || value > 1.0) return false;
    return true;
  }
  bool operator==(const MacroSettings& other) const { return version == other.version && positions == other.positions; }
  bool operator!=(const MacroSettings& other) const { return !(*this == other); }
};

class CompoundPatch {
public:
  using KeyNotePatch = std::pair<int, SimplePatch>;
  using OscillatorParameterValues = SimplePatch::OscillatorParameterValues;

  struct ResolvedNoteSpan {
    const SimplePatch* lowerPatch{nullptr};
    const SimplePatch* upperPatch{nullptr};
    const EqCurve* lowerEqCurve{nullptr};
    const EqCurve* upperEqCurve{nullptr};
    const ResonanceSettings* lowerResonance{nullptr};
    const ResonanceSettings* upperResonance{nullptr};
    double t{0.0};
  };

  static constexpr int kMinMidiNote = 0;
  static constexpr int kMaxMidiNote = 127;

  CompoundPatch();
  explicit CompoundPatch(std::initializer_list<KeyNotePatch> keyNotePatches);

  SimplePatch GetPatchForMidiNote(double midiNote) const;
  const SimplePatch* GetKeyNotePatch(double midiNote) const;
  const EqCurve* GetKeyNoteEqCurve(double midiNote) const;
  const std::map<int, SimplePatch>& GetKeyNotePatches() const;
  bool HasKeyNotePatch(double midiNote) const;
  int GetNumKeyNotePatches() const;
  bool IsAllKeyNotesEnabled(OscillatorSettings::Parameter parameter) const;
  const OscillatorParameterValues& GetAllKeyNotesValues(OscillatorSettings::Parameter parameter) const;
  bool IsAllKeyNotesEqEnabled() const;
  const EqCurve& GetAllKeyNotesEqCurve() const;
  const ResonanceSettings* GetKeyNoteResonance(double midiNote) const;
  bool IsAllKeyNotesResonanceEnabled() const;
  const ResonanceSettings& GetAllKeyNotesResonance() const;
  ResolvedNoteSpan ResolveNoteSpan(double midiNote) const;
  OscillatorSettings InterpolateOscillatorSettings(const ResolvedNoteSpan& span, int oscillatorIndex) const;
  double EvaluateEqVisualY(const ResolvedNoteSpan& span, double frequencyHz) const;

  bool AddKeyNotePatch(double midiNote);
  void SetKeyNotePatch(int midiNote, const SimplePatch& patch);
  bool SetKeyNoteOscillatorParameter(double midiNote, int oscillatorIndex, OscillatorSettings::Parameter parameter, double value);
  bool SetKeyNoteOscillatorParameterValues(double midiNote, OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values,
                                           const MacroSettings& macros = {});
  const MacroSettings& GetMacroSettings(double midiNote, OscillatorSettings::Parameter parameter) const;
  void SetMacroSettings(double midiNote, OscillatorSettings::Parameter parameter, const MacroSettings& macros);
  bool SetKeyNoteEqCurve(double midiNote, const EqCurve& curve);
  bool SetKeyNoteResonance(double midiNote, const ResonanceSettings& resonance);
  void EnableAllKeyNotes(OscillatorSettings::Parameter parameter, const OscillatorParameterValues& values, const MacroSettings& macros = {});
  void SetAllKeyNotesEnabled(OscillatorSettings::Parameter parameter, bool enabled, double sourceMidiNote = kMinMidiNote);
  void EnableAllKeyNotesEq(const EqCurve& curve);
  void SetAllKeyNotesEqEnabled(bool enabled);
  void EnableAllKeyNotesResonance(const ResonanceSettings& resonance);
  bool RemoveKeyNotePatch(int midiNote);
  void ClearKeyNotePatches();

private:
  // A curve (or curve-based setup) stored per key note, which can instead be shared by all key notes.
  template <typename Curve> class KeyNoteCurves {
  public:
    const Curve* Find(int midiNote) const {
      const auto it = mKeyNoteCurves.find(midiNote);
      return it == mKeyNoteCurves.end() ? nullptr : &it->second;
    }

    const Curve& GetOrDefault(int midiNote) const {
      const Curve* curve = mAllKeyNotesEnabled ? &mAllKeyNotesCurve : Find(midiNote);
      return curve ? *curve : kDefaultCurve;
    }

    bool IsAllKeyNotesEnabled() const { return mAllKeyNotesEnabled; }
    const Curve& GetAllKeyNotesCurve() const { return mAllKeyNotesCurve; }

    void Set(int midiNote, const Curve& curve) {
      mKeyNoteCurves[midiNote] = curve;
      if (mAllKeyNotesEnabled) EnableAllKeyNotes(curve);
    }

    void EnableAllKeyNotes(const Curve& curve) {
      mAllKeyNotesCurve = curve;
      mAllKeyNotesEnabled = true;
      for (auto& [_, keyNoteCurve] : mKeyNoteCurves) keyNoteCurve = mAllKeyNotesCurve;
    }

    void SetAllKeyNotesEnabled(bool enabled) {
      mAllKeyNotesEnabled = false;
      if (enabled) EnableAllKeyNotes(mKeyNoteCurves.empty() ? mAllKeyNotesCurve : Curve{mKeyNoteCurves.begin()->second});
    }

    void Erase(int midiNote) { mKeyNoteCurves.erase(midiNote); }
    void Clear() { mKeyNoteCurves.clear(); }

  private:
    inline static const Curve kDefaultCurve{};
    std::map<int, Curve> mKeyNoteCurves{};
    Curve mAllKeyNotesCurve{};
    bool mAllKeyNotesEnabled{false};
  };

  static int ClampMidiNote(int midiNote);
  static int RoundAndClampMidiNote(double midiNote);
  static std::size_t ParameterIndex(OscillatorSettings::Parameter parameter);
  void ApplyAllKeyNotesValues(SimplePatch& patch) const;

  std::map<int, SimplePatch> mKeyNotePatches{};
  KeyNoteCurves<EqCurve> mEqCurves{};
  KeyNoteCurves<ResonanceSettings> mResonance{};
  using TabMacroSettings = std::array<MacroSettings, OscillatorSettings::kNumParameters>;
  std::map<int, TabMacroSettings> mKeyNoteMacros{};
  TabMacroSettings mAllKeyNotesMacros{};
  std::array<bool, OscillatorSettings::kNumParameters> mAllKeyNotesEnabled{};
  std::array<OscillatorParameterValues, OscillatorSettings::kNumParameters> mAllKeyNotesValues{};
};

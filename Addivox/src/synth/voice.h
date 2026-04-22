#pragma once

#include <array>
#include <cstdint>
#include <limits>

#include "IPlugConstants.h"
#include "oscillator.h"
#include "../dsp/shared.h"
#include "../settings/global.h"
#include "../settings/oscillator.h"

class SynthVoice
{
public:
  using VisualizerFrame = HarmonicVisualizerFrame;

  SynthVoice();
  bool IsActive() const;
  void Start(double pitch, double pitchBend, double breath, double previousBreath = 0.0);
  void Stop();
  void SetPitchBend(double pitchBend);
  void SetBreath(double breath);
  void SetPortamentoControl(double control);
  void SetTransposeSemitones(double transposeSemitones);
  void SetGlobalVoiceSettings(const GlobalVoiceSettings& settings);
  void SetCompoundPreset(const CompoundPreset& preset);
  bool AddKeyNotePreset(double midiNote);
  bool RemoveKeyNotePreset(double midiNote);
  bool SetKeyNoteOscillatorParameter(double midiNote,
                                     int oscillatorIndex,
                                     OscillatorSettings::Parameter parameter,
                                     double value);
  bool SetKeyNoteOscillatorParameterValues(
    double midiNote,
    OscillatorSettings::Parameter parameter,
    const std::array<double, SimplePreset::kNumOscillators>& values);
  bool SetKeyNoteEqCurve(double midiNote, const EqCurve& curve);
  bool SetKeyNoteNoiseAttackProfile(double midiNote, const NoiseBandProfile& profile);
  bool SetKeyNoteNoiseSustainProfile(double midiNote, const NoiseBandProfile& profile);
  bool SetAllKeyNotesEnabled(OscillatorSettings::Parameter parameter, bool enabled, double midiNote);
  bool SetAllKeyNotesEqEnabled(bool enabled);
  bool SetAllKeyNotesNoiseAttackEnabled(bool enabled);
  bool SetAllKeyNotesNoiseSustainEnabled(bool enabled);
  void Clear();
  void ProcessSamplesAccumulating(iplug::sample** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);
  void GetVisualizerFrame(VisualizerFrame& frame) const;

private:
  struct NoiseComponent
  {
    int bandIndex{0};
    double frequencyHz{0.0};
    double phase{0.0};
    double phaseIncrement{0.0};
    double lifecycleProgress{0.0};
    double lifecycleIncrement{0.0};
    double randomPan{0.0};
    double panLeftGain{0.70710678118654752440};
    double panRightGain{0.70710678118654752440};
    double eqGain{1.0};
  };

  double GetTargetMidiPitch() const;
  void UpdatePitch();
  void UpdateLevels();
  void UpdateLevels(const CompoundPreset::ResolvedNoteSpan& noteSpan);
  void UpdateNoiseTargets(const CompoundPreset::ResolvedNoteSpan& noteSpan);
  void ResetNoiseState();
  void RespawnNoiseComponent(NoiseComponent& component,
                             const CompoundPreset::ResolvedNoteSpan& noteSpan,
                             bool randomLifecycle);
  void UpdateNoiseComponentRates();
  void UpdateNoiseComponentEqGains(const CompoundPreset::ResolvedNoteSpan& noteSpan);
  void UpdateNoiseComponentPanGains();
  void UpdateNoiseSustainGainSmoothing();
  void UpdateNoiseAttackTargetSmoothing();
  void UpdateNoiseAttackDetectorSmoothing();
  void UpdateNoiseAttackTransientRise();
  void UpdateNoiseAttackTransientDecay();
  std::array<double, 2> ProcessNoise();
  static double NextNoiseRandomUnit(uint32_t& state);
  static double NextNoiseRandomSignedUnit(uint32_t& state);
  double RandomNoiseFrequencyHz(int bandIndex);
  double GetNoiseComponentPan(double randomPan) const;
  static double EvaluateNoiseLifecycleLevel(double lifecycleProgress);
  void UpdatePitchRate();
  void RefreshNoteDependentState(int lookAheadSamples);
  void AdvanceRenderedPitch(int numSamples);
  double PredictRenderedMidiPitch(int numSamples) const;
  double GetPortamentoTimeSec() const;
  double SmoothBreath(double breath);
  static double AdvancePitchTowards(double currentPitch, double targetPitch, double maxDeltaSemitones);
  static double GetOscillatorBasePitchSemitones(int harmonic,
                                                const OscillatorSettings& settings,
                                                double fundamentalPitchSemitones,
                                                const GlobalVoiceSettings& globalSettings);
  static double PitchSemitonesToFrequencyHz(double pitchSemitones);
  void ApplyOscillatorSettings(int harmonic,
                               const OscillatorSettings& currentSettings,
                               const OscillatorSettings& futurePitchSettings,
                               double futureFundamentalPitchSemitones);

  static constexpr int kNumHarmonics = SimplePreset::kNumOscillators;
  static constexpr int kNumNoiseBands = NoiseBandProfile::kNumBands;
  static constexpr int kNoiseComponentsPerBand = VisualizerFrame::kNoiseComponentsPerBand;
  static constexpr int kNumNoiseComponents = VisualizerFrame::kNumNoiseComponents;
  static constexpr int kNoteControlIntervalSamples = 16;

  // Pitch is in MIDI note numbers (0-127), where 69 corresponds to A4 (440 Hz).
  double mNotePitch{0.0};

  // Pitch bend is a semitone offset in MIDI-note units.
  double mPitchBend{0.0};

  double mTransposeSemitones{0.0};
  double mRenderedMidiPitch{0.0};
  double mTargetMidiPitch{0.0};
  double mSampleRate{44100.0};
  double mPitchRatePerSample{std::numeric_limits<double>::infinity()};
  int mNoteControlSamplesUntilUpdate{0};

  // Raw breath is the controller input in the range [0, 1]. mBreath is the smoothed breath.
  double mRawBreath{0.0};
  double mBreath{0.0};
  double mPortamentoControl{0.0};

  std::array<Oscillator, kNumHarmonics> mOscs;
  std::array<NoiseComponent, kNumNoiseComponents> mNoiseComponents{};
  std::array<double, kNumNoiseBands> mNoiseAttackBandWeights{};
  std::array<double, kNumNoiseBands> mNoiseSustainBandGains{};
  std::array<double, kNumNoiseBands> mTargetNoiseSustainBandGains{};
  double mNoiseAttackTargetBreathLevel{0.0};
  double mNoiseAttackSmoothedTargetBreathLevel{0.0};
  double mNoiseAttackDetectorBreath{0.0};
  double mNoiseAttackTargetSmoothingCoefficient{1.0};
  double mNoiseAttackDetectorSmoothingCoefficient{1.0};
  double mNoiseAttackTransient{0.0};
  double mNoiseAttackTransientRiseCoefficient{1.0};
  double mNoiseAttackTransientDecayCoefficient{1.0};
  double mNoiseSustainGainSmoothingCoefficient{1.0};
  uint32_t mNoiseRandomState{0xC1A551E5u};
  CompoundPreset mCompoundPreset;
  GlobalVoiceSettings mGlobalVoiceSettings;
};

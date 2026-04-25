#pragma once

#include <array>
#include <limits>

#include "IPlugConstants.h"
#include "oscillator.h"
#include "../settings/global.h"
#include "../settings/oscillator.h"

class SynthVoice
{
public:
  using VisualizerFrame = HarmonicVisualizerFrame;

  SynthVoice();
  bool IsActive() const;
  void Start(double pitch, double pitchBend, double breath);
  void Stop();
  void SetPitchBend(double pitchBend);
  void SetBreath(double breath);
  void SetPortamentoControl(double control);
  void SetTransposeSemitones(double transposeSemitones);
  void SetGlobalVoiceSettings(const GlobalVoiceSettings& settings);
  void SetCompoundPatch(const CompoundPatch& patch);
  bool AddKeyNotePatch(double midiNote);
  bool RemoveKeyNotePatch(double midiNote);
  bool SetKeyNoteOscillatorParameter(double midiNote,
                                     int oscillatorIndex,
                                     OscillatorSettings::Parameter parameter,
                                     double value);
  bool SetKeyNoteOscillatorParameterValues(
    double midiNote,
    OscillatorSettings::Parameter parameter,
    const std::array<double, SimplePatch::kNumOscillators>& values);
  bool SetKeyNoteEqCurve(double midiNote, const EqCurve& curve);
  bool SetAllKeyNotesEnabled(OscillatorSettings::Parameter parameter, bool enabled, double midiNote);
  bool SetAllKeyNotesEqEnabled(bool enabled);
  void Clear();
  void ProcessSamplesAccumulating(iplug::sample** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);
  void GetVisualizerFrame(VisualizerFrame& frame) const;

private:
  double GetTargetMidiPitch() const;
  void UpdatePitch();
  void UpdateLevels();
  void UpdateLevels(const CompoundPatch::ResolvedNoteSpan& noteSpan);
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

  static constexpr int kNumHarmonics = SimplePatch::kNumOscillators;
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

  // Breath is a linear value from 0 to 1.
  double mBreath{0.0};
  double mPortamentoControl{0.0};

  std::array<Oscillator, kNumHarmonics> mOscs;
  CompoundPatch mCompoundPatch;
  GlobalVoiceSettings mGlobalVoiceSettings;
};

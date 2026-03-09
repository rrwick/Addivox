#pragma once

#include <array>

#include "IPlugConstants.h"
#include "oscillator.h"
#include "../settings/settings_global.h"
#include "../settings/settings_oscillator.h"

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
  void SetGlobalVoiceSettings(const GlobalVoiceSettings& settings);
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
  void Clear();
  void ProcessSamplesAccumulating(iplug::sample** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);
  void GetVisualizerFrame(VisualizerFrame& frame) const;

private:
  void UpdatePitch();
  void UpdateLevels();
  double GetPortamentoTimeSec() const;
  double SmoothBreath(double breath);
  void ApplyOscillatorSettings(int harmonic, const OscillatorSettings& settings, double fundamentalPitchSemitones);

  static constexpr int kNumHarmonics = SimplePreset::kNumOscillators;

  // Pitch is in MIDI note numbers (0-127), where 69 corresponds to A4 (440 Hz).
  double mPitch{0.0};

  // Pitch bend is a semitone offset in MIDI-note units.
  double mPitchBend{0.0};

  // Breath is a linear value from 0 to 1.
  double mBreath{0.0};
  double mPortamentoControl{0.0};

  std::array<Oscillator, kNumHarmonics> mOscs;
  CompoundPreset mCompoundPreset;
  GlobalVoiceSettings mGlobalVoiceSettings;
};

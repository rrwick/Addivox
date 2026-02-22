#pragma once

#include <array>

#include "oscillator.h"
#include "settings.h"

struct GlobalOscillatorModifiers
{
  float attackScale{1.f};
  float releaseScale{1.f};
  float pitchOffsetCents{0.f};
  float panOffset{0.f};
  float intensityVariationAmplitudeScale{1.f};
  float intensityVariationRateScale{1.f};
  float pitchVariationAmplitudeScale{1.f};
  float pitchVariationRateScale{1.f};
  float panVariationAmplitudeScale{1.f};
  float panVariationRateScale{1.f};
  float portamentoTimeAtCC5MinSec{0.f};
  float portamentoTimeAtCC5MaxSec{0.f};
};

template<typename T>
class SynthVoice
{
public:
  using VisualizerFrame = HarmonicVisualizerFrame;

  SynthVoice();
  bool IsActive() const;
  void Start(float pitch, float pitchBend, float breath);
  void Stop();
  void SetPitchBend(float pitchBend);
  void SetBreath(float breath);
  void SetPortamentoControl(float control);
  void SetGlobalOscillatorModifiers(const GlobalOscillatorModifiers& modifiers);
  void Clear();
  void ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);
  void GetVisualizerFrame(VisualizerFrame& frame) const;

private:
  void UpdateFrequency();
  void UpdateLevels();
  float GetPortamentoTimeSec() const;
  float SmoothBreath(float breath);
  static float MidiPitchToFrequency(float midiPitch);
  void ApplyOscillatorSettings(int harmonic, const OscillatorSettings& settings, float fundamentalFreq);

  static constexpr int kNumHarmonics = SimplePreset::kNumOscillators;

  // Pitch is in MIDI note numbers (0-127), where 69 corresponds to A4 (440 Hz).
  float mPitch{0.f};

  // Pitch bend is a semitone offset in MIDI-note units.
  float mPitchBend{0.f};

  // Breath is a linear value from 0 to 1.
  float mBreath{0.f};
  float mPortamentoControl{0.f};

  std::array<Oscillator, kNumHarmonics> mOscs;
  CompoundPreset mCompoundPreset;
  GlobalOscillatorModifiers mGlobalOscillatorModifiers;
};

extern template class SynthVoice<float>;
extern template class SynthVoice<double>;

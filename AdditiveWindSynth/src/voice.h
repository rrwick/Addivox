#pragma once

#include <array>

#include "oscillator.h"
#include "settings.h"

struct GlobalOscillatorModifiers
{
  float attackScale{1.f};
  float releaseScale{1.f};
  double pitchOffsetCents{0.0};
  float panOffset{0.f};
  float intensityVariationAmplitudeScale{1.f};
  float intensityVariationRateScale{1.f};
  double pitchVariationAmplitudeScale{1.0};
  double pitchVariationRateScale{1.0};
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
  void Start(double pitch, double pitchBend, float breath);
  void Stop();
  void SetPitchBend(double pitchBend);
  void SetBreath(float breath);
  void SetPortamentoControl(float control);
  void SetGlobalOscillatorModifiers(const GlobalOscillatorModifiers& modifiers);
  void Clear();
  void ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);
  void GetVisualizerFrame(VisualizerFrame& frame) const;

private:
  void UpdatePitch();
  void UpdateLevels();
  double GetPortamentoTimeSec() const;
  float SmoothBreath(float breath);
  void ApplyOscillatorSettings(int harmonic, const OscillatorSettings& settings, double fundamentalPitchCents);

  static constexpr int kNumHarmonics = SimplePreset::kNumOscillators;

  // Pitch is in MIDI note numbers (0-127), where 69 corresponds to A4 (440 Hz).
  double mPitch{0.0};

  // Pitch bend is a semitone offset in MIDI-note units.
  double mPitchBend{0.0};

  // Breath is a linear value from 0 to 1.
  float mBreath{0.f};
  float mPortamentoControl{0.f};

  std::array<Oscillator, kNumHarmonics> mOscs;
  CompoundPreset mCompoundPreset;
  GlobalOscillatorModifiers mGlobalOscillatorModifiers;
};

extern template class SynthVoice<float>;
extern template class SynthVoice<double>;

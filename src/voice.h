#pragma once

#include <array>

#include "harmonics.h"
#include "oscillator.h"

template<typename T>
class SynthVoice
{
public:
  bool IsActive() const;
  void Start(float pitch, float pitchBend, float breath);
  void Stop();
  void SetPitchBend(float pitchBend);
  void SetBreath(float breath);
  void Clear();
  void ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);

private:
  void UpdateFrequency();
  void UpdateLevels();

  static constexpr int kNumHarmonics = HarmonicIntensities::kNumHarmonics;

  // Pitch is in MIDI note numbers (0-127), where 69 corresponds to A4 (440 Hz).
  float mPitch{0.f};

  // Pitch bend is a semitone offset in MIDI-note units.
  float mPitchBend{0.f};

  // Breath is a linear value from 0 to 1.
  float mBreath{0.f};

  std::array<Oscillator, kNumHarmonics> mOscs;
  HarmonicIntensities::Spectrum mHarmonicIntensities{};
};

extern template class SynthVoice<float>;
extern template class SynthVoice<double>;

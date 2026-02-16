#pragma once

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

  float mPitch{0.f};
  float mPitchBend{0.f};
  Oscillator mOsc;
};

extern template class SynthVoice<float>;
extern template class SynthVoice<double>;

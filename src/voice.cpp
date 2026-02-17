#include "voice.h"

#include <cmath>

template<typename T>
bool SynthVoice<T>::IsActive() const
{
  for(const auto& osc : mOscs)
  {
    if(osc.IsActive())
      return true;
  }
  return false;
}

template<typename T>
void SynthVoice<T>::Start(float pitch, float pitchBend, float breath)
{
  const bool retrigger = !IsActive();
  mPitch = pitch;
  mPitchBend = pitchBend;
  UpdateFrequency();
  SetBreath(breath);

  if(retrigger)
  {
    for(auto& osc : mOscs)
      osc.Reset();
  }
}

template<typename T>
void SynthVoice<T>::Stop()
{
  SetBreath(0.f);
}

template<typename T>
void SynthVoice<T>::SetPitchBend(float pitchBend)
{
  mPitchBend = pitchBend;
  UpdateFrequency();
}

template<typename T>
void SynthVoice<T>::SetBreath(float breath)
{
  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
    mOscs[harmonic].SetLevel(breath * kHarmonicIntensities[harmonic]);
}

template<typename T>
void SynthVoice<T>::Clear()
{
  mPitch = 0.f;
  mPitchBend = 0.f;
  Stop();
}

template<typename T>
void SynthVoice<T>::UpdateFrequency()
{
  const float fundamentalFreq = 440.f * std::exp2(mPitch + mPitchBend);
  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
    mOscs[harmonic].SetFrequency(fundamentalFreq * static_cast<float>(harmonic + 1));
}

template<typename T>
void SynthVoice<T>::ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames)
{
  const int endIdx = startIdx + nFrames;
  for(int i = startIdx; i < endIdx; i++)
  {
    T sample = 0;
    for(auto& osc : mOscs)
      sample += static_cast<T>(osc.Process());
    outputs[0][i] += sample;
    outputs[1][i] += sample;
  }
}

template<typename T>
void SynthVoice<T>::SetSampleRate(double sampleRate)
{
  for(auto& osc : mOscs)
    osc.SetSampleRate(sampleRate);
}

template class SynthVoice<float>;
template class SynthVoice<double>;

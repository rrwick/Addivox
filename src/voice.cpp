#include "voice.h"

#include <cmath>

template<typename T>
bool SynthVoice<T>::IsActive() const
{
  return mOsc.IsActive();
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
    mOsc.Reset();
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
  mOsc.SetLevel(breath);
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
  mOsc.SetFrequency(fundamentalFreq);
}

template<typename T>
void SynthVoice<T>::ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames)
{
  const int endIdx = startIdx + nFrames;
  for(int i = startIdx; i < endIdx; i++)
  {
    const T sample = static_cast<T>(mOsc.Process());
    outputs[0][i] += sample;
    outputs[1][i] += sample;
  }
}

template<typename T>
void SynthVoice<T>::SetSampleRate(double sampleRate)
{
  mOsc.SetSampleRate(sampleRate);
}

template class SynthVoice<float>;
template class SynthVoice<double>;

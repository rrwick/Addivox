#include "voice.h"

#include <cmath>

template<typename T>
bool SynthVoice<T>::GetBusy() const
{
  return mNoteOn;
}

template<typename T>
void SynthVoice<T>::Trigger()
{
  mOSC.Reset();
}

template<typename T>
void SynthVoice<T>::ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames)
{
  const float osc1Freq = 440.f * std::pow(2.f, mPitch + mPitchBend);
  mOSC.SetFrequency(osc1Freq);

  for(int i = startIdx; i < startIdx + nFrames; i++)
  {
    const T sample = static_cast<T>(mOSC.Process() * mBreath);
    outputs[0][i] += sample;
    outputs[1][i] += sample;
  }
}

template<typename T>
void SynthVoice<T>::SetSampleRate(double sampleRate)
{
  mOSC.SetSampleRate(sampleRate);
}

template class SynthVoice<float>;
template class SynthVoice<double>;

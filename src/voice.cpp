#include "voice.h"

#include <cmath>

template<typename T>
bool SynthVoice<T>::GetBusy() const
{
  return mOsc.IsActive();
}

template<typename T>
void SynthVoice<T>::Trigger()
{
  mOsc.Reset();
  mOsc.SetLevel(mBreath);
}

template<typename T>
void SynthVoice<T>::ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames)
{
  const float osc1Freq = 440.f * std::pow(2.f, mPitch + mPitchBend);
  mOsc.SetFrequency(osc1Freq);
  mOsc.SetLevel(mBreath);

  for(int i = startIdx; i < startIdx + nFrames; i++)
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

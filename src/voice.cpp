#include "voice.h"

#include <cmath>

namespace
{
const HarmonicIntensities& GetHarmonicIntensities()
{
  static const HarmonicIntensities sHarmonicIntensities;
  return sHarmonicIntensities;
}
} // namespace

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
  mBreath = breath;
  UpdateFrequency();

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
  mBreath = breath;
  UpdateLevels();
}

template<typename T>
void SynthVoice<T>::Clear()
{
  mPitch = 0.f;
  mPitchBend = 0.f;
  mBreath = 0.f;
  Stop();
}

template<typename T>
void SynthVoice<T>::UpdateFrequency()
{
  const float midiPitch = mPitch + mPitchBend;
  const float fundamentalFreq = 440.f * std::exp2((midiPitch - 69.f) / 12.f);
  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
    mOscs[harmonic].SetFrequency(fundamentalFreq * static_cast<float>(harmonic + 1));

  mHarmonicIntensities = GetHarmonicIntensities().GetIntensities(midiPitch);
  UpdateLevels();
}

template<typename T>
void SynthVoice<T>::UpdateLevels()
{
  float breathPower = mBreath;
  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
  {
    mOscs[harmonic].SetLevel(mHarmonicIntensities[harmonic] * breathPower);
    breathPower *= mBreath;
  }
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

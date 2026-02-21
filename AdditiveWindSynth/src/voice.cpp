#include "voice.h"

#include "presets.h"

#include <cstdint>
#include <cmath>

template<typename T>
SynthVoice<T>::SynthVoice()
: mCompoundPreset(presets::MakeBrassCompoundPreset())
{
  const uint32_t voiceSeed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
  {
    const uint32_t harmonicSeed = voiceSeed ^ (0x9E3779B9u * static_cast<uint32_t>(harmonic + 1));
    mOscs[harmonic].SetVariationSeed(harmonicSeed);
  }
}

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
  mBreath = SmoothBreath(breath);
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
float SynthVoice<T>::MidiPitchToFrequency(float midiPitch)
{
  return 440.f * std::exp2((midiPitch - 69.f) / 12.f);
}

template<typename T>
void SynthVoice<T>::ApplyOscillatorSettings(int harmonic, const OscillatorSettings& settings, float fundamentalFreq)
{
  mOscs[harmonic].SetFrequency(fundamentalFreq * static_cast<float>(harmonic + 1));
  mOscs[harmonic].SetPitch(settings.pitch);
  mOscs[harmonic].SetPitchVariation(settings.pitch_variation_amplitude, settings.pitch_variation_rate);
  mOscs[harmonic].SetAttackTime(settings.attack);
  mOscs[harmonic].SetReleaseTime(settings.release);
  mOscs[harmonic].SetPan(settings.pan);
  mOscs[harmonic].SetPanVariation(settings.pan_variation_amplitude, settings.pan_variation_rate);
  mOscs[harmonic].SetIntensityVariation(settings.intensity_variation_amplitude, settings.intensity_variation_rate);
}

template<typename T>
void SynthVoice<T>::SetBreath(float breath)
{
  const float smoothedBreath = SmoothBreath(breath);
  if(smoothedBreath == mBreath)
    return;

  mBreath = smoothedBreath;
  UpdateLevels();
}

template<typename T>
float SynthVoice<T>::SmoothBreath(float breath)
{
  // Input and output breath are in the range [0, 1].

  // Breath is smoothed near zero to avoid obvious note-on at low breath values, but is near-linear
  // for the upper part of the range.
  // https://www.desmos.com/calculator/ntwh4mbkwn
  constexpr float k = 5.0f;
  static const float inv_denom = 1.0f / (k - 1.0f + std::exp(-k));
  breath = (k * breath - 1.0f + std::exp(-k * breath)) * inv_denom;

  // Cap breath at 97% to avoid ringing tone from brick wall at top of harmonic series.
  // TODO: this is a bit of a hack - would be better to use a more gradual roll-off of the highest
  //       harmonic intensities.
  breath *= 0.97f;

  return breath;
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
  const float fundamentalFreq = MidiPitchToFrequency(midiPitch);
  const SimplePreset& preset = mCompoundPreset.GetPresetForMidiNote(midiPitch);

  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
  {
    const OscillatorSettings& settings = preset.GetOscillatorSettings(harmonic);
    ApplyOscillatorSettings(harmonic, settings, fundamentalFreq);
  }

  UpdateLevels();
}

template<typename T>
void SynthVoice<T>::UpdateLevels()
{
  const float midiPitch = mPitch + mPitchBend;
  const SimplePreset& preset = mCompoundPreset.GetPresetForMidiNote(midiPitch);

  // Levels are scaled by breath ^ harmonicNumber, which dampens higher harmonics more than lower
  // ones, giving a bit of a low-pass filter effect as breath is reduced.
  float breathPower = mBreath;
  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
  {
    const OscillatorSettings& settings = preset.GetOscillatorSettings(harmonic);
    mOscs[harmonic].SetLevel(settings.intensity * breathPower);
    breathPower *= mBreath;
  }
}

template<typename T>
void SynthVoice<T>::ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames)
{
  const int endIdx = startIdx + nFrames;
  for(int i = startIdx; i < endIdx; i++)
  {
    T leftSample = 0;
    T rightSample = 0;
    for(auto& osc : mOscs)
    {
      const auto sample = osc.Process();
      leftSample += static_cast<T>(sample[0]);
      rightSample += static_cast<T>(sample[1]);
    }
    outputs[0][i] += leftSample;
    outputs[1][i] += rightSample;
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

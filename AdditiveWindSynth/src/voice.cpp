#include "voice.h"

#include "presets.h"

#include <algorithm>
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
void SynthVoice<T>::Start(double pitch, double pitchBend, float breath)
{
  const bool retrigger = !IsActive();
  mPitch = pitch;
  mPitchBend = pitchBend;
  mBreath = SmoothBreath(breath);
  UpdatePitch();

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
void SynthVoice<T>::SetPitchBend(double pitchBend)
{
  mPitchBend = pitchBend;
  UpdatePitch();
}

template<typename T>
void SynthVoice<T>::ApplyOscillatorSettings(int harmonic, const OscillatorSettings& settings, double fundamentalPitchCents)
{
  const double harmonicPitchOffsetCents = 1200.0 * std::log2(static_cast<double>(harmonic + 1));
  const double totalPitchCents = fundamentalPitchCents + harmonicPitchOffsetCents
    + settings.pitch + mGlobalOscillatorModifiers.pitchOffsetCents;
  mOscs[harmonic].SetPitch(totalPitchCents);
  mOscs[harmonic].SetPitchTime(GetPortamentoTimeSec());
  mOscs[harmonic].SetPitchVariation(
    settings.pitch_variation_amplitude * mGlobalOscillatorModifiers.pitchVariationAmplitudeScale,
    settings.pitch_variation_rate * mGlobalOscillatorModifiers.pitchVariationRateScale);
  mOscs[harmonic].SetAttackTime(settings.attack * mGlobalOscillatorModifiers.attackScale);
  mOscs[harmonic].SetReleaseTime(settings.release * mGlobalOscillatorModifiers.releaseScale);
  mOscs[harmonic].SetPan(static_cast<float>(
    std::clamp(settings.pan + static_cast<double>(mGlobalOscillatorModifiers.panOffset), -1.0, 1.0)));
  mOscs[harmonic].SetPanVariation(
    settings.pan_variation_amplitude * mGlobalOscillatorModifiers.panVariationAmplitudeScale,
    settings.pan_variation_rate * mGlobalOscillatorModifiers.panVariationRateScale);
  mOscs[harmonic].SetIntensityVariation(
    settings.intensity_variation_amplitude * mGlobalOscillatorModifiers.intensityVariationAmplitudeScale,
    settings.intensity_variation_rate * mGlobalOscillatorModifiers.intensityVariationRateScale);
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
void SynthVoice<T>::SetPortamentoControl(float control)
{
  const float clampedControl = std::clamp(control, 0.f, 1.f);
  mPortamentoControl = clampedControl;
  const double pitchTimeSec = GetPortamentoTimeSec();
  for(auto& osc : mOscs)
    osc.SetPitchTime(pitchTimeSec);
}

template<typename T>
void SynthVoice<T>::SetGlobalOscillatorModifiers(const GlobalOscillatorModifiers& modifiers)
{
  mGlobalOscillatorModifiers.attackScale = std::max(0.f, modifiers.attackScale);
  mGlobalOscillatorModifiers.releaseScale = std::max(0.f, modifiers.releaseScale);
  mGlobalOscillatorModifiers.pitchOffsetCents = modifiers.pitchOffsetCents;
  mGlobalOscillatorModifiers.panOffset = std::clamp(modifiers.panOffset, -1.f, 1.f);
  mGlobalOscillatorModifiers.intensityVariationAmplitudeScale = std::max(0.f, modifiers.intensityVariationAmplitudeScale);
  mGlobalOscillatorModifiers.intensityVariationRateScale = std::max(0.f, modifiers.intensityVariationRateScale);
  mGlobalOscillatorModifiers.pitchVariationAmplitudeScale = std::max(0.0, modifiers.pitchVariationAmplitudeScale);
  mGlobalOscillatorModifiers.pitchVariationRateScale = std::max(0.0, modifiers.pitchVariationRateScale);
  mGlobalOscillatorModifiers.panVariationAmplitudeScale = std::max(0.f, modifiers.panVariationAmplitudeScale);
  mGlobalOscillatorModifiers.panVariationRateScale = std::max(0.f, modifiers.panVariationRateScale);
  mGlobalOscillatorModifiers.portamentoTimeAtCC5MinSec = std::max(0.f, modifiers.portamentoTimeAtCC5MinSec);
  mGlobalOscillatorModifiers.portamentoTimeAtCC5MaxSec = std::max(0.f, modifiers.portamentoTimeAtCC5MaxSec);

  UpdatePitch();
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
  mPitch = 0.0;
  mPitchBend = 0.0;
  mBreath = 0.f;
  mPortamentoControl = 0.f;
  Stop();
}

template<typename T>
double SynthVoice<T>::GetPortamentoTimeSec() const
{
  return mGlobalOscillatorModifiers.portamentoTimeAtCC5MinSec
    + (mGlobalOscillatorModifiers.portamentoTimeAtCC5MaxSec - mGlobalOscillatorModifiers.portamentoTimeAtCC5MinSec)
      * mPortamentoControl;
}

template<typename T>
void SynthVoice<T>::UpdatePitch()
{
  const double midiPitch = mPitch + mPitchBend;
  const double fundamentalPitchCents = (midiPitch - 69.0) * 100.0;
  const SimplePreset& preset = mCompoundPreset.GetPresetForMidiNote(midiPitch);

  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
  {
    const OscillatorSettings& settings = preset.GetOscillatorSettings(harmonic);
    ApplyOscillatorSettings(harmonic, settings, fundamentalPitchCents);
  }

  UpdateLevels();
}

template<typename T>
void SynthVoice<T>::UpdateLevels()
{
  const double midiPitch = mPitch + mPitchBend;
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

template<typename T>
void SynthVoice<T>::GetVisualizerFrame(VisualizerFrame& frame) const
{
  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
    frame.harmonics[harmonic] = mOscs[harmonic].GetVisualizerState();
}

template class SynthVoice<float>;
template class SynthVoice<double>;

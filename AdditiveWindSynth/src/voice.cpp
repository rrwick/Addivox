#include "voice.h"

#include "presets.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

SynthVoice::SynthVoice()
: mCompoundPreset(presets::MakeBrassCompoundPreset())
{
  const uint32_t voiceSeed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
  {
    const uint32_t harmonicSeed = voiceSeed ^ (0x9E3779B9u * static_cast<uint32_t>(harmonic + 1));
    mOscs[harmonic].SetVariationSeed(harmonicSeed);
  }
}

bool SynthVoice::IsActive() const
{
  for(const auto& osc : mOscs)
  {
    if(osc.IsActive())
      return true;
  }
  return false;
}

void SynthVoice::Start(double pitch, double pitchBend, double breath)
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

void SynthVoice::Stop()
{
  SetBreath(0.0);
}

void SynthVoice::SetPitchBend(double pitchBend)
{
  mPitchBend = pitchBend;
  UpdatePitch();
}

void SynthVoice::ApplyOscillatorSettings(int harmonic, const OscillatorSettings& settings, double fundamentalPitchSemitones)
{
  const double harmonicPitchOffsetSemitones = 12.0 * std::log2(static_cast<double>(harmonic + 1));
  const double presetPitchOffsetSemitones =
    (settings.pitch + mGlobalOscillatorModifiers.pitchOffsetCents) / 100.0;
  const double totalPitchSemitones =
    fundamentalPitchSemitones + harmonicPitchOffsetSemitones + presetPitchOffsetSemitones;
  mOscs[harmonic].SetPitch(totalPitchSemitones);
  mOscs[harmonic].SetPitchTime(GetPortamentoTimeSec());
  mOscs[harmonic].SetPitchVariation(
    (settings.pitch_variation_amplitude * mGlobalOscillatorModifiers.pitchVariationAmplitudeScale) / 100.0,
    settings.pitch_variation_rate * mGlobalOscillatorModifiers.pitchVariationRateScale);
  mOscs[harmonic].SetAttackTime(settings.attack * mGlobalOscillatorModifiers.attackScale);
  mOscs[harmonic].SetReleaseTime(settings.release * mGlobalOscillatorModifiers.releaseScale);
  mOscs[harmonic].SetPan(std::clamp(settings.pan + mGlobalOscillatorModifiers.panOffset, -1.0, 1.0));
  mOscs[harmonic].SetPanVariation(
    settings.pan_variation_amplitude * mGlobalOscillatorModifiers.panVariationAmplitudeScale,
    settings.pan_variation_rate * mGlobalOscillatorModifiers.panVariationRateScale);
  mOscs[harmonic].SetIntensityVariation(
    settings.intensity_variation_amplitude * mGlobalOscillatorModifiers.intensityVariationAmplitudeScale,
    settings.intensity_variation_rate * mGlobalOscillatorModifiers.intensityVariationRateScale);
}

void SynthVoice::SetBreath(double breath)
{
  const double smoothedBreath = SmoothBreath(breath);
  if(smoothedBreath == mBreath)
    return;

  mBreath = smoothedBreath;
  UpdateLevels();
}

void SynthVoice::SetPortamentoControl(double control)
{
  const double clampedControl = std::clamp(control, 0.0, 1.0);
  mPortamentoControl = clampedControl;
  const double pitchTimeSec = GetPortamentoTimeSec();
  for(auto& osc : mOscs)
    osc.SetPitchTime(pitchTimeSec);
}

void SynthVoice::SetGlobalOscillatorModifiers(const GlobalOscillatorModifiers& modifiers)
{
  mGlobalOscillatorModifiers = global_settings::Sanitize(modifiers);

  UpdatePitch();
}

double SynthVoice::SmoothBreath(double breath)
{
  // Input and output breath are in the range [0, 1].

  // Breath is smoothed near zero to avoid obvious note-on at low breath values, but is near-linear
  // for the upper part of the range.
  // https://www.desmos.com/calculator/ntwh4mbkwn
  constexpr double k = 5.0;
  static const double invDenominator = 1.0 / (k - 1.0 + std::exp(-k));
  breath = (k * breath - 1.0 + std::exp(-k * breath)) * invDenominator;

  // Cap breath at 97% to avoid ringing tone from brick wall at top of harmonic series.
  // TODO: this is a bit of a hack - would be better to use a more gradual roll-off of the highest
  //       harmonic intensities.
  breath *= 0.97;

  return breath;
}

void SynthVoice::Clear()
{
  mPitch = 0.0;
  mPitchBend = 0.0;
  mBreath = 0.0;
  mPortamentoControl = 0.0;
  Stop();
}

double SynthVoice::GetPortamentoTimeSec() const
{
  return mGlobalOscillatorModifiers.portamentoTimeAtCC5MinSec
    + (mGlobalOscillatorModifiers.portamentoTimeAtCC5MaxSec - mGlobalOscillatorModifiers.portamentoTimeAtCC5MinSec)
      * mPortamentoControl;
}

void SynthVoice::UpdatePitch()
{
  const double midiPitch = mPitch + mPitchBend;
  const double fundamentalPitchSemitones = midiPitch - 69.0;
  const SimplePreset& preset = mCompoundPreset.GetPresetForMidiNote(midiPitch);

  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
  {
    const OscillatorSettings& settings = preset.GetOscillatorSettings(harmonic);
    ApplyOscillatorSettings(harmonic, settings, fundamentalPitchSemitones);
  }

  UpdateLevels();
}

void SynthVoice::UpdateLevels()
{
  const double midiPitch = mPitch + mPitchBend;
  const SimplePreset& preset = mCompoundPreset.GetPresetForMidiNote(midiPitch);

  for(int harmonic = 0; harmonic < kNumHarmonics; harmonic++)
  {
    const OscillatorSettings& settings = preset.GetOscillatorSettings(harmonic);
    mOscs[harmonic].SetLevel(settings.intensity * std::pow(mBreath, settings.breath_power));
  }
}

void SynthVoice::ProcessSamplesAccumulating(iplug::sample** outputs, int startIdx, int nFrames)
{
  const int endIdx = startIdx + nFrames;
  for(int i = startIdx; i < endIdx; i++)
  {
    iplug::sample leftSample = 0.0;
    iplug::sample rightSample = 0.0;
    for(auto& osc : mOscs)
    {
      const auto sample = osc.Process();
      leftSample += sample[0];
      rightSample += sample[1];
    }
    outputs[0][i] += leftSample;
    outputs[1][i] += rightSample;
  }
}

void SynthVoice::SetSampleRate(double sampleRate)
{
  for(auto& osc : mOscs)
    osc.SetSampleRate(sampleRate);
}

void SynthVoice::GetVisualizerFrame(VisualizerFrame& frame) const
{
  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
    frame.harmonics[harmonic] = mOscs[harmonic].GetVisualizerState();
}

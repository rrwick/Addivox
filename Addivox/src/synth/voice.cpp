#include "voice.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace
{
// Set to higher levels to make the curve more exponential (greater difference in volume between
// low-breath and high-breath) and lower levels to make it more linear (less difference in volume
// between low-breath and high-breath).
constexpr double kBreathCurveScale = 2.0;

const double kBreathCurveLog = std::log1p(kBreathCurveScale);

double EvaluateBreathLevel(double poweredBreath)
{
  if(poweredBreath <= 0.0)
    return 0.0;

  return std::expm1(kBreathCurveLog * poweredBreath) / kBreathCurveScale;
}
} // namespace

SynthVoice::SynthVoice()
{
  const uint32_t voiceSeed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
  {
    const uint32_t harmonicSeed = voiceSeed ^ (0x9E3779B9u * static_cast<uint32_t>(harmonic + 1));
    mOscs[harmonic].SetVariationSeed(harmonicSeed);
  }

  UpdatePitchRate();
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
  mNotePitch = pitch;
  mPitchBend = pitchBend;
  mTargetMidiPitch = GetTargetMidiPitch();
  if(retrigger)
    mRenderedMidiPitch = mTargetMidiPitch;
  mBreath = SmoothBreath(breath);
  UpdatePitch();

  if(retrigger)
  {
    for(auto& osc : mOscs)
      osc.Reset();

    UpdateLevels();
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

void SynthVoice::ApplyOscillatorSettings(int harmonic,
                                         const OscillatorSettings& currentSettings,
                                         const OscillatorSettings& futurePitchSettings,
                                         double futureFundamentalPitchSemitones)
{
  const double totalPitchSemitones = GetOscillatorBasePitchSemitones(
    harmonic,
    futurePitchSettings,
    futureFundamentalPitchSemitones,
    mGlobalVoiceSettings);
  mOscs[harmonic].SetPitch(totalPitchSemitones);
  mOscs[harmonic].SetPitchTime(GetPortamentoTimeSec());
  mOscs[harmonic].SetPitchVariation(
    (currentSettings.pitch_variation_amplitude * mGlobalVoiceSettings.pitchVariationAmplitudeScale) / 100.0,
    currentSettings.pitch_variation_rate * mGlobalVoiceSettings.pitchVariationRateScale);
  mOscs[harmonic].SetAttackTime(currentSettings.attack * mGlobalVoiceSettings.attackScale);
  mOscs[harmonic].SetReleaseTime(currentSettings.release * mGlobalVoiceSettings.releaseScale);
  mOscs[harmonic].SetPan(std::clamp(currentSettings.pan + mGlobalVoiceSettings.panOffset, -1.0, 1.0));
  mOscs[harmonic].SetPanVariation(
    currentSettings.pan_variation_amplitude * mGlobalVoiceSettings.panVariationAmplitudeScale,
    currentSettings.pan_variation_rate * mGlobalVoiceSettings.panVariationRateScale);
  mOscs[harmonic].SetIntensityVariation(
    currentSettings.intensity_variation_amplitude * mGlobalVoiceSettings.intensityVariationAmplitudeScale,
    currentSettings.intensity_variation_rate * mGlobalVoiceSettings.intensityVariationRateScale);
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
  UpdatePitchRate();
  const double pitchTimeSec = GetPortamentoTimeSec();
  for(auto& osc : mOscs)
    osc.SetPitchTime(pitchTimeSec);

  UpdatePitch();
}

void SynthVoice::SetTransposeSemitones(double transposeSemitones)
{
  const double clampedTransposeSemitones = std::clamp(std::round(transposeSemitones), -36.0, 36.0);
  if(clampedTransposeSemitones == mTransposeSemitones)
    return;

  mTransposeSemitones = clampedTransposeSemitones;
  UpdatePitch();
}

void SynthVoice::SetGlobalVoiceSettings(const GlobalVoiceSettings& settings)
{
  mGlobalVoiceSettings = global_settings::Sanitize(settings);
  UpdatePitchRate();
  UpdatePitch();
}

void SynthVoice::SetCompoundPreset(const CompoundPreset& preset)
{
  mCompoundPreset = preset;
  UpdatePitch();
}

bool SynthVoice::AddKeyNotePreset(double midiNote)
{
  if(mCompoundPreset.HasKeyNotePreset(midiNote))
    return false;

  const int roundedMidiNote = static_cast<int>(std::lround(midiNote));
  mCompoundPreset.SetKeyNotePreset(roundedMidiNote, mCompoundPreset.GetPresetForMidiNote(midiNote));
  UpdatePitch();
  return true;
}

bool SynthVoice::RemoveKeyNotePreset(double midiNote)
{
  const int roundedMidiNote = static_cast<int>(std::lround(midiNote));
  const bool removed = mCompoundPreset.RemoveKeyNotePreset(roundedMidiNote);
  if(!removed)
    return false;

  UpdatePitch();
  return true;
}

bool SynthVoice::SetKeyNoteOscillatorParameter(double midiNote,
                                               int oscillatorIndex,
                                               OscillatorSettings::Parameter parameter,
                                               double value)
{
  const bool updated = mCompoundPreset.SetKeyNoteOscillatorParameter(midiNote, oscillatorIndex, parameter, value);
  if(!updated)
    return false;

  UpdatePitch();
  return true;
}

bool SynthVoice::SetKeyNoteOscillatorParameterValues(
  double midiNote,
  OscillatorSettings::Parameter parameter,
  const std::array<double, SimplePreset::kNumOscillators>& values)
{
  const bool updated = mCompoundPreset.SetKeyNoteOscillatorParameterValues(midiNote, parameter, values);
  if(!updated)
    return false;

  UpdatePitch();
  return true;
}

bool SynthVoice::SetKeyNoteEqCurve(double midiNote, const EqCurve& curve)
{
  const bool updated = mCompoundPreset.SetKeyNoteEqCurve(midiNote, curve);
  if(!updated)
    return false;

  UpdateLevels();
  return true;
}

bool SynthVoice::SetAllKeyNotesEnabled(OscillatorSettings::Parameter parameter, bool enabled, double midiNote)
{
  mCompoundPreset.SetAllKeyNotesEnabled(parameter, enabled, midiNote);
  UpdatePitch();
  return true;
}

bool SynthVoice::SetAllKeyNotesEqEnabled(bool enabled)
{
  mCompoundPreset.SetAllKeyNotesEqEnabled(enabled);
  UpdateLevels();
  return true;
}

double SynthVoice::SmoothBreath(double breath)
{
  // Input and output breath are in the range [0, 1].

  // Breath is smoothed near zero to avoid obvious note-on at low breath values, but is near-linear
  // for the upper part of the range.
  // https://www.desmos.com/calculator/ntwh4mbkwn
  constexpr double k = 5.0;
  static const double invDenominator = 1.0 / (k - 1.0 + std::exp(-k));
  return (k * breath - 1.0 + std::exp(-k * breath)) * invDenominator;
}

void SynthVoice::Clear()
{
  Stop();
  mNotePitch = 0.0;
  mPitchBend = 0.0;
  mBreath = 0.0;
  mPortamentoControl = 0.0;
  mRenderedMidiPitch = 0.0;
  mTargetMidiPitch = 0.0;
  mNoteControlSamplesUntilUpdate = 0;
  UpdatePitchRate();
}

double SynthVoice::GetPortamentoTimeSec() const
{
  return mGlobalVoiceSettings.portamentoTimeAtCC5MinSec
    + (mGlobalVoiceSettings.portamentoTimeAtCC5MaxSec - mGlobalVoiceSettings.portamentoTimeAtCC5MinSec)
      * mPortamentoControl;
}

double SynthVoice::GetTargetMidiPitch() const
{
  return mNotePitch + mPitchBend + mTransposeSemitones;
}

double SynthVoice::GetOscillatorBasePitchSemitones(int harmonic,
                                                   const OscillatorSettings& settings,
                                                   double fundamentalPitchSemitones,
                                                   const GlobalVoiceSettings& globalSettings)
{
  const double harmonicPitchOffsetSemitones = 12.0 * std::log2(static_cast<double>(harmonic + 1));
  const double presetPitchOffsetSemitones =
    (settings.pitch + globalSettings.tuningCents) / 100.0;
  return fundamentalPitchSemitones + harmonicPitchOffsetSemitones + presetPitchOffsetSemitones;
}

double SynthVoice::PitchSemitonesToFrequencyHz(double pitchSemitones)
{
  return 440.0 * std::exp2(pitchSemitones / 12.0);
}

double SynthVoice::AdvancePitchTowards(double currentPitch, double targetPitch, double maxDeltaSemitones)
{
  const double pitchDelta = targetPitch - currentPitch;
  if(std::abs(pitchDelta) <= maxDeltaSemitones || !std::isfinite(maxDeltaSemitones))
    return targetPitch;

  return currentPitch + std::copysign(maxDeltaSemitones, pitchDelta);
}

void SynthVoice::UpdatePitchRate()
{
  const double pitchTimeSec = GetPortamentoTimeSec();
  if(pitchTimeSec <= 0.0 || mSampleRate <= 0.0)
  {
    mPitchRatePerSample = std::numeric_limits<double>::infinity();
    return;
  }

  mPitchRatePerSample = 1.0 / (pitchTimeSec * mSampleRate);
}

double SynthVoice::PredictRenderedMidiPitch(int numSamples) const
{
  const int clampedSamples = std::max(numSamples, 0);
  return AdvancePitchTowards(
    mRenderedMidiPitch,
    mTargetMidiPitch,
    mPitchRatePerSample * static_cast<double>(clampedSamples));
}

void SynthVoice::AdvanceRenderedPitch(int numSamples)
{
  mRenderedMidiPitch = PredictRenderedMidiPitch(numSamples);
}

void SynthVoice::UpdatePitch()
{
  mTargetMidiPitch = GetTargetMidiPitch();
  if(!IsActive())
    mRenderedMidiPitch = mTargetMidiPitch;

  RefreshNoteDependentState(kNoteControlIntervalSamples);
}

void SynthVoice::UpdateLevels()
{
  UpdateLevels(mCompoundPreset.ResolveNoteSpan(mRenderedMidiPitch));
}

void SynthVoice::UpdateLevels(const CompoundPreset::ResolvedNoteSpan& noteSpan)
{
  const double breath = mBreath;
  const double levelScale = mGlobalVoiceSettings.levelScale;

  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
  {
    const OscillatorSettings settings = mCompoundPreset.InterpolateOscillatorSettings(noteSpan, harmonic);
    const double frequencyHz = PitchSemitonesToFrequencyHz(mOscs[harmonic].GetCurrentPitchSemitones());
    const double eqGain = mCompoundPreset.EvaluateEqGain(noteSpan, frequencyHz);
    const double breathLevel = EvaluateBreathLevel(std::pow(breath, settings.breath_power));
    const double level = settings.intensity
      * breathLevel
      * eqGain
      * levelScale;
    mOscs[harmonic].SetLevel(level);
  }
}

void SynthVoice::RefreshNoteDependentState(int lookAheadSamples)
{
  const int clampedLookAheadSamples = std::max(lookAheadSamples, 0);
  const CompoundPreset::ResolvedNoteSpan currentSpan = mCompoundPreset.ResolveNoteSpan(mRenderedMidiPitch);
  const double futureMidiPitch = PredictRenderedMidiPitch(clampedLookAheadSamples);
  const double futureFundamentalPitchSemitones = futureMidiPitch - 69.0;
  const CompoundPreset::ResolvedNoteSpan futureSpan = mCompoundPreset.ResolveNoteSpan(futureMidiPitch);

  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
  {
    const OscillatorSettings currentSettings = mCompoundPreset.InterpolateOscillatorSettings(currentSpan, harmonic);
    const OscillatorSettings futurePitchSettings = mCompoundPreset.InterpolateOscillatorSettings(futureSpan, harmonic);
    ApplyOscillatorSettings(
      harmonic,
      currentSettings,
      futurePitchSettings,
      futureFundamentalPitchSemitones);
  }

  UpdateLevels(currentSpan);
  mNoteControlSamplesUntilUpdate = kNoteControlIntervalSamples;
}

void SynthVoice::ProcessSamplesAccumulating(iplug::sample** outputs, int startIdx, int nFrames)
{
  const int endIdx = startIdx + nFrames;
  for(int i = startIdx; i < endIdx; i++)
  {
    const bool pitchIsMoving = (mRenderedMidiPitch != mTargetMidiPitch);
    if(pitchIsMoving && mNoteControlSamplesUntilUpdate <= 0)
      RefreshNoteDependentState(kNoteControlIntervalSamples);

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

    if(pitchIsMoving)
    {
      AdvanceRenderedPitch(1);
      --mNoteControlSamplesUntilUpdate;
    }
  }
}

void SynthVoice::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
  for(auto& osc : mOscs)
    osc.SetSampleRate(sampleRate);

  UpdatePitchRate();
  mNoteControlSamplesUntilUpdate = 0;
}

void SynthVoice::GetVisualizerFrame(VisualizerFrame& frame) const
{
  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
    frame.harmonics[harmonic] = mOscs[harmonic].GetVisualizerState();
}

#include "voice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
// Set to higher levels to make the curve more exponential (greater difference in volume between
// low-breath and high-breath) and lower levels to make it more linear (less difference in volume
// between low-breath and high-breath).
constexpr double kBreathCurveScale = 2.0;

const double kBreathCurveLog = std::log1p(kBreathCurveScale);
constexpr double kNoiseAttackDerivativeToExcitationScale = 0.1;
constexpr double kNoiseAttackTargetSmoothingTimeSec = 0.02;
constexpr double kNoiseAttackDetectorTimeSec = 0.003;
constexpr double kNoiseAttackDerivativeDeadbandPerSecond = 0.15;
constexpr double kNoiseAttackTransientRiseTimeSec = 0.001;
constexpr double kNoiseAttackTransientDecayTimeSec = 0.05;
constexpr double kNoiseActivityEpsilon = 1.0e-5;
constexpr double kNoiseSustainGainSmoothingTimeSec = 0.01;
constexpr double kNoiseComponentLifespanSec = 0.35;
constexpr double kNoiseComponentPanRange = 0.5;

const double kNoiseComponentLevelScale =
  std::sqrt(3.0 / static_cast<double>(HarmonicVisualizerFrame::kNoiseComponentsPerBand));

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

  mNoiseRandomState = voiceSeed ^ 0x4F1BBCDCu;
  UpdateNoiseComponentRates();
  UpdateNoiseComponentPanGains();
  UpdateNoiseSustainGainSmoothing();
  UpdateNoiseAttackTargetSmoothing();
  UpdateNoiseAttackDetectorSmoothing();
  UpdateNoiseAttackTransientRise();
  UpdateNoiseAttackTransientDecay();
  ResetNoiseState();
  UpdatePitchRate();
}

bool SynthVoice::IsActive() const
{
  for(const auto& osc : mOscs)
  {
    if(osc.IsActive())
      return true;
  }

  if(mNoiseAttackTransient > kNoiseActivityEpsilon)
    return true;

  if((mNoiseAttackTargetBreathLevel - mNoiseAttackDetectorBreath) > kNoiseActivityEpsilon)
    return true;

  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const auto index = static_cast<std::size_t>(bandIndex);
    if(mNoiseSustainBandGains[index] > kNoiseActivityEpsilon
      || mTargetNoiseSustainBandGains[index] > kNoiseActivityEpsilon)
    {
      return true;
    }
  }

  return false;
}

void SynthVoice::Start(double pitch,
                       double pitchBend,
                       double breath,
                       double previousBreath)
{
  const bool retrigger = !IsActive();
  mNotePitch = pitch;
  mPitchBend = pitchBend;
  mTargetMidiPitch = GetTargetMidiPitch();
  if(retrigger)
    mRenderedMidiPitch = mTargetMidiPitch;
  mRawBreath = std::clamp(breath, 0.0, 1.0);
  mBreath = SmoothBreath(mRawBreath);
  mNoiseAttackTargetBreathLevel = EvaluateBreathLevel(mBreath);
  UpdatePitch();

  if(retrigger)
  {
    for(auto& osc : mOscs)
      osc.Reset();

    ResetNoiseState();
    mNoiseAttackDetectorBreath = EvaluateBreathLevel(SmoothBreath(std::clamp(previousBreath, 0.0, 1.0)));
  }

  UpdateLevels();
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
  const double clampedBreath = std::clamp(breath, 0.0, 1.0);
  const double smoothedBreath = SmoothBreath(clampedBreath);
  if(clampedBreath == mRawBreath && smoothedBreath == mBreath)
    return;

  mRawBreath = clampedBreath;
  mBreath = smoothedBreath;
  mNoiseAttackTargetBreathLevel = EvaluateBreathLevel(mBreath);
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
  UpdateNoiseComponentPanGains();
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

bool SynthVoice::SetKeyNoteNoiseAttackProfile(double midiNote, const NoiseBandProfile& profile)
{
  const bool updated = mCompoundPreset.SetKeyNoteNoiseAttackProfile(midiNote, profile);
  if(!updated)
    return false;

  UpdateLevels();
  return true;
}

bool SynthVoice::SetKeyNoteNoiseSustainProfile(double midiNote, const NoiseBandProfile& profile)
{
  const bool updated = mCompoundPreset.SetKeyNoteNoiseSustainProfile(midiNote, profile);
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

bool SynthVoice::SetAllKeyNotesNoiseAttackEnabled(bool enabled)
{
  mCompoundPreset.SetAllKeyNotesNoiseAttackEnabled(enabled);
  UpdateLevels();
  return true;
}

bool SynthVoice::SetAllKeyNotesNoiseSustainEnabled(bool enabled)
{
  mCompoundPreset.SetAllKeyNotesNoiseSustainEnabled(enabled);
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
  mRawBreath = 0.0;
  mBreath = 0.0;
  mNoiseAttackTargetBreathLevel = 0.0;
  mNoiseAttackSmoothedTargetBreathLevel = 0.0;
  mNoiseAttackDetectorBreath = 0.0;
  mNoiseAttackTransient = 0.0;
  mPortamentoControl = 0.0;
  mRenderedMidiPitch = 0.0;
  mTargetMidiPitch = 0.0;
  mNoteControlSamplesUntilUpdate = 0;
  ResetNoiseState();
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

  UpdateNoiseTargets(noteSpan);
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

    const auto noiseSample = ProcessNoise();
    leftSample += static_cast<iplug::sample>(noiseSample[0]);
    rightSample += static_cast<iplug::sample>(noiseSample[1]);

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

  UpdateNoiseComponentRates();
  UpdateNoiseSustainGainSmoothing();
  UpdateNoiseAttackTargetSmoothing();
  UpdateNoiseAttackDetectorSmoothing();
  UpdateNoiseAttackTransientRise();
  UpdateNoiseAttackTransientDecay();
  UpdatePitchRate();
  mNoteControlSamplesUntilUpdate = 0;
}

void SynthVoice::GetVisualizerFrame(VisualizerFrame& frame) const
{
  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
    frame.harmonics[harmonic] = mOscs[harmonic].GetVisualizerState();

  for(int componentIndex = 0; componentIndex < kNumNoiseComponents; ++componentIndex)
  {
    const auto& component = mNoiseComponents[static_cast<std::size_t>(componentIndex)];
    const double bandGain = mNoiseSustainBandGains[static_cast<std::size_t>(component.bandIndex)]
      + (mNoiseAttackTransient * mNoiseAttackBandWeights[static_cast<std::size_t>(component.bandIndex)]);
    const double level = std::max(
      0.0,
      bandGain * component.eqGain * EvaluateNoiseLifecycleLevel(component.lifecycleProgress) * kNoiseComponentLevelScale);
    frame.noiseComponents[static_cast<std::size_t>(componentIndex)] = HarmonicVisualizerNoiseBand{
      static_cast<float>(component.frequencyHz),
      static_cast<float>(level),
      static_cast<float>(component.panLeftGain),
      static_cast<float>(component.panRightGain)};
  }
}

void SynthVoice::UpdateNoiseTargets(const CompoundPreset::ResolvedNoteSpan& noteSpan)
{
  const NoiseBandProfile attackProfile = mCompoundPreset.InterpolateNoiseAttackProfile(noteSpan);
  const NoiseBandProfile sustainProfile = mCompoundPreset.InterpolateNoiseSustainProfile(noteSpan);
  const auto& attackValues = attackProfile.GetValues();
  const auto& sustainValues = sustainProfile.GetValues();
  const double levelScale = mGlobalVoiceSettings.levelScale;
  const double attackScale = levelScale * mGlobalVoiceSettings.noiseAttackScale;
  const double sustainScale =
    EvaluateBreathLevel(mBreath) * levelScale * mGlobalVoiceSettings.noiseSustainScale;

  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const auto index = static_cast<std::size_t>(bandIndex);
    mNoiseAttackBandWeights[index] =
      NoiseBandProfile::ClampBandValue(attackValues[index]) * attackScale;
    mTargetNoiseSustainBandGains[index] =
      NoiseBandProfile::ClampBandValue(sustainValues[index]) * sustainScale;
  }

  UpdateNoiseComponentEqGains(noteSpan);
}

void SynthVoice::ResetNoiseState()
{
  mNoiseSustainBandGains.fill(0.0);
  mTargetNoiseSustainBandGains.fill(0.0);
  mNoiseAttackTransient = 0.0;
  mNoiseAttackSmoothedTargetBreathLevel = mNoiseAttackTargetBreathLevel;

  const CompoundPreset::ResolvedNoteSpan noteSpan = mCompoundPreset.ResolveNoteSpan(mRenderedMidiPitch);
  for(int componentIndex = 0; componentIndex < kNumNoiseComponents; ++componentIndex)
  {
    auto& component = mNoiseComponents[static_cast<std::size_t>(componentIndex)];
    component.bandIndex = componentIndex / kNoiseComponentsPerBand;
    RespawnNoiseComponent(component, noteSpan, true);
  }
}

void SynthVoice::RespawnNoiseComponent(NoiseComponent& component,
                                       const CompoundPreset::ResolvedNoteSpan& noteSpan,
                                       bool randomLifecycle)
{
  component.frequencyHz = RandomNoiseFrequencyHz(component.bandIndex);
  component.phase = NextNoiseRandomUnit(mNoiseRandomState);
  component.lifecycleProgress = randomLifecycle ? NextNoiseRandomUnit(mNoiseRandomState) : 0.0;
  component.randomPan = NextNoiseRandomSignedUnit(mNoiseRandomState) * kNoiseComponentPanRange;
  component.eqGain = mCompoundPreset.EvaluateEqGain(noteSpan, component.frequencyHz);

  const double safeSampleRate = mSampleRate > 0.0 ? mSampleRate : 44100.0;
  component.phaseIncrement = component.frequencyHz / safeSampleRate;
  component.lifecycleIncrement = 1.0 / std::max(1.0, kNoiseComponentLifespanSec * safeSampleRate);

  const double pan = GetNoiseComponentPan(component.randomPan);
  dsp::PanToGains(pan, component.panLeftGain, component.panRightGain);
}

void SynthVoice::UpdateNoiseComponentRates()
{
  const double safeSampleRate = mSampleRate > 0.0 ? mSampleRate : 44100.0;
  const double lifecycleIncrement = 1.0 / std::max(1.0, kNoiseComponentLifespanSec * safeSampleRate);
  for(auto& component : mNoiseComponents)
  {
    component.phaseIncrement = component.frequencyHz / safeSampleRate;
    component.lifecycleIncrement = lifecycleIncrement;
  }
}

void SynthVoice::UpdateNoiseComponentEqGains(const CompoundPreset::ResolvedNoteSpan& noteSpan)
{
  for(auto& component : mNoiseComponents)
    component.eqGain = mCompoundPreset.EvaluateEqGain(noteSpan, component.frequencyHz);
}

void SynthVoice::UpdateNoiseComponentPanGains()
{
  for(auto& component : mNoiseComponents)
  {
    const double pan = GetNoiseComponentPan(component.randomPan);
    dsp::PanToGains(pan, component.panLeftGain, component.panRightGain);
  }
}

void SynthVoice::UpdateNoiseSustainGainSmoothing()
{
  mNoiseSustainGainSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseSustainGainSmoothingTimeSec);
}

void SynthVoice::UpdateNoiseAttackTargetSmoothing()
{
  mNoiseAttackTargetSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseAttackTargetSmoothingTimeSec);
}

void SynthVoice::UpdateNoiseAttackDetectorSmoothing()
{
  mNoiseAttackDetectorSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseAttackDetectorTimeSec);
}

void SynthVoice::UpdateNoiseAttackTransientRise()
{
  mNoiseAttackTransientRiseCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseAttackTransientRiseTimeSec);
}

void SynthVoice::UpdateNoiseAttackTransientDecay()
{
  mNoiseAttackTransientDecayCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseAttackTransientDecayTimeSec);
}

std::array<double, 2> SynthVoice::ProcessNoise()
{
  double maxTargetGain = 0.0;
  double maxCurrentGain = 0.0;
  double maxAttackWeight = 0.0;
  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const auto index = static_cast<std::size_t>(bandIndex);
    maxTargetGain = std::max(maxTargetGain, mTargetNoiseSustainBandGains[index]);
    maxCurrentGain = std::max(maxCurrentGain, mNoiseSustainBandGains[index]);
    maxAttackWeight = std::max(maxAttackWeight, mNoiseAttackBandWeights[index]);
  }

  if(maxTargetGain <= kNoiseActivityEpsilon
    && maxCurrentGain <= kNoiseActivityEpsilon
    && mNoiseAttackTransient <= kNoiseActivityEpsilon
    && (((mNoiseAttackTargetBreathLevel - mNoiseAttackDetectorBreath) <= kNoiseActivityEpsilon)
      || maxAttackWeight <= kNoiseActivityEpsilon))
  {
    return {0.0, 0.0};
  }

  mNoiseAttackSmoothedTargetBreathLevel = dsp::SmoothValue(
    mNoiseAttackSmoothedTargetBreathLevel,
    mNoiseAttackTargetBreathLevel,
    mNoiseAttackTargetSmoothingCoefficient);
  if(std::abs(mNoiseAttackSmoothedTargetBreathLevel) < dsp::kDenormalFloor)
    mNoiseAttackSmoothedTargetBreathLevel = 0.0;

  const double previousDetectorBreath = mNoiseAttackDetectorBreath;
  mNoiseAttackDetectorBreath = dsp::SmoothValue(
    mNoiseAttackDetectorBreath,
    mNoiseAttackSmoothedTargetBreathLevel,
    mNoiseAttackDetectorSmoothingCoefficient);
  if(std::abs(mNoiseAttackDetectorBreath) < dsp::kDenormalFloor)
    mNoiseAttackDetectorBreath = 0.0;

  const double safeSampleRate = mSampleRate > 0.0 ? mSampleRate : 44100.0;
  const double positiveDerivativePerSecond = std::max(
    0.0,
    ((mNoiseAttackDetectorBreath - previousDetectorBreath) * safeSampleRate)
      - kNoiseAttackDerivativeDeadbandPerSecond);
  const double attackExcitation =
    positiveDerivativePerSecond * kNoiseAttackDerivativeToExcitationScale;
  const double attackTransientCoefficient =
    (attackExcitation > mNoiseAttackTransient)
      ? mNoiseAttackTransientRiseCoefficient
      : mNoiseAttackTransientDecayCoefficient;
  mNoiseAttackTransient = dsp::SmoothValue(
    mNoiseAttackTransient,
    attackExcitation,
    attackTransientCoefficient);
  if(std::abs(mNoiseAttackTransient) < dsp::kDenormalFloor)
    mNoiseAttackTransient = 0.0;

  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const auto index = static_cast<std::size_t>(bandIndex);
    mNoiseSustainBandGains[index] = dsp::SmoothValue(
      mNoiseSustainBandGains[index],
      mTargetNoiseSustainBandGains[index],
      mNoiseSustainGainSmoothingCoefficient);
    if(std::abs(mNoiseSustainBandGains[index]) < dsp::kDenormalFloor)
      mNoiseSustainBandGains[index] = 0.0;
  }

  std::array<double, 2> output{0.0, 0.0};
  bool haveRespawnSpan = false;
  CompoundPreset::ResolvedNoteSpan respawnSpan{};

  for(auto& component : mNoiseComponents)
  {
    const double bandGain = mNoiseSustainBandGains[static_cast<std::size_t>(component.bandIndex)]
      + (mNoiseAttackTransient * mNoiseAttackBandWeights[static_cast<std::size_t>(component.bandIndex)]);
    if(bandGain > kNoiseActivityEpsilon)
    {
      const double componentLevel = bandGain
        * component.eqGain
        * EvaluateNoiseLifecycleLevel(component.lifecycleProgress)
        * kNoiseComponentLevelScale;
      if(componentLevel > dsp::kDenormalFloor)
      {
        const double sample = std::sin((2.0 * dsp::kPi) * component.phase) * componentLevel;
        output[0] += sample * component.panLeftGain;
        output[1] += sample * component.panRightGain;
      }
    }

    component.phase += component.phaseIncrement;
    if(component.phase >= 1.0)
      component.phase -= std::floor(component.phase);

    component.lifecycleProgress += component.lifecycleIncrement;
    if(component.lifecycleProgress >= 1.0)
    {
      if(!haveRespawnSpan)
      {
        respawnSpan = mCompoundPreset.ResolveNoteSpan(mRenderedMidiPitch);
        haveRespawnSpan = true;
      }
      RespawnNoiseComponent(component, respawnSpan, false);
    }
  }

  return {
    dsp::FlushDenormal(output[0]),
    dsp::FlushDenormal(output[1])};
}

double SynthVoice::NextNoiseRandomUnit(uint32_t& state)
{
  if(state == 0u)
    state = 0xA511E9B3u;

  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return static_cast<double>(state) / 4294967295.0;
}

double SynthVoice::NextNoiseRandomSignedUnit(uint32_t& state)
{
  return (NextNoiseRandomUnit(state) * 2.0) - 1.0;
}

double SynthVoice::RandomNoiseFrequencyHz(int bandIndex)
{
  const double safeSampleRate = mSampleRate > 0.0 ? mSampleRate : 44100.0;
  const double maxFrequencyHz = 0.49 * safeSampleRate;
  const double lowerHz = std::max(1.0, std::min(NoiseBandProfile::GetBandLowerFrequencyHz(bandIndex), maxFrequencyHz));
  const double upperHz = std::max(lowerHz, std::min(NoiseBandProfile::GetBandUpperFrequencyHz(bandIndex), maxFrequencyHz));
  if(upperHz <= lowerHz)
    return lowerHz;

  const double t = NextNoiseRandomUnit(mNoiseRandomState);
  const double logLowerHz = std::log(lowerHz);
  const double logUpperHz = std::log(upperHz);
  return std::exp(logLowerHz + ((logUpperHz - logLowerHz) * t));
}

double SynthVoice::GetNoiseComponentPan(double randomPan) const
{
  const double globalPan = std::clamp(mGlobalVoiceSettings.panOffset, -1.0, 1.0);
  const double randomPanScale = 1.0 - std::abs(globalPan);
  return std::clamp(globalPan + (randomPan * randomPanScale), -1.0, 1.0);
}

double SynthVoice::EvaluateNoiseLifecycleLevel(double lifecycleProgress)
{
  const double clampedProgress = std::clamp(lifecycleProgress, 0.0, 1.0);
  return 1.0 - std::abs((clampedProgress * 2.0) - 1.0);
}

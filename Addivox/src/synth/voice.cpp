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
constexpr double kNoiseAttackDerivativeToExcitationScale = 0.1;
constexpr double kNoiseAttackDetectorTimeSec = 0.003;
constexpr double kNoiseAttackDerivativeDeadbandPerSecond = 0.05;
constexpr double kNoiseAttackVisualizationDecayTimeSec = 0.05;
constexpr double kNoiseAttackActivityEpsilon = 1.0e-5;
constexpr double kNoiseOutputScale = 0.25;
constexpr double kNoiseSustainGainSmoothingTimeSec = 0.01;
constexpr double kNoiseSustainPanSmoothingTimeSec = 0.02;
constexpr double kNoiseSustainActivityEpsilon = 1.0e-5;
constexpr double kNoiseSustainBandpassCascadeQScale = 2.2989622870706805;
constexpr int kNoiseSustainEqSamplesPerBand = 5;

double EvaluateBreathLevel(double poweredBreath)
{
  if(poweredBreath <= 0.0)
    return 0.0;

  return std::expm1(kBreathCurveLog * poweredBreath) / kBreathCurveScale;
}

double EvaluateNoiseBandEqGain(const CompoundPreset& preset,
                               const CompoundPreset::ResolvedNoteSpan& noteSpan,
                               int bandIndex)
{
  const double lowerHz = NoiseBandProfile::GetBandLowerFrequencyHz(bandIndex);
  const double upperHz = NoiseBandProfile::GetBandUpperFrequencyHz(bandIndex);
  if(!(lowerHz > 0.0) || !(upperHz > lowerHz))
    return 1.0;

  const double logLowerHz = std::log(lowerHz);
  const double logUpperHz = std::log(upperHz);
  double gainSquaredSum = 0.0;
  for(int sampleIndex = 0; sampleIndex < kNoiseSustainEqSamplesPerBand; ++sampleIndex)
  {
    const double t = kNoiseSustainEqSamplesPerBand > 1
      ? static_cast<double>(sampleIndex) / static_cast<double>(kNoiseSustainEqSamplesPerBand - 1)
      : 0.5;
    const double frequencyHz = std::exp(logLowerHz + ((logUpperHz - logLowerHz) * t));
    const double gain = preset.EvaluateEqGain(noteSpan, frequencyHz);
    gainSquaredSum += gain * gain;
  }

  return std::sqrt(gainSquaredSum / static_cast<double>(kNoiseSustainEqSamplesPerBand));
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

  mNoiseSustainNoiseState = voiceSeed ^ 0x4F1BBCDCu;
  mNoiseAttackNoiseState = voiceSeed ^ 0x91A4DC27u;
  UpdateNoiseSustainFilters();
  UpdateNoiseSustainGainSmoothing();
  UpdateNoiseAttackDetectorSmoothing();
  UpdateNoiseSustainPanTargets();
  UpdateNoiseSustainPanSmoothing();
  UpdateNoiseAttackVisualizationDecay();
  mNoiseSustainPanLeftGain = mTargetNoiseSustainPanLeftGain;
  mNoiseSustainPanRightGain = mTargetNoiseSustainPanRightGain;
  UpdatePitchRate();
}

bool SynthVoice::IsActive() const
{
  for(const auto& osc : mOscs)
  {
    if(osc.IsActive())
      return true;
  }

  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const auto index = static_cast<std::size_t>(bandIndex);
    if(mNoiseAttackBandVisualization[index] > kNoiseAttackActivityEpsilon)
      return true;

    if(mNoiseSustainBandGains[index] > kNoiseSustainActivityEpsilon
      || mTargetNoiseSustainBandGains[index] > kNoiseSustainActivityEpsilon)
    {
      return true;
    }
  }

  return (mNoiseAttackTargetBreathLevel - mNoiseAttackDetectorBreath) > kNoiseAttackActivityEpsilon;
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

    ResetNoiseAttackState();
    ResetNoiseSustainState();
    mNoiseAttackDetectorBreath = EvaluateBreathLevel(SmoothBreath(std::clamp(previousBreath, 0.0, 1.0)));
    mNoiseSustainPanLeftGain = mTargetNoiseSustainPanLeftGain;
    mNoiseSustainPanRightGain = mTargetNoiseSustainPanRightGain;
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
  UpdateNoiseSustainPanTargets();
  if(!IsActive())
  {
    mNoiseSustainPanLeftGain = mTargetNoiseSustainPanLeftGain;
    mNoiseSustainPanRightGain = mTargetNoiseSustainPanRightGain;
  }
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
  mNoiseAttackDetectorBreath = 0.0;
  mPortamentoControl = 0.0;
  mRenderedMidiPitch = 0.0;
  mTargetMidiPitch = 0.0;
  mNoteControlSamplesUntilUpdate = 0;
  ResetNoiseAttackState();
  ResetNoiseSustainState();
  mNoiseSustainPanLeftGain = mTargetNoiseSustainPanLeftGain;
  mNoiseSustainPanRightGain = mTargetNoiseSustainPanRightGain;
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

  UpdateNoiseAttackTargets(noteSpan);
  UpdateNoiseSustainTargets(noteSpan);
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

    mNoiseSustainPanLeftGain = dsp::SmoothValue(
      mNoiseSustainPanLeftGain,
      mTargetNoiseSustainPanLeftGain,
      mNoiseSustainPanSmoothingCoefficient);
    mNoiseSustainPanRightGain = dsp::SmoothValue(
      mNoiseSustainPanRightGain,
      mTargetNoiseSustainPanRightGain,
      mNoiseSustainPanSmoothingCoefficient);

    const double noiseSample = ProcessNoiseAttack() + ProcessNoiseSustain();
    leftSample += static_cast<iplug::sample>(noiseSample * mNoiseSustainPanLeftGain);
    rightSample += static_cast<iplug::sample>(noiseSample * mNoiseSustainPanRightGain);

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

  UpdateNoiseSustainFilters();
  UpdateNoiseSustainGainSmoothing();
  UpdateNoiseAttackDetectorSmoothing();
  UpdateNoiseSustainPanSmoothing();
  UpdateNoiseAttackVisualizationDecay();
  UpdatePitchRate();
  mNoteControlSamplesUntilUpdate = 0;
}

void SynthVoice::GetVisualizerFrame(VisualizerFrame& frame) const
{
  for(int harmonic = 0; harmonic < kNumHarmonics; ++harmonic)
    frame.harmonics[harmonic] = mOscs[harmonic].GetVisualizerState();

  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const auto index = static_cast<std::size_t>(bandIndex);
    const double bandLevel = std::max(0.0, mNoiseSustainBandGains[index]) + std::max(0.0, mNoiseAttackBandVisualization[index]);
    frame.noiseBands[index] = HarmonicVisualizerNoiseBand{
      static_cast<float>(bandLevel * std::max(0.0, mNoiseSustainPanLeftGain)),
      static_cast<float>(bandLevel * std::max(0.0, mNoiseSustainPanRightGain))};
  }
}

void SynthVoice::UpdateNoiseAttackTargets(const CompoundPreset::ResolvedNoteSpan& noteSpan)
{
  const NoiseBandProfile profile = mCompoundPreset.InterpolateNoiseAttackProfile(noteSpan);
  const auto& values = profile.GetValues();

  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const double eqGain = EvaluateNoiseBandEqGain(mCompoundPreset, noteSpan, bandIndex);
    mNoiseAttackBandWeights[static_cast<std::size_t>(bandIndex)] =
      NoiseBandProfile::ClampBandValue(values[static_cast<std::size_t>(bandIndex)]) * eqGain;
  }
}

void SynthVoice::UpdateNoiseSustainTargets(const CompoundPreset::ResolvedNoteSpan& noteSpan)
{
  const NoiseBandProfile profile = mCompoundPreset.InterpolateNoiseSustainProfile(noteSpan);
  const auto& values = profile.GetValues();
  const double breathScale =
    EvaluateBreathLevel(mBreath) * mGlobalVoiceSettings.levelScale * mGlobalVoiceSettings.noiseSustainScale;

  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const double eqGain = EvaluateNoiseBandEqGain(mCompoundPreset, noteSpan, bandIndex);
    mTargetNoiseSustainBandGains[static_cast<std::size_t>(bandIndex)] =
      NoiseBandProfile::ClampBandValue(values[static_cast<std::size_t>(bandIndex)]) * breathScale * eqGain;
  }
}

void SynthVoice::ResetNoiseAttackState()
{
  for(auto& bandpassStages : mNoiseAttackBandpasses)
  {
    for(auto& bandpass : bandpassStages)
      bandpass.Clear();
  }

  mNoiseAttackBandVisualization.fill(0.0);
}

void SynthVoice::ResetNoiseSustainState()
{
  for(auto& bandpassStages : mNoiseSustainBandpasses)
  {
    for(auto& bandpass : bandpassStages)
      bandpass.Clear();
  }

  mNoiseSustainBandGains.fill(0.0);
  mTargetNoiseSustainBandGains.fill(0.0);
}

void SynthVoice::UpdateNoiseSustainFilters()
{
  const double safeSampleRate = mSampleRate > 0.0 ? mSampleRate : 44100.0;
  const double nyquist = 0.5 * safeSampleRate;

  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const double lowerHz = NoiseBandProfile::GetBandLowerFrequencyHz(bandIndex);
    const double upperHz = NoiseBandProfile::GetBandUpperFrequencyHz(bandIndex);
    const double centerHz = NoiseBandProfile::GetBandCenterFrequencyHz(bandIndex);
    const double bandwidthHz = std::max(upperHz - lowerHz, 1.0);
    const double q = std::max(centerHz / bandwidthHz, 0.1);
    const double stageQ = std::max(q / kNoiseSustainBandpassCascadeQScale, 0.1);
    for(auto& bandpass : mNoiseAttackBandpasses[static_cast<std::size_t>(bandIndex)])
    {
      bandpass.SetBandpassHz(
        safeSampleRate,
        centerHz,
        stageQ,
        1.0,
        0.49,
        0.1,
        100.0);
    }
    for(auto& bandpass : mNoiseSustainBandpasses[static_cast<std::size_t>(bandIndex)])
    {
      bandpass.SetBandpassHz(
        safeSampleRate,
        centerHz,
        stageQ,
        1.0,
        0.49,
        0.1,
        100.0);
    }
    mNoiseSustainBandNormalizations[static_cast<std::size_t>(bandIndex)] =
      std::sqrt(nyquist / bandwidthHz);
  }
}

void SynthVoice::UpdateNoiseSustainGainSmoothing()
{
  mNoiseSustainGainSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseSustainGainSmoothingTimeSec);
}

void SynthVoice::UpdateNoiseAttackDetectorSmoothing()
{
  mNoiseAttackDetectorSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseAttackDetectorTimeSec);
}

void SynthVoice::UpdateNoiseSustainPanTargets()
{
  dsp::PanToGains(
    std::clamp(mGlobalVoiceSettings.panOffset, -1.0, 1.0),
    mTargetNoiseSustainPanLeftGain,
    mTargetNoiseSustainPanRightGain);
}

void SynthVoice::UpdateNoiseSustainPanSmoothing()
{
  mNoiseSustainPanSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseSustainPanSmoothingTimeSec);
}

void SynthVoice::UpdateNoiseAttackVisualizationDecay()
{
  mNoiseAttackVisualizationDecayCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kNoiseAttackVisualizationDecayTimeSec);
}

double SynthVoice::ProcessNoiseAttack()
{
  double maxVisualization = 0.0;
  for(const double value : mNoiseAttackBandVisualization)
    maxVisualization = std::max(maxVisualization, value);

  if((mNoiseAttackTargetBreathLevel - mNoiseAttackDetectorBreath) <= kNoiseAttackActivityEpsilon
    && maxVisualization <= kNoiseAttackActivityEpsilon)
  {
    return 0.0;
  }

  const double previousDetectorBreath = mNoiseAttackDetectorBreath;
  mNoiseAttackDetectorBreath = dsp::SmoothValue(
    mNoiseAttackDetectorBreath,
    mNoiseAttackTargetBreathLevel,
    mNoiseAttackDetectorSmoothingCoefficient);
  if(std::abs(mNoiseAttackDetectorBreath) < dsp::kDenormalFloor)
    mNoiseAttackDetectorBreath = 0.0;

  const double safeSampleRate = mSampleRate > 0.0 ? mSampleRate : 44100.0;
  const double positiveDerivativePerSecond = std::max(
    0.0,
    ((mNoiseAttackDetectorBreath - previousDetectorBreath) * safeSampleRate)
      - kNoiseAttackDerivativeDeadbandPerSecond);
  const double excitation = (positiveDerivativePerSecond > 0.0)
    ? (NextWhiteNoiseSample(mNoiseAttackNoiseState)
      * positiveDerivativePerSecond
      * kNoiseAttackDerivativeToExcitationScale)
    : 0.0;

  const double outputScale = mGlobalVoiceSettings.levelScale * mGlobalVoiceSettings.noiseAttackScale;
  double output = 0.0;
  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const auto index = static_cast<std::size_t>(bandIndex);
    double bandSample = excitation;
    for(auto& bandpass : mNoiseAttackBandpasses[index])
      bandSample = bandpass.Process(bandSample);

    const double weightedBandSample = bandSample * mNoiseSustainBandNormalizations[index] * mNoiseAttackBandWeights[index] * outputScale;
    output += weightedBandSample;

    mNoiseAttackBandVisualization[index] = std::max(
      std::abs(weightedBandSample),
      dsp::SmoothValue(mNoiseAttackBandVisualization[index], 0.0, mNoiseAttackVisualizationDecayCoefficient));
    if(mNoiseAttackBandVisualization[index] < dsp::kDenormalFloor)
      mNoiseAttackBandVisualization[index] = 0.0;
  }

  return dsp::FlushDenormal(output * kNoiseOutputScale);
}

double SynthVoice::ProcessNoiseSustain()
{
  double maxTargetGain = 0.0;
  for(const double gain : mTargetNoiseSustainBandGains)
    maxTargetGain = std::max(maxTargetGain, gain);

  double maxCurrentGain = 0.0;
  for(const double gain : mNoiseSustainBandGains)
    maxCurrentGain = std::max(maxCurrentGain, gain);

  if(maxTargetGain <= kNoiseSustainActivityEpsilon && maxCurrentGain <= kNoiseSustainActivityEpsilon)
    return 0.0;

  const double whiteNoise = NextWhiteNoiseSample(mNoiseSustainNoiseState);

  double output = 0.0;
  for(int bandIndex = 0; bandIndex < kNumNoiseBands; ++bandIndex)
  {
    const auto index = static_cast<std::size_t>(bandIndex);
    mNoiseSustainBandGains[index] = dsp::SmoothValue(
      mNoiseSustainBandGains[index],
      mTargetNoiseSustainBandGains[index],
      mNoiseSustainGainSmoothingCoefficient);
    if(std::abs(mNoiseSustainBandGains[index]) < dsp::kDenormalFloor)
      mNoiseSustainBandGains[index] = 0.0;

    double bandSample = whiteNoise;
    for(auto& bandpass : mNoiseSustainBandpasses[index])
      bandSample = bandpass.Process(bandSample);
    output += bandSample * mNoiseSustainBandNormalizations[index] * mNoiseSustainBandGains[index];
  }

  return dsp::FlushDenormal(output * kNoiseOutputScale);
}

double SynthVoice::NextWhiteNoiseSample(uint32_t& state)
{
  if(state == 0u)
    state = 0xA511E9B3u;

  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return (static_cast<double>(state) / 4294967295.0) * 2.0 - 1.0;
}

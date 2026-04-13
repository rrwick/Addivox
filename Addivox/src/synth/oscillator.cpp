#include "oscillator.h"

#include "../dsp/gradient_noise.h"
#include "../dsp/shared.h"

#include <algorithm>
#include <cmath>
#include <limits>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? sampleRate : dsp::kDefaultSampleRate;
  mInverseSampleRate = 1.0 / mSampleRate;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdatePhaseIncrement(frequencyHz);
  UpdatePitchRate();
  UpdateLevelRates();
  UpdatePanSlewRate();
  UpdateVariationParameterSmoothingRate();
  RefreshVariationTargets();
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ 0x17D39EF5u));
  UpdateLevelTarget(CurrentVariationNoise(mIntensityVariation, mVariationSeed ^ 0xF1023A17u));
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ 0xC29B3F4Bu));
}

void Oscillator::SetVariationSeed(uint32_t seed)
{
  mVariationSeed = (seed != 0u) ? seed : kDefaultVariationSeed;

  // Give each variation stream its own deterministic starting point.
  mIntensityVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0x68E31DA4u) + 1.0) * 100.0;
  mPitchVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0xB5297A4Du) + 1.0) * 100.0;
  mPanVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0x1B56C4E9u) + 1.0) * 100.0;
  mIntensityVariation.InvalidateNoiseCache();
  mPitchVariation.InvalidateNoiseCache();
  mPanVariation.InvalidateNoiseCache();

  RefreshVariationTargets();
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ 0x17D39EF5u));
  UpdateLevelTarget(CurrentVariationNoise(mIntensityVariation, mVariationSeed ^ 0xF1023A17u));
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ 0xC29B3F4Bu));
}

void Oscillator::SetPitch(double pitchSemitones)
{
  mBasePitch = pitchSemitones;
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ 0x17D39EF5u));
}

void Oscillator::SetPitchTime(double pitchTimeSec)
{
  mPitchTimeSec = std::max(0.0, pitchTimeSec);
  UpdatePitchRate();
}

void Oscillator::SetPitchVariation(double amplitudeSemitones, double rateHz)
{
  mPitchVariation.SetTargets(amplitudeSemitones, rateHz);
}

void Oscillator::SetAttackTime(double attackTimeSec)
{
  mAttackTimeSec = std::max(0.0, attackTimeSec);
  UpdateLevelRates();
}

void Oscillator::SetReleaseTime(double releaseTimeSec)
{
  mReleaseTimeSec = std::max(0.0, releaseTimeSec);
  UpdateLevelRates();
}

void Oscillator::SetIntensityVariation(double amplitude, double rateHz)
{
  mIntensityVariation.SetTargets(amplitude, rateHz);
}

void Oscillator::SetPan(double pan)
{
  mBasePan = std::clamp(pan, -1.0, 1.0);
  dsp::PanToGains(mBasePan, mBasePanLeftGain, mBasePanRightGain);
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ 0xC29B3F4Bu));
}

void Oscillator::SetPanVariation(double amplitude, double rateHz)
{
  mPanVariation.SetTargets(amplitude, rateHz);
}

void Oscillator::SetLevel(double level)
{
  mBaseLevel = (level >= 0.0) ? level : 0.0;
  UpdateLevelTarget(CurrentVariationNoise(mIntensityVariation, mVariationSeed ^ 0xF1023A17u));
}

void Oscillator::Reset()
{
  mPhase = 0.0;
  mLevel = 0.0;
  RefreshVariationTargets();
  mIntensityVariation.SnapToTargets();
  mPitchVariation.SnapToTargets();
  mPanVariation.SnapToTargets();
  mVariationTargetRefreshCountdown = kVariationTargetRefreshIntervalSamples;
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ 0x17D39EF5u));
  mPitch = mTargetPitch;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdateLevelTarget(CurrentVariationNoise(mIntensityVariation, mVariationSeed ^ 0xF1023A17u));
  // On full retrigger, start from the current pan target.
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ 0xC29B3F4Bu));
  mPanLeftGain = mTargetPanLeftGain;
  mPanRightGain = mTargetPanRightGain;
  UpdatePhaseIncrement(frequencyHz);
}

std::array<iplug::sample, 2> Oscillator::Process()
{
  if(mVariationTargetRefreshCountdown <= 0)
  {
    RefreshVariationTargets();
    mVariationTargetRefreshCountdown = kVariationTargetRefreshIntervalSamples;
  }
  --mVariationTargetRefreshCountdown;

  if(mHasIntensityVariation)
  {
    SmoothVariationParameters(mIntensityVariation);
    if(IsVariationActiveNow(mIntensityVariation))
    {
      const double intensityNoise = VariationNoise(mIntensityVariation, mVariationSeed ^ 0xF1023A17u);
      mIntensityVariation.position += mIntensityVariation.positionIncrement;
      UpdateLevelTarget(intensityNoise);
    }
    else
      UpdateLevelTarget();
  }

  if(mHasPanVariation)
  {
    SmoothVariationParameters(mPanVariation);
    if(IsVariationActiveNow(mPanVariation))
    {
      const double panNoise = VariationNoise(mPanVariation, mVariationSeed ^ 0xC29B3F4Bu);
      mPanVariation.position += mPanVariation.positionIncrement;
      UpdatePanTargetGains(panNoise);
    }
    else
      UpdatePanTargetGains();
  }

  if(mHasPitchVariation)
  {
    SmoothVariationParameters(mPitchVariation);
    if(IsVariationActiveNow(mPitchVariation))
    {
      const double pitchNoise = VariationNoise(mPitchVariation, mVariationSeed ^ 0x17D39EF5u);
      mPitchVariation.position += mPitchVariation.positionIncrement;
      UpdatePitchTarget(pitchNoise);
    }
    else
      UpdatePitchTarget();
  }

  const double rate = (mTargetLevel > mLevel) ? mAttackRate : mReleaseRate;
  mLevel += (mTargetLevel - mLevel) * rate;

  const double panLeftDelta = mTargetPanLeftGain - mPanLeftGain;
  const double panLeftStep = std::min(std::abs(panLeftDelta), mPanSlewPerSample);
  mPanLeftGain += std::copysign(panLeftStep, panLeftDelta);

  const double panRightDelta = mTargetPanRightGain - mPanRightGain;
  const double panRightStep = std::min(std::abs(panRightDelta), mPanSlewPerSample);
  mPanRightGain += std::copysign(panRightStep, panRightDelta);

  const double pitchDelta = mTargetPitch - mPitch;
  const double pitchStep = std::min(std::abs(pitchDelta), mPitchRatePerSample);
  mPitch += std::copysign(pitchStep, pitchDelta);
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdatePhaseIncrement(frequencyHz);

  if(mTargetLevel <= kLevelEpsilon && mLevel < kLevelEpsilon)
    mLevel = 0.0;

  const iplug::sample out = static_cast<iplug::sample>(std::sin((2.0 * dsp::kPi) * mPhase) * mLevel);

  mPhase += mPhaseIncrement;
  if(mPhase >= 1.0)
    mPhase -= std::floor(mPhase);

  // Deactivate oscillator if frequency is out of range - prevents aliasing
  if(frequencyHz > mSampleRate * 0.5 || frequencyHz < kMinFrequencyHz)
    return {0.0, 0.0};

  return {
    out * static_cast<iplug::sample>(mPanLeftGain),
    out * static_cast<iplug::sample>(mPanRightGain)};
}

bool Oscillator::IsActive() const
{
  // A held note must keep processing even when variation temporarily drives the target level to
  // zero, otherwise the variation phase freezes and the oscillator can never recover.
  return (mBaseLevel > kLevelEpsilon) || (mTargetLevel > kLevelEpsilon) || (mLevel > kLevelEpsilon);
}

HarmonicVisualizerOscillator Oscillator::GetVisualizerState() const
{
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  return HarmonicVisualizerOscillator{
    static_cast<float>(frequencyHz),
    static_cast<float>(mLevel),
    static_cast<float>(mPanLeftGain),
    static_cast<float>(mPanRightGain)};
}

void Oscillator::UpdatePhaseIncrement(double frequencyHz)
{
  mPhaseIncrement = frequencyHz * mInverseSampleRate;
}

void Oscillator::UpdatePitchRate()
{
  if(mPitchTimeSec <= 0.0 || mSampleRate <= 0.0)
  {
    mPitchRatePerSample = std::numeric_limits<double>::infinity();
    return;
  }

  // Linear portamento: fixed semitone step per sample.
  mPitchRatePerSample = 1.0 / (mPitchTimeSec * mSampleRate);
}

void Oscillator::UpdateLevelRates()
{
  mAttackRate = dsp::ExponentialSmoothingCoefficient(mSampleRate, mAttackTimeSec);
  mReleaseRate = dsp::ExponentialSmoothingCoefficient(mSampleRate, mReleaseTimeSec);
}

void Oscillator::UpdatePanSlewRate()
{
  if(kPanSlewTimeSec <= 0.0 || mSampleRate <= 0.0)
  {
    mPanSlewPerSample = std::numeric_limits<double>::infinity();
    return;
  }

  // Linear pan slew: fixed max gain step per sample.
  mPanSlewPerSample = mInverseSampleRate / kPanSlewTimeSec;
}

void Oscillator::UpdateVariationParameterSmoothingRate()
{
  mVariationParameterSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kVariationParameterSmoothingTimeSec);
}

void Oscillator::RefreshVariationTargets()
{
  mIntensityVariation.RefreshTargets();
  mPitchVariation.RefreshTargets();
  mPanVariation.RefreshTargets();
  mHasIntensityVariation =
    (mIntensityVariation.amplitude > kVariationParameterEpsilon
      || mIntensityVariation.targetAmplitude > kVariationParameterEpsilon)
    && (mIntensityVariation.rateHz > kVariationParameterEpsilon
      || mIntensityVariation.targetRateHz > kVariationParameterEpsilon);
  mHasPitchVariation =
    (mPitchVariation.amplitude > kVariationParameterEpsilon
      || mPitchVariation.targetAmplitude > kVariationParameterEpsilon)
    && (mPitchVariation.rateHz > kVariationParameterEpsilon
      || mPitchVariation.targetRateHz > kVariationParameterEpsilon);
  mHasPanVariation =
    (mPanVariation.amplitude > kVariationParameterEpsilon
      || mPanVariation.targetAmplitude > kVariationParameterEpsilon)
    && (mPanVariation.rateHz > kVariationParameterEpsilon
      || mPanVariation.targetRateHz > kVariationParameterEpsilon);
}

void Oscillator::UpdatePitchTarget(double pitchNoise)
{
  const double pitchSemitones = mBasePitch + (mPitchVariation.amplitude * pitchNoise);
  mTargetPitch = std::max(mMinPitchSemitones, pitchSemitones);
}

void Oscillator::UpdateLevelTarget(double intensityNoise)
{
  const double levelScale = std::max(0.0, 1.0 + (mIntensityVariation.amplitude * intensityNoise));
  mTargetLevel = mBaseLevel * levelScale;
}

void Oscillator::UpdatePanTargetGains(double panNoise)
{
  if(panNoise == 0.0)
  {
    mTargetPanLeftGain = mBasePanLeftGain;
    mTargetPanRightGain = mBasePanRightGain;
    return;
  }

  double modulatedPan = mBasePan + (mPanVariation.amplitude * panNoise);
  if(modulatedPan < -1.0)
    modulatedPan = -1.0;
  else if(modulatedPan > 1.0)
    modulatedPan = 1.0;
  dsp::PanToGains(modulatedPan, mTargetPanLeftGain, mTargetPanRightGain);
}

void Oscillator::SmoothVariationParameters(VariationState& variation)
{
  const double amplitudeDelta = variation.targetAmplitude - variation.amplitude;
  variation.amplitude += mVariationParameterSmoothingCoefficient * amplitudeDelta;
  if(amplitudeDelta <= kVariationParameterEpsilon && amplitudeDelta >= -kVariationParameterEpsilon)
    variation.amplitude = variation.targetAmplitude;

  const double rateDelta = variation.targetRateHz - variation.rateHz;
  variation.rateHz += mVariationParameterSmoothingCoefficient * rateDelta;
  if(rateDelta <= kVariationParameterEpsilon && rateDelta >= -kVariationParameterEpsilon)
    variation.rateHz = variation.targetRateHz;

  variation.positionIncrement = variation.rateHz * mInverseSampleRate;
}

bool Oscillator::IsVariationActiveNow(const VariationState& variation)
{
  return variation.amplitude > kVariationParameterEpsilon
    && variation.rateHz > kVariationParameterEpsilon;
}

double Oscillator::CurrentVariationNoise(VariationState& variation, uint32_t seed)
{
  if(!IsVariationActiveNow(variation))
    return 0.0;

  return VariationNoise(variation, seed);
}

double Oscillator::VariationNoise(VariationState& variation, uint32_t seed)
{
  const int lattice = static_cast<int>(variation.position);
  if(!variation.noiseCacheValid || lattice < variation.noiseLattice || lattice > (variation.noiseLattice + 1))
  {
    variation.noiseLattice = lattice;
    variation.noiseGradient0 =
      dsp::HashToSignedUnitFloat(dsp::HashUint32(static_cast<uint32_t>(variation.noiseLattice) ^ seed));
    variation.noiseGradient1 =
      dsp::HashToSignedUnitFloat(dsp::HashUint32(static_cast<uint32_t>(variation.noiseLattice + 1) ^ seed));
    variation.noiseCacheValid = true;
  }
  else if(lattice == (variation.noiseLattice + 1))
  {
    variation.noiseLattice = lattice;
    variation.noiseGradient0 = variation.noiseGradient1;
    variation.noiseGradient1 =
      dsp::HashToSignedUnitFloat(dsp::HashUint32(static_cast<uint32_t>(variation.noiseLattice + 1) ^ seed));
  }

  const double t = variation.position - static_cast<double>(variation.noiseLattice);
  const double fade = dsp::Quintic(t);
  const double value0 = variation.noiseGradient0 * t;
  const double value1 = variation.noiseGradient1 * (t - 1.0);
  const double scaled = (value0 + ((value1 - value0) * fade)) * 1.8;
  if(scaled < -1.0)
    return -1.0;
  if(scaled > 1.0)
    return 1.0;
  return scaled;
}

#include "oscillator.h"

#include "../dsp/gradient_noise.h"
#include "../dsp/shared.h"

#include <algorithm>
#include <cmath>
#include <limits>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? sampleRate : dsp::kDefaultSampleRate;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdatePhaseIncrement(frequencyHz);
  UpdatePitchRate();
  UpdateLevelRates();
  UpdatePanSlewRate();
  UpdateVariationParameterSmoothingRate();
  UpdatePitchTarget();
  UpdateLevelTarget();
  UpdatePanTargetGains();
}

void Oscillator::SetVariationSeed(uint32_t seed)
{
  mVariationSeed = (seed != 0u) ? seed : kDefaultVariationSeed;

  // Give each variation stream its own deterministic starting point.
  mIntensityVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0x68E31DA4u) + 1.0) * 100.0;
  mPitchVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0xB5297A4Du) + 1.0) * 100.0;
  mPanVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0x1B56C4E9u) + 1.0) * 100.0;

  UpdatePitchTarget();
  UpdateLevelTarget();
  UpdatePanTargetGains();
}

void Oscillator::SetPitch(double pitchSemitones)
{
  mBasePitch = pitchSemitones;
  UpdatePitchTarget();
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
  const auto panGains = dsp::PanToGains(mBasePan);
  mBasePanLeftGain = panGains[0];
  mBasePanRightGain = panGains[1];
  UpdatePanTargetGains();
}

void Oscillator::SetPanVariation(double amplitude, double rateHz)
{
  mPanVariation.SetTargets(amplitude, rateHz);
}

void Oscillator::SetLevel(double level)
{
  mBaseLevel = (level >= 0.0) ? level : 0.0;
  UpdateLevelTarget();
}

void Oscillator::Reset()
{
  mPhase = 0.0;
  mLevel = 0.0;
  mIntensityVariation.SnapToTargets();
  mPitchVariation.SnapToTargets();
  mPanVariation.SnapToTargets();
  UpdatePitchTarget();
  mPitch = mTargetPitch;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdateLevelTarget();
  // On full retrigger, start from the current pan target.
  UpdatePanTargetGains();
  mPanLeftGain = mTargetPanLeftGain;
  mPanRightGain = mTargetPanRightGain;
  UpdatePhaseIncrement(frequencyHz);
}

std::array<iplug::sample, 2> Oscillator::Process()
{
  ProcessVariation(mIntensityVariation, [this]() { UpdateLevelTarget(); });
  ProcessVariation(mPanVariation, [this]() { UpdatePanTargetGains(); });
  ProcessVariation(mPitchVariation, [this]() { UpdatePitchTarget(); });

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
  mPhaseIncrement = frequencyHz / mSampleRate;
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
  mPanSlewPerSample = 1.0 / (kPanSlewTimeSec * mSampleRate);
}

void Oscillator::UpdateVariationParameterSmoothingRate()
{
  mVariationParameterSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kVariationParameterSmoothingTimeSec);
}

void Oscillator::UpdatePitchTarget()
{
  const double pitchNoise = VariationNoise(mPitchVariation, mVariationSeed ^ 0x17D39EF5u);
  const double pitchSemitones = mBasePitch + (mPitchVariation.amplitude * pitchNoise);
  const double minPitchSemitones = kSemitonesPerOctave * std::log2(kMinFrequencyHz / kA4FrequencyHz);
  mTargetPitch = std::max(minPitchSemitones, pitchSemitones);
}

void Oscillator::UpdateLevelTarget()
{
  if(!IsVariationActiveNow(mIntensityVariation))
  {
    mTargetLevel = mBaseLevel;
    return;
  }

  const double intensityNoise = VariationNoise(mIntensityVariation, mVariationSeed ^ 0xF1023A17u);
  const double levelScale = std::max(0.0, 1.0 + (mIntensityVariation.amplitude * intensityNoise));
  mTargetLevel = mBaseLevel * levelScale;
}

void Oscillator::UpdatePanTargetGains()
{
  if(!IsVariationActiveNow(mPanVariation))
  {
    mTargetPanLeftGain = mBasePanLeftGain;
    mTargetPanRightGain = mBasePanRightGain;
    return;
  }

  const double panNoise = VariationNoise(mPanVariation, mVariationSeed ^ 0xC29B3F4Bu);
  const double modulatedPan = std::clamp(mBasePan + (mPanVariation.amplitude * panNoise), -1.0, 1.0);
  const auto panGains = dsp::PanToGains(modulatedPan);
  mTargetPanLeftGain = panGains[0];
  mTargetPanRightGain = panGains[1];
}

void Oscillator::SmoothVariationParameters(VariationState& variation)
{
  const double targetAmplitude = variation.targetAmplitude.load(std::memory_order_relaxed);
  const double targetRateHz = variation.targetRateHz.load(std::memory_order_relaxed);

  variation.amplitude =
    dsp::SmoothValue(variation.amplitude, targetAmplitude, mVariationParameterSmoothingCoefficient);
  if(std::abs(variation.amplitude - targetAmplitude) <= kVariationParameterEpsilon)
    variation.amplitude = targetAmplitude;

  variation.rateHz =
    dsp::SmoothValue(variation.rateHz, targetRateHz, mVariationParameterSmoothingCoefficient);
  if(std::abs(variation.rateHz - targetRateHz) <= kVariationParameterEpsilon)
    variation.rateHz = targetRateHz;
}

void Oscillator::AdvanceVariationPosition(VariationState& variation)
{
  if(!IsVariationActiveNow(variation) || mSampleRate <= 0.0)
    return;

  variation.position += variation.rateHz / mSampleRate;
}

bool Oscillator::IsVariationActiveNow(const VariationState& variation)
{
  return variation.amplitude > kVariationParameterEpsilon
    && variation.rateHz > kVariationParameterEpsilon;
}

bool Oscillator::HasVariation(const VariationState& variation)
{
  const double targetAmplitude = variation.targetAmplitude.load(std::memory_order_relaxed);
  const double targetRateHz = variation.targetRateHz.load(std::memory_order_relaxed);
  return std::max(variation.amplitude, targetAmplitude) > kVariationParameterEpsilon
    && std::max(variation.rateHz, targetRateHz) > kVariationParameterEpsilon;
}

double Oscillator::VariationNoise(const VariationState& variation, uint32_t seed)
{
  if(!IsVariationActiveNow(variation))
    return 0.0;

  return dsp::GradientNoise1D(variation.position, seed);
}

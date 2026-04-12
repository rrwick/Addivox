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
  UpdatePanVariationSmoothingRate();
  UpdateControlRateVariationTargets();
  UpdatePanTargetGains();
}

void Oscillator::SetVariationSeed(uint32_t seed)
{
  mVariationSeed = (seed != 0u) ? seed : kDefaultVariationSeed;

  // Give each variation stream its own deterministic starting point.
  mIntensityVariationPosition = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0x68E31DA4u) + 1.0) * 100.0;
  mPitchVariationPosition = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0xB5297A4Du) + 1.0) * 100.0;
  mPanVariationPosition = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0x1B56C4E9u) + 1.0) * 100.0;

  UpdateControlRateVariationTargets();
  UpdatePanTargetGains();
}

void Oscillator::SetPitch(double pitchSemitones)
{
  mBasePitch = pitchSemitones;
  UpdateControlRateVariationTargets();
}

void Oscillator::SetPitchTime(double pitchTimeSec)
{
  mPitchTimeSec = std::max(0.0, pitchTimeSec);
  UpdatePitchRate();
}

void Oscillator::SetPitchVariation(double amplitudeSemitones, double rateHz)
{
  mPitchVariationAmplitude = std::max(0.0, amplitudeSemitones);
  mPitchVariationRateHz = std::max(0.0, rateHz);
  UpdateControlRateVariationTargets();
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
  mIntensityVariationAmplitude = std::max(0.0, amplitude);
  mIntensityVariationRateHz = std::max(0.0, rateHz);
  UpdateControlRateVariationTargets();
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
  mTargetPanVariationAmplitude.store(std::max(0.0, amplitude), std::memory_order_relaxed);
  mTargetPanVariationRateHz.store(std::max(0.0, rateHz), std::memory_order_relaxed);
}

void Oscillator::SetLevel(double level)
{
  mBaseLevel = (level >= 0.0) ? level : 0.0;
  UpdateControlRateVariationTargets();
}

void Oscillator::Reset()
{
  mPhase = 0.0;
  mLevel = 0.0;
  mPitch = mTargetPitch;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  mPanVariationAmplitude = mTargetPanVariationAmplitude.load(std::memory_order_relaxed);
  mPanVariationRateHz = mTargetPanVariationRateHz.load(std::memory_order_relaxed);
  // On full retrigger, start from the current pan target.
  UpdatePanTargetGains();
  mPanLeftGain = mTargetPanLeftGain;
  mPanRightGain = mTargetPanRightGain;
  UpdatePhaseIncrement(frequencyHz);
  mVariationSamplesUntilUpdate = 0;
}

std::array<iplug::sample, 2> Oscillator::Process()
{
  if(mVariationSamplesUntilUpdate <= 0)
  {
    UpdateControlRateVariationTargets();
    AdvanceControlRateVariationPositions(kVariationControlIntervalSamples);
    mVariationSamplesUntilUpdate = kVariationControlIntervalSamples;
  }
  --mVariationSamplesUntilUpdate;

  if(HasPanVariation())
  {
    SmoothPanVariationParameters();
    UpdatePanTargetGains();
    AdvancePanVariationPosition();
  }
  else
  {
    mTargetPanLeftGain = mBasePanLeftGain;
    mTargetPanRightGain = mBasePanRightGain;
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

void Oscillator::UpdatePanVariationSmoothingRate()
{
  mPanVariationSmoothingCoefficient =
    dsp::ExponentialSmoothingCoefficient(mSampleRate, kPanVariationSmoothingTimeSec);
}

void Oscillator::UpdateControlRateVariationTargets()
{
  const double intensityNoise = VariationNoise(
    mIntensityVariationAmplitude,
    mIntensityVariationRateHz,
    mIntensityVariationPosition,
    mVariationSeed ^ 0xF1023A17u);
  const double levelScale = std::max(0.0, 1.0 + mIntensityVariationAmplitude * intensityNoise);
  mTargetLevel = mBaseLevel * levelScale;

  const double pitchNoise = VariationNoise(
    mPitchVariationAmplitude,
    mPitchVariationRateHz,
    mPitchVariationPosition,
    mVariationSeed ^ 0x17D39EF5u);
  const double pitchSemitones = mBasePitch + mPitchVariationAmplitude * pitchNoise;
  const double minPitchSemitones = kSemitonesPerOctave * std::log2(kMinFrequencyHz / kA4FrequencyHz);
  mTargetPitch = std::max(minPitchSemitones, pitchSemitones);
}

void Oscillator::UpdatePanTargetGains()
{
  if(!IsPanVariationActiveNow())
  {
    mTargetPanLeftGain = mBasePanLeftGain;
    mTargetPanRightGain = mBasePanRightGain;
    return;
  }

  const double panNoise = dsp::GradientNoise1D(mPanVariationPosition, mVariationSeed ^ 0xC29B3F4Bu);
  const double modulatedPan = std::clamp(mBasePan + (mPanVariationAmplitude * panNoise), -1.0, 1.0);
  const auto panGains = dsp::PanToGains(modulatedPan);
  mTargetPanLeftGain = panGains[0];
  mTargetPanRightGain = panGains[1];
}

void Oscillator::SmoothPanVariationParameters()
{
  const double targetAmplitude = mTargetPanVariationAmplitude.load(std::memory_order_relaxed);
  const double targetRateHz = mTargetPanVariationRateHz.load(std::memory_order_relaxed);

  mPanVariationAmplitude =
    dsp::SmoothValue(mPanVariationAmplitude, targetAmplitude, mPanVariationSmoothingCoefficient);
  if(std::abs(mPanVariationAmplitude - targetAmplitude) <= kPanVariationParameterEpsilon)
    mPanVariationAmplitude = targetAmplitude;

  mPanVariationRateHz =
    dsp::SmoothValue(mPanVariationRateHz, targetRateHz, mPanVariationSmoothingCoefficient);
  if(std::abs(mPanVariationRateHz - targetRateHz) <= kPanVariationParameterEpsilon)
    mPanVariationRateHz = targetRateHz;
}

void Oscillator::AdvanceControlRateVariationPositions(int numSamples)
{
  if(numSamples <= 0 || mSampleRate <= 0.0)
    return;

  const double deltaTimeSec = static_cast<double>(numSamples) / mSampleRate;
  mIntensityVariationPosition += mIntensityVariationRateHz * deltaTimeSec;
  mPitchVariationPosition += mPitchVariationRateHz * deltaTimeSec;
}

void Oscillator::AdvancePanVariationPosition()
{
  if(!IsPanVariationActiveNow() || mSampleRate <= 0.0)
    return;

  mPanVariationPosition += mPanVariationRateHz / mSampleRate;
}

bool Oscillator::IsPanVariationActiveNow() const
{
  return mPanVariationAmplitude > kPanVariationParameterEpsilon
    && mPanVariationRateHz > kPanVariationParameterEpsilon;
}

bool Oscillator::HasPanVariation() const
{
  const double targetAmplitude = mTargetPanVariationAmplitude.load(std::memory_order_relaxed);
  const double targetRateHz = mTargetPanVariationRateHz.load(std::memory_order_relaxed);
  return std::max(mPanVariationAmplitude, targetAmplitude) > kPanVariationParameterEpsilon
    && std::max(mPanVariationRateHz, targetRateHz) > kPanVariationParameterEpsilon;
}

double Oscillator::VariationNoise(double amplitude, double rateHz, double position, uint32_t seed)
{
  if(amplitude <= 0.0 || rateHz <= 0.0)
    return 0.0;

  return dsp::GradientNoise1D(position, seed);
}

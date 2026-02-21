#include "oscillator.h"

#include <algorithm>
#include <cmath>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? static_cast<float>(sampleRate) : 44100.f;
  UpdatePhaseIncrement();
  UpdateLevelRates();
  UpdatePanSlewRate();
  UpdateVariationTargets();
}

void Oscillator::SetVariationSeed(uint32_t seed)
{
  mVariationSeed = (seed != 0u) ? seed : kDefaultVariationSeed;

  // Give each variation stream its own deterministic starting point.
  mIntensityVariationPosition = (HashToSignedUnitFloat(mVariationSeed ^ 0x68E31DA4u) + 1.f) * 100.f;
  mPitchVariationPosition = (HashToSignedUnitFloat(mVariationSeed ^ 0xB5297A4Du) + 1.f) * 100.f;
  mPanVariationPosition = (HashToSignedUnitFloat(mVariationSeed ^ 0x1B56C4E9u) + 1.f) * 100.f;

  UpdateVariationTargets();
}

void Oscillator::SetFrequency(float frequencyHz)
{
  mBaseFrequencyHz = (frequencyHz >= 0.f) ? frequencyHz : 0.f;
  UpdateVariationTargets();
}

void Oscillator::SetPitch(float pitchCents)
{
  mBasePitchCents = pitchCents;
  UpdateVariationTargets();
}

void Oscillator::SetPitchVariation(float amplitudeCents, float rateHz)
{
  mPitchVariationAmplitudeCents = ClampNonNegative(amplitudeCents);
  mPitchVariationRateHz = ClampNonNegative(rateHz);
  UpdateVariationTargets();
}

void Oscillator::SetAttackTime(float attackTimeSec)
{
  mAttackTimeSec = std::max(0.f, attackTimeSec);
  UpdateLevelRates();
}

void Oscillator::SetReleaseTime(float releaseTimeSec)
{
  mReleaseTimeSec = std::max(0.f, releaseTimeSec);
  UpdateLevelRates();
}

void Oscillator::SetIntensityVariation(float amplitude, float rateHz)
{
  mIntensityVariationAmplitude = ClampNonNegative(amplitude);
  mIntensityVariationRateHz = ClampNonNegative(rateHz);
  UpdateVariationTargets();
}

void Oscillator::SetPan(float pan)
{
  mBasePan = ClampPan(pan);
  UpdateVariationTargets();
}

void Oscillator::SetPanVariation(float amplitude, float rateHz)
{
  mPanVariationAmplitude = ClampNonNegative(amplitude);
  mPanVariationRateHz = ClampNonNegative(rateHz);
  UpdateVariationTargets();
}

void Oscillator::SetLevel(float level)
{
  mBaseLevel = (level >= 0.f) ? level : 0.f;
  UpdateVariationTargets();
}

void Oscillator::Reset()
{
  mPhase = 0.f;
  mLevel = 0.f;
  mVariationSamplesUntilUpdate = 0;
}

std::array<float, 2> Oscillator::Process()
{
  if(mVariationSamplesUntilUpdate <= 0)
  {
    UpdateVariationTargets();
    AdvanceVariationPositions(kVariationControlIntervalSamples);
    mVariationSamplesUntilUpdate = kVariationControlIntervalSamples;
  }
  --mVariationSamplesUntilUpdate;

  const float rate = (mTargetLevel > mLevel) ? mAttackRate : mReleaseRate;
  mLevel += (mTargetLevel - mLevel) * rate;

  mPanLeftGain += (mTargetPanLeftGain - mPanLeftGain) * mPanSlewRate;
  mPanRightGain += (mTargetPanRightGain - mPanRightGain) * mPanSlewRate;

  if(mTargetLevel <= kLevelEpsilon && mLevel < kLevelEpsilon)
    mLevel = 0.f;

  const float out = std::sin(kTwoPi * mPhase) * mLevel;

  mPhase += mPhaseIncrement;
  if(mPhase >= 1.f)
    mPhase -= std::floor(mPhase);

  // Deactivate oscillator if frequency is out of range - prevents aliasing
  if(mFrequencyHz > 20000.f)
    return {0.f, 0.f};

  return {out * mPanLeftGain, out * mPanRightGain};
}

bool Oscillator::IsActive() const
{
  return (mTargetLevel > kLevelEpsilon) || (mLevel > kLevelEpsilon);
}

void Oscillator::UpdatePhaseIncrement()
{
  mPhaseIncrement = mFrequencyHz / mSampleRate;
}

void Oscillator::UpdateLevelRates()
{
  mAttackRate = TimeToRate(mAttackTimeSec, mSampleRate);
  mReleaseRate = TimeToRate(mReleaseTimeSec, mSampleRate);
}

void Oscillator::UpdatePanSlewRate()
{
  mPanSlewRate = TimeToRate(kPanSlewTimeSec, mSampleRate);
}

void Oscillator::UpdateVariationTargets()
{
  const float intensityNoise = VariationNoise(
    mIntensityVariationAmplitude,
    mIntensityVariationRateHz,
    mIntensityVariationPosition,
    mVariationSeed ^ 0xF1023A17u);
  const float levelScale = std::max(0.f, 1.f + mIntensityVariationAmplitude * intensityNoise);
  mTargetLevel = mBaseLevel * levelScale;

  const float pitchNoise = VariationNoise(
    mPitchVariationAmplitudeCents,
    mPitchVariationRateHz,
    mPitchVariationPosition,
    mVariationSeed ^ 0x17D39EF5u);
  const float pitchCents = mBasePitchCents + mPitchVariationAmplitudeCents * pitchNoise;
  mFrequencyHz = mBaseFrequencyHz * CentsToRatio(pitchCents);
  UpdatePhaseIncrement();

  const float panNoise = VariationNoise(
    mPanVariationAmplitude,
    mPanVariationRateHz,
    mPanVariationPosition,
    mVariationSeed ^ 0xC29B3F4Bu);
  const float modulatedPan = ClampPan(mBasePan + mPanVariationAmplitude * panNoise);
  const auto panGains = PanToGains(modulatedPan);
  mTargetPanLeftGain = panGains[0];
  mTargetPanRightGain = panGains[1];

  // Apply pan instantly when silent; slew only during active output.
  if(!IsActive())
  {
    mPanLeftGain = mTargetPanLeftGain;
    mPanRightGain = mTargetPanRightGain;
  }
}

void Oscillator::AdvanceVariationPositions(int numSamples)
{
  if(numSamples <= 0 || mSampleRate <= 0.f)
    return;

  const float deltaTimeSec = static_cast<float>(numSamples) / mSampleRate;
  mIntensityVariationPosition += mIntensityVariationRateHz * deltaTimeSec;
  mPitchVariationPosition += mPitchVariationRateHz * deltaTimeSec;
  mPanVariationPosition += mPanVariationRateHz * deltaTimeSec;
}

float Oscillator::TimeToRate(float timeSec, float sampleRate)
{
  if(timeSec <= 0.f || sampleRate <= 0.f)
    return 1.f;

  return 1.f - std::exp(-1.f / (timeSec * sampleRate));
}

float Oscillator::ClampPan(float pan)
{
  return std::clamp(pan, -1.f, 1.f);
}

std::array<float, 2> Oscillator::PanToGains(float pan)
{
  // Equal-power pan law keeps perceived loudness stable across stereo position.
  const float clampedPan = ClampPan(pan);
  const float angle = (clampedPan + 1.f) * (kPi * 0.25f);
  return {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
}

float Oscillator::ClampNonNegative(float value)
{
  return std::max(0.f, value);
}

float Oscillator::CentsToRatio(float cents)
{
  return std::exp2(cents / 1200.f);
}

float Oscillator::VariationNoise(float amplitude, float rateHz, float position, uint32_t seed)
{
  if(amplitude <= 0.f || rateHz <= 0.f)
    return 0.f;

  return GradientNoise1D(position, seed);
}

float Oscillator::Quintic(float t)
{
  // Perlin fade curve for smooth interpolation.
  return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}

uint32_t Oscillator::HashUint32(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

float Oscillator::HashToSignedUnitFloat(uint32_t x)
{
  return (static_cast<float>(x) / 4294967295.f) * 2.f - 1.f;
}

float Oscillator::GradientNoise1D(float position, uint32_t seed)
{
  const int lattice0 = static_cast<int>(std::floor(position));
  const float t = position - static_cast<float>(lattice0);
  const float fade = Quintic(t);

  const float gradient0 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0) ^ seed));
  const float gradient1 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0 + 1) ^ seed));

  const float value0 = gradient0 * t;
  const float value1 = gradient1 * (t - 1.f);
  const float blended = value0 + (value1 - value0) * fade;
  return std::clamp(blended * 1.8f, -1.f, 1.f);
}

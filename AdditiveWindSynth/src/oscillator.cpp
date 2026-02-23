#include "oscillator.h"

#include <algorithm>
#include <cmath>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
  UpdatePhaseIncrement();
  UpdatePitchRate();
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

void Oscillator::SetPitch(double pitchCents)
{
  mBasePitchCents = pitchCents;
  UpdateVariationTargets();
}

void Oscillator::SetPitchTime(double pitchTimeSec)
{
  mPitchTimeSec = std::max(0.0, pitchTimeSec);
  UpdatePitchRate();
}

void Oscillator::SetPitchVariation(double amplitudeCents, double rateHz)
{
  mPitchVariationAmplitudeCents = std::max(0.0, amplitudeCents);
  mPitchVariationRateHz = std::max(0.0, rateHz);
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
  mPhase = 0.0;
  mLevel = 0.f;
  mPitch = mTargetPitch;
  mFrequencyHz = std::exp2(mPitch);
  // On full retrigger, start from the current pan target.
  mPanLeftGain = mTargetPanLeftGain;
  mPanRightGain = mTargetPanRightGain;
  UpdatePhaseIncrement();
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

  mPitch += (mTargetPitch - mPitch) * mPitchRate;
  mFrequencyHz = std::exp2(mPitch);
  UpdatePhaseIncrement();

  if(mTargetLevel <= kLevelEpsilon && mLevel < kLevelEpsilon)
    mLevel = 0.f;

  const float out = static_cast<float>(std::sin(kTwoPi * mPhase) * static_cast<double>(mLevel));

  mPhase += mPhaseIncrement;
  if(mPhase >= 1.0)
    mPhase -= std::floor(mPhase);

  // Deactivate oscillator if frequency is out of range - prevents aliasing
  if(mFrequencyHz > 20000.0)
    return {0.f, 0.f};

  return {out * mPanLeftGain, out * mPanRightGain};
}

bool Oscillator::IsActive() const
{
  return (mTargetLevel > kLevelEpsilon) || (mLevel > kLevelEpsilon);
}

HarmonicVisualizerOscillator Oscillator::GetVisualizerState() const
{
  return HarmonicVisualizerOscillator{static_cast<float>(mFrequencyHz), mLevel, mPanLeftGain, mPanRightGain};
}

void Oscillator::UpdatePhaseIncrement()
{
  mPhaseIncrement = mFrequencyHz / mSampleRate;
}

void Oscillator::UpdatePitchRate()
{
  mPitchRate = TimeToRate(mPitchTimeSec, mSampleRate);
}

void Oscillator::UpdateLevelRates()
{
  mAttackRate = static_cast<float>(TimeToRate(mAttackTimeSec, mSampleRate));
  mReleaseRate = static_cast<float>(TimeToRate(mReleaseTimeSec, mSampleRate));
}

void Oscillator::UpdatePanSlewRate()
{
  mPanSlewRate = static_cast<float>(TimeToRate(kPanSlewTimeSec, mSampleRate));
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

  const double pitchNoise = VariationNoise(
    mPitchVariationAmplitudeCents,
    mPitchVariationRateHz,
    mPitchVariationPosition,
    mVariationSeed ^ 0x17D39EF5u);
  const double pitchCents = mBasePitchCents + mPitchVariationAmplitudeCents * pitchNoise;
  const double minPitch = std::log2(kMinFrequencyHz);
  mTargetPitch = std::max(minPitch, kLog2A4 + pitchCents / 1200.0);

  const float panNoise = VariationNoise(
    mPanVariationAmplitude,
    mPanVariationRateHz,
    mPanVariationPosition,
    mVariationSeed ^ 0xC29B3F4Bu);
  const float modulatedPan = ClampPan(mBasePan + mPanVariationAmplitude * panNoise);
  const auto panGains = PanToGains(modulatedPan);
  mTargetPanLeftGain = panGains[0];
  mTargetPanRightGain = panGains[1];

}

void Oscillator::AdvanceVariationPositions(int numSamples)
{
  if(numSamples <= 0 || mSampleRate <= 0.0)
    return;

  const double deltaTimeSec = static_cast<double>(numSamples) / mSampleRate;
  mIntensityVariationPosition += mIntensityVariationRateHz * deltaTimeSec;
  mPitchVariationPosition += mPitchVariationRateHz * deltaTimeSec;
  mPanVariationPosition += mPanVariationRateHz * deltaTimeSec;
}

double Oscillator::TimeToRate(double timeSec, double sampleRate)
{
  if(timeSec <= 0.0 || sampleRate <= 0.0)
    return 1.0;

  return 1.0 - std::exp(-1.0 / (timeSec * sampleRate));
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

double Oscillator::VariationNoise(double amplitude, double rateHz, double position, uint32_t seed)
{
  if(amplitude <= 0.0 || rateHz <= 0.0)
    return 0.0;

  return GradientNoise1D(position, seed);
}

double Oscillator::Quintic(double t)
{
  // Perlin fade curve for smooth interpolation.
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
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

double Oscillator::HashToSignedUnitFloat(uint32_t x)
{
  return (static_cast<double>(x) / 4294967295.0) * 2.0 - 1.0;
}

double Oscillator::GradientNoise1D(double position, uint32_t seed)
{
  const int lattice0 = static_cast<int>(std::floor(position));
  const double t = position - static_cast<double>(lattice0);
  const double fade = Quintic(t);

  const double gradient0 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0) ^ seed));
  const double gradient1 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0 + 1) ^ seed));

  const double value0 = gradient0 * t;
  const double value1 = gradient1 * (t - 1.0);
  const double blended = value0 + (value1 - value0) * fade;
  return std::clamp(blended * 1.8, -1.0, 1.0);
}

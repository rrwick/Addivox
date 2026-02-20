#include "oscillator.h"

#include <algorithm>
#include <cmath>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? static_cast<float>(sampleRate) : 44100.f;
  UpdatePhaseIncrement();
  UpdateLevelRates();
}

void Oscillator::SetFrequency(float frequencyHz)
{
  mFrequencyHz = (frequencyHz >= 0.f) ? frequencyHz : 0.f;
  UpdatePhaseIncrement();
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

void Oscillator::SetLevel(float level)
{
  mTargetLevel = (level >= 0.f) ? level : 0.f;
}

void Oscillator::Reset()
{
  mPhase = 0.f;
  mLevel = 0.f;
}

float Oscillator::Process()
{
  const float rate = (mTargetLevel > mLevel) ? mAttackRate : mReleaseRate;
  mLevel += (mTargetLevel - mLevel) * rate;

  if(mTargetLevel <= kLevelEpsilon && mLevel < kLevelEpsilon)
    mLevel = 0.f;

  const float out = std::sin(kTwoPi * mPhase) * mLevel;

  mPhase += mPhaseIncrement;
  if(mPhase >= 1.f)
    mPhase -= std::floor(mPhase);

  // Deactivate oscillator if frequency is out of range - prevents aliasing
  if (mFrequencyHz > 20000.f)
    return 0.f;

  return out;
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

float Oscillator::TimeToRate(float timeSec, float sampleRate)
{
  if(timeSec <= 0.f || sampleRate <= 0.f)
    return 1.f;

  return 1.f - std::exp(-1.f / (timeSec * sampleRate));
}

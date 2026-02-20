#include "oscillator.h"

#include <algorithm>
#include <cmath>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? static_cast<float>(sampleRate) : 44100.f;
  UpdatePhaseIncrement();
  UpdateLevelRates();
  UpdatePanSlewRate();
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

void Oscillator::SetPan(float pan)
{
  const auto panGains = PanToGains(ClampPan(pan));
  mTargetPanLeftGain = panGains[0];
  mTargetPanRightGain = panGains[1];

  // Apply pan instantly when silent; slew only during active output.
  if(!IsActive())
  {
    mPanLeftGain = mTargetPanLeftGain;
    mPanRightGain = mTargetPanRightGain;
  }
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

std::array<float, 2> Oscillator::Process()
{
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
  if (mFrequencyHz > 20000.f)
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

std::array<float, 2> Oscillator::PanToGains(float pan)
{
  // Equal-power pan law keeps perceived loudness stable across stereo position.
  const float clampedPan = ClampPan(pan);
  const float angle = (clampedPan + 1.f) * (kPi * 0.25f);
  return {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
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

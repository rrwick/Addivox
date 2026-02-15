#include "oscillator.h"

#include <cmath>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? static_cast<float>(sampleRate) : 44100.f;
  UpdatePhaseIncrement();
}

void Oscillator::SetFrequency(float frequencyHz)
{
  mFrequencyHz = (frequencyHz >= 0.f) ? frequencyHz : 0.f;
  UpdatePhaseIncrement();
}

void Oscillator::SetBreath(float breath)
{
  mBreath = breath;
}

void Oscillator::Reset(float phase01)
{
  mPhase = phase01 - std::floor(phase01);
  mLevel = mBreath;
}

float Oscillator::Process()
{
  const float out = std::sin(kTwoPi * mPhase) * mLevel;

  const float newPhase = mPhase + mPhaseIncrement;
  if ((mPhase < 0.5f && newPhase >= 0.5f) || (mPhase < 1.f && newPhase >= 1.f))
    mLevel = mBreath;

  mPhase = newPhase;
  if(mPhase >= 1.f)
    mPhase -= 1.f;

  return out;
}

void Oscillator::UpdatePhaseIncrement()
{
  mPhaseIncrement = mFrequencyHz / mSampleRate;
}

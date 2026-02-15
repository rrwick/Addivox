#include "oscillator.h"

#include <cmath>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? static_cast<float>(sampleRate) : 44100.f;
  UpdatePhaseIncrement();
  mBreathSmoothingCoeff = 1.f - std::exp(-1.f / (kBreathSmoothingTimeSec * mSampleRate));
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
  mLevel += (mBreath - mLevel) * mBreathSmoothingCoeff;
  const float out = std::sin(kTwoPi * mPhase) * mLevel;

  mPhase += mPhaseIncrement;
  if(mPhase >= 1.f)
    mPhase -= 1.f;

  return out;
}

void Oscillator::UpdatePhaseIncrement()
{
  mPhaseIncrement = mFrequencyHz / mSampleRate;
}

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

void Oscillator::Reset(float phase01)
{
  mPhase = phase01 - std::floor(phase01);
}

float Oscillator::Process()
{
  const float out = std::sin(kTwoPi * mPhase);

  mPhase += mPhaseIncrement;
  if(mPhase >= 1.f)
    mPhase -= 1.f;

  return out;
}

void Oscillator::UpdatePhaseIncrement()
{
  mPhaseIncrement = mFrequencyHz / mSampleRate;
}

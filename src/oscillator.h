#pragma once

class Oscillator
{
public:
  void SetSampleRate(double sampleRate);
  void SetFrequency(float frequencyHz);
  void SetBreath(float breath);
  void Reset(float phase01 = 0.f);
  float Process();

private:
  void UpdatePhaseIncrement();

  static constexpr float kTwoPi = 6.28318530717958647692f;
  static constexpr float kBreathSmoothingTimeSec = 0.002f;

  float mSampleRate = 44100.f;
  float mFrequencyHz = 440.f;
  float mBreath = 1.f;
  float mLevel = 1.f;
  float mBreathSmoothingCoeff = 1.f;
  float mPhase = 0.f;
  float mPhaseIncrement = 440.f / 44100.f;
};

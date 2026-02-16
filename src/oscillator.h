#pragma once

class Oscillator
{
public:
  void SetSampleRate(double sampleRate);
  void SetFrequency(float frequencyHz);
  void SetLevel(float level);
  void Reset();
  float Process();
  bool IsActive() const;

private:
  void UpdatePhaseIncrement();
  void UpdateLevelRates();
  static float TimeToRate(float timeSec, float sampleRate);

  static constexpr float kTwoPi = 6.28318530717958647692f;
  static constexpr float kAttackTimeSec = 0.002f;
  static constexpr float kReleaseTimeSec = 0.030f;
  static constexpr float kLevelEpsilon = 0.0001f;

  float mSampleRate = 44100.f;
  float mFrequencyHz = 440.f;
  float mTargetLevel = 0.f;
  float mLevel = 0.f;
  float mAttackRate = 1.f;
  float mReleaseRate = 1.f;
  float mPhase = 0.f;
  float mPhaseIncrement = 440.f / 44100.f;
};

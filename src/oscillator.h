#pragma once

#include <array>

class Oscillator
{
public:
  void SetSampleRate(double sampleRate);
  void SetFrequency(float frequencyHz);
  void SetAttackTime(float attackTimeSec);
  void SetReleaseTime(float releaseTimeSec);
  void SetPan(float pan);
  void SetLevel(float level);
  void Reset();
  std::array<float, 2> Process();
  bool IsActive() const;

private:
  void UpdatePhaseIncrement();
  void UpdateLevelRates();
  void UpdatePanSlewRate();
  static float TimeToRate(float timeSec, float sampleRate);
  static float ClampPan(float pan);
  static std::array<float, 2> PanToGains(float pan);

  static constexpr float kTwoPi = 6.28318530717958647692f;
  static constexpr float kPi = 3.14159265358979323846f;
  static constexpr float kLevelEpsilon = 0.00001f;
  static constexpr float kPanSlewTimeSec = 0.005f;

  float mSampleRate = 44100.f;
  float mFrequencyHz = 440.f;
  float mTargetLevel = 0.f;
  float mLevel = 0.f;
  float mAttackTimeSec = 0.f;
  float mReleaseTimeSec = 0.f;
  float mPanLeftGain = 0.70710678f;
  float mPanRightGain = 0.70710678f;
  float mTargetPanLeftGain = 0.70710678f;
  float mTargetPanRightGain = 0.70710678f;
  float mPanSlewRate = 1.f;
  float mAttackRate = 1.f;
  float mReleaseRate = 1.f;
  float mPhase = 0.f;
  float mPhaseIncrement = 440.f / 44100.f;
};

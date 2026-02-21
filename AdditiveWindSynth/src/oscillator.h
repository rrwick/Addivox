#pragma once

#include <array>
#include <cstdint>

class Oscillator
{
public:
  void SetSampleRate(double sampleRate);
  void SetVariationSeed(uint32_t seed);
  void SetFrequency(float frequencyHz);
  void SetPitch(float pitchCents);
  void SetPitchVariation(float amplitudeCents, float rateHz);
  void SetAttackTime(float attackTimeSec);
  void SetReleaseTime(float releaseTimeSec);
  void SetIntensityVariation(float amplitude, float rateHz);
  void SetPan(float pan);
  void SetPanVariation(float amplitude, float rateHz);
  void SetLevel(float level);
  void Reset();
  std::array<float, 2> Process();
  bool IsActive() const;

private:
  void UpdatePhaseIncrement();
  void UpdateLevelRates();
  void UpdatePanSlewRate();
  void UpdateVariationTargets();
  void AdvanceVariationPositions(int numSamples);
  static float TimeToRate(float timeSec, float sampleRate);
  static float ClampPan(float pan);
  static std::array<float, 2> PanToGains(float pan);
  static float ClampNonNegative(float value);
  static float Quintic(float t);
  static uint32_t HashUint32(uint32_t x);
  static float HashToSignedUnitFloat(uint32_t x);
  static float GradientNoise1D(float position, uint32_t seed);

  static constexpr float kTwoPi = 6.28318530717958647692f;
  static constexpr float kPi = 3.14159265358979323846f;
  static constexpr float kLevelEpsilon = 0.00001f;
  static constexpr float kPanSlewTimeSec = 0.005f;
  static constexpr int kVariationControlIntervalSamples = 16;
  static constexpr uint32_t kDefaultVariationSeed = 0xA53C9D1Fu;

  float mSampleRate = 44100.f;
  float mFrequencyHz = 440.f;
  float mBaseFrequencyHz = 440.f;
  float mBasePitchCents = 0.f;
  float mTargetLevel = 0.f;
  float mBaseLevel = 0.f;
  float mLevel = 0.f;
  float mAttackTimeSec = 0.f;
  float mReleaseTimeSec = 0.f;
  float mBasePan = 0.f;
  float mPanLeftGain = 0.70710678f;
  float mPanRightGain = 0.70710678f;
  float mTargetPanLeftGain = 0.70710678f;
  float mTargetPanRightGain = 0.70710678f;
  float mPanSlewRate = 1.f;
  float mIntensityVariationAmplitude = 0.f;
  float mIntensityVariationRateHz = 0.f;
  float mPitchVariationAmplitudeCents = 0.f;
  float mPitchVariationRateHz = 0.f;
  float mPanVariationAmplitude = 0.f;
  float mPanVariationRateHz = 0.f;
  float mIntensityVariationPosition = 0.f;
  float mPitchVariationPosition = 0.f;
  float mPanVariationPosition = 0.f;
  uint32_t mVariationSeed = kDefaultVariationSeed;
  int mVariationSamplesUntilUpdate = 0;
  float mAttackRate = 1.f;
  float mReleaseRate = 1.f;
  float mPhase = 0.f;
  float mPhaseIncrement = 440.f / 44100.f;
};

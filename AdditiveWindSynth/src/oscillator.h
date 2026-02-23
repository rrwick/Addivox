#pragma once

#include <array>
#include <cstdint>

#include "IPlugConstants.h"
#include "harmonic_visualizer_frame.h"

class Oscillator
{
public:
  void SetSampleRate(double sampleRate);
  void SetVariationSeed(uint32_t seed);
  void SetPitch(double pitchCents);
  void SetPitchTime(double pitchTimeSec);
  void SetPitchVariation(double amplitudeCents, double rateHz);
  void SetAttackTime(float attackTimeSec);
  void SetReleaseTime(float releaseTimeSec);
  void SetIntensityVariation(float amplitude, float rateHz);
  void SetPan(float pan);
  void SetPanVariation(float amplitude, float rateHz);
  void SetLevel(float level);
  void Reset();
  std::array<iplug::sample, 2> Process();
  bool IsActive() const;
  HarmonicVisualizerOscillator GetVisualizerState() const;

private:
  void UpdatePhaseIncrement();
  void UpdatePitchRate();
  void UpdateLevelRates();
  void UpdatePanSlewRate();
  void UpdateVariationTargets();
  void AdvanceVariationPositions(int numSamples);
  static double TimeToRate(double timeSec, double sampleRate);
  static float ClampPan(float pan);
  static std::array<float, 2> PanToGains(float pan);
  static float ClampNonNegative(float value);
  static double VariationNoise(double amplitude, double rateHz, double position, uint32_t seed);
  static double Quintic(double t);
  static uint32_t HashUint32(uint32_t x);
  static double HashToSignedUnitFloat(uint32_t x);
  static double GradientNoise1D(double position, uint32_t seed);

  static constexpr double kTwoPi = 6.28318530717958647692;
  static constexpr float kPi = 3.14159265358979323846f;
  static constexpr double kLog2A4 = 8.7813597135246596;
  static constexpr double kMinFrequencyHz = 0.000001;
  static constexpr float kLevelEpsilon = 0.00001f;
  static constexpr float kPanSlewTimeSec = 0.005f;
  static constexpr int kVariationControlIntervalSamples = 16;
  static constexpr uint32_t kDefaultVariationSeed = 0xA53C9D1Fu;

  double mSampleRate = 44100.0;
  double mFrequencyHz = 440.0;
  // Absolute pitch in cents relative to A4 (440 Hz).
  double mBasePitchCents = 0.0;
  double mPitch = kLog2A4;
  double mTargetPitch = kLog2A4;
  float mTargetLevel = 0.f;
  float mBaseLevel = 0.f;
  float mLevel = 0.f;
  float mAttackTimeSec = 0.f;
  float mReleaseTimeSec = 0.f;
  double mPitchTimeSec = 0.0;
  float mBasePan = 0.f;
  float mPanLeftGain = 0.70710678f;
  float mPanRightGain = 0.70710678f;
  float mTargetPanLeftGain = 0.70710678f;
  float mTargetPanRightGain = 0.70710678f;
  float mPanSlewRate = 1.f;
  float mIntensityVariationAmplitude = 0.f;
  float mIntensityVariationRateHz = 0.f;
  double mPitchVariationAmplitudeCents = 0.0;
  double mPitchVariationRateHz = 0.0;
  float mPanVariationAmplitude = 0.f;
  float mPanVariationRateHz = 0.f;
  float mIntensityVariationPosition = 0.f;
  double mPitchVariationPosition = 0.0;
  float mPanVariationPosition = 0.f;
  uint32_t mVariationSeed = kDefaultVariationSeed;
  int mVariationSamplesUntilUpdate = 0;
  float mAttackRate = 1.f;
  float mReleaseRate = 1.f;
  double mPitchRate = 1.0;
  double mPhase = 0.0;
  double mPhaseIncrement = 440.0 / 44100.0;
};

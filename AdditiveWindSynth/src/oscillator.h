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
  void SetAttackTime(double attackTimeSec);
  void SetReleaseTime(double releaseTimeSec);
  void SetIntensityVariation(double amplitude, double rateHz);
  void SetPan(double pan);
  void SetPanVariation(double amplitude, double rateHz);
  void SetLevel(double level);
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
  static double ClampPan(double pan);
  static std::array<double, 2> PanToGains(double pan);
  static double ClampNonNegative(double value);
  static double VariationNoise(double amplitude, double rateHz, double position, uint32_t seed);
  static double Quintic(double t);
  static uint32_t HashUint32(uint32_t x);
  static double HashToSignedUnitFloat(uint32_t x);
  static double GradientNoise1D(double position, uint32_t seed);

  static constexpr double kTwoPi = 6.28318530717958647692;
  static constexpr double kPi = 3.14159265358979323846;
  static constexpr double kLog2A4 = 8.7813597135246596;
  static constexpr double kMinFrequencyHz = 0.000001;
  static constexpr double kLevelEpsilon = 0.00001;
  static constexpr double kPanSlewTimeSec = 0.005;
  static constexpr int kVariationControlIntervalSamples = 16;
  static constexpr uint32_t kDefaultVariationSeed = 0xA53C9D1Fu;

  double mSampleRate = 44100.0;
  double mFrequencyHz = 440.0;
  // Absolute pitch in cents relative to A4 (440 Hz).
  double mBasePitchCents = 0.0;
  double mPitch = kLog2A4;
  double mTargetPitch = kLog2A4;
  double mTargetLevel = 0.0;
  double mBaseLevel = 0.0;
  double mLevel = 0.0;
  double mAttackTimeSec = 0.0;
  double mReleaseTimeSec = 0.0;
  double mPitchTimeSec = 0.0;
  double mBasePan = 0.0;
  double mPanLeftGain = 0.70710678;
  double mPanRightGain = 0.70710678;
  double mTargetPanLeftGain = 0.70710678;
  double mTargetPanRightGain = 0.70710678;
  double mPanSlewRate = 1.0;
  double mIntensityVariationAmplitude = 0.0;
  double mIntensityVariationRateHz = 0.0;
  double mPitchVariationAmplitudeCents = 0.0;
  double mPitchVariationRateHz = 0.0;
  double mPanVariationAmplitude = 0.0;
  double mPanVariationRateHz = 0.0;
  double mIntensityVariationPosition = 0.0;
  double mPitchVariationPosition = 0.0;
  double mPanVariationPosition = 0.0;
  uint32_t mVariationSeed = kDefaultVariationSeed;
  int mVariationSamplesUntilUpdate = 0;
  double mAttackRate = 1.0;
  double mReleaseRate = 1.0;
  double mPitchRate = 1.0;
  double mPhase = 0.0;
  double mPhaseIncrement = 440.0 / 44100.0;
};

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "../dsp/gradient_noise.h"
#include "../visualizer/harmonic_visualizer_frame.h"
#include "IPlugConstants.h"

class Oscillator {
public:
  // Render stereo output and advance by one sample.
  std::array<iplug::sample, 2> Process();
  void SetSampleRate(double sampleRate);
  void SetVariationSeed(uint32_t seed);
  // Pitch is in semitones relative to A4 (440 Hz); portamento is seconds per semitone (0 = immediate).
  void SetPitch(double pitchSemitones);
  void SetPitchTime(double pitchTimeSec);
  void SetPitchVariation(double amplitudeSemitones, double rateHz);
  void SetAttackTime(double attackTimeSec);
  void SetReleaseTime(double releaseTimeSec);
  void SetLevelVariation(double amplitude, double rateHz);
  void SetPan(double pan); // [-1, 1]: left to right
  void SetPanVariation(double amplitude, double rateHz);
  void SetLevel(double level); // Linear gain
  void Reset();
  bool IsActive() const;
  HarmonicVisualizerOscillator GetVisualizerState() const;
  double GetFrequencyHz() const { return mFrequencyHz; }

private:
  double mSampleRate = 44100.0;
  double mInverseSampleRate = 1.0 / 44100.0;

  double mPitch = 0.0;
  double mBasePitch = 0.0;          // target pitch before pitch-variation modulation
  double mTargetPitch = 0.0;        // target pitch after pitch-variation modulation
  double mPitchTimeSec = 0.0;       // portamento time in seconds per semitone, 0=immediate change
  double mPitchRatePerSample = 1.0; // portamento rate in semitones/sample
  static constexpr double kA4FrequencyHz = 440.0;
  static constexpr double kSemitonesPerOctave = 12.0;
  static constexpr double kMinFrequencyHz = 0.01;
  void UpdatePitchRate();

  // One-pole attack/release follows the breath-controlled target level throughout each note.
  double mLevel = 0.0;
  double mBaseLevel = 0.0;   // target level before level variation modulation
  double mTargetLevel = 0.0; // target level after level variation modulation
  double mAttackTimeSec = 0.0;
  double mAttackRate = 1.0; // one-pole per-sample attack coefficient
  double mReleaseTimeSec = 0.0;
  double mReleaseRate = 1.0;                       // one-pole per-sample release coefficient
  static constexpr double kLevelEpsilon = 0.00001; // levels below this are considered silent

  // Enforce click-prevention floors here so global scaling and hand-edited patches cannot bypass them.
  static constexpr double kMinAttackTimeSec = 0.001;
  static constexpr double kMinReleaseTimeSec = 0.001;
  void UpdateLevelRates();

  // Phase is in cycles; harmonics remain in phase when pitch is unmodulated.
  double mPhase = 0.0;
  double mFrequencyHz = kA4FrequencyHz;
  double mPhaseIncrement = 440.0 / 44100.0;
  void UpdateFrequency();

  // Constant-power pan with sample-accurate variation and slew-limited gain changes.
  double mBasePan = 0.0; // ranges from -1 (left) to +1 (right)
  double mBasePanLeftGain = 0.70710678;
  double mBasePanRightGain = 0.70710678;
  double mPanLeftGain = 0.70710678;
  double mPanRightGain = 0.70710678;
  double mTargetPanLeftGain = 0.70710678;
  double mTargetPanRightGain = 0.70710678;
  static constexpr double kPanSlewTimeSec = 0.02; // time (seconds) for a pan to move by 1.0
  double mPanSlewPerSample = 1.0;                 // pan change rate in pan units per sample
  void UpdatePanSlewRate();

  static constexpr double kVariationParameterSmoothingTimeSec = 0.01;
  static constexpr double kVariationParameterEpsilon = 1.0e-6;
  static constexpr int kVariationTargetRefreshIntervalSamples = 16;

  struct VariationState {
    void SetTargets(double amplitudeIn, double rateHzIn) {
      pendingTargetAmplitude.store(std::max(0.0, amplitudeIn), std::memory_order_relaxed);
      pendingTargetRateHz.store(std::max(0.0, rateHzIn), std::memory_order_relaxed);
    }

    void RefreshTargets() {
      targetAmplitude = pendingTargetAmplitude.load(std::memory_order_relaxed);
      targetRateHz = pendingTargetRateHz.load(std::memory_order_relaxed);
    }

    void SnapToTargets() {
      RefreshTargets();
      amplitude = targetAmplitude;
      rateHz = targetRateHz;
    }

    double amplitude{0.0};
    double targetAmplitude{0.0};
    std::atomic<double> pendingTargetAmplitude{0.0};
    double rateHz{0.0};
    double targetRateHz{0.0};
    std::atomic<double> pendingTargetRateHz{0.0};
    double position{0.0};
    dsp::CachedNoise1D noiseCache;

    void InvalidateNoiseCache() { noiseCache.reset(); }
  };

  VariationState mLevelVariation{};
  VariationState mPitchVariation{};
  VariationState mPanVariation{};
  bool mHasLevelVariation = false;
  bool mHasPitchVariation = false;
  bool mHasPanVariation = false;
  static constexpr uint32_t kDefaultVariationSeed  = 0xA53C9D1Fu;
  static constexpr uint32_t kPitchVariationSeedXor = 0x17D39EF5u;
  static constexpr uint32_t kLevelVariationSeedXor = 0xF1023A17u;
  static constexpr uint32_t kPanVariationSeedXor = 0xC29B3F4Bu;
  inline static const double kMinPitchSemitones = kSemitonesPerOctave * std::log2(kMinFrequencyHz / kA4FrequencyHz);
  void UpdateVariationParameterSmoothingRate();
  void RefreshVariationTargets();
  void UpdateModulatedTargets();
  void UpdatePitchTarget(double pitchNoise = 0.0);
  void UpdateLevelTarget(double levelNoise = 0.0);
  void UpdatePanTargetGains(double panNoise = 0.0);
  double ProcessVariation(VariationState& variation, uint32_t seed);
  void SmoothVariationParameters(VariationState& variation);
  static bool IsVariationActiveNow(const VariationState& variation);
  static double CurrentVariationNoise(VariationState& variation, uint32_t seed);
  double mVariationParameterSmoothingCoefficient = 1.0;
  int mVariationTargetRefreshCountdown = 0;

  // Independent deterministic gradient-noise streams drive level, pitch and pan variation.
  uint32_t mVariationSeed = kDefaultVariationSeed;
};

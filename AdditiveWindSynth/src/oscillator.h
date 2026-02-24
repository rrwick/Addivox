#pragma once

#include <array>
#include <cstdint>

#include "IPlugConstants.h"
#include "harmonic_visualizer_frame.h"

class Oscillator
{
public:
  // Set DSP sample rate in Hz.
  void SetSampleRate(double sampleRate);

  // Set deterministic seed for all variation streams.
  void SetVariationSeed(uint32_t seed);

  // Set base pitch in semitones relative to A4 (440 Hz), with 12 semitones per octave.
  void SetPitch(double pitchSemitones);

  // Set portamento speed in seconds per semitone. 0 means immediate pitch changes.
  void SetPitchTime(double pitchTimeSec);

  // Set pitch modulation depth (semitones) and speed (Hz).
  void SetPitchVariation(double amplitudeSemitones, double rateHz);

  // Set envelope attack/release times in seconds.
  void SetAttackTime(double attackTimeSec);
  void SetReleaseTime(double releaseTimeSec);

  // Set amplitude modulation depth (unitless multiplier domain) and speed (Hz).
  void SetIntensityVariation(double amplitude, double rateHz);

  // Set base pan in [-1, 1], where -1 is left and +1 is right.
  void SetPan(double pan);

  // Set pan modulation depth (unitless pan amount) and speed (Hz).
  void SetPanVariation(double amplitude, double rateHz);

  // Set base output level (linear gain).
  void SetLevel(double level);

  // Reset oscillator phase and dynamic state for a clean retrigger.
  void Reset();

  // Render one stereo frame.
  std::array<iplug::sample, 2> Process();

  // True while envelope is still active or decaying.
  bool IsActive() const;

  // Snapshot used by the visualizer.
  HarmonicVisualizerOscillator GetVisualizerState() const;

private:
  void UpdatePhaseIncrement(double frequencyHz);
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

  static constexpr double kTwoPi = 6.28318530717958647692; // Sine phase conversion.
  static constexpr double kPi = 3.14159265358979323846;
  static constexpr double kA4FrequencyHz = 440.0;           // Pitch reference.
  static constexpr double kSemitonesPerOctave = 12.0;       // Equal temperament.
  static constexpr double kMinFrequencyHz = 0.000001;       // Lower clamp for stability.
  static constexpr double kLevelEpsilon = 0.00001;          // Denormal/activity threshold.
  static constexpr double kPanSlewTimeSec = 0.005;          // Pan smoothing time constant.
  static constexpr int kVariationControlIntervalSamples = 16;
  static constexpr uint32_t kDefaultVariationSeed = 0xA53C9D1Fu;

  // Global DSP configuration.
  double mSampleRate = 44100.0;

  // Pitch is measured in semitones relative to A4 (440 Hz).
  double mPitch = 0.0;               // the actual pitch that the oscillator is currently producing
  double mBasePitch = 0.0;           // target pitch before pitch-variation modulation
  double mTargetPitch = 0.0;         // target pitch after pitch-variation modulation
  double mPitchTimeSec = 0.0;        // portamento time in seconds per semitone, 0=immediate change
  double mPitchRatePerSample = 1.0;  // portamento rate in semitones/sample

  // Amplitude envelope is just for attack and release shaping (decay and sustain are controlled by
  // the player's breath). Not strictly for the start/end of notes but more generally for
  // controlling how quickly the oscillator level can increase or decrease. Smoothing is done with
  // a simple one-pole approach, so attack is exponential rise toward the target level and release
  // is exponential decay toward the target level.
  double mLevel = 0.0;           // the actual level that the oscillator is currently producing
  double mBaseLevel = 0.0;       // target level before intensity variation modulation
  double mTargetLevel = 0.0;     // target level after intensity variation modulation
  double mAttackTimeSec = 0.0;   // attack time in seconds, 0=immediate attack
  double mAttackRate = 1.0;      // one-pole per-sample attack coefficient
  double mReleaseTimeSec = 0.0;  // release time in seconds, 0=immediate release
  double mReleaseRate = 1.0;     // one-pole per-sample release coefficient

  // Oscillator phase accumulator. Output level is determined by the current phase and level. Phase
  // is advanced by an increment (determined by the current pitch) each sample. If no pitch
  // variation is used, then all harmonics will remain in phase with each other.
  double mPhase = 0.0;                       // 0 to 1, where 1 = full cycle
  double mPhaseIncrement = 440.0 / 44100.0;  // how much phase advances each sample

  // Stereo pan state (equal-power gains).
  double mBasePan = 0.0;
  double mPanLeftGain = 0.70710678;
  double mPanRightGain = 0.70710678;
  double mTargetPanLeftGain = 0.70710678; // Modulated/smoothed target.
  double mTargetPanRightGain = 0.70710678;
  double mPanSlewRate = 1.0; // One-pole per-sample coefficient.

  // Variation controls and state.
  double mIntensityVariationAmplitude = 0.0;
  double mIntensityVariationRateHz = 0.0;
  double mIntensityVariationPosition = 0.0; // Noise-domain position.
  double mPitchVariationAmplitude = 0.0;    // Semitones.
  double mPitchVariationRateHz = 0.0;
  double mPitchVariationPosition = 0.0;     // Noise-domain position.
  double mPanVariationAmplitude = 0.0;
  double mPanVariationRateHz = 0.0;
  double mPanVariationPosition = 0.0;       // Noise-domain position.

  // Variation update scheduling.
  uint32_t mVariationSeed = kDefaultVariationSeed;
  int mVariationSamplesUntilUpdate = 0;     // Counts down to next control-rate update.
};

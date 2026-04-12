#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

#include "IPlugConstants.h"
#include "../visualizer/harmonic_visualizer_frame.h"

class Oscillator
{
public:
  // This function generates the output of the oscillator (two values for left and right channels)
  // and advances the internal state by one sample.
  std::array<iplug::sample, 2> Process();

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

  // True while the oscillator is still held or its envelope is decaying.
  bool IsActive() const;

  // Snapshot used by the visualizer.
  HarmonicVisualizerOscillator GetVisualizerState() const;

  // Current oscillator pitch in semitones relative to A4.
  double GetCurrentPitchSemitones() const
  {
    return mPitch;
  }

private:
  // Global DSP configuration.
  double mSampleRate = 44100.0;

  // Pitch is measured in semitones relative to A4 (440 Hz).
  double mPitch = 0.0;  // the actual pitch that the oscillator is currently producing
  double mBasePitch = 0.0;  // target pitch before pitch-variation modulation
  double mTargetPitch = 0.0;  // target pitch after pitch-variation modulation
  double mPitchTimeSec = 0.0;  // portamento time in seconds per semitone, 0=immediate change
  double mPitchRatePerSample = 1.0;  // portamento rate in semitones/sample
  static constexpr double kA4FrequencyHz = 440.0;  // pitch reference
  static constexpr double kSemitonesPerOctave = 12.0;  // equal temperament
  static constexpr double kMinFrequencyHz = 0.01;  // minimum frequency to avoid maths issues
  void UpdatePitchRate();  // called when sample rate or portamento time changes

  // Amplitude envelope is just for attack and release shaping (decay and sustain are controlled by
  // the player's breath). Not strictly for the start/end of notes but more generally for
  // controlling how quickly the oscillator level can increase or decrease. Smoothing is done with
  // a simple one-pole approach, so attack is exponential rise toward the target level and release
  // is exponential decay toward the target level.
  double mLevel = 0.0;  // the actual level that the oscillator is currently producing
  double mBaseLevel = 0.0;  // target level before intensity variation modulation
  double mTargetLevel = 0.0;  // target level after intensity variation modulation
  double mAttackTimeSec = 0.0;  // attack time in seconds, 0=immediate attack
  double mAttackRate = 1.0;  // one-pole per-sample attack coefficient
  double mReleaseTimeSec = 0.0;  // release time in seconds, 0=immediate release
  double mReleaseRate = 1.0;  // one-pole per-sample release coefficient
  static constexpr double kLevelEpsilon = 0.00001;  // levels below this are considered silent
  void UpdateLevelRates();

  // Oscillator phase accumulator. Output level is determined by the current phase and level. Phase
  // is advanced by an increment (determined by the current pitch) each sample. If no pitch
  // variation is used, then all harmonics will remain in phase with each other.
  double mPhase = 0.0;  // 0 to 1, where 1 = full cycle
  double mPhaseIncrement = 440.0 / 44100.0;  // how much phase advances each sample
  void UpdatePhaseIncrement(double frequencyHz);  // called every time a sample is rendered

  // Stereo pan state. Uses constant-power pan law (-3 dB). Pan variation is evaluated
  // sample-accurately to avoid control-rate zipper noise; a separate slew limiter still protects
  // against abrupt pan jumps from UI or automation changes.
  double mBasePan = 0.0;  // ranges from -1 (left) to +1 (right)
  double mBasePanLeftGain = 0.70710678;
  double mBasePanRightGain = 0.70710678;
  double mPanLeftGain = 0.70710678;
  double mPanRightGain = 0.70710678;
  double mTargetPanLeftGain = 0.70710678;
  double mTargetPanRightGain = 0.70710678;
  static constexpr double kPanSlewTimeSec = 0.02;  // time (seconds) for a pan to move by 1.0
  double mPanSlewPerSample = 1.0;  // pan change rate in pan units per sample
  void UpdatePanSlewRate();

  static constexpr double kVariationParameterSmoothingTimeSec = 0.01;
  static constexpr double kVariationParameterEpsilon = 1.0e-6;

  struct VariationState
  {
    void SetTargets(double amplitudeIn, double rateHzIn)
    {
      targetAmplitude.store(std::max(0.0, amplitudeIn), std::memory_order_relaxed);
      targetRateHz.store(std::max(0.0, rateHzIn), std::memory_order_relaxed);
    }

    void SnapToTargets()
    {
      amplitude = targetAmplitude.load(std::memory_order_relaxed);
      rateHz = targetRateHz.load(std::memory_order_relaxed);
    }

    double amplitude{0.0};
    std::atomic<double> targetAmplitude{0.0};
    double rateHz{0.0};
    std::atomic<double> targetRateHz{0.0};
    double position{0.0};
  };

  // Variation controls and state.
  VariationState mIntensityVariation{};
  VariationState mPitchVariation{};
  VariationState mPanVariation{};
  static constexpr uint32_t kDefaultVariationSeed = 0xA53C9D1Fu;
  void UpdateVariationParameterSmoothingRate();
  void UpdatePitchTarget();
  void UpdateLevelTarget();
  void UpdatePanTargetGains();
  template <typename UpdateTargetFn>
  void ProcessVariation(VariationState& variation, UpdateTargetFn&& updateTarget)
  {
    if(!HasVariation(variation))
      return;

    SmoothVariationParameters(variation);
    updateTarget();
    AdvanceVariationPosition(variation);
  }
  void SmoothVariationParameters(VariationState& variation);
  void AdvanceVariationPosition(VariationState& variation);
  static bool IsVariationActiveNow(const VariationState& variation);
  static bool HasVariation(const VariationState& variation);
  static double VariationNoise(const VariationState& variation, uint32_t seed);
  double mVariationParameterSmoothingCoefficient = 1.0;

  // The shape of variation (intensity, pitch, pan) over time is determined by gradient noise, which
  // is a smooth random function that is similar to Perlin noise.
  uint32_t mVariationSeed = kDefaultVariationSeed;
};

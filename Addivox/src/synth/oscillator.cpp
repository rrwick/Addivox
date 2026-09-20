#include "oscillator.h"

#include "../dsp/shared.h"

#include <algorithm>
#include <cmath>
#include <limits>

void Oscillator::SetSampleRate(double sampleRate) {
  const double nextSampleRate = (sampleRate > 0.0) ? sampleRate : dsp::kDefaultSampleRate;
  if (nextSampleRate == mSampleRate) return;

  mSampleRate = nextSampleRate;
  mInverseSampleRate = 1.0 / mSampleRate;
  UpdateFrequency();
  UpdatePitchRate();
  UpdateLevelRates();
  UpdatePanSlewRate();
  UpdateVariationParameterSmoothingRate();
  RefreshVariationTargets();
  UpdateModulatedTargets();
}

void Oscillator::SetVariationSeed(uint32_t seed) {
  mVariationSeed = (seed != 0u) ? seed : kDefaultVariationSeed;

  // Give each variation stream its own deterministic starting point.
  mLevelVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0x68E31DA4u) + 1.0) * 100.0;
  mPitchVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0xB5297A4Du) + 1.0) * 100.0;
  mPanVariation.position = (dsp::HashToSignedUnitFloat(mVariationSeed ^ 0x1B56C4E9u) + 1.0) * 100.0;
  mLevelVariation.InvalidateNoiseCache();
  mPitchVariation.InvalidateNoiseCache();
  mPanVariation.InvalidateNoiseCache();

  RefreshVariationTargets();
  UpdateModulatedTargets();
}

void Oscillator::SetPitch(double pitchSemitones) {
  if (pitchSemitones == mBasePitch) return;

  mBasePitch = pitchSemitones;
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ kPitchVariationSeedXor));
}

void Oscillator::SetPitchTime(double pitchTimeSec) {
  const double clampedPitchTimeSec = std::max(0.0, pitchTimeSec);
  if (clampedPitchTimeSec == mPitchTimeSec) return;

  mPitchTimeSec = clampedPitchTimeSec;
  UpdatePitchRate();
}

void Oscillator::SetPitchVariation(double amplitudeSemitones, double rateHz) { mPitchVariation.SetTargets(amplitudeSemitones, rateHz); }

void Oscillator::SetAttackTime(double attackTimeSec) {
  const double clampedAttackTimeSec = std::max(kMinAttackTimeSec, attackTimeSec);
  if (clampedAttackTimeSec == mAttackTimeSec) return;

  mAttackTimeSec = clampedAttackTimeSec;
  mAttackRate = dsp::ExponentialSmoothingCoefficient(mSampleRate, mAttackTimeSec);
}

void Oscillator::SetReleaseTime(double releaseTimeSec) {
  const double clampedReleaseTimeSec = std::max(kMinReleaseTimeSec, releaseTimeSec);
  if (clampedReleaseTimeSec == mReleaseTimeSec) return;

  mReleaseTimeSec = clampedReleaseTimeSec;
  mReleaseRate = dsp::ExponentialSmoothingCoefficient(mSampleRate, mReleaseTimeSec);
}

void Oscillator::SetLevelVariation(double amplitude, double rateHz) { mLevelVariation.SetTargets(amplitude, rateHz); }

void Oscillator::SetPan(double pan) {
  const double clampedPan = std::clamp(pan, -1.0, 1.0);
  if (clampedPan == mBasePan) return;

  mBasePan = clampedPan;
  dsp::PanToGains(mBasePan, mBasePanLeftGain, mBasePanRightGain);
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ kPanVariationSeedXor));
}

void Oscillator::SetPanVariation(double amplitude, double rateHz) { mPanVariation.SetTargets(amplitude, rateHz); }

void Oscillator::SetLevel(double level) {
  const double clampedLevel = (level >= 0.0) ? level : 0.0;
  if (clampedLevel == mBaseLevel) return;

  mBaseLevel = clampedLevel;
  UpdateLevelTarget(CurrentVariationNoise(mLevelVariation, mVariationSeed ^ kLevelVariationSeedXor));
}

void Oscillator::Reset() {
  mPhase = 0.0;
  mLevel = 0.0;
  RefreshVariationTargets();
  mLevelVariation.SnapToTargets();
  mPitchVariation.SnapToTargets();
  mPanVariation.SnapToTargets();
  mVariationTargetRefreshCountdown = kVariationTargetRefreshIntervalSamples;
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ kPitchVariationSeedXor));
  mPitch = mTargetPitch;
  UpdateLevelTarget(CurrentVariationNoise(mLevelVariation, mVariationSeed ^ kLevelVariationSeedXor));
  // On full retrigger, start from the current pan target.
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ kPanVariationSeedXor));
  mPanLeftGain = mTargetPanLeftGain;
  mPanRightGain = mTargetPanRightGain;
  UpdateFrequency();
}

std::array<iplug::sample, 2> Oscillator::Process() {
  if (mVariationTargetRefreshCountdown <= 0) {
    RefreshVariationTargets();
    mVariationTargetRefreshCountdown = kVariationTargetRefreshIntervalSamples;
  }
  --mVariationTargetRefreshCountdown;

  if (mHasLevelVariation) UpdateLevelTarget(ProcessVariation(mLevelVariation, mVariationSeed ^ kLevelVariationSeedXor));
  if (mHasPanVariation) UpdatePanTargetGains(ProcessVariation(mPanVariation, mVariationSeed ^ kPanVariationSeedXor));
  if (mHasPitchVariation) UpdatePitchTarget(ProcessVariation(mPitchVariation, mVariationSeed ^ kPitchVariationSeedXor));

  const double rate = (mTargetLevel > mLevel) ? mAttackRate : mReleaseRate;
  mLevel += (mTargetLevel - mLevel) * rate;

  mPanLeftGain += std::clamp(mTargetPanLeftGain - mPanLeftGain, -mPanSlewPerSample, mPanSlewPerSample);
  mPanRightGain += std::clamp(mTargetPanRightGain - mPanRightGain, -mPanSlewPerSample, mPanSlewPerSample);
  if (mPitch != mTargetPitch) {
    const double previousPitch = mPitch;
    mPitch += std::clamp(mTargetPitch - mPitch, -mPitchRatePerSample, mPitchRatePerSample);
    if (mPitch != previousPitch) UpdateFrequency();
  }

  if (mTargetLevel <= kLevelEpsilon && mLevel < kLevelEpsilon) mLevel = 0.0;

  const double phase = mPhase;
  mPhase += mPhaseIncrement;
  if (mPhase >= 1.0) mPhase -= 1.0;

  if (mLevel == 0.0) return {0.0, 0.0};

  // Deactivate oscillator if frequency is out of range - prevents aliasing
  if (mFrequencyHz > mSampleRate * 0.5) return {0.0, 0.0};

  const iplug::sample out = static_cast<iplug::sample>(std::sin((2.0 * dsp::kPi) * phase) * mLevel);

  return {out * static_cast<iplug::sample>(mPanLeftGain), out * static_cast<iplug::sample>(mPanRightGain)};
}

bool Oscillator::IsActive() const {
  // A held note must keep processing even when variation temporarily drives the
  // target level to zero, otherwise the variation phase freezes and the
  // oscillator can never recover.
  return (mBaseLevel > kLevelEpsilon) || (mTargetLevel > kLevelEpsilon) || (mLevel > kLevelEpsilon);
}

HarmonicVisualizerOscillator Oscillator::GetVisualizerState() const {
  return HarmonicVisualizerOscillator{static_cast<float>(mFrequencyHz), static_cast<float>(mLevel), static_cast<float>(mPanLeftGain),
                                      static_cast<float>(mPanRightGain)};
}

void Oscillator::UpdateFrequency() {
  mFrequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  mPhaseIncrement = mFrequencyHz * mInverseSampleRate;
}

void Oscillator::UpdatePitchRate() {
  if (mPitchTimeSec <= 0.0 || mSampleRate <= 0.0) {
    mPitchRatePerSample = std::numeric_limits<double>::infinity();
    return;
  }

  // Linear portamento: fixed semitone step per sample.
  mPitchRatePerSample = 1.0 / (mPitchTimeSec * mSampleRate);
}

void Oscillator::UpdateLevelRates() {
  mAttackRate = dsp::ExponentialSmoothingCoefficient(mSampleRate, mAttackTimeSec);
  mReleaseRate = dsp::ExponentialSmoothingCoefficient(mSampleRate, mReleaseTimeSec);
}

void Oscillator::UpdatePanSlewRate() {
  if (kPanSlewTimeSec <= 0.0 || mSampleRate <= 0.0) {
    mPanSlewPerSample = std::numeric_limits<double>::infinity();
    return;
  }

  // Linear pan slew: fixed max gain step per sample.
  mPanSlewPerSample = mInverseSampleRate / kPanSlewTimeSec;
}

void Oscillator::UpdateVariationParameterSmoothingRate() {
  mVariationParameterSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kVariationParameterSmoothingTimeSec);
}

void Oscillator::RefreshVariationTargets() {
  mLevelVariation.RefreshTargets();
  mPitchVariation.RefreshTargets();
  mPanVariation.RefreshTargets();
  mHasLevelVariation = (mLevelVariation.amplitude > kVariationParameterEpsilon || mLevelVariation.targetAmplitude > kVariationParameterEpsilon) &&
                       (mLevelVariation.rateHz > kVariationParameterEpsilon || mLevelVariation.targetRateHz > kVariationParameterEpsilon);
  mHasPitchVariation = (mPitchVariation.amplitude > kVariationParameterEpsilon || mPitchVariation.targetAmplitude > kVariationParameterEpsilon) &&
                       (mPitchVariation.rateHz > kVariationParameterEpsilon || mPitchVariation.targetRateHz > kVariationParameterEpsilon);
  mHasPanVariation = (mPanVariation.amplitude > kVariationParameterEpsilon || mPanVariation.targetAmplitude > kVariationParameterEpsilon) &&
                     (mPanVariation.rateHz > kVariationParameterEpsilon || mPanVariation.targetRateHz > kVariationParameterEpsilon);
}

void Oscillator::UpdatePitchTarget(double pitchNoise) { mTargetPitch = std::max(kMinPitchSemitones, mBasePitch + (mPitchVariation.amplitude * pitchNoise)); }

void Oscillator::UpdateLevelTarget(double levelNoise) { mTargetLevel = mBaseLevel * std::max(0.0, 1.0 + (mLevelVariation.amplitude * levelNoise)); }

inline void Oscillator::UpdatePanTargetGains(double panNoise) {
  if (panNoise == 0.0) {
    mTargetPanLeftGain = mBasePanLeftGain;
    mTargetPanRightGain = mBasePanRightGain;
    return;
  }

  dsp::PanToGains(mBasePan + (mPanVariation.amplitude * panNoise), mTargetPanLeftGain, mTargetPanRightGain);
}

void Oscillator::SmoothVariationParameters(VariationState& variation) {
  if (variation.amplitude == variation.targetAmplitude && variation.rateHz == variation.targetRateHz) return;

  const double amplitudeDelta = variation.targetAmplitude - variation.amplitude;
  variation.amplitude += mVariationParameterSmoothingCoefficient * amplitudeDelta;
  if (amplitudeDelta <= kVariationParameterEpsilon && amplitudeDelta >= -kVariationParameterEpsilon) variation.amplitude = variation.targetAmplitude;

  const double rateDelta = variation.targetRateHz - variation.rateHz;
  variation.rateHz += mVariationParameterSmoothingCoefficient * rateDelta;
  if (rateDelta <= kVariationParameterEpsilon && rateDelta >= -kVariationParameterEpsilon) variation.rateHz = variation.targetRateHz;
}

bool Oscillator::IsVariationActiveNow(const VariationState& variation) {
  return variation.amplitude > kVariationParameterEpsilon && variation.rateHz > kVariationParameterEpsilon;
}

double Oscillator::CurrentVariationNoise(VariationState& variation, uint32_t seed) {
  if (!IsVariationActiveNow(variation)) return 0.0;

  return variation.noiseCache.evaluate(variation.position, seed);
}

#if defined(_MSC_VER)
__forceinline
#elif defined(__clang__) || defined(__GNUC__)
__attribute__((always_inline)) inline
#endif
double Oscillator::ProcessVariation(VariationState& variation, uint32_t seed) {
  SmoothVariationParameters(variation);
  const double positionIncrement = variation.rateHz * mInverseSampleRate;
  if (!IsVariationActiveNow(variation)) return 0.0;

  const double noise = variation.noiseCache.evaluate(variation.position, seed);
  variation.position += positionIncrement;
  return noise;
}

void Oscillator::UpdateModulatedTargets() {
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ kPitchVariationSeedXor));
  UpdateLevelTarget(CurrentVariationNoise(mLevelVariation, mVariationSeed ^ kLevelVariationSeedXor));
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ kPanVariationSeedXor));
}

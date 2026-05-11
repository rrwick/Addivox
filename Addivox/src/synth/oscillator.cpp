#include "oscillator.h"

#include "../dsp/gradient_noise.h"
#include "../dsp/shared.h"

#include <algorithm>
#include <cmath>
#include <limits>

void Oscillator::SetSampleRate(double sampleRate) {
  const double nextSampleRate = (sampleRate > 0.0) ? sampleRate : dsp::kDefaultSampleRate;
  if (nextSampleRate == mSampleRate) return;

  mSampleRate = nextSampleRate;
  mInverseSampleRate = 1.0 / mSampleRate;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdatePhaseIncrement(frequencyHz);
  UpdatePitchRate();
  UpdateLevelRates();
  UpdatePanSlewRate();
  UpdateVariationParameterSmoothingRate();
  RefreshVariationTargets();
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ kPitchVariationSeedXor));
  UpdateLevelTarget(CurrentVariationNoise(mLevelVariation, mVariationSeed ^ kLevelVariationSeedXor));
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ kPanVariationSeedXor));
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
  UpdatePitchTarget(CurrentVariationNoise(mPitchVariation, mVariationSeed ^ kPitchVariationSeedXor));
  UpdateLevelTarget(CurrentVariationNoise(mLevelVariation, mVariationSeed ^ kLevelVariationSeedXor));
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ kPanVariationSeedXor));
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
  const double clampedAttackTimeSec = std::max(0.0, attackTimeSec);
  if (clampedAttackTimeSec == mAttackTimeSec) return;

  mAttackTimeSec = clampedAttackTimeSec;
  UpdateLevelRates();
}

void Oscillator::SetReleaseTime(double releaseTimeSec) {
  const double clampedReleaseTimeSec = std::max(0.0, releaseTimeSec);
  if (clampedReleaseTimeSec == mReleaseTimeSec) return;

  mReleaseTimeSec = clampedReleaseTimeSec;
  UpdateLevelRates();
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
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdateLevelTarget(CurrentVariationNoise(mLevelVariation, mVariationSeed ^ kLevelVariationSeedXor));
  // On full retrigger, start from the current pan target.
  UpdatePanTargetGains(CurrentVariationNoise(mPanVariation, mVariationSeed ^ kPanVariationSeedXor));
  mPanLeftGain = mTargetPanLeftGain;
  mPanRightGain = mTargetPanRightGain;
  UpdatePhaseIncrement(frequencyHz);
}

std::array<iplug::sample, 2> Oscillator::Process() {
  if (mVariationTargetRefreshCountdown <= 0) {
    RefreshVariationTargets();
    mVariationTargetRefreshCountdown = kVariationTargetRefreshIntervalSamples;
  }
  --mVariationTargetRefreshCountdown;

  if (mHasLevelVariation) {
    SmoothVariationParameters(mLevelVariation);
    if (IsVariationActiveNow(mLevelVariation)) {
      const double levelNoise = mLevelVariation.noiseCache.evaluate(mLevelVariation.position, mVariationSeed ^ kLevelVariationSeedXor);
      mLevelVariation.position += mLevelVariation.positionIncrement;
      UpdateLevelTarget(levelNoise);
    } else
      UpdateLevelTarget();
  }

  if (mHasPanVariation) {
    SmoothVariationParameters(mPanVariation);
    if (IsVariationActiveNow(mPanVariation)) {
      const double panNoise = mPanVariation.noiseCache.evaluate(mPanVariation.position, mVariationSeed ^ kPanVariationSeedXor);
      mPanVariation.position += mPanVariation.positionIncrement;
      UpdatePanTargetGains(panNoise);
    } else
      UpdatePanTargetGains();
  }

  if (mHasPitchVariation) {
    SmoothVariationParameters(mPitchVariation);
    if (IsVariationActiveNow(mPitchVariation)) {
      const double pitchNoise = mPitchVariation.noiseCache.evaluate(mPitchVariation.position, mVariationSeed ^ kPitchVariationSeedXor);
      mPitchVariation.position += mPitchVariation.positionIncrement;
      UpdatePitchTarget(pitchNoise);
    } else
      UpdatePitchTarget();
  }

  const double rate = (mTargetLevel > mLevel) ? mAttackRate : mReleaseRate;
  mLevel += (mTargetLevel - mLevel) * rate;

  mPanLeftGain += std::clamp(mTargetPanLeftGain - mPanLeftGain, -mPanSlewPerSample, mPanSlewPerSample);
  mPanRightGain += std::clamp(mTargetPanRightGain - mPanRightGain, -mPanSlewPerSample, mPanSlewPerSample);
  mPitch += std::clamp(mTargetPitch - mPitch, -mPitchRatePerSample, mPitchRatePerSample);
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  mPhaseIncrement = frequencyHz * mInverseSampleRate; // inlined UpdatePhaseIncrement

  if (mTargetLevel <= kLevelEpsilon && mLevel < kLevelEpsilon) mLevel = 0.0;

  const double phase = mPhase;
  mPhase += mPhaseIncrement;
  if (mPhase >= 1.0) mPhase -= 1.0;

  if (mLevel == 0.0) return {0.0, 0.0};

  // Deactivate oscillator if frequency is out of range - prevents aliasing
  if (frequencyHz > mSampleRate * 0.5) return {0.0, 0.0};

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
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  return HarmonicVisualizerOscillator{static_cast<float>(frequencyHz), static_cast<float>(mLevel), static_cast<float>(mPanLeftGain),
                                      static_cast<float>(mPanRightGain)};
}

void Oscillator::UpdatePhaseIncrement(double frequencyHz) { mPhaseIncrement = frequencyHz * mInverseSampleRate; }

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

void Oscillator::UpdatePitchTarget(double pitchNoise) {
  mTargetPitch = std::max(mMinPitchSemitones, mBasePitch + (mPitchVariation.amplitude * pitchNoise));
}

void Oscillator::UpdateLevelTarget(double levelNoise) {
  mTargetLevel = mBaseLevel * std::max(0.0, 1.0 + (mLevelVariation.amplitude * levelNoise));
}

void Oscillator::UpdatePanTargetGains(double panNoise) {
  if (panNoise == 0.0) {
    mTargetPanLeftGain = mBasePanLeftGain;
    mTargetPanRightGain = mBasePanRightGain;
    return;
  }

  dsp::PanToGains(mBasePan + (mPanVariation.amplitude * panNoise), mTargetPanLeftGain, mTargetPanRightGain);
}

void Oscillator::SmoothVariationParameters(VariationState& variation) {
  const double amplitudeDelta = variation.targetAmplitude - variation.amplitude;
  variation.amplitude += mVariationParameterSmoothingCoefficient * amplitudeDelta;
  if (amplitudeDelta <= kVariationParameterEpsilon && amplitudeDelta >= -kVariationParameterEpsilon) variation.amplitude = variation.targetAmplitude;

  const double rateDelta = variation.targetRateHz - variation.rateHz;
  variation.rateHz += mVariationParameterSmoothingCoefficient * rateDelta;
  if (rateDelta <= kVariationParameterEpsilon && rateDelta >= -kVariationParameterEpsilon) variation.rateHz = variation.targetRateHz;

  variation.positionIncrement = variation.rateHz * mInverseSampleRate;
}

bool Oscillator::IsVariationActiveNow(const VariationState& variation) {
  return variation.amplitude > kVariationParameterEpsilon && variation.rateHz > kVariationParameterEpsilon;
}

double Oscillator::CurrentVariationNoise(VariationState& variation, uint32_t seed) {
  if (!IsVariationActiveNow(variation)) return 0.0;

  return variation.noiseCache.evaluate(variation.position, seed);
}

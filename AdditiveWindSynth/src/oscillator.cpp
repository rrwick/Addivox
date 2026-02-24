#include "oscillator.h"

#include <algorithm>
#include <cmath>

void Oscillator::SetSampleRate(double sampleRate)
{
  mSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdatePhaseIncrement(frequencyHz);
  UpdatePitchRate();
  UpdateLevelRates();
  UpdatePanSlewRate();
  UpdateVariationTargets();
}

void Oscillator::SetVariationSeed(uint32_t seed)
{
  mVariationSeed = (seed != 0u) ? seed : kDefaultVariationSeed;

  // Give each variation stream its own deterministic starting point.
  mIntensityVariationPosition = (HashToSignedUnitFloat(mVariationSeed ^ 0x68E31DA4u) + 1.0) * 100.0;
  mPitchVariationPosition = (HashToSignedUnitFloat(mVariationSeed ^ 0xB5297A4Du) + 1.0) * 100.0;
  mPanVariationPosition = (HashToSignedUnitFloat(mVariationSeed ^ 0x1B56C4E9u) + 1.0) * 100.0;

  UpdateVariationTargets();
}

void Oscillator::SetPitch(double pitchSemitones)
{
  mBasePitch = pitchSemitones;
  UpdateVariationTargets();
}

void Oscillator::SetPitchTime(double pitchTimeSec)
{
  mPitchTimeSec = std::max(0.0, pitchTimeSec);
  UpdatePitchRate();
}

void Oscillator::SetPitchVariation(double amplitudeSemitones, double rateHz)
{
  mPitchVariationAmplitudeSemitones = std::max(0.0, amplitudeSemitones);
  mPitchVariationRateHz = std::max(0.0, rateHz);
  UpdateVariationTargets();
}

void Oscillator::SetAttackTime(double attackTimeSec)
{
  mAttackTimeSec = std::max(0.0, attackTimeSec);
  UpdateLevelRates();
}

void Oscillator::SetReleaseTime(double releaseTimeSec)
{
  mReleaseTimeSec = std::max(0.0, releaseTimeSec);
  UpdateLevelRates();
}

void Oscillator::SetIntensityVariation(double amplitude, double rateHz)
{
  mIntensityVariationAmplitude = ClampNonNegative(amplitude);
  mIntensityVariationRateHz = ClampNonNegative(rateHz);
  UpdateVariationTargets();
}

void Oscillator::SetPan(double pan)
{
  mBasePan = ClampPan(pan);
  UpdateVariationTargets();
}

void Oscillator::SetPanVariation(double amplitude, double rateHz)
{
  mPanVariationAmplitude = ClampNonNegative(amplitude);
  mPanVariationRateHz = ClampNonNegative(rateHz);
  UpdateVariationTargets();
}

void Oscillator::SetLevel(double level)
{
  mBaseLevel = (level >= 0.0) ? level : 0.0;
  UpdateVariationTargets();
}

void Oscillator::Reset()
{
  mPhase = 0.0;
  mLevel = 0.0;
  mPitch = mTargetPitch;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  // On full retrigger, start from the current pan target.
  mPanLeftGain = mTargetPanLeftGain;
  mPanRightGain = mTargetPanRightGain;
  UpdatePhaseIncrement(frequencyHz);
  mVariationSamplesUntilUpdate = 0;
}

std::array<iplug::sample, 2> Oscillator::Process()
{
  if(mVariationSamplesUntilUpdate <= 0)
  {
    UpdateVariationTargets();
    AdvanceVariationPositions(kVariationControlIntervalSamples);
    mVariationSamplesUntilUpdate = kVariationControlIntervalSamples;
  }
  --mVariationSamplesUntilUpdate;

  const double rate = (mTargetLevel > mLevel) ? mAttackRate : mReleaseRate;
  mLevel += (mTargetLevel - mLevel) * rate;

  mPanLeftGain += (mTargetPanLeftGain - mPanLeftGain) * mPanSlewRate;
  mPanRightGain += (mTargetPanRightGain - mPanRightGain) * mPanSlewRate;

  mPitch += (mTargetPitch - mPitch) * mPitchRate;
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  UpdatePhaseIncrement(frequencyHz);

  if(mTargetLevel <= kLevelEpsilon && mLevel < kLevelEpsilon)
    mLevel = 0.0;

  const iplug::sample out = static_cast<iplug::sample>(std::sin(kTwoPi * mPhase) * mLevel);

  mPhase += mPhaseIncrement;
  if(mPhase >= 1.0)
    mPhase -= std::floor(mPhase);

  // Deactivate oscillator if frequency is out of range - prevents aliasing
  if(frequencyHz > 20000.0)
    return {0.0, 0.0};

  return {
    out * static_cast<iplug::sample>(mPanLeftGain),
    out * static_cast<iplug::sample>(mPanRightGain)};
}

bool Oscillator::IsActive() const
{
  return (mTargetLevel > kLevelEpsilon) || (mLevel > kLevelEpsilon);
}

HarmonicVisualizerOscillator Oscillator::GetVisualizerState() const
{
  const double frequencyHz = kA4FrequencyHz * std::exp2(mPitch / kSemitonesPerOctave);
  return HarmonicVisualizerOscillator{
    static_cast<float>(frequencyHz),
    static_cast<float>(mLevel),
    static_cast<float>(mPanLeftGain),
    static_cast<float>(mPanRightGain)};
}

void Oscillator::UpdatePhaseIncrement(double frequencyHz)
{
  mPhaseIncrement = frequencyHz / mSampleRate;
}

void Oscillator::UpdatePitchRate()
{
  mPitchRate = TimeToRate(mPitchTimeSec, mSampleRate);
}

void Oscillator::UpdateLevelRates()
{
  mAttackRate = TimeToRate(mAttackTimeSec, mSampleRate);
  mReleaseRate = TimeToRate(mReleaseTimeSec, mSampleRate);
}

void Oscillator::UpdatePanSlewRate()
{
  mPanSlewRate = TimeToRate(kPanSlewTimeSec, mSampleRate);
}

void Oscillator::UpdateVariationTargets()
{
  const double intensityNoise = VariationNoise(
    mIntensityVariationAmplitude,
    mIntensityVariationRateHz,
    mIntensityVariationPosition,
    mVariationSeed ^ 0xF1023A17u);
  const double levelScale = std::max(0.0, 1.0 + mIntensityVariationAmplitude * intensityNoise);
  mTargetLevel = mBaseLevel * levelScale;

  const double pitchNoise = VariationNoise(
    mPitchVariationAmplitudeSemitones,
    mPitchVariationRateHz,
    mPitchVariationPosition,
    mVariationSeed ^ 0x17D39EF5u);
  const double pitchSemitones = mBasePitch + mPitchVariationAmplitudeSemitones * pitchNoise;
  const double minPitchSemitones = kSemitonesPerOctave * std::log2(kMinFrequencyHz / kA4FrequencyHz);
  mTargetPitch = std::max(minPitchSemitones, pitchSemitones);

  const double panNoise = VariationNoise(
    mPanVariationAmplitude,
    mPanVariationRateHz,
    mPanVariationPosition,
    mVariationSeed ^ 0xC29B3F4Bu);
  const double modulatedPan = ClampPan(mBasePan + mPanVariationAmplitude * panNoise);
  const auto panGains = PanToGains(modulatedPan);
  mTargetPanLeftGain = panGains[0];
  mTargetPanRightGain = panGains[1];

}

void Oscillator::AdvanceVariationPositions(int numSamples)
{
  if(numSamples <= 0 || mSampleRate <= 0.0)
    return;

  const double deltaTimeSec = static_cast<double>(numSamples) / mSampleRate;
  mIntensityVariationPosition += mIntensityVariationRateHz * deltaTimeSec;
  mPitchVariationPosition += mPitchVariationRateHz * deltaTimeSec;
  mPanVariationPosition += mPanVariationRateHz * deltaTimeSec;
}

double Oscillator::TimeToRate(double timeSec, double sampleRate)
{
  if(timeSec <= 0.0 || sampleRate <= 0.0)
    return 1.0;

  return 1.0 - std::exp(-1.0 / (timeSec * sampleRate));
}

double Oscillator::ClampPan(double pan)
{
  return std::clamp(pan, -1.0, 1.0);
}

std::array<double, 2> Oscillator::PanToGains(double pan)
{
  // Equal-power pan law keeps perceived loudness stable across stereo position.
  const double clampedPan = ClampPan(pan);
  const double angle = (clampedPan + 1.0) * (kPi * 0.25);
  return {std::cos(angle), std::sin(angle)};
}

double Oscillator::ClampNonNegative(double value)
{
  return std::max(0.0, value);
}

double Oscillator::VariationNoise(double amplitude, double rateHz, double position, uint32_t seed)
{
  if(amplitude <= 0.0 || rateHz <= 0.0)
    return 0.0;

  return GradientNoise1D(position, seed);
}

double Oscillator::Quintic(double t)
{
  // Perlin fade curve for smooth interpolation.
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

uint32_t Oscillator::HashUint32(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

double Oscillator::HashToSignedUnitFloat(uint32_t x)
{
  return (static_cast<double>(x) / 4294967295.0) * 2.0 - 1.0;
}

double Oscillator::GradientNoise1D(double position, uint32_t seed)
{
  const int lattice0 = static_cast<int>(std::floor(position));
  const double t = position - static_cast<double>(lattice0);
  const double fade = Quintic(t);

  const double gradient0 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0) ^ seed));
  const double gradient1 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0 + 1) ^ seed));

  const double value0 = gradient0 * t;
  const double value1 = gradient1 * (t - 1.0);
  const double blended = value0 + (value1 - value0) * fade;
  return std::clamp(blended * 1.8, -1.0, 1.0);
}

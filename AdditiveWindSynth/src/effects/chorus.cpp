#include "chorus.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDenormalFloor = 1.0e-18;
constexpr double kBypassThreshold = 1.0e-6;
constexpr int kNumVoices = 3;
constexpr double kWetNormalization = 0.50;
constexpr double kMaxBaseDelayMs = 19.0;
constexpr double kMaxDepthMs = 8.5;
constexpr double kBufferMarginMs = 4.0;
constexpr std::array<double, kNumVoices> kVoiceDelayOffsetsMs{0.0, 4.8, 9.8};
constexpr std::array<double, kNumVoices> kVoiceBaseRateHz{0.17, 0.23, 0.31};
constexpr std::array<double, kNumVoices> kVoicePanPositions{-0.85, 0.0, 0.85};
constexpr std::array<uint32_t, kNumVoices> kVoiceSeeds{
  0x51A3C0DEu,
  0x79B4E281u,
  0xA56D9F33u,
};
} // namespace

void effects::Chorus::DelayLine::Resize(int size)
{
  buffer.assign(static_cast<std::size_t>(std::max(2, size)), 0.0);
  writeIndex = 0;
}

void effects::Chorus::DelayLine::Clear()
{
  std::fill(buffer.begin(), buffer.end(), 0.0);
  writeIndex = 0;
}

void effects::Chorus::DelayLine::Write(double input)
{
  if(buffer.empty())
    return;

  buffer[static_cast<std::size_t>(writeIndex)] = FlushDenormal(input);
  writeIndex = (writeIndex + 1) % static_cast<int>(buffer.size());
}

double effects::Chorus::DelayLine::Read(double delaySamples) const
{
  if(buffer.size() < 2)
    return 0.0;

  const double clampedDelay = std::clamp(delaySamples, 1.0, static_cast<double>(buffer.size() - 2));
  double readPos = static_cast<double>(writeIndex) - clampedDelay;
  if(readPos < 0.0)
    readPos += static_cast<double>(buffer.size());

  const int index0 = static_cast<int>(readPos);
  const int index1 = (index0 + 1) % static_cast<int>(buffer.size());
  const double frac = readPos - static_cast<double>(index0);
  const double sample0 = buffer[static_cast<std::size_t>(index0)];
  const double sample1 = buffer[static_cast<std::size_t>(index1)];
  return sample0 + ((sample1 - sample0) * frac);
}

void effects::Chorus::OnePoleLowpass::Clear()
{
  state = 0.0;
}

double effects::Chorus::OnePoleLowpass::Process(double input)
{
  state += coefficient * (input - state);
  state = Chorus::FlushDenormal(state);
  return state;
}

void effects::Chorus::OnePoleHighpass::Clear()
{
  lowState = 0.0;
}

double effects::Chorus::OnePoleHighpass::Process(double input)
{
  lowState += coefficient * (input - lowState);
  lowState = Chorus::FlushDenormal(lowState);
  return input - lowState;
}

double effects::Chorus::MillisecondsToSamples(double milliseconds, double sampleRate)
{
  return (milliseconds * sampleRate) / 1000.0;
}

double effects::Chorus::FlushDenormal(double value)
{
  return std::abs(value) < kDenormalFloor ? 0.0 : value;
}

double effects::Chorus::SmoothValue(double current, double target, double coefficient)
{
  return current + (coefficient * (target - current));
}

double effects::Chorus::CutoffHzToCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

double effects::Chorus::Quintic(double t)
{
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

uint32_t effects::Chorus::HashUint32(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

double effects::Chorus::HashToSignedUnitFloat(uint32_t x)
{
  return (static_cast<double>(x) / 4294967295.0) * 2.0 - 1.0;
}

double effects::Chorus::GradientNoise1D(double position, uint32_t seed)
{
  const int lattice0 = static_cast<int>(std::floor(position));
  const double t = position - static_cast<double>(lattice0);
  const double fade = Quintic(t);

  const double gradient0 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0) ^ seed));
  const double gradient1 = HashToSignedUnitFloat(HashUint32(static_cast<uint32_t>(lattice0 + 1) ^ seed));

  const double value0 = gradient0 * t;
  const double value1 = gradient1 * (t - 1.0);
  const double blended = value0 + ((value1 - value0) * fade);
  return std::clamp(blended * 1.8, -1.0, 1.0);
}

std::array<double, 2> effects::Chorus::PanToGains(double pan)
{
  const double clampedPan = std::clamp(pan, -1.0, 1.0);
  const double angle = (clampedPan + 1.0) * (kPi * 0.25);
  return {std::cos(angle), std::sin(angle)};
}

void effects::Chorus::InitializeVoiceStates()
{
  for(int i = 0; i < kNumVoices; ++i)
  {
    VoiceState& voice = mVoices[static_cast<std::size_t>(i)];
    voice.modSeed = kVoiceSeeds[static_cast<std::size_t>(i)];
    voice.modPosition =
      (HashToSignedUnitFloat(voice.modSeed ^ 0xB8C9F52Du) + 1.0) * 100.0
      + (17.0 * static_cast<double>(i));
  }
}

void effects::Chorus::Reset(double sampleRate, int blockSize)
{
  (void) blockSize;

  mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  mAmountSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.030 * mSampleRate));
  mToneSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.045 * mSampleRate));
  mTargetAmount = 0.0;
  mCurrentAmount = 0.0;
  mActive = false;

  const double maxDelayMs =
    kMaxBaseDelayMs + kVoiceDelayOffsetsMs.back() + kMaxDepthMs + kBufferMarginMs;
  const int delaySize =
    static_cast<int>(std::ceil(MillisecondsToSamples(maxDelayMs, mSampleRate))) + 2;

  for(auto& voice : mVoices)
  {
    voice.delay.Resize(delaySize);
    voice.toneFilter.coefficient = CutoffHzToCoefficient(mSampleRate, 12000.0);
  }

  mInputHighpass.coefficient = CutoffHzToCoefficient(mSampleRate, 120.0);
  Clear();
}

void effects::Chorus::Clear()
{
  mInputHighpass.Clear();
  InitializeVoiceStates();

  for(auto& voice : mVoices)
  {
    voice.delay.Clear();
    voice.toneFilter.Clear();
  }
}

void effects::Chorus::SetAmount(double amount)
{
  mTargetAmount = std::clamp(amount, 0.0, 100.0);

  if(mTargetAmount > kBypassThreshold && !mActive)
  {
    Clear();
    mCurrentAmount = 0.0;
    mActive = true;
  }
}

void effects::Chorus::ProcessBlock(iplug::sample** outputs, int nFrames)
{
  if(!mActive || nFrames <= 0)
    return;

  for(int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex)
  {
    mCurrentAmount = SmoothValue(mCurrentAmount, mTargetAmount, mAmountSmoothingCoefficient);
    const double t = std::clamp(mCurrentAmount * 0.01, 0.0, 1.0);
    const double presence = std::sqrt(t);
    const double lush = t * t;
    const double wetMix = (0.16 * presence) + (0.66 * t);
    const double baseDelayMs = 8.0 + (4.5 * presence) + (5.5 * lush);
    const double depthMs = 0.25 + (2.10 * presence) + (6.0 * lush);
    const double width = 0.32 + (0.50 * presence) + (0.30 * t);
    const double rateScale = 0.65 + (0.65 * presence) + (1.00 * t);
    const double toneCoefficientTarget = CutoffHzToCoefficient(
      mSampleRate,
      12000.0 - (1200.0 * presence) - (700.0 * lush));
    const double baseDelaySamples = MillisecondsToSamples(baseDelayMs, mSampleRate);
    const double depthSamples = MillisecondsToSamples(depthMs, mSampleRate);
    const double monoInput =
      mInputHighpass.Process(0.5 * (outputs[0][sampleIndex] + outputs[1][sampleIndex]));

    double wetLeft = 0.0;
    double wetRight = 0.0;

    for(int voiceIndex = 0; voiceIndex < kNumVoices; ++voiceIndex)
    {
      VoiceState& voice = mVoices[static_cast<std::size_t>(voiceIndex)];
      const double rateHz = kVoiceBaseRateHz[static_cast<std::size_t>(voiceIndex)] * rateScale;
      voice.modPosition += rateHz / mSampleRate;
      voice.toneFilter.coefficient = SmoothValue(
        voice.toneFilter.coefficient,
        toneCoefficientTarget,
        mToneSmoothingCoefficient);

      const double noise = GradientNoise1D(voice.modPosition, voice.modSeed);
      const double delaySamples = std::max(
        1.0,
        baseDelaySamples
          + MillisecondsToSamples(kVoiceDelayOffsetsMs[static_cast<std::size_t>(voiceIndex)], mSampleRate)
          + (depthSamples * noise));
      const double delayed = voice.toneFilter.Process(voice.delay.Read(delaySamples));
      const auto panGains = PanToGains(kVoicePanPositions[static_cast<std::size_t>(voiceIndex)] * width);

      wetLeft += delayed * panGains[0] * kWetNormalization;
      wetRight += delayed * panGains[1] * kWetNormalization;
      voice.delay.Write(monoInput);
    }

    outputs[0][sampleIndex] =
      static_cast<iplug::sample>(FlushDenormal(outputs[0][sampleIndex] + (wetLeft * wetMix)));
    outputs[1][sampleIndex] =
      static_cast<iplug::sample>(FlushDenormal(outputs[1][sampleIndex] + (wetRight * wetMix)));
  }

  if(mTargetAmount <= kBypassThreshold && mCurrentAmount <= kBypassThreshold)
  {
    mCurrentAmount = 0.0;
    mActive = false;
    Clear();
  }
}

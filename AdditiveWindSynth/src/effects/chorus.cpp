#include "chorus.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDenormalFloor = 1.0e-18;
constexpr double kBypassThreshold = 1.0e-6;
constexpr int kNumVoices = 8;
using VoiceLevels = std::array<double, kNumVoices>;

constexpr double kLegacyVoiceCount = 4.0;
constexpr double kWetNormalization = 0.48;
constexpr double kAmountSmoothingTimeSeconds = 0.030;
constexpr double kToneSmoothingTimeSeconds = 0.045;
constexpr double kInitialVoiceToneCutoffHz = 12000.0;
constexpr double kWetToneCutoffHz = 10000.0;
constexpr double kInputHighpassCutoffHz = 120.0;
constexpr double kMaxBaseDelayMs = 24.0;
constexpr double kMaxDepthMs = 18.0;
constexpr double kBufferMarginMs = 4.0;
constexpr double kVoiceFadeBase = 1.5;
constexpr double kVoiceFadeShift = 3.0;

struct VoiceSetup
{
  double delayOffsetMs;
  double baseRateHz;
  double panPosition;
  uint32_t seed;
};

struct ChorusParameters
{
  double wetMix{0.0};
  double baseDelaySamples{0.0};
  double depthSamples{0.0};
  double width{0.0};
  double rateScale{0.0};
  double toneCoefficient{1.0};
  VoiceLevels voiceLevels{};
  double voiceMixScale{0.0};
};

constexpr std::array<VoiceSetup, kNumVoices> kVoiceSetups{{
  {0.0, 0.17, -0.10, 0x51A3C0DEu},
  {1.8, 0.23, 0.10, 0x79B4E281u},
  {3.6, 0.31, -0.28, 0xA56D9F33u},
  {5.6, 0.41, 0.28, 0x3EC7B41Fu},
  {7.5, 0.20, -0.62, 0x6D14AF27u},
  {9.6, 0.27, 0.62, 0x92B7C54Eu},
  {11.8, 0.36, -0.92, 0xC14E83A5u},
  {14.2, 0.49, 0.92, 0xE57AC918u},
}};

constexpr double kMaxVoiceDelayOffsetMs = kVoiceSetups[kNumVoices - 1].delayOffsetMs;

double MillisecondsToSamples(double milliseconds, double sampleRate)
{
  return (milliseconds * sampleRate) / 1000.0;
}

double FlushDenormal(double value)
{
  return std::abs(value) < kDenormalFloor ? 0.0 : value;
}

double SmoothValue(double current, double target, double coefficient)
{
  return current + (coefficient * (target - current));
}

double CutoffHzToCoefficient(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  return 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

double ComputeVoiceLevel(double knob, int voiceIndex)
{
  if(knob <= 0.0)
    return 0.0;

  const double exponent = std::pow(kVoiceFadeBase, static_cast<double>(voiceIndex) - kVoiceFadeShift);
  return std::pow(std::clamp(knob, 0.0, 1.0), exponent);
}

double ComputeVoiceMixScale(const VoiceLevels& voiceLevels)
{
  double sumSquares = 0.0;

  for(const double level : voiceLevels)
    sumSquares += level * level;

  if(sumSquares <= 0.0)
    return 0.0;

  // Keep the wet level referenced to the old fully-on 4-voice chorus.
  return kWetNormalization * std::sqrt(kLegacyVoiceCount / sumSquares);
}

double Quintic(double t)
{
  return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

uint32_t HashUint32(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

double HashToSignedUnitFloat(uint32_t x)
{
  return (static_cast<double>(x) / 4294967295.0) * 2.0 - 1.0;
}

double GradientNoise1D(double position, uint32_t seed)
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

std::array<double, 2> PanToGains(double pan)
{
  const double clampedPan = std::clamp(pan, -1.0, 1.0);
  const double angle = (clampedPan + 1.0) * (kPi * 0.25);
  return {std::cos(angle), std::sin(angle)};
}

ChorusParameters ComputeParameters(double amount, double sampleRate)
{
  const double knob = std::clamp(amount * 0.01, 0.0, 1.0);  // convert from [0, 100] to [0, 1]
  const double sqrtKnob = std::sqrt(knob);

  ChorusParameters parameters;
  parameters.wetMix = 1.5 * knob;
  parameters.baseDelaySamples = MillisecondsToSamples(10.0 + (20.0 * knob), sampleRate);
  parameters.depthSamples = MillisecondsToSamples(10.0 * sqrtKnob, sampleRate);
  parameters.width = 1.5 * knob;
  parameters.rateScale = 1.0 + (2.0 * knob);
  parameters.toneCoefficient = CutoffHzToCoefficient(sampleRate, kWetToneCutoffHz);

  for(int voiceIndex = 0; voiceIndex < kNumVoices; ++voiceIndex)
    parameters.voiceLevels[static_cast<std::size_t>(voiceIndex)] = ComputeVoiceLevel(knob, voiceIndex);

  parameters.voiceMixScale = ComputeVoiceMixScale(parameters.voiceLevels);
  return parameters;
}
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
  state = FlushDenormal(state);
  return state;
}

void effects::Chorus::OnePoleHighpass::Clear()
{
  lowState = 0.0;
}

double effects::Chorus::OnePoleHighpass::Process(double input)
{
  lowState += coefficient * (input - lowState);
  lowState = FlushDenormal(lowState);
  return input - lowState;
}

void effects::Chorus::InitializeVoiceStates()
{
  for(int i = 0; i < kNumVoices; ++i)
  {
    VoiceState& voice = mVoices[static_cast<std::size_t>(i)];
    const VoiceSetup& setup = kVoiceSetups[static_cast<std::size_t>(i)];
    voice.modSeed = setup.seed;
    voice.modPosition =
      (HashToSignedUnitFloat(voice.modSeed ^ 0xB8C9F52Du) + 1.0) * 100.0
      + (17.0 * static_cast<double>(i));
  }
}

void effects::Chorus::Reset(double sampleRate, int blockSize)
{
  (void) blockSize;

  mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  mAmountSmoothingCoefficient = 1.0 - std::exp(-1.0 / (kAmountSmoothingTimeSeconds * mSampleRate));
  mToneSmoothingCoefficient = 1.0 - std::exp(-1.0 / (kToneSmoothingTimeSeconds * mSampleRate));
  mTargetAmount = 0.0;
  mCurrentAmount = 0.0;
  mActive = false;

  const double maxDelayMs = kMaxBaseDelayMs + kMaxVoiceDelayOffsetMs + kMaxDepthMs + kBufferMarginMs;
  const int delaySize = static_cast<int>(std::ceil(MillisecondsToSamples(maxDelayMs, mSampleRate))) + 2;

  for(auto& voice : mVoices)
  {
    voice.delay.Resize(delaySize);
    voice.toneFilter.coefficient = CutoffHzToCoefficient(mSampleRate, kInitialVoiceToneCutoffHz);
  }

  mInputHighpass.coefficient = CutoffHzToCoefficient(mSampleRate, kInputHighpassCutoffHz);
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
    const ChorusParameters parameters = ComputeParameters(mCurrentAmount, mSampleRate);
    const double monoInput = mInputHighpass.Process(0.5 * (outputs[0][sampleIndex] + outputs[1][sampleIndex]));

    double wetLeft = 0.0;
    double wetRight = 0.0;

    for(int voiceIndex = 0; voiceIndex < kNumVoices; ++voiceIndex)
    {
      VoiceState& voice = mVoices[static_cast<std::size_t>(voiceIndex)];
      const VoiceSetup& setup = kVoiceSetups[static_cast<std::size_t>(voiceIndex)];
      const double voiceLevel = parameters.voiceLevels[static_cast<std::size_t>(voiceIndex)];
      const double rateHz = setup.baseRateHz * parameters.rateScale;
      voice.modPosition += rateHz / mSampleRate;
      voice.toneFilter.coefficient = SmoothValue(
        voice.toneFilter.coefficient,
        parameters.toneCoefficient,
        mToneSmoothingCoefficient);

      const double noise = GradientNoise1D(voice.modPosition, voice.modSeed);
      const double delaySamples = std::max(
        1.0,
        parameters.baseDelaySamples
          + MillisecondsToSamples(setup.delayOffsetMs, mSampleRate)
          + (parameters.depthSamples * noise));
      const double delayed = voice.toneFilter.Process(voice.delay.Read(delaySamples));
      const auto panGains = PanToGains(setup.panPosition * parameters.width);
      const double voiceMix = voiceLevel * parameters.voiceMixScale;

      wetLeft += delayed * panGains[0] * voiceMix;
      wetRight += delayed * panGains[1] * voiceMix;
      voice.delay.Write(monoInput);
    }

    outputs[0][sampleIndex] =
      static_cast<iplug::sample>(FlushDenormal(outputs[0][sampleIndex] + (wetLeft * parameters.wetMix)));
    outputs[1][sampleIndex] =
      static_cast<iplug::sample>(FlushDenormal(outputs[1][sampleIndex] + (wetRight * parameters.wetMix)));
  }

  if(mTargetAmount <= kBypassThreshold && mCurrentAmount <= kBypassThreshold)
  {
    mCurrentAmount = 0.0;
    mActive = false;
    Clear();
  }
}

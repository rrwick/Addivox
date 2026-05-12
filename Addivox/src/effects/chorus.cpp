#include "chorus.h"

#include "../dsp/gradient_noise.h"

#include <algorithm>
#include <cmath>

namespace {
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

struct VoiceSetup {
  double delayOffsetMs;
  double baseRateHz;
  double panPosition;
  uint32_t seed;
};

struct ChorusParameters {
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

double ComputeVoiceLevel(double knob, int voiceIndex) {
  if (knob <= 0.0) return 0.0;

  const double exponent = std::pow(kVoiceFadeBase, static_cast<double>(voiceIndex) - kVoiceFadeShift);
  return std::pow(std::clamp(knob, 0.0, 1.0), exponent);
}

double ComputeVoiceMixScale(const VoiceLevels& voiceLevels) {
  double sumSquares = 0.0;

  for (const double level : voiceLevels) sumSquares += level * level;

  if (sumSquares <= 0.0) return 0.0;

  // Keep the wet level referenced to the old fully-on 4-voice chorus.
  return kWetNormalization * std::sqrt(kLegacyVoiceCount / sumSquares);
}

ChorusParameters ComputeParameters(double amount, double sampleRate) {
  const double knob = std::clamp(amount * 0.01, 0.0, 1.0); // convert from [0, 100] to [0, 1]
  const double sqrtKnob = std::sqrt(knob);

  ChorusParameters parameters;
  parameters.wetMix = 1.5 * knob;
  parameters.baseDelaySamples = dsp::MillisecondsToSamples(10.0 + (20.0 * knob), sampleRate);
  parameters.depthSamples = dsp::MillisecondsToSamples(10.0 * sqrtKnob, sampleRate);
  parameters.width = 1.5 * knob;
  parameters.rateScale = 1.0 + (2.0 * knob);
  parameters.toneCoefficient = dsp::CutoffHzToCoefficient(sampleRate, kWetToneCutoffHz);

  for (std::size_t i = 0; i < parameters.voiceLevels.size(); ++i)
    parameters.voiceLevels[i] = ComputeVoiceLevel(knob, static_cast<int>(i));

  parameters.voiceMixScale = ComputeVoiceMixScale(parameters.voiceLevels);
  return parameters;
}
} // namespace

void effects::Chorus::InitializeVoiceStates() {
  for (std::size_t i = 0; i < mVoices.size(); ++i) {
    VoiceState& voice = mVoices[i];
    const VoiceSetup& setup = kVoiceSetups[i];
    voice.modSeed = setup.seed;
    voice.modPosition = (dsp::HashToSignedUnitFloat(voice.modSeed ^ 0xB8C9F52Du) + 1.0) * 100.0 + (17.0 * static_cast<double>(i));
    voice.noiseCache.reset();
  }
}

void effects::Chorus::Reset(double sampleRate, int blockSize) {
  (void)blockSize;

  mSampleRate = sampleRate > 0.0 ? sampleRate : dsp::kDefaultSampleRate;
  mAmountSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kAmountSmoothingTimeSeconds);
  mToneSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kToneSmoothingTimeSeconds);
  mTargetAmount = 0.0;
  mCurrentAmount = 0.0;
  mActive = false;

  const double maxDelayMs = kMaxBaseDelayMs + kMaxVoiceDelayOffsetMs + kMaxDepthMs + kBufferMarginMs;
  const int delaySize = static_cast<int>(std::ceil(dsp::MillisecondsToSamples(maxDelayMs, mSampleRate))) + 2;

  for (auto& voice : mVoices) {
    voice.delay.Resize(delaySize);
    voice.toneFilter.coefficient = dsp::CutoffHzToCoefficient(mSampleRate, kInitialVoiceToneCutoffHz);
  }

  mInputHighpass.coefficient = dsp::CutoffHzToCoefficient(mSampleRate, kInputHighpassCutoffHz);
  Clear();
}

void effects::Chorus::Clear() {
  mHasStoredSignal = false;
  mInputHighpass.Clear();
  InitializeVoiceStates();

  for (auto& voice : mVoices) {
    voice.delay.Clear();
    voice.toneFilter.Clear();
  }
}

void effects::Chorus::SetAmount(double amount) {
  mTargetAmount = std::clamp(amount, 0.0, 100.0);

  if (mTargetAmount > kBypassThreshold && !mActive) {
    Clear();
    mCurrentAmount = 0.0;
    mActive = true;
  }
}

void effects::Chorus::AdvanceSilentBlock(int nFrames) {
  for (int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex) {
    mCurrentAmount = dsp::SmoothValue(mCurrentAmount, mTargetAmount, mAmountSmoothingCoefficient);
    const ChorusParameters parameters = ComputeParameters(mCurrentAmount, mSampleRate);

    for (std::size_t i = 0; i < mVoices.size(); ++i) {
      VoiceState& voice = mVoices[i];
      const VoiceSetup& setup = kVoiceSetups[i];
      voice.modPosition += (setup.baseRateHz * parameters.rateScale) / mSampleRate;
      voice.toneFilter.coefficient = dsp::SmoothValue(voice.toneFilter.coefficient, parameters.toneCoefficient, mToneSmoothingCoefficient);
    }
  }
}

bool effects::Chorus::HasStoredSignal() const {
  if (mInputHighpass.lowState != 0.0) return true;

  for (const auto& voice : mVoices) {
    if (voice.delay.HasSignal() || voice.toneFilter.state != 0.0) return true;
  }

  return false;
}

void effects::Chorus::ProcessBlock(iplug::sample** outputs, int nFrames) {
  if (!mActive || nFrames <= 0) return;

  bool inputBlockSilent = true;
  for (int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex) {
    if (outputs[0][sampleIndex] != 0.0 || outputs[1][sampleIndex] != 0.0) {
      inputBlockSilent = false;
      mHasStoredSignal = true;
      break;
    }
  }

  if (inputBlockSilent && !mHasStoredSignal) {
    AdvanceSilentBlock(nFrames);

    if (mTargetAmount <= kBypassThreshold && mCurrentAmount <= kBypassThreshold) {
      mCurrentAmount = 0.0;
      mActive = false;
      Clear();
    }

    return;
  }

  ChorusParameters parameters{};
  double prevAmount = -1.0;
  std::array<double, kNumVoices> voiceScaleLeft{};
  std::array<double, kNumVoices> voiceScaleRight{};
  std::array<double, kNumVoices> voicePhaseIncrement{};
  std::array<double, kNumVoices> voiceBaseDelaySamples{};
  for (int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex) {
    mCurrentAmount = dsp::SmoothValue(mCurrentAmount, mTargetAmount, mAmountSmoothingCoefficient);
    if (mCurrentAmount != prevAmount) {
      parameters = ComputeParameters(mCurrentAmount, mSampleRate);
      prevAmount = mCurrentAmount;
      for (std::size_t i = 0; i < kNumVoices; ++i) {
        const auto panGains = dsp::PanToGains(kVoiceSetups[i].panPosition * parameters.width);
        const double voiceMix = parameters.voiceLevels[i] * parameters.voiceMixScale;
        voiceScaleLeft[i] = panGains[0] * voiceMix;
        voiceScaleRight[i] = panGains[1] * voiceMix;
        voicePhaseIncrement[i] = kVoiceSetups[i].baseRateHz * parameters.rateScale / mSampleRate;
        voiceBaseDelaySamples[i] = parameters.baseDelaySamples + dsp::MillisecondsToSamples(kVoiceSetups[i].delayOffsetMs, mSampleRate);
      }
    }
    const double monoInput = mInputHighpass.Process(0.5 * (outputs[0][sampleIndex] + outputs[1][sampleIndex]));

    double wetLeft = 0.0;
    double wetRight = 0.0;

    for (std::size_t i = 0; i < mVoices.size(); ++i) {
      VoiceState& voice = mVoices[i];
      voice.modPosition += voicePhaseIncrement[i];
      voice.toneFilter.coefficient = dsp::SmoothValue(voice.toneFilter.coefficient, parameters.toneCoefficient, mToneSmoothingCoefficient);

      const double noise = voice.noiseCache.evaluate(voice.modPosition, voice.modSeed);
      const double delaySamples = std::max(1.0, voiceBaseDelaySamples[i] + (parameters.depthSamples * noise));
      const double delayed = voice.toneFilter.Process(voice.delay.Read(delaySamples));

      wetLeft += delayed * voiceScaleLeft[i];
      wetRight += delayed * voiceScaleRight[i];
      voice.delay.Write(monoInput);
    }

    outputs[0][sampleIndex] = static_cast<iplug::sample>(dsp::FlushDenormal(outputs[0][sampleIndex] + (wetLeft * parameters.wetMix)));
    outputs[1][sampleIndex] = static_cast<iplug::sample>(dsp::FlushDenormal(outputs[1][sampleIndex] + (wetRight * parameters.wetMix)));
  }

  if (mTargetAmount <= kBypassThreshold && mCurrentAmount <= kBypassThreshold) {
    mCurrentAmount = 0.0;
    mActive = false;
    Clear();
  }

  if (inputBlockSilent && !HasStoredSignal()) mHasStoredSignal = false;
}

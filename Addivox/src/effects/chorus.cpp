#include "chorus.h"
#include "shared.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
constexpr double kBypassThreshold = 1.0e-6;
constexpr int kNumVoices = 8;

constexpr double kLegacyVoiceCount = 4.0;
constexpr double kWetNormalization = 0.48;
constexpr double kAmountSmoothingTimeSeconds = 0.030;
constexpr double kToneSmoothingTimeSeconds = 0.045;
constexpr double kInitialVoiceToneCutoffHz = 12000.0;
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

constexpr double kMaxVoiceDelayOffsetMs = kVoiceSetups.back().delayOffsetMs;

double ComputeVoiceLevel(double knob, int voiceIndex) {
  if (knob <= 0.0) return 0.0;

  const double exponent = std::pow(kVoiceFadeBase, static_cast<double>(voiceIndex) - kVoiceFadeShift);
  return std::pow(std::clamp(knob, 0.0, 1.0), exponent);
}

} // namespace

effects::Chorus::Parameters effects::Chorus::ComputeParameters(double amount) const {
  const double knob = std::clamp(amount * 0.01, 0.0, 1.0);
  const double baseDelaySamples = dsp::MillisecondsToSamples(10.0 + (20.0 * knob), mSampleRate);
  const double width = 1.5 * knob;
  const double rateScale = 1.0 + (2.0 * knob);
  std::array<double, kNumVoices> voiceLevels{};
  double sumSquares = 0.0;
  for (std::size_t i = 0; i < voiceLevels.size(); ++i) {
    voiceLevels[i] = ComputeVoiceLevel(knob, static_cast<int>(i));
    sumSquares += voiceLevels[i] * voiceLevels[i];
  }
  // Keep the wet level referenced to the old fully-on 4-voice chorus.
  const double voiceMixScale = sumSquares <= 0.0 ? 0.0 : kWetNormalization * std::sqrt(kLegacyVoiceCount / sumSquares);

  Parameters parameters;
  parameters.dryMix = 1.0 - (0.50 * knob);
  parameters.wetMix = knob;
  parameters.depthSamples = dsp::MillisecondsToSamples(10.0 * std::sqrt(knob), mSampleRate);
  for (std::size_t i = 0; i < parameters.voices.size(); ++i) {
    const auto& setup = kVoiceSetups[i];
    const auto panGains = dsp::PanToGains(setup.panPosition * width);
    const double voiceMix = voiceLevels[i] * voiceMixScale;
    parameters.voices[i] = {panGains[0] * voiceMix, panGains[1] * voiceMix, setup.baseRateHz * rateScale / mSampleRate,
                            baseDelaySamples + dsp::MillisecondsToSamples(setup.delayOffsetMs, mSampleRate)};
  }
  return parameters;
}

void effects::Chorus::Reset(double sampleRate) {
  mSampleRate = sampleRate > 0.0 ? sampleRate : dsp::kDefaultSampleRate;
  mAmountSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kAmountSmoothingTimeSeconds);
  mToneSmoothingCoefficient = dsp::ExponentialSmoothingCoefficient(mSampleRate, kToneSmoothingTimeSeconds);
  mWetToneCoefficient = dsp::CutoffHzToCoefficient(mSampleRate, kWetToneCutoffHz);
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

  for (std::size_t i = 0; i < mVoices.size(); ++i) {
    auto& voice = mVoices[i];
    voice.modPosition = (dsp::HashToSignedUnitFloat(kVoiceSetups[i].seed ^ 0xB8C9F52Du) + 1.0) * 100.0 + (17.0 * static_cast<double>(i));
    voice.noiseCache.reset();
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
    const double rateScale = 1.0 + (2.0 * std::clamp(mCurrentAmount * 0.01, 0.0, 1.0));

    for (std::size_t i = 0; i < mVoices.size(); ++i) {
      VoiceState& voice = mVoices[i];
      const VoiceSetup& setup = kVoiceSetups[i];
      voice.modPosition += (setup.baseRateHz * rateScale) / mSampleRate;
      voice.toneFilter.coefficient = dsp::SmoothValue(voice.toneFilter.coefficient, mWetToneCoefficient, mToneSmoothingCoefficient);
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

void effects::Chorus::DeactivateIfBypassed() {
  if (mTargetAmount <= kBypassThreshold && mCurrentAmount <= kBypassThreshold) {
    mCurrentAmount = 0.0;
    mActive = false;
    Clear();
  }
}

std::array<double, 2> effects::Chorus::ProcessWetSample(double monoInput, const Parameters& parameters) {
  double wetLeft = 0.0;
  double wetRight = 0.0;

  for (std::size_t i = 0; i < mVoices.size(); ++i) {
    VoiceState& voice = mVoices[i];
    const auto& voiceParameters = parameters.voices[i];
    voice.modPosition += voiceParameters.phaseIncrement;
    voice.toneFilter.coefficient = dsp::SmoothValue(voice.toneFilter.coefficient, mWetToneCoefficient, mToneSmoothingCoefficient);

    const double noise = voice.noiseCache.evaluate(voice.modPosition, kVoiceSetups[i].seed);
    const double delaySamples = std::max(1.0, voiceParameters.baseDelaySamples + (parameters.depthSamples * noise));
    const double delayed = voice.toneFilter.Process(voice.delay.Read(delaySamples));

    wetLeft += delayed * voiceParameters.scaleLeft;
    wetRight += delayed * voiceParameters.scaleRight;
    voice.delay.Write(monoInput);
  }
  return {wetLeft, wetRight};
}

void effects::Chorus::ProcessBlock(iplug::sample** outputs, int nFrames) {
  if (!mActive || nFrames <= 0) return;

  const bool inputBlockSilent = IsStereoBlockSilent(outputs, nFrames);
  if (!inputBlockSilent) mHasStoredSignal = true;

  if (inputBlockSilent && !mHasStoredSignal) {
    AdvanceSilentBlock(nFrames);

    DeactivateIfBypassed();

    return;
  }

  Parameters parameters{};
  double prevAmount = -1.0;
  for (int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex) {
    mCurrentAmount = dsp::SmoothValue(mCurrentAmount, mTargetAmount, mAmountSmoothingCoefficient);
    if (mCurrentAmount != prevAmount) {
      parameters = ComputeParameters(mCurrentAmount);
      prevAmount = mCurrentAmount;
    }
    const double monoInput = mInputHighpass.Process(0.5 * (outputs[0][sampleIndex] + outputs[1][sampleIndex]));

    const auto [wetLeft, wetRight] = ProcessWetSample(monoInput, parameters);

    outputs[0][sampleIndex] = static_cast<iplug::sample>(dsp::FlushDenormal((outputs[0][sampleIndex] * parameters.dryMix) + (wetLeft * parameters.wetMix)));
    outputs[1][sampleIndex] = static_cast<iplug::sample>(dsp::FlushDenormal((outputs[1][sampleIndex] * parameters.dryMix) + (wetRight * parameters.wetMix)));
  }

  DeactivateIfBypassed();

  if (inputBlockSilent && !HasStoredSignal()) mHasStoredSignal = false;
}

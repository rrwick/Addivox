#include "reverb.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kDenormalFloor = 1.0e-18;
constexpr double kBypassThreshold = 1.0e-6;
constexpr int kNumDiffusers = 3;
constexpr int kNumDelayLines = 4;

constexpr std::array<double, kNumDiffusers> kDiffuserDelayMs{5.7, 9.9, 14.8};
constexpr std::array<double, kNumDiffusers> kDiffuserFeedbacks{0.69, 0.63, 0.57};
constexpr std::array<double, kNumDelayLines> kDelayTimesMs{34.7, 45.9, 58.8, 72.4};
constexpr std::array<double, kNumDelayLines> kModDepthMs{0.11, 0.17, 0.23, 0.31};
constexpr std::array<double, kNumDelayLines> kModRateHz{0.11, 0.07, 0.17, 0.13};

double FlushDenormal(double value)
{
  return std::abs(value) < kDenormalFloor ? 0.0 : value;
}
} // namespace

void effects::Reverb::DelayLine::Resize(int size)
{
  buffer.assign(static_cast<std::size_t>(std::max(2, size)), 0.0);
  writeIndex = 0;
}

void effects::Reverb::DelayLine::Clear()
{
  std::fill(buffer.begin(), buffer.end(), 0.0);
  writeIndex = 0;
}

void effects::Reverb::DelayLine::Write(double input)
{
  if(buffer.empty())
    return;

  buffer[static_cast<std::size_t>(writeIndex)] = FlushDenormal(input);
  writeIndex = (writeIndex + 1) % static_cast<int>(buffer.size());
}

double effects::Reverb::DelayLine::Read(double delaySamples) const
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

void effects::Reverb::OnePoleLowpass::SetCutoffHz(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  coefficient = 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

void effects::Reverb::OnePoleLowpass::Clear()
{
  state = 0.0;
}

double effects::Reverb::OnePoleLowpass::Process(double input)
{
  state += coefficient * (input - state);
  state = FlushDenormal(state);
  return state;
}

void effects::Reverb::OnePoleHighpass::SetCutoffHz(double sampleRate, double cutoffHz)
{
  const double safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  const double safeCutoff = std::clamp(cutoffHz, 1.0, 0.49 * safeSampleRate);
  coefficient = 1.0 - std::exp((-2.0 * kPi * safeCutoff) / safeSampleRate);
}

void effects::Reverb::OnePoleHighpass::Clear()
{
  lowState = 0.0;
}

double effects::Reverb::OnePoleHighpass::Process(double input)
{
  lowState += coefficient * (input - lowState);
  lowState = FlushDenormal(lowState);
  return input - lowState;
}

void effects::Reverb::AllpassDiffuser::Reset(double sampleRate, double delayMs, double feedbackValue)
{
  delaySamples = std::max(1.0, Reverb::MillisecondsToSamples(delayMs, sampleRate));
  feedback = std::clamp(feedbackValue, -0.95, 0.95);
  delay.Resize(static_cast<int>(std::ceil(delaySamples)) + 2);
  Clear();
}

void effects::Reverb::AllpassDiffuser::Clear()
{
  delay.Clear();
}

double effects::Reverb::AllpassDiffuser::Process(double input)
{
  const double delayed = delay.Read(delaySamples);
  const double output = delayed - (feedback * input);
  delay.Write(input + (feedback * output));
  return FlushDenormal(output);
}

double effects::Reverb::MillisecondsToSamples(double milliseconds, double sampleRate)
{
  return (milliseconds * sampleRate) / 1000.0;
}

void effects::Reverb::Reset(double sampleRate, int blockSize)
{
  (void) blockSize;

  mSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
  mPreDelay.Resize(static_cast<int>(std::ceil(MillisecondsToSamples(24.0, mSampleRate))) + 2);

  for(int i = 0; i < kNumDiffusers; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    mDiffusers[index].Reset(mSampleRate, kDiffuserDelayMs[index], kDiffuserFeedbacks[index]);
  }

  for(int i = 0; i < kNumDelayLines; ++i)
  {
    const double maxDelayMs = kDelayTimesMs[static_cast<std::size_t>(i)] + kModDepthMs[static_cast<std::size_t>(i)] + 2.0;
    mDelayLines[static_cast<std::size_t>(i)].Resize(static_cast<int>(std::ceil(MillisecondsToSamples(maxDelayMs, mSampleRate))) + 2);
  }

  Clear();
  UpdateParameters();
}

void effects::Reverb::Clear()
{
  mInputLowpass.Clear();
  mInputHighpass.Clear();
  mPreDelay.Clear();

  for(auto& diffuser : mDiffusers)
    diffuser.Clear();

  for(auto& delayLine : mDelayLines)
    delayLine.Clear();

  for(auto& filter : mLoopDampingFilters)
    filter.Clear();

  for(auto& phase : mModPhase)
    phase = 0.0;
}

void effects::Reverb::SetAmount(double amount)
{
  const double sanitizedAmount = std::clamp(amount, 0.0, 100.0);
  const bool shouldBeActive = sanitizedAmount > kBypassThreshold;
  const bool activeStateChanged = shouldBeActive != mActive;

  mAmount = sanitizedAmount;
  mActive = shouldBeActive;

  if(activeStateChanged)
    Clear();

  if(mActive)
    UpdateParameters();
}

void effects::Reverb::UpdateParameters()
{
  const double space = std::clamp(mAmount * 0.01, 0.0, 1.0);
  const double decayShape = space * space;
  const double rt60Seconds = 0.40 + (1.90 * decayShape);
  const double preDelayMs = 7.0 + (11.0 * space);
  const double inputLowpassHz = 9600.0 - (3600.0 * space);
  const double loopDampingHz = 5200.0 - (2400.0 * space);
  const double modulationScale = 0.35 + (0.65 * space);

  mWetMix = 0.28 * space;
  mPreDelaySamples = std::max(1.0, MillisecondsToSamples(preDelayMs, mSampleRate));
  mInputLowpass.SetCutoffHz(mSampleRate, inputLowpassHz);
  mInputHighpass.SetCutoffHz(mSampleRate, 220.0);

  for(int i = 0; i < kNumDelayLines; ++i)
  {
    const std::size_t index = static_cast<std::size_t>(i);
    mBaseDelaySamples[index] = MillisecondsToSamples(kDelayTimesMs[index], mSampleRate);
    mFeedbackGains[index] = std::pow(10.0, (-3.0 * kDelayTimesMs[index] * 0.001) / rt60Seconds);
    mModDepthSamples[index] = MillisecondsToSamples(kModDepthMs[index] * modulationScale, mSampleRate);
    mModPhaseIncrement[index] = (2.0 * kPi * kModRateHz[index]) / mSampleRate;
    mLoopDampingFilters[index].SetCutoffHz(
      mSampleRate,
      std::max(1800.0, loopDampingHz * (1.0 - (0.12 * static_cast<double>(i)))));
  }
}

void effects::Reverb::ProcessBlock(iplug::sample** outputs, int nFrames)
{
  if(!mActive || nFrames <= 0)
    return;

  for(int sampleIndex = 0; sampleIndex < nFrames; ++sampleIndex)
  {
    const double dryLeft = outputs[0][sampleIndex];
    const double dryRight = outputs[1][sampleIndex];
    const double mid = 0.5 * (dryLeft + dryRight);
    const double side = 0.2 * (dryLeft - dryRight);

    double diffusedInput = mInputLowpass.Process(mid);
    diffusedInput = mInputHighpass.Process(diffusedInput);
    const double predelayed = mPreDelay.Read(mPreDelaySamples);
    mPreDelay.Write(diffusedInput);

    double diffused = predelayed;
    for(auto& diffuser : mDiffusers)
      diffused = diffuser.Process(diffused);

    std::array<double, kNumDelayLines> delayOutputs{};
    std::array<double, kNumDelayLines> dampedOutputs{};

    for(int i = 0; i < kNumDelayLines; ++i)
    {
      const std::size_t index = static_cast<std::size_t>(i);
      double delaySamples = mBaseDelaySamples[index];
      if(mModDepthSamples[index] > 0.0)
      {
        delaySamples += mModDepthSamples[index] * std::sin(mModPhase[index]);
        mModPhase[index] += mModPhaseIncrement[index];
        if(mModPhase[index] >= (2.0 * kPi))
          mModPhase[index] -= (2.0 * kPi);
      }

      delayOutputs[index] = mDelayLines[index].Read(delaySamples);
      dampedOutputs[index] = mLoopDampingFilters[index].Process(delayOutputs[index]);
    }

    const double feedback0 = 0.5 * (dampedOutputs[0] + dampedOutputs[1] + dampedOutputs[2] + dampedOutputs[3]);
    const double feedback1 = 0.5 * (dampedOutputs[0] - dampedOutputs[1] + dampedOutputs[2] - dampedOutputs[3]);
    const double feedback2 = 0.5 * (dampedOutputs[0] + dampedOutputs[1] - dampedOutputs[2] - dampedOutputs[3]);
    const double feedback3 = 0.5 * (dampedOutputs[0] - dampedOutputs[1] - dampedOutputs[2] + dampedOutputs[3]);

    mDelayLines[0].Write((0.5 * (diffused + side)) + (mFeedbackGains[0] * feedback0));
    mDelayLines[1].Write((0.5 * (diffused - side)) + (mFeedbackGains[1] * feedback1));
    mDelayLines[2].Write((0.5 * (-diffused + side)) + (mFeedbackGains[2] * feedback2));
    mDelayLines[3].Write((0.5 * (-diffused - side)) + (mFeedbackGains[3] * feedback3));

    const double wetLeft = 0.5 * (dampedOutputs[0] + dampedOutputs[1] - dampedOutputs[2] - dampedOutputs[3]);
    const double wetRight = 0.5 * (dampedOutputs[0] - dampedOutputs[1] + dampedOutputs[2] - dampedOutputs[3]);

    outputs[0][sampleIndex] = static_cast<iplug::sample>(dryLeft + (mWetMix * wetLeft));
    outputs[1][sampleIndex] = static_cast<iplug::sample>(dryRight + (mWetMix * wetRight));
  }
}

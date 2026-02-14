#pragma once

#include <cmath>

using namespace iplug;

class Oscillator
{
public:
  void SetSampleRate(double sampleRate)
  {
    mSampleRate = (sampleRate > 0.0) ? static_cast<float>(sampleRate) : 44100.f;
    UpdatePhaseIncrement();
  }

  void SetFrequency(float frequencyHz)
  {
    mFrequencyHz = (frequencyHz >= 0.f) ? frequencyHz : 0.f;
    UpdatePhaseIncrement();
  }

  void Reset(float phase01 = 0.f)
  {
    mPhase = phase01 - std::floor(phase01);
  }

  float Process()
  {
    const float out = std::sin(kTwoPi * mPhase);

    mPhase += mPhaseIncrement;
    if(mPhase >= 1.f)
      mPhase -= 1.f;

    return out;
  }

  float Process(float frequencyHz)
  {
    SetFrequency(frequencyHz);
    return Process();
  }

private:
  void UpdatePhaseIncrement()
  {
    mPhaseIncrement = mFrequencyHz / mSampleRate;
  }

  static constexpr float kTwoPi = 6.28318530717958647692f;

  float mSampleRate = 44100.f;
  float mFrequencyHz = 440.f;
  float mPhase = 0.f;
  float mPhaseIncrement = 440.f / 44100.f;
};

template<typename T>
class SynthVoice
{
public:
  bool GetBusy() const
  {
    return mGate > 0.;
  }

  void Trigger(double level, bool isRetrigger)
  {
    (void) level;
    (void) isRetrigger;
    mOSC.Reset();
  }

  void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs, int startIdx, int nFrames)
  {
    (void) inputs;
    (void) nInputs;
    (void) nOutputs;

    const float gate = (mGate > 0.f) ? 1.f : 0.f;
    const float osc1Freq = 440.f * std::pow(2.f, mPitch + mPitchBend);

    for(int i = startIdx; i < startIdx + nFrames; i++)
    {
      const T sample = static_cast<T>(mOSC.Process(osc1Freq) * gate * mGain * mBreath);
      outputs[0][i] += sample;
      outputs[1][i] += sample;
    }
  }

  void SetSampleRateAndBlockSize(double sampleRate, int blockSize)
  {
    (void) blockSize;
    mOSC.SetSampleRate(sampleRate);
  }

private:
  template <typename>
  friend class iplug::MidiSynth;

  float mGate{0.f};
  float mPitch{0.f};
  float mPitchBend{0.f};
  float mBreath{1.f};
  float mGain{0.f};
  Oscillator mOSC;
};

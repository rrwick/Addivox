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
    return mNoteOn;
  }

  void Trigger()
  {
    mOSC.Reset();
  }

  void ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames)
  {
    const float osc1Freq = 440.f * std::pow(2.f, mPitch + mPitchBend);
    mOSC.SetFrequency(osc1Freq);

    for(int i = startIdx; i < startIdx + nFrames; i++)
    {
      const T sample = static_cast<T>(mOSC.Process() * mBreath);
      outputs[0][i] += sample;
      outputs[1][i] += sample;
    }
  }

  void SetSampleRate(double sampleRate)
  {
    mOSC.SetSampleRate(sampleRate);
  }

private:
  template <typename>
  friend class iplug::MidiSynth;

  bool mNoteOn{false};
  float mPitch{0.f};
  float mPitchBend{0.f};
  float mBreath{1.f};
  Oscillator mOSC;
};

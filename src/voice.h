#pragma once

#include "Oscillator.h"
#include <cmath>

using namespace iplug;

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

    const T gate = mGate > 0. ? 1. : 0.;
    const double osc1Freq = 440. * pow(2., mPitch + mPitchBend);

    for(int i = startIdx; i < startIdx + nFrames; i++)
    {
      const T sample = mOSC.Process(osc1Freq) * gate * mGain * mBreath;
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

  double mGate{0.};
  double mPitch{0.};
  double mPitchBend{0.};
  double mBreath{1.};
  double mGain{0.};
  FastSinOscillator<T> mOSC;
};

#pragma once

#include "midi_synth.h"
#include "Smoothers.h"
#include "voice.h"
#include <cstring>

using namespace iplug;

template<typename T>
class SynthEngine
{
public:
  void ProcessBlock(T** outputs, int nFrames)
  {
    constexpr int kNumOutputs = 2;

    for(int i = 0; i < kNumOutputs; i++)
      memset(outputs[i], 0, nFrames * sizeof(T));

    mSynth.ProcessBlock(outputs, nFrames);

    for(int s = 0; s < nFrames; s++)
    {
      const T smoothedGain = mParamSmoother.Process(mGainTarget);
      outputs[0][s] *= smoothedGain;
      outputs[1][s] *= smoothedGain;
    }
  }

  void Reset(double sampleRate, int blockSize)
  {
    mSynth.SetSampleRateAndBlockSize(sampleRate, blockSize);
    mSynth.Reset();
  }

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    mSynth.AddMidiMsgToQueue(msg);
  }

  void SetParam(int paramIdx, double value)
  {
    (void) paramIdx;
    mGainTarget = static_cast<T>(value / 100.);
  }

public:
  MidiSynth<SynthVoice<T>> mSynth { MidiSynth<SynthVoice<T>>::kDefaultBlockSize };
  LogParamSmooth<T, 1> mParamSmoother;
  sample mGainTarget{1.};
};

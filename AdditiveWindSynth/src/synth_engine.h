#pragma once

#include "midi_synth.h"
#include "params.h"
#include "Smoothers.h"
#include "voice.h"
#include <cstring>

using namespace iplug;

class SynthEngine
{
public:
  using VisualizerFrame = SynthVoice::VisualizerFrame;

  void ProcessBlock(sample** outputs, int nFrames)
  {
    constexpr int kNumOutputs = 2;

    for(int i = 0; i < kNumOutputs; i++)
      memset(outputs[i], 0, nFrames * sizeof(sample));

    mSynth.ProcessBlock(outputs, nFrames);

    for(int s = 0; s < nFrames; s++)
    {
      const sample smoothedGain = mParamSmoother.Process(mGainTarget);
      outputs[0][s] *= smoothedGain;
      outputs[1][s] *= smoothedGain;
    }
  }

  void Reset(double sampleRate, int blockSize)
  {
    mSynth.SetSampleRateAndBlockSize(sampleRate, blockSize);
    mSynth.Reset();
    mSynth.GetVoice().SetGlobalOscillatorModifiers(mGlobalOscillatorModifiers);
  }

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    mSynth.AddMidiMsgToQueue(msg);
  }

  void SetParam(int paramIdx, double value)
  {
    if(paramIdx == kParamGain)
    {
      mGainTarget = static_cast<sample>(value / 100.);
      return;
    }

    if(global_settings::ApplyParam(paramIdx, value, mGlobalOscillatorModifiers))
      mSynth.GetVoice().SetGlobalOscillatorModifiers(mGlobalOscillatorModifiers);
  }

  void GetVisualizerFrame(VisualizerFrame& frame) const
  {
    mSynth.GetVoice().GetVisualizerFrame(frame);
  }

public:
  MidiSynth<SynthVoice> mSynth { MidiSynth<SynthVoice>::kDefaultBlockSize };
  LogParamSmooth<sample, 1> mParamSmoother;
  sample mGainTarget{1.};
  GlobalOscillatorModifiers mGlobalOscillatorModifiers{};
};

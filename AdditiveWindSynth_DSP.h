#pragma once

#include "Synth/MidiSynth.h"
#include "Oscillator.h"
#include "Smoothers.h"

using namespace iplug;

template<typename T>
class AdditiveWindSynthDSP
{
public:
#pragma mark - Voice
  class Voice : public SynthVoice
  {
  public:
    bool GetBusy() const override
    {
      return mGate > 0.;
    }

    void Trigger(double level, bool isRetrigger) override
    {
      (void) level;
      (void) isRetrigger;
      mOSC.Reset();
    }
    
    void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs, int startIdx, int nFrames) override
    {
      (void) inputs;
      (void) nInputs;
      (void) nOutputs;

      const T gate = mGate > 0. ? 1. : 0.;
      double pitch = mPitch;
      double pitchBend = mPitchBend;
      
      // convert from "1v/oct" pitch space to frequency in Hertz
      double osc1Freq = 440. * pow(2., pitch + pitchBend);
      
      // make sound output for each output channel
      for(auto i = startIdx; i < startIdx + nFrames; i++)
      {
        const T sample = mOSC.Process(osc1Freq) * gate * mGain;
        outputs[0][i] += sample;
        outputs[1][i] += sample;
      }
    }

    void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
    {
      (void) blockSize;
      mOSC.SetSampleRate(sampleRate);
    }

  public:
    FastSinOscillator<T> mOSC;

  };

public:
#pragma mark -
  AdditiveWindSynthDSP()
  {
    // Monosynth: one voice.
    mSynth.AddVoice(new Voice());
  }

  void ProcessBlock(T** outputs, int nFrames)
  {
    constexpr int kNumOutputs = 2;

    // clear outputs
    for(auto i = 0; i < kNumOutputs; i++)
    {
      memset(outputs[i], 0, nFrames * sizeof(T));
    }
    
    mSynth.ProcessBlock(nullptr, outputs, 0, kNumOutputs, nFrames);
    
    for(int s=0; s < nFrames;s++)
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
    switch (paramIdx) {
      case kParamGain:
        mGainTarget = (T) value / 100.;
        break;
      default:
        break;
    }
  }
  
public:
  MidiSynth mSynth { MidiSynth::kDefaultBlockSize };
  LogParamSmooth<T, 1> mParamSmoother;
  sample mGainTarget{1.};
};

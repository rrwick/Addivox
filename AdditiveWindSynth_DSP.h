#pragma once

#include "Synth/MidiSynth.h"
#include "Oscillator.h"
#include "Smoothers.h"

using namespace iplug;

enum EModulations
{
  kModGainSmoother = 0,
  kNumModulations,
};

template<typename T>
class AdditiveWindSynthDSP
{
public:
#pragma mark - Voice
  class Voice : public SynthVoice
  {
  public:
    Voice() {}

    bool GetBusy() const override
    {
      return mInputs[kVoiceControlGate].endValue > 0.;
    }

    void Trigger(double level, bool isRetrigger) override
    {
      mOSC.Reset();
    }
    
    void Release() override
    {
    }

    void ProcessSamplesAccumulating(T** inputs, T** outputs, int nInputs, int nOutputs, int startIdx, int nFrames) override
    {
      // inputs to the synthesizer can just fetch a value every block, like this:
//      double gate = mInputs[kVoiceControlGate].endValue;
      double pitch = mInputs[kVoiceControlPitch].endValue;
      double pitchBend = mInputs[kVoiceControlPitchBend].endValue;

      // or write the entire control ramp to a buffer, like this, to get sample-accurate ramps:
      mInputs[kVoiceControlGate].Write(mGateBuffer.Get(), startIdx, nFrames);
      mInputs[kVoiceControlTimbre].Write(mTimbreBuffer.Get(), startIdx, nFrames);
      
      // convert from "1v/oct" pitch space to frequency in Hertz
      double osc1Freq = 440. * pow(2., pitch + pitchBend);
      
      // make sound output for each output channel
      for(auto i = startIdx; i < startIdx + nFrames; i++)
      {
        float noise = mTimbreBuffer.Get()[i] * Rand();
        const T gate = mGateBuffer.Get()[i] > 0. ? 1. : 0.;
        const T sample = (mOSC.Process(osc1Freq) + noise) * gate * mGain;
        outputs[0][i] += sample;
        outputs[1][i] += sample;
      }
    }

    void SetSampleRateAndBlockSize(double sampleRate, int blockSize) override
    {
      mOSC.SetSampleRate(sampleRate);
      mGateBuffer.Resize(blockSize);
      mTimbreBuffer.Resize(blockSize);
    }

    void SetProgramNumber(int pgm) override
    {
      //TODO:
    }

    // this is called by the VoiceAllocator to set generic control values.
    void SetControl(int controlNumber, float value) override
    {
      //TODO:
    }

  public:
    FastSinOscillator<T> mOSC;

  private:
    WDL_TypedBuf<float> mGateBuffer;
    WDL_TypedBuf<float> mTimbreBuffer;

    // noise generator for test
    uint32_t mRandSeed = 0;
    
    // return single-precision floating point number on [-1, 1]
    float Rand()
    {
      mRandSeed = mRandSeed * 0x0019660D + 0x3C6EF35F;
      uint32_t temp = ((mRandSeed >> 9) & 0x007FFFFF) | 0x3F800000;
      return (*reinterpret_cast<float*>(&temp))*2.f - 3.f;
    }

  };

public:
#pragma mark -
  AdditiveWindSynthDSP()
  {
    // Monosynth: one voice in one zone.
    mSynth.AddVoice(new Voice(), 0);

    // some MidiSynth API examples:
    // mSynth.SetKeyToPitchFn([](int k){return (k - 69.)/24.;}); // quarter-tone scale
  }

  void ProcessBlock(T** inputs, T** outputs, int nOutputs, int nFrames)
  {
    // clear outputs
    for(auto i = 0; i < nOutputs; i++)
    {
      memset(outputs[i], 0, nFrames * sizeof(T));
    }
    
    mParamSmoother.ProcessBlock(mParamsToSmooth, mModulations.GetList(), nFrames);
    mSynth.ProcessBlock(mModulations.GetList(), outputs, 0, nOutputs, nFrames);
    
    for(int s=0; s < nFrames;s++)
    {
      T smoothedGain = mModulations.GetList()[kModGainSmoother][s];
      outputs[0][s] *= smoothedGain;
      outputs[1][s] *= smoothedGain;
    }
  }

  void Reset(double sampleRate, int blockSize)
  {
    mSynth.SetSampleRateAndBlockSize(sampleRate, blockSize);
    mSynth.Reset();
    mModulationsData.Resize(blockSize * kNumModulations);
    mModulations.Empty();
    
    for(int i = 0; i < kNumModulations; i++)
    {
      mModulations.Add(mModulationsData.Get() + (blockSize * i));
    }
  }

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    mSynth.AddMidiMsgToQueue(msg);
  }

  void SetParam(int paramIdx, double value)
  {
    switch (paramIdx) {
      case kParamGain:
        mParamsToSmooth[kModGainSmoother] = (T) value / 100.;
        break;
      default:
        break;
    }
  }
  
public:
  MidiSynth mSynth { MidiSynth::kDefaultBlockSize };
  WDL_TypedBuf<T> mModulationsData; // Sample data for global modulations (e.g. smoothed gain)
  WDL_PtrList<T> mModulations; // Ptrlist for global modulations
  LogParamSmooth<T, kNumModulations> mParamSmoother;
  sample mParamsToSmooth[kNumModulations] {1.};
};

#pragma once

#include "midi_synth.h"
#include "params.h"
#include "Smoothers.h"
#include "voice.h"
#include <algorithm>
#include <cstring>

using namespace iplug;

template<typename T>
class SynthEngine
{
public:
  using VisualizerFrame = typename SynthVoice<T>::VisualizerFrame;

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
    mSynth.GetVoice().SetGlobalOscillatorModifiers(mGlobalOscillatorModifiers);
  }

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    mSynth.AddMidiMsgToQueue(msg);
  }

  void SetParam(int paramIdx, double value)
  {
    switch(paramIdx)
    {
      case kParamGain:
        mGainTarget = static_cast<T>(value / 100.);
        return;
      case kParamGlobalAttackScale:
        mGlobalOscillatorModifiers.attackScale = std::max(0.f, static_cast<float>(value));
        break;
      case kParamGlobalReleaseScale:
        mGlobalOscillatorModifiers.releaseScale = std::max(0.f, static_cast<float>(value));
        break;
      case kParamGlobalPitchShift:
        mGlobalOscillatorModifiers.pitchOffsetCents = static_cast<float>(value);
        break;
      case kParamGlobalPanShift:
        mGlobalOscillatorModifiers.panOffset = std::clamp(static_cast<float>(value), -1.f, 1.f);
        break;
      case kParamGlobalIntensityVariationAmplitudeScale:
        mGlobalOscillatorModifiers.intensityVariationAmplitudeScale = std::max(0.f, static_cast<float>(value));
        break;
      case kParamGlobalIntensityVariationRateScale:
        mGlobalOscillatorModifiers.intensityVariationRateScale = std::max(0.f, static_cast<float>(value));
        break;
      case kParamGlobalPitchVariationAmplitudeScale:
        mGlobalOscillatorModifiers.pitchVariationAmplitudeScale = std::max(0.f, static_cast<float>(value));
        break;
      case kParamGlobalPitchVariationRateScale:
        mGlobalOscillatorModifiers.pitchVariationRateScale = std::max(0.f, static_cast<float>(value));
        break;
      case kParamGlobalPanVariationAmplitudeScale:
        mGlobalOscillatorModifiers.panVariationAmplitudeScale = std::max(0.f, static_cast<float>(value));
        break;
      case kParamGlobalPanVariationRateScale:
        mGlobalOscillatorModifiers.panVariationRateScale = std::max(0.f, static_cast<float>(value));
        break;
      default:
        return;
    }

    mSynth.GetVoice().SetGlobalOscillatorModifiers(mGlobalOscillatorModifiers);
  }

  void GetVisualizerFrame(VisualizerFrame& frame) const
  {
    mSynth.GetVoice().GetVisualizerFrame(frame);
  }

public:
  MidiSynth<SynthVoice<T>> mSynth { MidiSynth<SynthVoice<T>>::kDefaultBlockSize };
  LogParamSmooth<T, 1> mParamSmoother;
  sample mGainTarget{1.};
  GlobalOscillatorModifiers mGlobalOscillatorModifiers{};
};

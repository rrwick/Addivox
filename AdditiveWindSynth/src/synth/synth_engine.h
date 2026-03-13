#pragma once

#include "midi_synth.h"
#include "../settings/params.h"
#include "../settings/effects.h"
#include "voice.h"
#include "../effects/drive.h"
#include "../effects/reverb.h"
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

    if(mDrive.IsActive())
      mDrive.ProcessBlock(outputs, nFrames);

    if(mReverb.IsActive())
      mReverb.ProcessBlock(outputs, nFrames);
  }

  void Reset(double sampleRate, int blockSize)
  {
    mSynth.SetSampleRateAndBlockSize(sampleRate, blockSize);
    mSynth.Reset();
    mSynth.GetVoice().SetGlobalVoiceSettings(mGlobalVoiceSettings);
    mDrive.Reset(sampleRate, blockSize);
    mDrive.SetAmount(mEffectsSettings.drive);
    mReverb.Reset(sampleRate, blockSize);
    mReverb.SetAmount(mEffectsSettings.reverb);
  }

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    mSynth.AddMidiMsgToQueue(msg);
  }

  void SetParam(int paramIdx, double value)
  {
    if(global_settings::ApplyParam(paramIdx, value, mGlobalVoiceSettings))
    {
      mSynth.GetVoice().SetGlobalVoiceSettings(mGlobalVoiceSettings);
      return;
    }

    if(effects_settings::ApplyParam(paramIdx, value, mEffectsSettings))
    {
      mDrive.SetAmount(mEffectsSettings.drive);
      mReverb.SetAmount(mEffectsSettings.reverb);
    }
  }

  void SetCompoundPreset(const CompoundPreset& preset)
  {
    mSynth.GetVoice().SetCompoundPreset(preset);
  }

  bool SetKeyNoteOscillatorParameter(double midiNote,
                                     int oscillatorIndex,
                                     OscillatorSettings::Parameter parameter,
                                     double value)
  {
    return mSynth.GetVoice().SetKeyNoteOscillatorParameter(midiNote, oscillatorIndex, parameter, value);
  }

  bool SetKeyNoteOscillatorParameterValues(
    double midiNote,
    OscillatorSettings::Parameter parameter,
    const std::array<double, SimplePreset::kNumOscillators>& values)
  {
    return mSynth.GetVoice().SetKeyNoteOscillatorParameterValues(midiNote, parameter, values);
  }

  bool AddKeyNotePreset(double midiNote)
  {
    return mSynth.GetVoice().AddKeyNotePreset(midiNote);
  }

  bool RemoveKeyNotePreset(double midiNote)
  {
    return mSynth.GetVoice().RemoveKeyNotePreset(midiNote);
  }

  void GetVisualizerFrame(VisualizerFrame& frame) const
  {
    mSynth.GetVoice().GetVisualizerFrame(frame);
  }

public:
  MidiSynth<SynthVoice> mSynth { MidiSynth<SynthVoice>::kDefaultBlockSize };
  GlobalVoiceSettings mGlobalVoiceSettings{};
  EffectsSettings mEffectsSettings{};
  effects::Drive mDrive;
  effects::Reverb mReverb;
};

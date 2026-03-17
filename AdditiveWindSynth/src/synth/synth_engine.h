#pragma once

#include "midi_synth.h"
#include "../settings/params.h"
#include "../settings/effects.h"
#include "voice.h"
#include "../effects/drive.h"
#include "../effects/tone.h"
#include "../effects/chorus.h"
#include "../effects/reverb.h"
#include <cmath>
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

    if(mTone.IsActive())
      mTone.ProcessBlock(outputs, nFrames);

    if(mChorus.IsActive())
      mChorus.ProcessBlock(outputs, nFrames);

    if(mReverb.IsActive())
      mReverb.ProcessBlock(outputs, nFrames);
  }

  void Reset(double sampleRate, int blockSize)
  {
    mSynth.SetSampleRateAndBlockSize(sampleRate, blockSize);
    mSynth.Reset();
    mSynth.SetBlipGuardDelayMs(mBlipGuardDelayMs);
    mSynth.SetBlipGuardIntervalSemitones(mBlipGuardIntervalSemitones);
    mSynth.GetVoice().SetGlobalVoiceSettings(mGlobalVoiceSettings);
    mSynth.GetVoice().SetTransposeSemitones(mTransposeSemitones);
    mDrive.Reset(sampleRate, blockSize);
    mDrive.SetAmount(mEffectsSettings.drive);
    mTone.Reset(sampleRate, blockSize);
    mTone.SetAmount(mEffectsSettings.tone);
    mChorus.Reset(sampleRate, blockSize);
    mChorus.SetAmount(mEffectsSettings.chorus);
    mReverb.Reset(sampleRate, blockSize);
    mReverb.SetAmount(mEffectsSettings.reverb);
  }

  void ProcessMidiMsg(const IMidiMsg& msg)
  {
    mSynth.AddMidiMsgToQueue(msg);
  }

  void SetParam(int paramIdx, double value)
  {
    if(paramIdx == kParamTranspose)
    {
      mTransposeSemitones = value;
      mSynth.GetVoice().SetTransposeSemitones(mTransposeSemitones);
      return;
    }

    if(paramIdx == kParamBlipGuardDelay)
    {
      mBlipGuardDelayMs = value;
      mSynth.SetBlipGuardDelayMs(mBlipGuardDelayMs);
      return;
    }

    if(paramIdx == kParamBlipGuardInterval)
    {
      mBlipGuardIntervalSemitones = Clip(
        static_cast<int>(std::lround(value)),
        BlipGuard::kMinIntervalSemitones,
        BlipGuard::kMaxIntervalSemitones);
      mSynth.SetBlipGuardIntervalSemitones(mBlipGuardIntervalSemitones);
      return;
    }

    if(global_settings::ApplyParam(paramIdx, value, mGlobalVoiceSettings))
    {
      mSynth.GetVoice().SetGlobalVoiceSettings(mGlobalVoiceSettings);
      return;
    }

    if(effects_settings::ApplyParam(paramIdx, value, mEffectsSettings))
    {
      mDrive.SetAmount(mEffectsSettings.drive);
      mTone.SetAmount(mEffectsSettings.tone);
      mChorus.SetAmount(mEffectsSettings.chorus);
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
  double mTransposeSemitones{0.0};
  double mBlipGuardDelayMs{0.0};
  int mBlipGuardIntervalSemitones{BlipGuard::kDefaultIntervalSemitones};
  effects::Drive mDrive;
  effects::Tone mTone;
  effects::Chorus mChorus;
  effects::Reverb mReverb;
};

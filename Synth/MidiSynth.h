/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
 */

#pragma once

/**
 * @file
 * @copydoc MidiSynth
 */

#include <stdint.h>

#include "IPlugConstants.h"
#include "IPlugMidi.h"
#include "IPlugLogger.h"

#include "SynthVoice.h"
#include "VoiceAllocator.h"

BEGIN_IPLUG_NAMESPACE

/** A monophonic synthesiser base class which can be supplied with a custom voice. */
class MidiSynth
{
public:
  /** This defines the size in samples of a single block of processing that will be done by the synth. */
  static constexpr int kDefaultBlockSize = 32;
  static constexpr int kDefaultPitchBendRange = 12;

#pragma mark - MidiSynth class

  MidiSynth(int blockSize = kDefaultBlockSize);
  ~MidiSynth() = default;

  MidiSynth(const MidiSynth&) = delete;
  MidiSynth& operator=(const MidiSynth&) = delete;
    
  void Reset()
  {
    mSampleTime = 0;
    mVoiceAllocator.Clear();
  }

  void SetSampleRateAndBlockSize(double sampleRate, int blockSize);

  /** Set the pitch bend range in semitones for all channels. */
  void SetPitchBendRange(int pitchBendRange)
  {
    const int clipped = Clip(pitchBendRange, 0, 96);

    for(int i=0; i<16; ++i)
    {
      mChannelStates[i].pitchBendRange = clipped;
    }
  }

  SynthVoice* GetVoice()
  {
    return mVoiceAllocator.GetVoice();
  }
  
  bool HasVoice() const
  {
    return mVoiceAllocator.HasVoice();
  }

  /** adds a SynthVoice to this MidiSynth, taking ownership of the object. */
  void AddVoice(SynthVoice* pVoice, uint8_t zone)
  {
    mVoiceAllocator.AddVoice(pVoice, zone);
  }

  void AddMidiMsgToQueue(const IMidiMsg& msg)
  {
    mMidiQueue.Add(msg);
  }

  /** Processes a block of audio samples
   * @param inputs Pointer to input Arrays
   * @param outputs Pointer to output Arrays
   * @param nInputs The number of input channels that contain valid data
   * @param nOutputs input channels that contain valid data
   * @param nFrames The number of sample frames to process
   * @return \c true if the synth is silent */
  bool ProcessBlock(sample** inputs, sample** outputs, int nInputs, int nOutputs, int nFrames);

private:

  // maintain the state for one MIDI channel including RPN receipt state and pitch bend range.
  struct ChannelState
  {
    uint8_t paramMSB;
    uint8_t paramLSB;
    uint8_t valueMSB;
    uint8_t valueLSB;
    uint8_t pitchBendRange; // in semitones
  };

  void SetChannelPitchBendRange(int channel, int range);

  VoiceInputEvent MidiMessageToEvent(const IMidiMsg& msg);
  void HandleRPN(IMidiMsg msg);

  // basic MIDI data
  VoiceAllocator mVoiceAllocator;
  IMidiQueue mMidiQueue;
  float mVelocityLUT[128];
  ChannelState mChannelStates[16]{};
  int mBlockSize;
  int64_t mSampleTime{0};
};

END_IPLUG_NAMESPACE

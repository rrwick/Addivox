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
#include <array>
#include <stdexcept>

#include "IPlugConstants.h"
#include "IPlugMidi.h"

#include "SynthVoice.h"

BEGIN_IPLUG_NAMESPACE

/** A monophonic synthesiser base class which can be supplied with a custom voice. */
class MidiSynth
{
public:
  /** This defines the size in samples of a single block of processing that will be done by the synth. */
  static constexpr int kDefaultBlockSize = 32;
  static constexpr int kDefaultPitchBendRange = 2;

#pragma mark - MidiSynth class

  MidiSynth(int blockSize = kDefaultBlockSize);
  ~MidiSynth() = default;

  MidiSynth(const MidiSynth&) = delete;
  MidiSynth& operator=(const MidiSynth&) = delete;
    
  void Reset()
  {
    mMidiState.currentPitchBend = 0.f;
    mMidiState.currentBreath = 1.f;
    mActiveChannel = 0;
    mActiveKey = kNoKey;
    KillVoice(true);
    ClearVoiceControls();
  }

  void SetSampleRateAndBlockSize(double sampleRate, int blockSize);

  /** Set the pitch bend range in semitones for the active mono channel state. */
  void SetPitchBendRange(int pitchBendRange)
  {
    mMidiState.pitchBendRange = static_cast<uint8_t>(Clip(pitchBendRange, 0, 96));
  }

  SynthVoice* GetVoice()
  {
    return mVoicePtr;
  }
  
  bool HasVoice() const
  {
    return mVoicePtr != nullptr;
  }

  /** adds a SynthVoice to this MidiSynth. */
  void AddVoice(SynthVoice* pVoice)
  {
    if(mVoicePtr)
      throw std::runtime_error{"MidiSynth: only one voice is supported"};

    mVoicePtr = pVoice;
    mActiveChannel = 0;
    mActiveKey = kNoKey;
    ClearVoiceControls();
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
  struct MonoMidiState
  {
    uint8_t activeChannel{0};
    uint8_t paramMSB;
    uint8_t paramLSB;
    uint8_t valueMSB;
    uint8_t valueLSB;
    uint8_t pitchBendRange; // in semitones
    float currentPitchBend;
    float currentBreath;
  };

  void SetPitchBendRangeFromRPN(int channel, int range);

  void HandlePerformanceMessage(const IMidiMsg& msg);
  void HandleRPN(IMidiMsg msg);
  void ProcessVoice(sample** inputs, sample** outputs, int nInputs, int nOutputs, int startIndex, int numFrames);
  void StartVoice(int channel, int key, float pitch, float velocity, bool retrig);
  void StopVoice();
  void KillVoice(bool hard);
  void AllNotesOff();
  void PitchBend(int channel, float value);
  void Breath(int channel, float value);
  bool IsActiveChannel(uint8_t channel) const;
  bool IsActiveNote(uint8_t channel, uint8_t key) const;
  void ClearVoiceControls();

  static constexpr uint8_t kNoKey = static_cast<uint8_t>(-1);

  // basic MIDI data
  IMidiQueue mMidiQueue;
  std::array<float, 128> mVelocityLUT{};
  MonoMidiState mMidiState{};
  SynthVoice* mVoicePtr{nullptr};
  uint8_t mActiveChannel{0};
  uint8_t mActiveKey{kNoKey};
};

END_IPLUG_NAMESPACE

/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
 */

#pragma once

/**
 * @file
 * @copydoc VoiceAllocator
 */

#include <array>
#include <stdint.h>
#include <functional>
#include <memory>
//#include <iostream>

#include "IPlugLogger.h"
#include "IPlugQueue.h"

#include "SynthVoice.h"

BEGIN_IPLUG_NAMESPACE

using namespace voiceControlNames;

struct VoiceAddress
{
  uint8_t mZone;
  uint8_t mChannel;
  uint8_t mKey;
  uint8_t mFlags;
};

const uint8_t kAllZones = UCHAR_MAX;
const uint8_t kAllChannels = UCHAR_MAX;
const uint8_t kAllKeys = UCHAR_MAX;

// flags
const uint8_t kVoiceAll = 1 << 0;

enum EVoiceAction
{
  kNullAction = 0,
  kNoteOnAction,
  kNoteOffAction,
  kPitchBendAction,
  kPressureAction,
  kTimbreAction,
  kControllerAction,
  kProgramChangeAction
};

/** A VoiceInputEvent describes a change in input to be applied to the voice.
 * mAddress specifies whether the voice should receive the change.
 * mAction is the type of property change.
 * mControllerNumber is the controller number to change if mAction is kController.
 * mValue is the new value associated with the change.
 * mSampleOffset is the number of samples into a processing buffer at which the change should occur.*/
struct VoiceInputEvent
{
  VoiceAddress mAddress;
  EVoiceAction mAction;
  int mControllerNumber;
  float mValue;
  int mSampleOffset;
};

#pragma mark - VoiceAllocator class

class VoiceAllocator final
{
public:

  enum EATMode
  {
    kATModeChannel = 0,
    kATModePoly,
    kNumATModes
  };

  // one voice worth of ramp generators
  using VoiceControlRamps = ControlRampProcessor::ProcessorArray<kNumVoiceControlRamps>;

  VoiceAllocator();
  ~VoiceAllocator();

  VoiceAllocator(const VoiceAllocator&) = delete;
  VoiceAllocator& operator=(const VoiceAllocator&) = delete;

  void Clear();

  void SetSampleRateAndBlockSize(double sampleRate, int blockSize) { (void) sampleRate; (void) blockSize; }

  /** Add a synth voice to the allocator. We do not take ownership ot the voice.
   @param pv Pointer to the voice to add.
   @param zone A zone can be specified to make multitimbral synths.*/
  void AddVoice(SynthVoice* pv, uint8_t zone);

  /** Add a single event to the input queue for the current processing block. */
  void AddEvent(VoiceInputEvent e) { mInputQueue.Push(e); }

  /** Process all input events and generate voice outputs. */
  void ProcessEvents(int samples, int64_t sampleTime);

  /** Turn the voice gate off, allowing any envelope to finish. */
  void SoftKillVoice();

  /** Stop the voice from making sound immediately. */
  void HardKillVoice();

  void SetKeyToPitchFunction(const std::function<float(int)>& fn) {mKeyToPitchFn = fn;}

  void ProcessVoice(sample** inputs, sample** outputs, int nInputs, int nOutputs, int startIndex, int blockSize);

  bool HasVoice() const { return mVoicePtr != nullptr; }
  SynthVoice* GetVoice() const { return mVoicePtr; }
  void SetPitchOffset(float offset) { mPitchOffset = offset; }

private:
  bool VoiceMatchesAddress(VoiceAddress va) const;

  void SendControlToVoiceInputs(bool voiceMatched, int ctlIdx, float val, int rampSamples);
  void SendControlToVoiceDirect(bool voiceMatched, int ctlIdx, float val);
  void SendProgramChangeToVoice(bool voiceMatched, int pgm);

  void StartVoice(int channel, int key, float pitch, float velocity, int sampleOffset, int64_t sampleTime, bool retrig);
  void StopVoice(int sampleOffset);

  void ClearVoiceInputs(SynthVoice* pVoice);

  void NoteOn(VoiceInputEvent e, int64_t sampleTime);
  void NoteOff(VoiceInputEvent e);

  IPlugQueue<VoiceInputEvent> mInputQueue{1024};

  SynthVoice* mVoicePtr{nullptr};
  std::unique_ptr<VoiceControlRamps> mVoiceRamps;

  std::function<float(int)> mKeyToPitchFn;
  double mPitchOffset{0.};

public:
  EATMode mATMode {kATModeChannel};
};

END_IPLUG_NAMESPACE

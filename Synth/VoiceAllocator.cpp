/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
 */

#include "VoiceAllocator.h"

using namespace iplug;

VoiceAllocator::VoiceAllocator()
{}

void VoiceAllocator::Clear()
{
  HardKillVoice();
}

void VoiceAllocator::ClearVoiceInputs(SynthVoice* pVoice)
{
  for(int i=0; i<kNumVoiceControlRamps; ++i)
  {
    pVoice->mInputs[i].Clear();
  }
}

void VoiceAllocator::AddVoice(SynthVoice* pVoice, uint8_t zone)
{
  if(!mVoicePtr)
  {
    mVoicePtr = pVoice;
    ClearVoiceInputs(mVoicePtr);
    mVoicePtr->mKey = -1;
    mVoicePtr->mZone = zone;

    // make ramp processors for the control ramps of the voice
    mVoiceRamps.reset(ControlRampProcessor::Create(mVoicePtr->mInputs));
    return;
  }

  throw std::runtime_error{"VoiceAllocator: only one voice is supported"};
}

bool VoiceAllocator::VoiceMatchesAddress(VoiceAddress addr) const
{
  if(!mVoicePtr)
    return false;

  if(addr.mZone != kAllZones && mVoicePtr->mZone != addr.mZone)
    return false;

  if(addr.mFlags & kVoiceAll)
    return true;

  if(addr.mChannel != kAllChannels && mVoicePtr->mChannel != addr.mChannel)
    return false;

  if(addr.mKey != kAllKeys && mVoicePtr->mKey != addr.mKey)
    return false;

  return true;
}

void VoiceAllocator::SendControlToVoiceInputs(bool voiceMatched, int ctlIdx, float val, int rampSamples)
{
  if(voiceMatched && mVoiceRamps)
    mVoiceRamps->at(ctlIdx).SetTarget(val, 0, rampSamples);
}

void VoiceAllocator::ProcessEvents(int blockSize, int64_t sampleTime)
{
  while(mInputQueue.ElementsAvailable())
  {
    VoiceInputEvent event;
    mInputQueue.Pop(event);
    const bool voiceMatched = VoiceMatchesAddress(event.mAddress);

    switch(event.mAction)
    {
      case kNoteOnAction:
      {
        NoteOn(event, sampleTime);
        break;
      }
      case kNoteOffAction:
      {
        if(event.mAddress.mFlags & kVoiceAll)
        {
          SoftKillVoice();
        }
        else
        {
          NoteOff(event);
        }
        break;
      }
      case kPitchBendAction:
      {
        SendControlToVoiceInputs(voiceMatched, kVoiceControlPitchBend, event.mValue, 1);
        break;
      }
      case kNullAction:
      default:
      {
        break;
      }
    }
  }

  // update control ramps and write voice control outputs
  if(mVoiceRamps)
  {
    for(int i=0; i<kNumVoiceControlRamps; ++i)
    {
      mVoiceRamps->at(i).Process(blockSize);
    }
  }
}

// start a single voice and set its current channel and key.
void VoiceAllocator::StartVoice(int channel, int key, float pitch, float velocity, int sampleOffset, int64_t sampleTime, bool retrig)
{
  if(!mVoicePtr || !mVoiceRamps)
    return;

  if(!retrig)
  {
    // add immediate sample-accurate change for trigger
    mVoiceRamps->at(kVoiceControlGate).SetTarget(velocity, sampleOffset, 1);
  }

  // update pitch immediately for the new note
  mVoiceRamps->at(kVoiceControlPitch).SetTarget(pitch, sampleOffset, 1);

  // set things directly in voice
  mVoicePtr->mLastTriggeredTime = sampleTime;
  mVoicePtr->mChannel = channel;
  mVoicePtr->mKey = key;
  mVoicePtr->mGain = 1.;

  // call voice's Trigger method
  mVoicePtr->Trigger(velocity, retrig);
}

void VoiceAllocator::StopVoice(int sampleOffset)
{
  if(!mVoicePtr || !mVoiceRamps)
    return;

  mVoiceRamps->at(kVoiceControlGate).SetTarget(0.0, sampleOffset, 1);
  mVoicePtr->mKey = -1;
}

void VoiceAllocator::SoftKillVoice()
{
  StopVoice(0);
}

void VoiceAllocator::HardKillVoice()
{
  SoftKillVoice();
  if(mVoicePtr)
    mVoicePtr->mGain = 0.;
}

void VoiceAllocator::NoteOn(VoiceInputEvent e, int64_t sampleTime)
{
  int channel = e.mAddress.mChannel;
  int key = e.mAddress.mKey;
  int offset = e.mSampleOffset;
  float velocity = e.mValue;
  float pitch = (key - 69.f) / 12.f;

  // TODO retrig / legato
  bool retrig = false;

  // Trigger the voice if its zone matches.
  if(VoiceMatchesAddress({e.mAddress.mZone, kAllChannels, kAllKeys, 0}))
    StartVoice(channel, key, pitch, velocity, offset, sampleTime, retrig);
}

void VoiceAllocator::NoteOff(VoiceInputEvent e)
{
  if(VoiceMatchesAddress(e.mAddress))
    StopVoice(e.mSampleOffset);
}

void VoiceAllocator::ProcessVoice(sample** inputs, sample** outputs, int nInputs, int nOutputs, int startIndex, int blockSize)
{
  if(mVoicePtr && mVoicePtr->GetBusy())
  {
    // TODO distribute processing across cores
    mVoicePtr->ProcessSamplesAccumulating(inputs, outputs, nInputs, nOutputs, startIndex, blockSize);
  }
}

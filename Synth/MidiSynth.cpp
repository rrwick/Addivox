/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
 */

#include "MidiSynth.h"

using namespace iplug;

MidiSynth::MidiSynth(int blockSize)
: mBlockSize(blockSize)
{
  for(size_t i = 0; i < mVelocityLUT.size(); ++i)
  {
    mVelocityLUT[i] = i / 127.f;
  }

  // initialize Channel states
  for(int i=0; i<16; ++i)
  {
    mChannelStates[i] = ChannelState{0};
    mChannelStates[i].pitchBendRange = kDefaultPitchBendRange;
  }
}

void MidiSynth::ClearVoiceInputs()
{
  if(!mVoicePtr)
    return;

  for(int i = 0; i < kNumVoiceControlRamps; ++i)
    mVoicePtr->mInputs[i].Clear();
}

bool MidiSynth::IsActiveChannel(uint8_t channel) const
{
  return mVoicePtr && mVoicePtr->mKey != kNoKey && mVoicePtr->mChannel == channel;
}

bool MidiSynth::IsActiveNote(uint8_t channel, uint8_t key) const
{
  return IsActiveChannel(channel) && mVoicePtr->mKey == key;
}

void MidiSynth::SendControlToVoiceInput(int ctlIdx, float val, int sampleOffset, int rampSamples)
{
  if(mVoiceRamps)
    mVoiceRamps->at(ctlIdx).SetTarget(val, sampleOffset, rampSamples);
}

void MidiSynth::StartVoice(int channel, int key, float pitch, float velocity, int sampleOffset, int64_t sampleTime, bool retrig)
{
  if(!mVoicePtr || !mVoiceRamps)
    return;

  if(!retrig)
    SendControlToVoiceInput(kVoiceControlGate, velocity, sampleOffset, 1);

  SendControlToVoiceInput(kVoiceControlPitch, pitch, sampleOffset, 1);
  mVoicePtr->mLastTriggeredTime = sampleTime;
  mVoicePtr->mChannel = channel;
  mVoicePtr->mKey = key;
  mVoicePtr->mGain = 1.;
  mVoicePtr->Trigger(velocity, retrig);
}

void MidiSynth::StopVoice(int sampleOffset)
{
  if(!mVoicePtr || !mVoiceRamps)
    return;

  SendControlToVoiceInput(kVoiceControlGate, 0.f, sampleOffset, 1);
  mVoicePtr->mKey = kNoKey;
}

void MidiSynth::KillVoice(int sampleOffset, bool hard)
{
  StopVoice(sampleOffset);

  if(hard && mVoicePtr)
    mVoicePtr->mGain = 0.;
}

void MidiSynth::AllNotesOff()
{
  KillVoice(0, false);
}

void MidiSynth::PitchBend(int channel, float value, int sampleOffset)
{
  if(IsActiveChannel(static_cast<uint8_t>(channel)))
    SendControlToVoiceInput(kVoiceControlPitchBend, value, sampleOffset, 1);
}

void MidiSynth::ProcessVoice(sample** inputs, sample** outputs, int nInputs, int nOutputs, int startIndex, int blockSize)
{
  if(mVoiceRamps)
  {
    for(int i = 0; i < kNumVoiceControlRamps; ++i)
      mVoiceRamps->at(i).Process(blockSize);
  }

  if(mVoicePtr && mVoicePtr->GetBusy())
  {
    // TODO distribute processing across cores
    mVoicePtr->ProcessSamplesAccumulating(inputs, outputs, nInputs, nOutputs, startIndex, blockSize);
  }
}

void MidiSynth::HandlePerformanceMessage(const IMidiMsg& msg, int blockStart, int64_t sampleTime)
{
  const int channel = msg.Channel();
  const int key = msg.NoteNumber();
  const int sampleOffset = msg.mOffset - blockStart;
  IMidiMsg::EStatusMsg status = msg.StatusMsg();

  switch (status)
  {
    case IMidiMsg::kNoteOn:
    {
      int v = Clip(msg.Velocity(), 0, 127);
      if(v == 0)
      {
        if(IsActiveNote(static_cast<uint8_t>(channel), static_cast<uint8_t>(key)))
          StopVoice(sampleOffset);
      }
      else
        StartVoice(channel, key, (key - 69.f) / 12.f, mVelocityLUT[v], sampleOffset, sampleTime, false);
      break;
    }
    case IMidiMsg::kNoteOff:
    {
      if(IsActiveNote(static_cast<uint8_t>(channel), static_cast<uint8_t>(key)))
        StopVoice(sampleOffset);
      break;
    }
    case IMidiMsg::kPitchWheel:
    {
      const float bendRange = mChannelStates[channel].pitchBendRange;
      const float bend = static_cast<float>(msg.PitchWheel()) * bendRange / 12.f;
      PitchBend(channel, bend, sampleOffset);
      break;
    }
    case IMidiMsg::kControlChange:
    {
      switch(msg.ControlChangeIdx())
      {
        case IMidiMsg::kAllNotesOff:
        {
          AllNotesOff();
          break;
        }
        default:
          break;
      }
      break;
    }
    default:
    {
      break;
    }
  }
}

void MidiSynth::SetChannelPitchBendRange(int channelParam, int rangeParam)
{
  const int channel = Clip(channelParam, 0, 15);
  const int range = Clip(rangeParam, 0, 96);
  mChannelStates[channel].pitchBendRange = range;
}

bool IsRPNMessage(IMidiMsg msg)
{
  if(msg.StatusMsg() != IMidiMsg::kControlChange) return false;
  int cc = msg.mData1;
  return(cc == 0x64)||(cc == 0x65)||(cc == 0x26)||(cc == 0x06);
}

void MidiSynth::HandleRPN(IMidiMsg msg)
{
  int channel = msg.Channel();
  ChannelState& state = mChannelStates[channel];

  uint8_t valueByte = msg.mData2;
  int param, value;
  switch (msg.mData1)
  {
    case 0x64:
      state.paramLSB = valueByte;
      state.valueMSB = state.valueLSB = 0xff;
      break;
    case 0x65:
      state.paramMSB = valueByte;
      state.valueMSB = state.valueLSB = 0xff;
      break;
    case 0x26:
      state.valueLSB = valueByte;
      break;
    case 0x06:
      // whenever the value MSB byte is received we constuct the value and take action on the RPN.
      // if only the MSB has been received, it is used as the entire value so the maximum possible value is 127.
      state.valueMSB = valueByte;
      param = ((state.paramMSB&0xFF) << 7) + (state.paramLSB&0xFF);
      if(state.valueLSB != 0xff)
      {
        value = ((state.valueMSB&0xFF) << 7) + (state.valueLSB&0xFF);
      }
      else
      {
        value = state.valueMSB&0xFF;
      }
      switch(param)
      {
        case 0: // RPN 0 : pitch bend range
          SetChannelPitchBendRange(channel, value);
          break;
        default:
          break;
      }
      break;

    default:
      break;
  }
}

bool MidiSynth::ProcessBlock(sample** inputs, sample** outputs, int nInputs, int nOutputs, int nFrames)
{
  assert(HasVoice());

  const auto* voice = GetVoice();
  if ((voice != nullptr && voice->GetBusy()) || !mMidiQueue.Empty())
  {
    int blockSize = mBlockSize;
    int samplesRemaining = nFrames;
    int startIndex = 0;

    while(samplesRemaining > 0)
    {
      if(samplesRemaining < blockSize)
        blockSize = samplesRemaining;

      while (!mMidiQueue.Empty())
      {
        IMidiMsg msg = mMidiQueue.Peek();

        // we assume the messages are in chronological order. If we find one later than the current block we are done.
        if (msg.mOffset > startIndex + blockSize) break;

        if(IsRPNMessage(msg))
        {
          HandleRPN(msg);
        }
        else
        {
          // message offset is relative to the start of this process block
          HandlePerformanceMessage(msg, startIndex, mSampleTime);
        }
        mMidiQueue.Remove();
      }

      ProcessVoice(inputs, outputs, nInputs, nOutputs, startIndex, blockSize);

      samplesRemaining -= blockSize;
      startIndex += blockSize;
      mSampleTime += blockSize;
    }

    mMidiQueue.Flush(nFrames);
  }
  else // empty block
  {
    return true;
  }

  return false; // made some noise
}

void MidiSynth::SetSampleRateAndBlockSize(double sampleRate, int blockSize)
{
  Reset();

  (void) sampleRate;
  mMidiQueue.Resize(blockSize);

  if (auto* voice = GetVoice())
    voice->SetSampleRateAndBlockSize(sampleRate, blockSize);
}

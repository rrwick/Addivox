/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
 */

#include "MidiSynth.h"

using namespace iplug;

MidiSynth::MidiSynth(int blockSize)
: mMidiQueue(blockSize)
{
  for(size_t i = 0; i < mVelocityLUT.size(); ++i)
  {
    mVelocityLUT[i] = i / 127.f;
  }

  mMidiState = MonoMidiState{0, 0, 0, 0xFF, 0xFF, kDefaultPitchBendRange, 0.f};
}

void MidiSynth::ClearVoiceControls()
{
  if(!mVoicePtr)
    return;

  mVoicePtr->mGate = 0.;
  mVoicePtr->mPitch = 0.;
  mVoicePtr->mPitchBend = 0.;
}

bool MidiSynth::IsActiveChannel(uint8_t channel) const
{
  return mVoicePtr && mVoicePtr->mKey != kNoKey && mMidiState.activeChannel == channel;
}

bool MidiSynth::IsActiveNote(uint8_t channel, uint8_t key) const
{
  return IsActiveChannel(channel) && mVoicePtr->mKey == key;
}

void MidiSynth::StartVoice(int channel, int key, float pitch, float velocity, int64_t sampleTime, bool retrig)
{
  if(!mVoicePtr)
    return;

  mVoicePtr->mGate = retrig ? mVoicePtr->mGate : velocity;
  mVoicePtr->mPitch = pitch;
  mVoicePtr->mPitchBend = mMidiState.currentPitchBend;
  mMidiState.activeChannel = static_cast<uint8_t>(channel);
  mVoicePtr->mLastTriggeredTime = sampleTime;
  mVoicePtr->mChannel = channel;
  mVoicePtr->mKey = key;
  mVoicePtr->mGain = 1.;
  mVoicePtr->Trigger(velocity, retrig);
}

void MidiSynth::StopVoice()
{
  if(!mVoicePtr)
    return;

  mVoicePtr->mGate = 0.;
  mVoicePtr->mKey = kNoKey;
}

void MidiSynth::KillVoice(bool hard)
{
  StopVoice();

  if(hard && mVoicePtr)
    mVoicePtr->mGain = 0.;
}

void MidiSynth::AllNotesOff()
{
  KillVoice(false);
}

void MidiSynth::PitchBend(int channel, float value)
{
  const uint8_t bendChannel = static_cast<uint8_t>(Clip(channel, 0, 15));

  if(mVoicePtr && mVoicePtr->mKey != kNoKey && !IsActiveChannel(bendChannel))
    return;

  mMidiState.activeChannel = bendChannel;
  mMidiState.currentPitchBend = value;

  if(mVoicePtr && mVoicePtr->mKey != kNoKey)
    mVoicePtr->mPitchBend = value;
}

void MidiSynth::ProcessVoice(sample** inputs, sample** outputs, int nInputs, int nOutputs, int startIndex, int numFrames)
{
  if(mVoicePtr && mVoicePtr->GetBusy())
  {
    // TODO distribute processing across cores
    mVoicePtr->ProcessSamplesAccumulating(inputs, outputs, nInputs, nOutputs, startIndex, numFrames);
  }
}

void MidiSynth::HandlePerformanceMessage(const IMidiMsg& msg, int64_t sampleTime)
{
  const int channel = msg.Channel();
  const int key = msg.NoteNumber();
  IMidiMsg::EStatusMsg status = msg.StatusMsg();

  switch (status)
  {
    case IMidiMsg::kNoteOn:
    {
      int v = Clip(msg.Velocity(), 0, 127);
      if(v == 0)
      {
        if(IsActiveNote(static_cast<uint8_t>(channel), static_cast<uint8_t>(key)))
          StopVoice();
      }
      else
        StartVoice(channel, key, (key - 69.f) / 12.f, mVelocityLUT[v], sampleTime, false);
      break;
    }
    case IMidiMsg::kNoteOff:
    {
      if(IsActiveNote(static_cast<uint8_t>(channel), static_cast<uint8_t>(key)))
        StopVoice();
      break;
    }
    case IMidiMsg::kPitchWheel:
    {
      const float bendRange = mMidiState.pitchBendRange;
      const float bend = static_cast<float>(msg.PitchWheel()) * bendRange / 12.f;
      PitchBend(channel, bend);
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

void MidiSynth::SetPitchBendRangeFromRPN(int channel, int rangeParam)
{
  mMidiState.activeChannel = static_cast<uint8_t>(Clip(channel, 0, 15));
  mMidiState.pitchBendRange = static_cast<uint8_t>(Clip(rangeParam, 0, 96));
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
  if(mVoicePtr && mVoicePtr->mKey != kNoKey && channel != mVoicePtr->mChannel)
    return;

  MonoMidiState& state = mMidiState;
  state.activeChannel = static_cast<uint8_t>(Clip(channel, 0, 15));

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
          SetPitchBendRangeFromRPN(channel, value);
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
    int startIndex = 0;

    while(startIndex < nFrames)
    {
      // Apply any events scheduled for the current sample before rendering audio.
      while(!mMidiQueue.Empty())
      {
        IMidiMsg msg = mMidiQueue.Peek();
        if(msg.mOffset > startIndex)
          break;

        if(IsRPNMessage(msg))
          HandleRPN(msg);
        else
          HandlePerformanceMessage(msg, mSampleTime);

        mMidiQueue.Remove();
      }

      int renderEnd = nFrames;
      if(!mMidiQueue.Empty())
      {
        renderEnd = Clip(static_cast<int>(mMidiQueue.Peek().mOffset), startIndex, nFrames);
      }

      const int numFrames = renderEnd - startIndex;
      if(numFrames <= 0)
        continue;

      ProcessVoice(inputs, outputs, nInputs, nOutputs, startIndex, numFrames);
      startIndex = renderEnd;
      mSampleTime += numFrames;
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

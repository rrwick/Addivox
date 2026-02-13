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
  for(int i=0; i<128; i++)
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

VoiceInputEvent MidiSynth::MidiMessageToEvent(const IMidiMsg& msg)
{
  VoiceInputEvent event{};

  IMidiMsg::EStatusMsg status = msg.StatusMsg();
  event.mSampleOffset = msg.mOffset;
  event.mAddress.mChannel = msg.Channel();
  event.mAddress.mKey = msg.NoteNumber();

  switch (status)
  {
    case IMidiMsg::kNoteOn:
    {
      int v = Clip(msg.Velocity(), 0, 127);
      event.mAction = (v == 0) ? kNoteOffAction : kNoteOnAction;
      event.mValue = mVelocityLUT[v];
      break;
    }
    case IMidiMsg::kNoteOff:
    {
      int v = Clip(msg.Velocity(), 0, 127);
      event.mAction = kNoteOffAction;
      event.mValue = mVelocityLUT[v];
      break;
    }
    case IMidiMsg::kPitchWheel:
    {
      event.mAction = kPitchBendAction;
      float bendRange = mChannelStates[event.mAddress.mChannel].pitchBendRange;
      event.mValue = static_cast<float>(msg.PitchWheel()) * bendRange / 12.f;
      break;
    }
    case IMidiMsg::kControlChange:
    {
      switch(msg.ControlChangeIdx())
      {
        case IMidiMsg::kAllNotesOff:
        {
          event.mAddress.mFlags = kVoiceAll;
          event.mAction = kNoteOffAction;
          event.mValue = 0.f;
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

  return event;
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
          // send performance messages to the voice allocator
          // message offset is relative to the start of this processSamples() block
          msg.mOffset -= startIndex;
          mVoiceAllocator.AddEvent(MidiMessageToEvent(msg));
        }
        mMidiQueue.Remove();
      }

      mVoiceAllocator.ProcessEvents(blockSize, mSampleTime);
      mVoiceAllocator.ProcessVoice(inputs, outputs, nInputs, nOutputs, startIndex, blockSize);

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

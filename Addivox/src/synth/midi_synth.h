#pragma once

/**
 * @file
 * @copydoc MidiSynth
 */

#include <algorithm>
#include <stdint.h>

#include "IPlugConstants.h"
#include "IPlugMidi.h"
#include "blip_guard.h"

BEGIN_IPLUG_NAMESPACE

/** A monophonic synthesiser base class that owns a concrete voice type. */
template <typename VoiceT>
class MidiSynth
{
public:
  /** This defines the size in samples of a single block of processing that will be done by the synth. */
  static constexpr int kDefaultBlockSize = 32;
  static constexpr int kDefaultPitchBendRange = 2;

public:
  MidiSynth(int blockSize = kDefaultBlockSize)
  : mMidiQueue(blockSize)
  {
    mMidiState = MonoMidiState{0, 0, 0xFF, 0xFF, kDefaultPitchBendRange, 0.0, 1.0, 0.0};
    ClearVoiceControls();
  }

  MidiSynth(const MidiSynth&) = delete;
  MidiSynth& operator=(const MidiSynth&) = delete;

  void Reset()
  {
    mMidiState.currentPitchBend = 0.0;
    mMidiState.currentBreath = 1.0;
    mMidiState.currentPortamento = 0.0;
    mTargetChannel = 0;
    mTargetKey = kNoKey;
    mOutputKey = kNoKey;
    StopVoice();
    mBlipGuard.ResetRuntime();
    ClearVoiceControls();
  }

  void SetSampleRateAndBlockSize(double sampleRate, int blockSize)
  {
    Reset();
    mMidiQueue.Resize(blockSize);
    mVoice.SetSampleRate(sampleRate);
    mBlipGuard.SetSampleRate(sampleRate);
  }

  /** Set the pitch bend range in semitones for the active mono channel state. */
  void SetPitchBendRange(int pitchBendRange)
  {
    mMidiState.pitchBendRange = static_cast<uint8_t>(Clip(pitchBendRange, 0, 96));
  }

  void AddMidiMsgToQueue(const IMidiMsg& msg)
  {
    mMidiQueue.Add(msg);
  }

  void SetBlipGuardDelayMs(double delayMs)
  {
    mBlipGuard.SetDelayMs(delayMs);
    ApplyPendingOutputChangeIfReady();
  }

  void SetBlipGuardIntervalSemitones(int intervalSemitones)
  {
    mBlipGuard.SetIntervalSemitones(intervalSemitones);
    ApplyPendingOutputChangeIfReady();
  }

  /** Processes a block of audio samples
   * @param outputs Pointer to output Arrays
   * @param nFrames The number of sample frames to process */
  void ProcessBlock(sample** outputs, int nFrames)
  {
    if(!HasPendingWork())
    {
      AdvanceTime(nFrames);
      return;
    }

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
          HandlePerformanceMessage(msg);

        mMidiQueue.Remove();
      }

      ApplyPendingOutputChangeIfReady();

      int renderEnd = nFrames;
      if(!mMidiQueue.Empty())
        renderEnd = Clip(static_cast<int>(mMidiQueue.Peek().mOffset), startIndex, nFrames);

      if(mBlipGuard.HasPending())
        renderEnd = std::min(renderEnd, startIndex + mBlipGuard.GetFramesUntilOutputChange());

      const int numFrames = renderEnd - startIndex;
      if(numFrames <= 0)
        continue;

      if(mVoice.IsActive())
        mVoice.ProcessSamplesAccumulating(outputs, startIndex, numFrames);

      AdvanceTime(numFrames);
      startIndex = renderEnd;
    }

    mMidiQueue.Flush(nFrames);
  }

  const VoiceT& GetVoice() const
  {
    return mVoice;
  }

  VoiceT& GetVoice()
  {
    return mVoice;
  }

private:
  enum class OutputNoteSource
  {
    Immediate,
    Delayed
  };

  struct MonoMidiState
  {
    uint8_t paramMSB;
    uint8_t paramLSB;
    uint8_t valueMSB;
    uint8_t valueLSB;
    uint8_t pitchBendRange; // in semitones
    double currentPitchBend;
    double currentBreath;
    double currentPortamento;
  };

  static bool IsRPNMessage(const IMidiMsg& msg)
  {
    if(msg.StatusMsg() != IMidiMsg::kControlChange)
      return false;

    const int cc = msg.mData1;
    return (cc == 0x64) || (cc == 0x65) || (cc == 0x26) || (cc == 0x06);
  }

  void HandlePerformanceMessage(const IMidiMsg& msg)
  {
    const int channel = msg.Channel();
    const int key = msg.NoteNumber();
    const IMidiMsg::EStatusMsg status = msg.StatusMsg();

    switch(status)
    {
      case IMidiMsg::kNoteOn:
      {
        if(msg.Velocity() == 0)
          HandleNoteOff(channel, key);
        else
          HandleNoteOn(channel, key);
        break;
      }
      case IMidiMsg::kNoteOff:
      {
        HandleNoteOff(channel, key);
        break;
      }
      case IMidiMsg::kPitchWheel:
      {
        const double bendRange = static_cast<double>(mMidiState.pitchBendRange);
        const double bend = static_cast<double>(msg.PitchWheel()) * bendRange;
        PitchBend(channel, bend);
        break;
      }
      case IMidiMsg::kControlChange:
      {
        switch(msg.ControlChangeIdx())
        {
          case IMidiMsg::kBreathController:
            Breath(channel, static_cast<double>(msg.ControlChange(IMidiMsg::kBreathController)));
            break;
          case IMidiMsg::kPortamentoTime:
            Portamento(channel, static_cast<double>(msg.ControlChange(IMidiMsg::kPortamentoTime)));
            break;
          case IMidiMsg::kAllNotesOff:
            StopVoice();
            break;
          default:
            break;
        }
        break;
      }
      default:
        break;
    }
  }

  void HandleRPN(const IMidiMsg& msg)
  {
    const int channel = msg.Channel();
    if(mTargetKey != kNoKey && channel != mTargetChannel)
      return;

    MonoMidiState& state = mMidiState;

    const uint8_t valueByte = msg.mData2;
    int param = 0;
    int value = 0;

    switch(msg.mData1)
    {
      case 0x64:
        state.paramLSB = valueByte;
        state.valueMSB = state.valueLSB = 0xFF;
        break;
      case 0x65:
        state.paramMSB = valueByte;
        state.valueMSB = state.valueLSB = 0xFF;
        break;
      case 0x26:
        state.valueLSB = valueByte;
        break;
      case 0x06:
      {
        // When value MSB arrives we construct and apply the RPN value.
        state.valueMSB = valueByte;
        param = ((state.paramMSB & 0xFF) << 7) + (state.paramLSB & 0xFF);
        if(state.valueLSB != 0xFF)
          value = ((state.valueMSB & 0xFF) << 7) + (state.valueLSB & 0xFF);
        else
          value = state.valueMSB & 0xFF;

        if(param == 0) // RPN 0 : pitch bend range
          mMidiState.pitchBendRange = static_cast<uint8_t>(Clip(value, 0, 96));

        break;
      }
      default:
        break;
    }
  }

  void StartVoice(uint8_t channel, uint8_t key, OutputNoteSource outputNoteSource = OutputNoteSource::Immediate)
  {
    mVoice.SetPortamentoControl(mMidiState.currentPortamento);
    mVoice.Start(static_cast<double>(key), mMidiState.currentPitchBend, mMidiState.currentBreath);
    mTargetChannel = channel;
    mTargetKey = key;
    mOutputKey = mTargetKey;
    if(outputNoteSource == OutputNoteSource::Delayed)
      mBlipGuard.NotifyDelayedOutputNote(mOutputKey);
    else
      mBlipGuard.NotifyImmediateOutputNote(mOutputKey);
  }

  void StopVoice()
  {
    mBreathGateOpen = false;
    ClearOutputContext();
    mVoice.Stop();
    mTargetKey = kNoKey;
  }

  void StopOutputClearContext()
  {
    ClearOutputContext();
    mVoice.Stop();
  }

  void StopOutputKeepRecentContext()
  {
    mVoice.Stop();
  }

  void HandleNoteOn(int channel, int key)
  {
    const uint8_t clippedChannel = static_cast<uint8_t>(Clip(channel, 0, 15));
    const uint8_t clippedKey = static_cast<uint8_t>(Clip(key, 0, 127));
    mTargetChannel = clippedChannel;
    mTargetKey = clippedKey;

    if(mMidiState.currentBreath < kBreathGateOnThreshold)
    {
      mBreathGateOpen = false;
      StopOutputClearContext();
      return;
    }

    mBreathGateOpen = true;

    if(!HasRecentOutputContext())
    {
      StartVoice(clippedChannel, clippedKey);
      return;
    }

    const bool shouldOutputNow =
      mBlipGuard.HandleNoteOn(mOutputKey, HasCurrentOutput(), clippedKey) == BlipGuard::NoteChangeAction::kOutputNow;
    if(shouldOutputNow && (mOutputKey != clippedKey || !mVoice.IsActive()))
      StartVoice(clippedChannel, clippedKey);
  }

  void HandleNoteOff(int channel, int key)
  {
    const uint8_t clippedChannel = static_cast<uint8_t>(Clip(channel, 0, 15));
    const uint8_t clippedKey = static_cast<uint8_t>(Clip(key, 0, 127));

    if(!IsRelevantNoteChannel(clippedChannel))
      return;

    // Pending note-offs must cancel the pending candidate before they are allowed
    // to silence the currently sounding note.
    if(CancelPendingNoteIfMatched(clippedKey))
      return;

    if(mOutputKey == clippedKey)
    {
      StopOutputKeepRecentContext();
      ClearTargetIfMatched(clippedKey);
      return;
    }

    ClearTargetIfMatched(clippedKey);
  }

  void ApplyPendingOutputChangeIfReady()
  {
    if(!mBlipGuard.HasPending())
      return;

    if(!mBreathGateOpen)
    {
      ClearOutputContext();
      return;
    }

    if(mTargetKey == kNoKey)
    {
      mBlipGuard.CancelPending();
      return;
    }

    const auto resolution = mBlipGuard.GetPendingResolution(mOutputKey, HasCurrentOutput());
    if(resolution == BlipGuard::PendingResolution::kNone)
      return;

    const uint8_t targetKey = mBlipGuard.PendingInputNote();
    if(mOutputKey != targetKey || !mVoice.IsActive())
    {
      const OutputNoteSource outputNoteSource =
        (resolution == BlipGuard::PendingResolution::kTimeout) ? OutputNoteSource::Delayed : OutputNoteSource::Immediate;
      StartVoice(mTargetChannel, targetKey, outputNoteSource);
    }
    else
      mBlipGuard.CancelPending();
  }

  void AdvanceTime(int numFrames)
  {
    mBlipGuard.Advance(numFrames);

    if(!mVoice.IsActive() && !mBlipGuard.HasPending() && !mBlipGuard.HasRecentTrustedContext())
      mOutputKey = kNoKey;
  }

  void PitchBend(int channel, double value)
  {
    const uint8_t bendChannel = static_cast<uint8_t>(Clip(channel, 0, 15));

    if(mTargetKey != kNoKey && !IsTargetChannel(bendChannel))
      return;

    mMidiState.currentPitchBend = value;

    if(mTargetKey != kNoKey)
      mVoice.SetPitchBend(value);
  }

  void Breath(int channel, double value)
  {
    const uint8_t breathChannel = static_cast<uint8_t>(Clip(channel, 0, 15));

    if(mTargetKey != kNoKey && !IsTargetChannel(breathChannel))
      return;

    mMidiState.currentBreath = value;

    if(mTargetKey == kNoKey)
    {
      if(value <= kBreathGateOffThreshold)
      {
        mBreathGateOpen = false;
        StopOutputClearContext();
      }
      return;
    }

    if(mBreathGateOpen)
    {
      if(value <= kBreathGateOffThreshold)
      {
        mBreathGateOpen = false;
        StopOutputClearContext();
      }
      else
      {
        mVoice.SetBreath(value);
      }
    }
    else if(value >= kBreathGateOnThreshold)
    {
      mBreathGateOpen = true;
      StartVoice(mTargetChannel, mTargetKey);
    }
  }

  void Portamento(int channel, double value)
  {
    const uint8_t portamentoChannel = static_cast<uint8_t>(Clip(channel, 0, 15));

    if(mTargetKey != kNoKey && !IsTargetChannel(portamentoChannel))
      return;

    mMidiState.currentPortamento = Clip(value, 0.0, 1.0);
    mVoice.SetPortamentoControl(mMidiState.currentPortamento);
  }

  bool HasPendingWork() const
  {
    return mVoice.IsActive() || !mMidiQueue.Empty() || mBlipGuard.HasPending();
  }

  bool HasTrackedNoteState() const
  {
    return mTargetKey != kNoKey || mOutputKey != kNoKey || mBlipGuard.HasPending();
  }

  bool IsRelevantNoteChannel(uint8_t channel) const
  {
    return !HasTrackedNoteState() || channel == mTargetChannel;
  }

  bool IsTargetChannel(uint8_t channel) const
  {
    return mTargetKey != kNoKey && mTargetChannel == channel;
  }

  bool HasCurrentOutput() const
  {
    return mVoice.IsActive() && mOutputKey != kNoKey;
  }

  bool HasRecentOutputContext() const
  {
    return mOutputKey != kNoKey && (HasCurrentOutput() || mBlipGuard.HasRecentTrustedContext());
  }

  void ClearOutputContext()
  {
    mOutputKey = kNoKey;
    mBlipGuard.ClearWindow();
  }

  void ClearTargetIfMatched(uint8_t key)
  {
    if(mTargetKey == key)
      mTargetKey = kNoKey;
  }

  bool CancelPendingNoteIfMatched(uint8_t key)
  {
    if(!mBlipGuard.HasPending() || key != mBlipGuard.PendingInputNote())
      return false;

    mBlipGuard.CancelPending();
    ClearTargetIfMatched(key);
    return true;
  }

  void ClearVoiceControls()
  {
    mVoice.Clear();
    mVoice.SetPortamentoControl(mMidiState.currentPortamento);
  }

  static constexpr uint8_t kNoKey = static_cast<uint8_t>(-1);
  static constexpr double kBreathGateOnThreshold = 2.0 / 127.0;
  static constexpr double kBreathGateOffThreshold = 0.0;

  IMidiQueue mMidiQueue;
  MonoMidiState mMidiState{};
  VoiceT mVoice{};
  uint8_t mTargetChannel{0};
  // Latest input note we are targeting. This can differ from mOutputKey while a
  // new note is still waiting behind the blip guard.
  uint8_t mTargetKey{kNoKey};
  // Current sounding note, or the most recent sounding note while we keep a
  // short post-release context window for fast note transitions.
  uint8_t mOutputKey{kNoKey};
  bool mBreathGateOpen{false};
  BlipGuard mBlipGuard{};
};

END_IPLUG_NAMESPACE

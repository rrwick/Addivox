#pragma once

/**
 * @file
 * @copydoc MidiSynth
 */

#include <array>
#include <stdint.h>

#include "../demo_mode.h"
#include "../midi/breath_control.h"
#include "IPlugConstants.h"
#include "IPlugMidi.h"

BEGIN_IPLUG_NAMESPACE

/** A monophonic synthesiser base class that owns a concrete voice type. */
template <typename VoiceT> class MidiSynth {
public:
  /** This defines the size in samples of a single block of processing that will
   * be done by the synth. */
  static constexpr int kDefaultBlockSize = 32;
  static constexpr int kDefaultPitchBendRange = 2;

public:
  MidiSynth(int blockSize = kDefaultBlockSize) : mMidiQueue(blockSize) {
    mMidiState = MonoMidiState{0, 0, 0xFF, 0xFF, kDefaultPitchBendRange, 0.0, 1.0, 0.0};
    mBreathCCSources.fill(kDefaultBreathCCSource);
    mBreathCCInputTracker.Reset();
    ClearVoiceControls();
  }

  MidiSynth(const MidiSynth&) = delete;
  MidiSynth& operator=(const MidiSynth&) = delete;

  void Reset() {
    mMidiState.currentPitchBend = 0.0;
    mMidiState.currentBreath = 1.0;
    mMidiState.currentPortamento = 0.0;
    mBreathCCInputTracker.Reset();
    mActiveChannel = 0;
    mActiveKey = kNoKey;
    StopVoice();
    ClearVoiceControls();
  }

  void SetSampleRateAndBlockSize(double sampleRate, int blockSize) {
    Reset();
    mMidiQueue.Resize(blockSize);
    mVoice.SetSampleRate(sampleRate);
  }

  /** Set the pitch bend range in semitones for the active mono channel state.
   */
  void SetPitchBendRange(int pitchBendRange) { mMidiState.pitchBendRange = static_cast<uint8_t>(Clip(pitchBendRange, 0, 96)); }

  void AddMidiMsgToQueue(const IMidiMsg& msg) { mMidiQueue.Add(msg); }

  void SetBreathCCSource(BreathCCSource source) {
    mBreathCCSources.fill(source);
    mBreathCCInputTracker.Reset();
  }

  void SetBreathCCSourceForChannel(int channel, BreathCCSource source) {
    const std::size_t index = static_cast<std::size_t>(Clip(channel, 0, 15));
    mBreathCCSources[index] = source;
    mBreathCCInputTracker.ResetChannel(static_cast<int>(index));
  }

  /** Processes a block of audio samples
   * @param outputs Pointer to output Arrays
   * @param nFrames The number of sample frames to process */
  void ProcessBlock(sample** outputs, int nFrames) {
    if (mVoice.IsActive() || !mMidiQueue.Empty()) {
      int startIndex = 0;

      while (startIndex < nFrames) {
        // Apply any events scheduled for the current sample before rendering audio.
        while (!mMidiQueue.Empty()) {
          IMidiMsg msg = mMidiQueue.Peek();
          if (msg.mOffset > startIndex) break;

          if (IsRPNMessage(msg)) HandleRPN(msg);
          else
            HandlePerformanceMessage(msg);

          mMidiQueue.Remove();
        }

        int renderEnd = nFrames;
        if (!mMidiQueue.Empty()) renderEnd = Clip(static_cast<int>(mMidiQueue.Peek().mOffset), startIndex, nFrames);

        const int numFrames = renderEnd - startIndex;
        if (numFrames <= 0) continue;

        if (mVoice.IsActive()) mVoice.ProcessSamplesAccumulating(outputs, startIndex, numFrames);
        startIndex = renderEnd;
      }

      mMidiQueue.Flush(nFrames);
    }
  }

  const VoiceT& GetVoice() const { return mVoice; }

  VoiceT& GetVoice() { return mVoice; }

private:
  struct MonoMidiState {
    uint8_t paramMSB;
    uint8_t paramLSB;
    uint8_t valueMSB;
    uint8_t valueLSB;
    uint8_t pitchBendRange; // in semitones
    double currentPitchBend;
    double currentBreath;
    double currentPortamento;
  };

  static bool IsRPNMessage(const IMidiMsg& msg) {
    if (msg.StatusMsg() != IMidiMsg::kControlChange) return false;

    const int cc = msg.mData1;
    return (cc == 0x64) || (cc == 0x65) || (cc == 0x26) || (cc == 0x06);
  }

  void HandlePerformanceMessage(const IMidiMsg& msg) {
    const int channel = msg.Channel();
    const int key = msg.NoteNumber();
    const IMidiMsg::EStatusMsg status = msg.StatusMsg();

    switch (status) {
    case IMidiMsg::kNoteOn: {
      if (msg.Velocity() == 0) {
        if (IsActiveNote(static_cast<uint8_t>(channel), static_cast<uint8_t>(key))) StopVoice();
      } else {
#if ADDIVOX_DEMO
        if (!addivox_demo::IsWhiteKeyMidiNote(key)) break;
#endif

        mActiveChannel = static_cast<uint8_t>(Clip(channel, 0, 15));
        mActiveKey = static_cast<uint8_t>(Clip(key, 0, 127));

        if (mMidiState.currentBreath >= kBreathGateOnThreshold) {
          mBreathGateOpen = true;
          StartVoice(channel, key, static_cast<double>(key));
        } else {
          // Keep note assignment active so breath can trigger the note later.
          mBreathGateOpen = false;
          mVoice.Stop();
        }
      }
      break;
    }
    case IMidiMsg::kNoteOff: {
      if (IsActiveNote(static_cast<uint8_t>(channel), static_cast<uint8_t>(key))) StopVoice();
      break;
    }
    case IMidiMsg::kPitchWheel: {
      const double bendRange = static_cast<double>(mMidiState.pitchBendRange);
      const double bend = static_cast<double>(msg.PitchWheel()) * bendRange;
      PitchBend(channel, bend);
      break;
    }
    case IMidiMsg::kControlChange: {
      const BreathCCValueUpdate breathUpdate = mBreathCCInputTracker.HandleMessage(mBreathCCSources[static_cast<std::size_t>(Clip(channel, 0, 15))], msg);
      if (breathUpdate.consumed) {
        if (breathUpdate.hasValue) Breath(channel, breathUpdate.value);
        break;
      }

      switch (msg.ControlChangeIdx()) {
      case IMidiMsg::kPortamentoTime: Portamento(channel, static_cast<double>(msg.ControlChange(IMidiMsg::kPortamentoTime))); break;
      case IMidiMsg::kAllNotesOff:    StopVoice(); break;
      default:                        break;
      }
      break;
    }
    default: break;
    }
  }

  void HandleRPN(const IMidiMsg& msg) {
    const int channel = msg.Channel();
    if (mActiveKey != kNoKey && channel != mActiveChannel) return;

    MonoMidiState& state = mMidiState;

    const uint8_t valueByte = msg.mData2;
    int param = 0;
    int value = 0;

    switch (msg.mData1) {
    case 0x64:
      state.paramLSB = valueByte;
      state.valueMSB = state.valueLSB = 0xFF;
      break;
    case 0x65:
      state.paramMSB = valueByte;
      state.valueMSB = state.valueLSB = 0xFF;
      break;
    case 0x26: state.valueLSB = valueByte; break;
    case 0x06: {
      // When value MSB arrives we construct and apply the RPN value.
      state.valueMSB = valueByte;
      param = ((state.paramMSB & 0xFF) << 7) + (state.paramLSB & 0xFF);
      if (state.valueLSB != 0xFF) value = ((state.valueMSB & 0xFF) << 7) + (state.valueLSB & 0xFF);
      else
        value = state.valueMSB & 0xFF;

      if (param == 0) // RPN 0 : pitch bend range
        mMidiState.pitchBendRange = static_cast<uint8_t>(Clip(value, 0, 96));

      break;
    }
    default: break;
    }
  }

  void StartVoice(int channel, int key, double pitch) {
    mVoice.SetPortamentoControl(mMidiState.currentPortamento);
    mVoice.Start(pitch, mMidiState.currentPitchBend, mMidiState.currentBreath);
    mActiveChannel = static_cast<uint8_t>(channel);
    mActiveKey = static_cast<uint8_t>(key);
  }

  void StopVoice() {
    mBreathGateOpen = false;
    mVoice.Stop();
    mActiveKey = kNoKey;
  }

  void PitchBend(int channel, double value) {
    const uint8_t bendChannel = static_cast<uint8_t>(Clip(channel, 0, 15));

    if (mActiveKey != kNoKey && !IsActiveChannel(bendChannel)) return;

    mMidiState.currentPitchBend = value;

    if (mActiveKey != kNoKey) mVoice.SetPitchBend(value);
  }

  void Breath(int channel, double value) {
    const uint8_t breathChannel = static_cast<uint8_t>(Clip(channel, 0, 15));

    if (mActiveKey != kNoKey && !IsActiveChannel(breathChannel)) return;

    mMidiState.currentBreath = value;

    if (mActiveKey == kNoKey) return;

    if (mBreathGateOpen) {
      if (value <= kBreathGateOffThreshold) {
        mBreathGateOpen = false;
        mVoice.Stop();
      } else {
        mVoice.SetBreath(value);
      }
    } else if (value >= kBreathGateOnThreshold) {
      mBreathGateOpen = true;
      StartVoice(static_cast<int>(mActiveChannel), static_cast<int>(mActiveKey), static_cast<double>(mActiveKey));
    }
  }

  void Portamento(int channel, double value) {
    const uint8_t portamentoChannel = static_cast<uint8_t>(Clip(channel, 0, 15));

    if (mActiveKey != kNoKey && !IsActiveChannel(portamentoChannel)) return;

    mMidiState.currentPortamento = Clip(value, 0.0, 1.0);
    mVoice.SetPortamentoControl(mMidiState.currentPortamento);
  }

  bool IsActiveChannel(uint8_t channel) const { return mActiveKey != kNoKey && mActiveChannel == channel; }

  bool IsActiveNote(uint8_t channel, uint8_t key) const { return IsActiveChannel(channel) && mActiveKey == key; }

  void ClearVoiceControls() {
    mVoice.Clear();
    mVoice.SetPortamentoControl(mMidiState.currentPortamento);
  }

  static constexpr uint8_t kNoKey = static_cast<uint8_t>(-1);
  static constexpr double kBreathGateOnThreshold = 2.0 / 127.0;
  static constexpr double kBreathGateOffThreshold = 0.0;

  IMidiQueue mMidiQueue;
  MonoMidiState mMidiState{};
  std::array<BreathCCSource, 16> mBreathCCSources{};
  BreathCCInputTracker mBreathCCInputTracker{};
  VoiceT mVoice{};
  uint8_t mActiveChannel{0};
  uint8_t mActiveKey{kNoKey};
  bool mBreathGateOpen{false};
};

END_IPLUG_NAMESPACE

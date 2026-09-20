#pragma once

#include <array>
#include <cstdint>

#include "../demo_mode.h"
#include "../midi/breath_control.h"
#include "IPlugConstants.h"
#include "IPlugMidi.h"

BEGIN_IPLUG_NAMESPACE

// A monophonic synthesiser that owns a concrete voice type.
template <typename VoiceT> class MidiSynth {
public:
  static constexpr int kDefaultBlockSize = 32;
  static constexpr int kDefaultPitchBendRange = 2;

  MidiSynth(int blockSize = kDefaultBlockSize) : mMidiQueue(blockSize) {
    mBreathCCSources.fill(kDefaultBreathCCSource);
    mBreathCCInputTracker.Reset();
    ClearVoiceControls();
  }

  MidiSynth(const MidiSynth&) = delete;
  MidiSynth& operator=(const MidiSynth&) = delete;

  void Reset() {
    mMidiQueue.Clear();
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

  void SetPitchBendRange(int pitchBendRange) {
    mMidiState.pitchBendRange = static_cast<uint8_t>(Clip(pitchBendRange, 0, 96));

    // A range of zero must also cancel any bend that is already sounding, not just neutralize future pitch wheel messages.
    if (mMidiState.pitchBendRange == 0) PitchBend(static_cast<int>(mActiveChannel), 0.0);
  }

  void AddMidiMsgToQueue(const IMidiMsg& msg) { mMidiQueue.Add(msg); }

  void SetPortamentoCC(int controller) {
    const int sanitizedController = controller == 65 ? 65 : 5;
    if (mPortamentoCC == sanitizedController) return;
    mPortamentoCC = sanitizedController;
    Portamento(mActiveChannel, 0.0);
  }

  void SetBreathCCSource(BreathCCSource source) {
    mBreathCCSources.fill(source);
    mBreathCCInputTracker.Reset();
  }

  void SetBreathCCSourceForChannel(int channel, BreathCCSource source) {
    const std::size_t index = static_cast<std::size_t>(Clip(channel, 0, 15));
    mBreathCCSources[index] = source;
    mBreathCCInputTracker.ResetChannel(static_cast<int>(index));
  }

  void ProcessBlock(sample** outputs, int nFrames) {
    if (!mVoice.IsActive() && mMidiQueue.Empty()) return;

    int startIndex = 0;
    while (startIndex < nFrames) {
      // Apply events at the current sample before rendering audio.
      while (!mMidiQueue.Empty() && mMidiQueue.Peek().mOffset <= startIndex) {
        HandleMidiMessage(mMidiQueue.Peek());
        mMidiQueue.Remove();
      }

      const int renderEnd = mMidiQueue.Empty() ? nFrames : Clip(static_cast<int>(mMidiQueue.Peek().mOffset), startIndex, nFrames);
      if (mVoice.IsActive()) mVoice.ProcessSamplesAccumulating(outputs, startIndex, renderEnd - startIndex);
      startIndex = renderEnd;
    }

    mMidiQueue.Flush(nFrames);
  }

  const VoiceT& GetVoice() const { return mVoice; }

  VoiceT& GetVoice() { return mVoice; }

private:
  struct MonoMidiState {
    uint8_t paramMSB{0x7F};
    uint8_t paramLSB{0x7F};
    uint8_t pitchBendRange{kDefaultPitchBendRange}; // in semitones
    double currentPitchBend{0.0};
    double currentBreath{1.0};
    double currentPortamento{0.0};
  };

  void HandleMidiMessage(const IMidiMsg& msg) {
    switch (msg.StatusMsg()) {
    case IMidiMsg::kNoteOn: HandleNoteOn(msg); break;
    case IMidiMsg::kNoteOff:
      if (IsActiveNote(msg.Channel(), msg.NoteNumber())) StopVoice();
      break;
    case IMidiMsg::kPitchWheel:    PitchBend(msg.Channel(), static_cast<double>(msg.PitchWheel()) * mMidiState.pitchBendRange); break;
    case IMidiMsg::kControlChange: HandleControlChange(msg); break;
    default:                       break;
    }
  }

  void HandleNoteOn(const IMidiMsg& msg) {
    const int channel = msg.Channel();
    const int key = msg.NoteNumber();
    if (msg.Velocity() == 0) {
      if (IsActiveNote(channel, key)) StopVoice();
      return;
    }
#if ADDIVOX_DEMO
    if (!addivox_demo::IsWhiteKeyMidiNote(key)) return;
#endif

    mActiveChannel = static_cast<uint8_t>(Clip(channel, 0, 15));
    mActiveKey = static_cast<uint8_t>(Clip(key, 0, 127));
    mBreathGateOpen = mMidiState.currentBreath >= kBreathGateOnThreshold;
    if (mBreathGateOpen) StartVoice(channel, key);
    else
      mVoice.Stop(); // Keep the note assigned so breath can trigger it later.
  }

  void HandleControlChange(const IMidiMsg& msg) {
    switch (msg.mData1) {
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x26:
    case 0x06: HandleRPN(msg); return;
    default:   break;
    }

    const int channel = msg.Channel();
    const auto breathUpdate = mBreathCCInputTracker.HandleMessage(mBreathCCSources[static_cast<std::size_t>(Clip(channel, 0, 15))], msg);
    if (breathUpdate.consumed) {
      if (breathUpdate.hasValue) Breath(channel, breathUpdate.value);
      return;
    }

    switch (msg.mData1) {
    case IMidiMsg::kPortamentoTime:
    case IMidiMsg::kPortamentoOnOff:
      if (msg.mData1 == mPortamentoCC) Portamento(channel, mPortamentoCC == 65 ? (msg.mData2 >= 64 ? 1.0 : 0.0) : msg.mData2 / 127.0);
      break;
    case 120: // All Sound Off
    case IMidiMsg::kAllNotesOff: StopVoice(); break;
    default:                     break;
    }
  }

  void HandleRPN(const IMidiMsg& msg) {
    if (!AcceptsChannel(msg.Channel())) return;

    switch (msg.mData1) {
    case 0x62: // Selecting an NRPN deselects the RPN, preventing NRPN data from changing the pitch bend range.
    case 0x63: mMidiState.paramMSB = mMidiState.paramLSB = 0x7F; break;
    case 0x64: mMidiState.paramLSB = msg.mData2; break;
    case 0x65: mMidiState.paramMSB = msg.mData2; break;
    case 0x06:
      // RPN 0 is pitch bend range: MSB is semitones; the cents LSB is ignored.
      if (mMidiState.paramMSB == 0 && mMidiState.paramLSB == 0) SetPitchBendRange(msg.mData2 & 0x7F);
      break;
    default: break;
    }
  }

  void StartVoice(int channel, int key) {
    mVoice.SetPortamentoControl(mMidiState.currentPortamento);
    mVoice.Start(static_cast<double>(key), mMidiState.currentPitchBend, mMidiState.currentBreath);
    mActiveChannel = static_cast<uint8_t>(channel);
    mActiveKey = static_cast<uint8_t>(key);
  }

  void StopVoice() {
    mBreathGateOpen = false;
    mVoice.Stop();
    mActiveKey = kNoKey;
  }

  void PitchBend(int channel, double value) {
    if (!AcceptsChannel(channel)) return;

    mMidiState.currentPitchBend = value;

    if (mActiveKey != kNoKey) mVoice.SetPitchBend(value);
  }

  void Breath(int channel, double value) {
    if (!AcceptsChannel(channel)) return;

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
      StartVoice(mActiveChannel, mActiveKey);
    }
  }

  void Portamento(int channel, double value) {
    if (!AcceptsChannel(channel)) return;

    mMidiState.currentPortamento = Clip(value, 0.0, 1.0);
    mVoice.SetPortamentoControl(mMidiState.currentPortamento);
  }

  bool AcceptsChannel(int channel) const { return mActiveKey == kNoKey || mActiveChannel == Clip(channel, 0, 15); }

  bool IsActiveNote(int channel, int key) const { return mActiveKey != kNoKey && mActiveChannel == channel && mActiveKey == key; }

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
  int mPortamentoCC{5};
  VoiceT mVoice{};
  uint8_t mActiveChannel{0};
  uint8_t mActiveKey{kNoKey};
  bool mBreathGateOpen{false};
};

END_IPLUG_NAMESPACE

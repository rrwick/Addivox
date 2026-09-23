#pragma once

#include "../effects/chorus.h"
#include "../effects/drive.h"
#include "../effects/reverb.h"
#include "../effects/tone.h"
#include "../demo_mode.h"
#include "../settings/effects.h"
#include "../settings/params.h"
#include "midi_synth.h"
#include "voice.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <optional>

using namespace iplug;

class SynthEngine {
public:
  using VisualizerFrame = SynthVoice::VisualizerFrame;

  void ProcessBlock(sample** outputs, int nFrames) {
    ApplyPendingSettings();
    constexpr int kNumOutputs = 2;

    for (int i = 0; i < kNumOutputs; i++) memset(outputs[i], 0, nFrames * sizeof(sample));

    mSynth.ProcessBlock(outputs, nFrames);
    mDrive.ProcessBlock(outputs, nFrames);
    mTone.ProcessBlock(outputs, nFrames);
    mChorus.ProcessBlock(outputs, nFrames);
    mReverb.ProcessBlock(outputs, nFrames);
  }

  void Reset(double sampleRate, int blockSize) {
    const std::lock_guard<std::mutex> lock(mSettingsMutex);
    mSynth.SetSampleRateAndBlockSize(sampleRate, blockSize);
    mSynth.Reset();
    mSynth.GetVoice().SetGlobalVoiceSettings(mGlobalVoiceSettings);
    mSynth.GetVoice().SetTransposeSemitones(mTransposeSemitones);
    mDrive.Reset(sampleRate);
    mDrive.SetAmount(mEffectsSettings.drive);
    mTone.Reset(sampleRate);
    mTone.SetAmount(mEffectsSettings.tone);
    mChorus.Reset(sampleRate);
    mChorus.SetAmount(mEffectsSettings.chorus);
    mReverb.Reset(sampleRate);
    mReverb.SetAmount(mEffectsSettings.reverb);
  }

  void ProcessMidiMsg(const IMidiMsg& msg) { mSynth.AddMidiMsgToQueue(msg); }

  void SetBreathCCSource(BreathCCSource source) {
    const std::lock_guard<std::mutex> lock(mSettingsMutex);
    mPendingBreathCCSource = source;
  }

  void SetPitchBendRange(int semitones) {
    const std::lock_guard<std::mutex> lock(mSettingsMutex);
    mPendingPitchBendRange = semitones;
  }

  void SetPortamentoCC(int controller) {
    const std::lock_guard<std::mutex> lock(mSettingsMutex);
    mPendingPortamentoCC = controller;
  }

  void SetBreathCCSourceForChannel(int channel, BreathCCSource source) { mSynth.SetBreathCCSourceForChannel(channel, source); }

  void SetParam(int paramIdx, double value) {
    if (paramIdx < 0 || paramIdx >= kNumParams) return;
    mPendingParamValues[paramIdx].store(value, std::memory_order_relaxed);
    mPendingParams.fetch_or(1u << paramIdx, std::memory_order_release);
  }

  void SetCompoundPatch(const CompoundPatch& patch) {
    const std::lock_guard<std::mutex> lock(mSettingsMutex);
    mEditablePatch = patch;
    mPendingPatch = mEditablePatch;
    mPatchPending = true;
  }

  template <typename Edit> bool EditCompoundPatch(Edit&& edit) {
    const std::lock_guard<std::mutex> lock(mSettingsMutex);
    if (!edit(mEditablePatch)) return false;
    mPendingPatch = mEditablePatch;
    mPatchPending = true;
    return true;
  }

  void GetVisualizerFrame(VisualizerFrame& frame) const { mSynth.GetVoice().GetVisualizerFrame(frame); }

private:
  void ApplyPendingSettings() {
    // Parameter callbacks can run on the audio thread too, so publishing them must not wait for patch preparation.
    const unsigned int params = mPendingParams.exchange(0, std::memory_order_acquire);
    if (params) {
      for (int i = 0; i < kNumParams; ++i) {
        if (!(params & (1u << i))) continue;
        const double value = mPendingParamValues[i].load(std::memory_order_relaxed);
        if (i == kParamTranspose) mTransposeSemitones = ADDIVOX_DEMO ? 0.0 : value;
        else if (!global_settings::ApplyParam(i, value, mGlobalVoiceSettings)) effects_settings::ApplyParam(i, value, mEffectsSettings);
      }
      mSynth.GetVoice().SetGlobalVoiceSettings(mGlobalVoiceSettings);
      mSynth.GetVoice().SetTransposeSemitones(mTransposeSemitones);
      mDrive.SetAmount(mEffectsSettings.drive);
      mTone.SetAmount(mEffectsSettings.tone);
      mChorus.SetAmount(mEffectsSettings.chorus);
      mReverb.SetAmount(mEffectsSettings.reverb);
    }

    // Never wait for the editor. Swapping leaves the retired patch here for the next producer update to reclaim.
    const std::unique_lock<std::mutex> lock(mSettingsMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    if (mPendingBreathCCSource) {
      mSynth.SetBreathCCSource(*mPendingBreathCCSource);
      mPendingBreathCCSource.reset();
    }
    if (mPendingPitchBendRange) {
      mSynth.SetPitchBendRange(*mPendingPitchBendRange);
      mPendingPitchBendRange.reset();
    }
    if (mPendingPortamentoCC) {
      mSynth.SetPortamentoCC(*mPendingPortamentoCC);
      mPendingPortamentoCC.reset();
    }
    if (mPatchPending) {
      mSynth.GetVoice().SwapCompoundPatch(mPendingPatch);
      mPatchPending = false;
    }
  }

  std::mutex mSettingsMutex;
  CompoundPatch mEditablePatch;
  CompoundPatch mPendingPatch;
  std::optional<BreathCCSource> mPendingBreathCCSource;
  std::optional<int> mPendingPitchBendRange;
  std::optional<int> mPendingPortamentoCC;
  bool mPatchPending{false};
  static_assert(kNumParams <= 32);
  std::array<std::atomic<double>, kNumParams> mPendingParamValues{};
  std::atomic<unsigned int> mPendingParams{0};

public:
  MidiSynth<SynthVoice> mSynth{MidiSynth<SynthVoice>::kDefaultBlockSize};
  GlobalVoiceSettings mGlobalVoiceSettings{};
  EffectsSettings mEffectsSettings{};
  double mTransposeSemitones{0.0};
  effects::Drive mDrive;
  effects::Tone mTone;
  effects::Chorus mChorus;
  effects::Reverb mReverb;
};

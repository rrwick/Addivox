#pragma once

#include <array>
#include <limits>

#include "../settings/global.h"
#include "../settings/oscillator.h"
#include "IPlugConstants.h"
#include "oscillator.h"

class SynthVoice {
public:
  using VisualizerFrame = HarmonicVisualizerFrame;

  SynthVoice();
  bool IsActive() const;
  void Start(double pitch, double pitchBend, double breath);
  void Stop();
  void SetPitchBend(double pitchBend);
  void SetBreath(double breath);
  void SetPortamentoControl(double control);
  void SetTransposeSemitones(double transposeSemitones);
  void SetGlobalVoiceSettings(const GlobalVoiceSettings& settings);
  void SwapCompoundPatch(CompoundPatch& patch);
  void Clear();
  void ProcessSamplesAccumulating(iplug::sample** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);
  void GetVisualizerFrame(VisualizerFrame& frame) const;

private:
  double GetTargetMidiPitch() const;
  void UpdatePitch();
  void UpdateLevels();
  void UpdateLevel(int harmonic, const OscillatorSettings& settings, const CompoundPatch::ResolvedNoteSpan& noteSpan);
  void UpdatePitchRate();
  void RefreshNoteDependentState();
  double PredictRenderedMidiPitch(int numSamples) const;
  double GetPortamentoTimeSec() const;
  static double ShapeBreath(double breath);
  void SnapBreath(double breath);
  static double AdvanceTowards(double current, double target, double maxDelta);
  static double GetOscillatorBasePitchSemitones(int harmonic, double pitchOffsetCents, double fundamentalPitchSemitones,
                                                const GlobalVoiceSettings& globalSettings);
  void ApplyOscillatorSettings(int harmonic, const OscillatorSettings& currentSettings, double futurePitchOffsetCents,
                               double futureFundamentalPitchSemitones);

  static constexpr int kNumHarmonics = SimplePatch::kNumOscillators;
  static constexpr int kNoteControlIntervalSamples = 8;

  // Ramp between breath CCs at control ticks to avoid zipper noise. Note on/off snaps for immediate articulation.
  static constexpr double kBreathRampTimeSec = 0.002;

  // Pitch is in MIDI note numbers (0-127), where 69 corresponds to A4 (440 Hz).
  double mNotePitch{0.0};

  // Pitch bend is a semitone offset in MIDI-note units.
  double mPitchBend{0.0};

  double mTransposeSemitones{0.0};
  double mRenderedMidiPitch{0.0};
  double mTargetMidiPitch{0.0};
  double mSampleRate{44100.0};
  double mPitchRatePerSample{std::numeric_limits<double>::infinity()};
  int mNoteControlSamplesUntilUpdate{0};

  // Breath is a linear value from 0 to 1. mBreath is the currently rendered
  // value, ramping towards mTargetBreath by mBreathRampPerTick per control tick.
  double mBreath{0.0};
  double mTargetBreath{0.0};
  double mBreathRampPerTick{0.0};
  double mPortamentoControl{0.0};

  std::array<Oscillator, kNumHarmonics> mOscs;
  CompoundPatch mCompoundPatch;
  GlobalVoiceSettings mGlobalVoiceSettings;
};

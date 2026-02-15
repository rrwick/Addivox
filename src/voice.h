#pragma once

#include "IPlugConstants.h"
#include "oscillator.h"

namespace iplug {
template<typename VoiceT>
class MidiSynth;
}

using namespace iplug;

template<typename T>
class SynthVoice
{
public:
  bool GetBusy() const;
  void Trigger();
  void ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);

private:
  template <typename>
  friend class iplug::MidiSynth;

  bool mNoteOn{false};
  float mPitch{0.f};
  float mPitchBend{0.f};
  float mBreath{1.f};
  Oscillator mOsc;
};

extern template class SynthVoice<float>;
extern template class SynthVoice<double>;

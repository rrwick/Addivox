#pragma once

#include <array>

#include "oscillator.h"

template<typename T>
class SynthVoice
{
public:
  bool IsActive() const;
  void Start(float pitch, float pitchBend, float breath);
  void Stop();
  void SetPitchBend(float pitchBend);
  void SetBreath(float breath);
  void Clear();
  void ProcessSamplesAccumulating(T** outputs, int startIdx, int nFrames);
  void SetSampleRate(double sampleRate);

private:
  void UpdateFrequency();
  static constexpr int kNumHarmonics = 100;
  static constexpr std::array<float, kNumHarmonics> kHarmonicIntensities{
    0.50000f, 1.00000f, 0.90000f, 0.81000f, 0.72900f, 0.65610f, 0.59049f, 0.53144f, 0.47830f, 0.43047f,
    0.38742f, 0.34868f, 0.31381f, 0.28243f, 0.25419f, 0.22877f, 0.20589f, 0.18530f, 0.16677f, 0.15009f,
    0.13509f, 0.12158f, 0.10942f, 0.09848f, 0.08863f, 0.07977f, 0.07179f, 0.06461f, 0.05815f, 0.05233f,
    0.04710f, 0.04239f, 0.03815f, 0.03434f, 0.03090f, 0.02781f, 0.02503f, 0.02253f, 0.02028f, 0.01825f,
    0.01642f, 0.01478f, 0.01330f, 0.01197f, 0.01078f, 0.00970f, 0.00873f, 0.00786f, 0.00707f, 0.00636f,
    0.00573f, 0.00515f, 0.00464f, 0.00417f, 0.00376f, 0.00338f, 0.00304f, 0.00274f, 0.00247f, 0.00222f,
    0.00200f, 0.00180f, 0.00162f, 0.00146f, 0.00131f, 0.00118f, 0.00106f, 0.00096f, 0.00086f, 0.00077f,
    0.00070f, 0.00063f, 0.00056f, 0.00051f, 0.00046f, 0.00041f, 0.00037f, 0.00033f, 0.00030f, 0.00027f,
    0.00024f, 0.00022f, 0.00020f, 0.00018f, 0.00016f, 0.00014f, 0.00013f, 0.00012f, 0.00010f, 0.00009f,
    0.00008f, 0.00008f, 0.00007f, 0.00006f, 0.00006f, 0.00005f, 0.00004f, 0.00004f, 0.00004f, 0.00003f
  };

  float mPitch{0.f};
  float mPitchBend{0.f};
  std::array<Oscillator, kNumHarmonics> mOscs;
};

extern template class SynthVoice<float>;
extern template class SynthVoice<double>;

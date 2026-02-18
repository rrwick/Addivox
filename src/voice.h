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
    0.12390f, 0.20462f, 0.32779f, 0.41076f, 0.41453f, 0.36736f, 0.30989f, 0.25972f, 0.21920f, 0.18668f,
    0.16028f, 0.13859f, 0.12058f, 0.10547f, 0.09270f, 0.08183f, 0.07251f, 0.06449f, 0.05754f, 0.05149f,
    0.04621f, 0.04157f, 0.03749f, 0.03389f, 0.03070f, 0.02786f, 0.02533f, 0.02307f, 0.02105f, 0.01924f,
    0.01760f, 0.01613f, 0.01481f, 0.01361f, 0.01252f, 0.01153f, 0.01063f, 0.00982f, 0.00907f, 0.00839f,
    0.00777f, 0.00720f, 0.00668f, 0.00620f, 0.00577f, 0.00536f, 0.00499f, 0.00465f, 0.00433f, 0.00404f,
    0.00377f, 0.00353f, 0.00330f, 0.00308f, 0.00288f, 0.00270f, 0.00253f, 0.00237f, 0.00223f, 0.00209f,
    0.00196f, 0.00184f, 0.00173f, 0.00163f, 0.00153f, 0.00144f, 0.00136f, 0.00128f, 0.00121f, 0.00114f,
    0.00107f, 0.00101f, 0.00096f, 0.00090f, 0.00085f, 0.00081f, 0.00076f, 0.00072f, 0.00068f, 0.00065f,
    0.00061f, 0.00058f, 0.00055f, 0.00052f, 0.00049f, 0.00047f, 0.00044f, 0.00042f, 0.00040f, 0.00038f,
    0.00036f, 0.00034f, 0.00033f, 0.00031f, 0.00030f, 0.00028f, 0.00027f, 0.00025f, 0.00024f, 0.00023f
  };

  float mPitch{0.f};
  float mPitchBend{0.f};
  std::array<Oscillator, kNumHarmonics> mOscs;
};

extern template class SynthVoice<float>;
extern template class SynthVoice<double>;

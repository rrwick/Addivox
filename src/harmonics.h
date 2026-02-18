#pragma once

#include <array>

class HarmonicIntensities
{
public:
  static constexpr int kNumHarmonics = 100;
  using Spectrum = std::array<float, kNumHarmonics>;

  HarmonicIntensities();
  Spectrum GetIntensities(float midiPitch) const;

private:
  static constexpr int kMinMidiNote = 0;
  static constexpr int kMaxMidiNote = 127;
  static constexpr int kNumMidiNotes = kMaxMidiNote - kMinMidiNote + 1;

  static Spectrum Interpolate(const Spectrum& lo, const Spectrum& hi, float t);

  std::array<Spectrum, kNumMidiNotes> mNoteTable{};
};

#pragma once

#include "../config.h"

#ifndef ADDIVOX_DEMO
#define ADDIVOX_DEMO 0
#endif

namespace addivox_demo {
inline constexpr bool kEnabled = ADDIVOX_DEMO != 0;
inline constexpr const char* kVisualizerTitle = "DEMO MODE";
inline constexpr const char* kLimitationsText = "White-key notes only. Transposition is disabled.";
inline constexpr const char* kAboutText = "DEMO MODE: white-key notes only, transposition is disabled.";

inline bool IsWhiteKeyMidiNote(int midiNote) {
  const int pitchClass = ((midiNote % 12) + 12) % 12;
  return pitchClass == 0 || pitchClass == 2 || pitchClass == 4 || pitchClass == 5 || pitchClass == 7 || pitchClass == 9 || pitchClass == 11;
}
} // namespace addivox_demo

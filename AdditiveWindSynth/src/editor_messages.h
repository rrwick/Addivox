#pragma once

#include <array>

#include "settings/settings_oscillator.h"

namespace editor_messages
{
inline constexpr int kMsgTagSetKeyNoteOscillatorParameter = 1000;
inline constexpr int kMsgTagAddKeyNotePreset = 1001;
inline constexpr int kMsgTagRemoveKeyNotePreset = 1002;
inline constexpr int kMsgTagSetKeyNoteOscillatorParameterValues = 1003;

struct SetKeyNoteOscillatorParameterPayload
{
  int midiNote{0};
  int oscillatorIndex{0};
  int parameter{0};
  double value{0.0};
};

struct KeyNotePresetPayload
{
  int midiNote{0};
};

struct SetKeyNoteOscillatorParameterValuesPayload
{
  int midiNote{0};
  int parameter{0};
  std::array<double, SimplePreset::kNumOscillators> values{};
};
} // namespace editor_messages

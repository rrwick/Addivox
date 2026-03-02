#pragma once

namespace preset_editor_messages
{
inline constexpr int kMsgTagSetKeyNoteOscillatorParameter = 1000;

struct SetKeyNoteOscillatorParameterPayload
{
  int midiNote{0};
  int oscillatorIndex{0};
  int parameter{0};
  double value{0.0};
};
} // namespace preset_editor_messages

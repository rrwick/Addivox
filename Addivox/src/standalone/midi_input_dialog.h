#pragma once

#include "IPlugPlatform.h"
#if defined(OS_MAC)
#include "IPlugSWELL.h"
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace addivox_standalone {
void PopulateMidiInputList(HWND dialog);
const char* MidiInputName(int index);
void SetMidiSettingsDialog(HWND dialog);
void PollMidiInputs();
} // namespace addivox_standalone

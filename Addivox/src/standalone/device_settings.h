#pragma once

#include "IPlugPlatform.h"
#include <cstdint>
#if defined(OS_MAC)
#include "IPlugSWELL.h"
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace addivox_standalone {
void PopulateMidiInputList(HWND dialog);
const char* MidiInputName(int index);
void SetDeviceSettingsDialog(HWND dialog);
void PollMidiInputs();

inline constexpr char kSystemAudioOutput[] = "Same as system";
inline constexpr UINT kSystemAudioChangedMessage = WM_USER + 101;
inline constexpr UINT kSystemAudioTimer = 1003;
bool SupportsSystemAudioOutput(uint32_t driver);
void ScheduleSystemAudioUpdate(UINT delay = 100);
void UpdateSystemAudioOutput();
} // namespace addivox_standalone

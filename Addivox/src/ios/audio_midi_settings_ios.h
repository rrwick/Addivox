#pragma once

namespace addivox_standalone {

inline constexpr int kIOSAudioSettingsSampleRates[] = {44100, 48000};
inline constexpr int kIOSAudioSettingsBufferSizes[] = {32, 64, 96, 128, 192, 256, 512, 1024, 2048, 4096};

bool IsAudioMidiSettingsMenuAvailable();

int GetPreferredSampleRate();
int GetPreferredBufferSize();

bool SetPreferredSampleRate(int sampleRate);
bool SetPreferredBufferSize(int bufferSize);

} // namespace addivox_standalone

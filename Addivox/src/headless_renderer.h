#pragma once

#include <optional>
#include <string>

struct HeadlessRenderOptions
{
  std::string presetPath;
  std::string outputPath;
  int note{60};
  double durationSeconds{1.0};
  int breathMidiValue{127};
  int sampleRate{48000};
  int numOutputChannels{2};
  std::optional<double> reverb;
  std::optional<double> pitchOffsetCents;
  std::optional<double> panOffset;
  std::optional<int> transposeSemitones;
};

bool RenderPresetNoteToWav(const HeadlessRenderOptions& options, std::string* errorMessage = nullptr);

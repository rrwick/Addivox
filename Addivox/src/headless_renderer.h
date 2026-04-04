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
  std::optional<double> levelScale;
  std::optional<double> attackScale;
  std::optional<double> releaseScale;
  std::optional<double> reverb;
  std::optional<double> drive;
  std::optional<double> tone;
  std::optional<double> chorus;
  std::optional<double> pitchOffsetCents;
  std::optional<double> panOffset;
  std::optional<double> intensityVariationAmplitudeScale;
  std::optional<double> intensityVariationRateScale;
  std::optional<double> pitchVariationAmplitudeScale;
  std::optional<double> pitchVariationRateScale;
  std::optional<double> panVariationAmplitudeScale;
  std::optional<double> panVariationRateScale;
  std::optional<double> portamentoTimeAtCC5MinSec;
  std::optional<double> portamentoTimeAtCC5MaxSec;
  std::optional<int> transposeSemitones;
};

bool RenderPresetNoteToWav(const HeadlessRenderOptions& options, std::string* errorMessage = nullptr);

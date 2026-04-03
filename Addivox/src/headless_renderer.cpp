#include "headless_renderer.h"

#include "IPlugConstants.h"
#include "IPlugMidi.h"
#include "settings/effects.h"
#include "settings/global.h"
#include "settings/preset_io.h"
#include "synth/synth_engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr int kRenderBlockSize = 64;
constexpr double kDefaultCliReverb = 50.0;
constexpr double kSilenceThreshold = 1.0e-5;
constexpr double kRequiredSilentTailSeconds = 0.25;
constexpr double kMaxTailSeconds = 8.0;

struct ScheduledMidiEvent
{
  int64_t sampleOffset{0};
  iplug::IMidiMsg message;
};

void SetErrorMessage(std::string* errorMessage, std::string message)
{
  if(errorMessage)
    *errorMessage = std::move(message);
}

void WriteUint16LE(std::ofstream& stream, uint16_t value)
{
  const std::array<char, 2> bytes{
    static_cast<char>(value & 0xFFu),
    static_cast<char>((value >> 8) & 0xFFu),
  };
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteUint32LE(std::ofstream& stream, uint32_t value)
{
  const std::array<char, 4> bytes{
    static_cast<char>(value & 0xFFu),
    static_cast<char>((value >> 8) & 0xFFu),
    static_cast<char>((value >> 16) & 0xFFu),
    static_cast<char>((value >> 24) & 0xFFu),
  };
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool WriteFloatWaveFile(std::string_view path,
                        const std::vector<float>& samples,
                        int sampleRate,
                        int numChannels,
                        std::string* errorMessage)
{
  if(sampleRate <= 0)
  {
    SetErrorMessage(errorMessage, "Sample rate must be positive");
    return false;
  }

  if(numChannels <= 0)
  {
    SetErrorMessage(errorMessage, "Channel count must be positive");
    return false;
  }

  if(samples.size() % static_cast<std::size_t>(numChannels) != 0)
  {
    SetErrorMessage(errorMessage, "Rendered sample buffer is not aligned to the channel count");
    return false;
  }

  const std::string parentPath = preset_io::detail::ParentPath(path);
  if(!parentPath.empty() && !preset_io::detail::EnsureDirectoryExists(parentPath))
  {
    SetErrorMessage(errorMessage, "Could not create output directory");
    return false;
  }

  std::ofstream stream{std::string{path}, std::ios::binary};
  if(!stream.is_open())
  {
    SetErrorMessage(errorMessage, "Could not open output WAV file");
    return false;
  }

  constexpr uint16_t kWaveFormatIeeeFloat = 3;
  constexpr uint16_t kBitsPerSample = 32;
  const uint32_t bytesPerSample = static_cast<uint32_t>(kBitsPerSample / 8);
  const uint32_t dataChunkSize = static_cast<uint32_t>(samples.size() * sizeof(float));
  const uint32_t sampleFrames = static_cast<uint32_t>(samples.size() / static_cast<std::size_t>(numChannels));
  const uint32_t byteRate = static_cast<uint32_t>(sampleRate * numChannels) * bytesPerSample;
  const uint16_t blockAlign = static_cast<uint16_t>(numChannels * bytesPerSample);
  const uint32_t riffChunkSize = 4u + (8u + 16u) + (8u + 4u) + (8u + dataChunkSize);

  stream.write("RIFF", 4);
  WriteUint32LE(stream, riffChunkSize);
  stream.write("WAVE", 4);

  stream.write("fmt ", 4);
  WriteUint32LE(stream, 16u);
  WriteUint16LE(stream, kWaveFormatIeeeFloat);
  WriteUint16LE(stream, static_cast<uint16_t>(numChannels));
  WriteUint32LE(stream, static_cast<uint32_t>(sampleRate));
  WriteUint32LE(stream, byteRate);
  WriteUint16LE(stream, blockAlign);
  WriteUint16LE(stream, kBitsPerSample);

  stream.write("fact", 4);
  WriteUint32LE(stream, 4u);
  WriteUint32LE(stream, sampleFrames);

  stream.write("data", 4);
  WriteUint32LE(stream, dataChunkSize);
  stream.write(reinterpret_cast<const char*>(samples.data()), static_cast<std::streamsize>(dataChunkSize));

  if(!stream.good())
  {
    SetErrorMessage(errorMessage, "Failed while writing the WAV file");
    return false;
  }

  return true;
}

std::vector<ScheduledMidiEvent> BuildRenderEvents(const HeadlessRenderOptions& options,
                                                  int64_t noteDurationFrames)
{
  std::vector<ScheduledMidiEvent> events;
  events.reserve(3);

  iplug::IMidiMsg breathMessage;
  breathMessage.MakeControlChangeMsg(
    iplug::IMidiMsg::kBreathController,
    static_cast<double>(options.breathMidiValue) / 127.0,
    0,
    0);
  events.push_back({0, breathMessage});

  iplug::IMidiMsg noteOnMessage;
  noteOnMessage.MakeNoteOnMsg(options.note, 127, 0, 0);
  events.push_back({0, noteOnMessage});

  iplug::IMidiMsg noteOffMessage;
  noteOffMessage.MakeNoteOffMsg(options.note, 0, 0);
  events.push_back({noteDurationFrames, noteOffMessage});

  return events;
}

void ApplyPresetAndOverrides(SynthEngine& engine,
                             const preset_io::PresetDocument& document,
                             const HeadlessRenderOptions& options)
{
  GlobalVoiceSettings voiceSettings = global_settings::Sanitize(document.voiceSettings);
  EffectsSettings effectsSettings = effects_settings::Sanitize(document.effectsSettings);

  effectsSettings.reverb = kDefaultCliReverb;

  if(options.pitchOffsetCents)
    voiceSettings.pitchOffsetCents = *options.pitchOffsetCents;

  if(options.panOffset)
    voiceSettings.panOffset = *options.panOffset;

  if(options.reverb)
    effectsSettings.reverb = *options.reverb;

  engine.mGlobalVoiceSettings = global_settings::Sanitize(voiceSettings);
  engine.mEffectsSettings = effects_settings::Sanitize(effectsSettings);
  engine.mTransposeSemitones = static_cast<double>(options.transposeSemitones.value_or(0));
  engine.Reset(static_cast<double>(options.sampleRate), kRenderBlockSize);
  engine.SetCompoundPreset(document.compoundPreset);
}
} // namespace

bool RenderPresetNoteToWav(const HeadlessRenderOptions& options, std::string* errorMessage)
{
  if(options.presetPath.empty())
  {
    SetErrorMessage(errorMessage, "Preset path is required");
    return false;
  }

  if(options.outputPath.empty())
  {
    SetErrorMessage(errorMessage, "Output path is required");
    return false;
  }

  if(options.note < 0 || options.note > 127)
  {
    SetErrorMessage(errorMessage, "Note must be in the MIDI range 0-127");
    return false;
  }

  if(!(options.durationSeconds > 0.0) || !std::isfinite(options.durationSeconds))
  {
    SetErrorMessage(errorMessage, "Duration must be a positive number of seconds");
    return false;
  }

  if(options.breathMidiValue < 0 || options.breathMidiValue > 127)
  {
    SetErrorMessage(errorMessage, "Breath must be in the MIDI CC range 0-127");
    return false;
  }

  if(options.sampleRate <= 0)
  {
    SetErrorMessage(errorMessage, "Sample rate must be positive");
    return false;
  }

  if(options.numOutputChannels != 1 && options.numOutputChannels != 2)
  {
    SetErrorMessage(errorMessage, "Output channel count must be 1 or 2");
    return false;
  }

  if(options.transposeSemitones && (*options.transposeSemitones < -36 || *options.transposeSemitones > 36))
  {
    SetErrorMessage(errorMessage, "Transpose must be in the range -36 to 36 semitones");
    return false;
  }

  preset_io::PresetDocument document;
  if(!preset_io::LoadPresetFromFile(options.presetPath, document, errorMessage))
    return false;

  SynthEngine engine;
  ApplyPresetAndOverrides(engine, document, options);

  const int64_t noteDurationFrames = std::max<int64_t>(
    1,
    static_cast<int64_t>(std::llround(options.durationSeconds * options.sampleRate)));
  const int64_t requiredSilentFrames = std::max<int64_t>(
    1,
    static_cast<int64_t>(std::llround(kRequiredSilentTailSeconds * options.sampleRate)));
  const int64_t maxTailFrames = std::max<int64_t>(0, static_cast<int64_t>(std::llround(kMaxTailSeconds * options.sampleRate)));
  const std::vector<ScheduledMidiEvent> events = BuildRenderEvents(options, noteDurationFrames);

  std::vector<float> renderedSamples;
  renderedSamples.reserve(
    static_cast<std::size_t>((noteDurationFrames + maxTailFrames + kRenderBlockSize) * options.numOutputChannels));

  std::vector<iplug::sample> leftBlock(static_cast<std::size_t>(kRenderBlockSize), 0.0);
  std::vector<iplug::sample> rightBlock(static_cast<std::size_t>(kRenderBlockSize), 0.0);
  iplug::sample* outputs[2] = {leftBlock.data(), rightBlock.data()};

  std::size_t nextEventIndex = 0;
  int64_t renderedFrames = 0;
  int64_t silentFramesAfterNoteOff = 0;
  int64_t tailFramesRendered = 0;

  while(true)
  {
    while(nextEventIndex < events.size() && events[nextEventIndex].sampleOffset <= renderedFrames)
    {
      iplug::IMidiMsg message = events[nextEventIndex].message;
      message.mOffset = 0;
      engine.ProcessMidiMsg(message);
      ++nextEventIndex;
    }

    const int blockFrames = kRenderBlockSize;
    const int64_t blockEndFrame = renderedFrames + blockFrames;

    while(nextEventIndex < events.size() && events[nextEventIndex].sampleOffset < blockEndFrame)
    {
      iplug::IMidiMsg message = events[nextEventIndex].message;
      message.mOffset = static_cast<int>(events[nextEventIndex].sampleOffset - renderedFrames);
      engine.ProcessMidiMsg(message);
      ++nextEventIndex;
    }

    engine.ProcessBlock(outputs, blockFrames);

    double blockPeak = 0.0;
    for(int frame = 0; frame < blockFrames; ++frame)
    {
      const float left = static_cast<float>(leftBlock[static_cast<std::size_t>(frame)]);
      const float right = static_cast<float>(rightBlock[static_cast<std::size_t>(frame)]);

      blockPeak = std::max(blockPeak, static_cast<double>(std::max(std::abs(left), std::abs(right))));

      if(options.numOutputChannels == 1)
      {
        renderedSamples.push_back(0.5f * (left + right));
      }
      else
      {
        renderedSamples.push_back(left);
        renderedSamples.push_back(right);
      }
    }

    renderedFrames += blockFrames;

    if(renderedFrames <= noteDurationFrames)
      continue;

    tailFramesRendered += blockFrames;
    if(blockPeak <= kSilenceThreshold)
      silentFramesAfterNoteOff += blockFrames;
    else
      silentFramesAfterNoteOff = 0;

    if(silentFramesAfterNoteOff >= requiredSilentFrames)
      break;

    if(tailFramesRendered >= maxTailFrames)
      break;
  }

  return WriteFloatWaveFile(
    options.outputPath,
    renderedSamples,
    options.sampleRate,
    options.numOutputChannels,
    errorMessage);
}

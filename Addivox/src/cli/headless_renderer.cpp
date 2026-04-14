#include "headless_renderer.h"

#include "IPlugConstants.h"
#include "IPlugMidi.h"
#include "midi/breath_control.h"
#include "midi_file_parser.h"
#include "settings/effects.h"
#include "settings/global.h"
#include "settings/preset_io.h"
#include "synth/synth_engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
constexpr int kRenderBlockSize = 64;
constexpr double kSilenceThreshold = 1.0e-5;
constexpr double kRequiredSilentTailSeconds = 0.25;
constexpr double kMaxTailSeconds = 8.0;

struct ScheduledMidiEvent
{
  int64_t sampleOffset{0};
  iplug::IMidiMsg message;
};

using DetectedBreathCCSources = std::array<std::optional<BreathCCSource>, 16>;

struct WaveFormatDescriptor
{
  uint16_t formatTag{0};
  uint16_t bitsPerSample{0};
  uint16_t bytesPerSample{0};
  bool isFloatingPoint{false};
};

void SetErrorMessage(std::string* errorMessage, std::string message)
{
  if(errorMessage)
    *errorMessage = std::move(message);
}

bool ValidateFiniteDouble(const char* label, double value, std::string* errorMessage)
{
  if(std::isfinite(value))
    return true;

  SetErrorMessage(errorMessage, std::string{label} + " must be a finite number");
  return false;
}

bool ValidateFiniteRange(const char* label,
                         double value,
                         double minimum,
                         double maximum,
                         std::string* errorMessage)
{
  if(!ValidateFiniteDouble(label, value, errorMessage))
    return false;

  if(value < minimum || value > maximum)
  {
    SetErrorMessage(
      errorMessage,
      std::string{label} + " must be in the range " + preset_io::detail::FormatDouble(minimum)
        + " to " + preset_io::detail::FormatDouble(maximum));
    return false;
  }

  return true;
}

bool ValidateOptionalNonNegative(const char* label, const std::optional<double>& value, std::string* errorMessage)
{
  if(!value)
    return true;

  if(!ValidateFiniteDouble(label, *value, errorMessage))
    return false;

  if(*value < 0.0)
  {
    SetErrorMessage(errorMessage, std::string{label} + " must be non-negative");
    return false;
  }

  return true;
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

void WriteUint64LE(std::ofstream& stream, uint64_t value)
{
  const std::array<char, 8> bytes{
    static_cast<char>(value & 0xFFu),
    static_cast<char>((value >> 8) & 0xFFu),
    static_cast<char>((value >> 16) & 0xFFu),
    static_cast<char>((value >> 24) & 0xFFu),
    static_cast<char>((value >> 32) & 0xFFu),
    static_cast<char>((value >> 40) & 0xFFu),
    static_cast<char>((value >> 48) & 0xFFu),
    static_cast<char>((value >> 56) & 0xFFu),
  };
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteFloat32LE(std::ofstream& stream, float value)
{
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  WriteUint32LE(stream, bits);
}

void WriteFloat64LE(std::ofstream& stream, double value)
{
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  WriteUint64LE(stream, bits);
}

WaveFormatDescriptor DescribeWaveFileFormat(WaveFileFormat waveFileFormat)
{
  constexpr uint16_t kWaveFormatPcm = 1;
  constexpr uint16_t kWaveFormatIeeeFloat = 3;

  switch(waveFileFormat)
  {
    case WaveFileFormat::Pcm16:
      return {kWaveFormatPcm, 16, 2, false};
    case WaveFileFormat::Pcm24:
      return {kWaveFormatPcm, 24, 3, false};
    case WaveFileFormat::Pcm32:
      return {kWaveFormatPcm, 32, 4, false};
    case WaveFileFormat::Float32:
      return {kWaveFormatIeeeFloat, 32, 4, true};
    case WaveFileFormat::Float64:
      return {kWaveFormatIeeeFloat, 64, 8, true};
  }

  return {};
}

int32_t QuantizePcmSample(double sample, int bitsPerSample)
{
  const int32_t maxValue = (bitsPerSample == 32)
    ? std::numeric_limits<int32_t>::max()
    : ((static_cast<int32_t>(1) << (bitsPerSample - 1)) - 1);
  const int32_t minValue = (bitsPerSample == 32)
    ? std::numeric_limits<int32_t>::min()
    : (-maxValue - 1);

  if(sample <= -1.0)
    return minValue;

  if(sample >= 1.0)
    return maxValue;

  return static_cast<int32_t>(std::llround(sample * static_cast<double>(maxValue)));
}

void WriteInt24LE(std::ofstream& stream, int32_t value)
{
  const uint32_t bits = static_cast<uint32_t>(value);
  const std::array<char, 3> bytes{
    static_cast<char>(bits & 0xFFu),
    static_cast<char>((bits >> 8) & 0xFFu),
    static_cast<char>((bits >> 16) & 0xFFu),
  };
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool WriteWaveSampleData(std::ofstream& stream,
                         const std::vector<double>& samples,
                         WaveFileFormat waveFileFormat)
{
  switch(waveFileFormat)
  {
    case WaveFileFormat::Pcm16:
      for(const double sample : samples)
      {
        const int16_t value = static_cast<int16_t>(QuantizePcmSample(sample, 16));
        WriteUint16LE(stream, static_cast<uint16_t>(value));
      }
      return stream.good();

    case WaveFileFormat::Pcm24:
      for(const double sample : samples)
        WriteInt24LE(stream, QuantizePcmSample(sample, 24));
      return stream.good();

    case WaveFileFormat::Pcm32:
      for(const double sample : samples)
        WriteUint32LE(stream, static_cast<uint32_t>(QuantizePcmSample(sample, 32)));
      return stream.good();

    case WaveFileFormat::Float32:
      for(const double sample : samples)
        WriteFloat32LE(stream, static_cast<float>(sample));
      return stream.good();

    case WaveFileFormat::Float64:
      for(const double sample : samples)
        WriteFloat64LE(stream, sample);
      return stream.good();
  }

  return false;
}

bool WriteWaveFile(std::string_view path,
                   const std::vector<double>& samples,
                   int sampleRate,
                   int numChannels,
                   WaveFileFormat waveFileFormat,
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

  const WaveFormatDescriptor format = DescribeWaveFileFormat(waveFileFormat);
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

  const uint64_t dataChunkSize64 = static_cast<uint64_t>(samples.size()) * format.bytesPerSample;
  const uint64_t factChunkSize64 = format.isFloatingPoint ? 12u : 0u;
  const uint32_t dataChunkPadding = static_cast<uint32_t>(dataChunkSize64 & 1u);
  const uint64_t riffChunkSize64 =
    4u + (8u + 16u) + factChunkSize64 + (8u + dataChunkSize64 + dataChunkPadding);
  const uint32_t sampleFrames = static_cast<uint32_t>(samples.size() / static_cast<std::size_t>(numChannels));
  if(dataChunkSize64 > std::numeric_limits<uint32_t>::max()
     || riffChunkSize64 > std::numeric_limits<uint32_t>::max())
  {
    SetErrorMessage(errorMessage, "Output WAV file would exceed 4 GB");
    return false;
  }

  const uint32_t dataChunkSize = static_cast<uint32_t>(dataChunkSize64);
  const uint32_t byteRate = static_cast<uint32_t>(sampleRate * numChannels) * format.bytesPerSample;
  const uint16_t blockAlign = static_cast<uint16_t>(numChannels * format.bytesPerSample);

  stream.write("RIFF", 4);
  WriteUint32LE(stream, static_cast<uint32_t>(riffChunkSize64));
  stream.write("WAVE", 4);

  stream.write("fmt ", 4);
  WriteUint32LE(stream, 16u);
  WriteUint16LE(stream, format.formatTag);
  WriteUint16LE(stream, static_cast<uint16_t>(numChannels));
  WriteUint32LE(stream, static_cast<uint32_t>(sampleRate));
  WriteUint32LE(stream, byteRate);
  WriteUint16LE(stream, blockAlign);
  WriteUint16LE(stream, format.bitsPerSample);

  if(format.isFloatingPoint)
  {
    stream.write("fact", 4);
    WriteUint32LE(stream, 4u);
    WriteUint32LE(stream, sampleFrames);
  }

  stream.write("data", 4);
  WriteUint32LE(stream, dataChunkSize);
  if(!WriteWaveSampleData(stream, samples, waveFileFormat))
  {
    SetErrorMessage(errorMessage, "Failed while writing the WAV file");
    return false;
  }

  if(dataChunkPadding != 0)
    stream.put('\0');

  if(!stream.good())
  {
    SetErrorMessage(errorMessage, "Failed while writing the WAV file");
    return false;
  }

  return true;
}

std::vector<ScheduledMidiEvent> BuildSingleNoteEvents(const HeadlessRenderOptions& options,
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

std::vector<ScheduledMidiEvent> BuildMidiFileEvents(const midi_file::ParsedFile& parsedFile,
                                                    int sampleRate,
                                                    int64_t& lastEventFrame)
{
  std::vector<ScheduledMidiEvent> events;
  events.reserve(parsedFile.events.size());

  lastEventFrame = 0;
  for(const midi_file::TimedEvent& timedEvent : parsedFile.events)
  {
    const int64_t sampleOffset = std::max<int64_t>(
      0,
      static_cast<int64_t>(std::llround(timedEvent.timeSeconds * static_cast<double>(sampleRate))));
    lastEventFrame = std::max(lastEventFrame, sampleOffset);
    events.push_back({sampleOffset, timedEvent.message});
  }

  return events;
}

bool DetectMidiFileBreathCCSources(const midi_file::ParsedFile& parsedFile,
                                   DetectedBreathCCSources& breathCCSources,
                                   std::string* errorMessage)
{
  std::array<std::array<bool, 128>, 16> seenControllers{};
  breathCCSources.fill(std::nullopt);

  for(const midi_file::TimedEvent& timedEvent : parsedFile.events)
  {
    const iplug::IMidiMsg& message = timedEvent.message;
    if(message.StatusMsg() != iplug::IMidiMsg::kControlChange)
      continue;

    const int controller = std::clamp(static_cast<int>(message.mData1), 0, 127);
    if(!IsSupportedBreathCCController(controller))
      continue;

    seenControllers[static_cast<std::size_t>(std::clamp(message.Channel(), 0, 15))]
                   [static_cast<std::size_t>(controller)] = true;
  }

  bool foundBreathCCSource = false;
  for(int channel = 0; channel < 16; ++channel)
  {
    breathCCSources[static_cast<std::size_t>(channel)] = DetectBreathCCSource(
      seenControllers[static_cast<std::size_t>(channel)]);
    foundBreathCCSource = foundBreathCCSource || breathCCSources[static_cast<std::size_t>(channel)].has_value();
  }

  if(foundBreathCCSource)
    return true;

  SetErrorMessage(
    errorMessage,
    "MIDI file contains no supported breath controllers; expected CC2/34, CC2, CC11/43, CC11, CC7/39, CC7, or CC1");
  return false;
}

void ApplyPresetAndOverrides(SynthEngine& engine,
                             const preset_io::PresetDocument& document,
                             const HeadlessRenderOptions& options)
{
  GlobalVoiceSettings voiceSettings = global_settings::Sanitize(document.voiceSettings);
  EffectsSettings effectsSettings = effects_settings::Sanitize(document.effectsSettings);

  if(options.levelScale)
    voiceSettings.levelScale = *options.levelScale;

  if(options.attackScale)
    voiceSettings.attackScale = *options.attackScale;

  if(options.releaseScale)
    voiceSettings.releaseScale = *options.releaseScale;

  if(options.pitchOffsetCents)
    voiceSettings.pitchOffsetCents = *options.pitchOffsetCents;

  if(options.panOffset)
    voiceSettings.panOffset = *options.panOffset;

  if(options.intensityVariationAmplitudeScale)
    voiceSettings.intensityVariationAmplitudeScale = *options.intensityVariationAmplitudeScale;

  if(options.intensityVariationRateScale)
    voiceSettings.intensityVariationRateScale = *options.intensityVariationRateScale;

  if(options.pitchVariationAmplitudeScale)
    voiceSettings.pitchVariationAmplitudeScale = *options.pitchVariationAmplitudeScale;

  if(options.pitchVariationRateScale)
    voiceSettings.pitchVariationRateScale = *options.pitchVariationRateScale;

  if(options.panVariationAmplitudeScale)
    voiceSettings.panVariationAmplitudeScale = *options.panVariationAmplitudeScale;

  if(options.panVariationRateScale)
    voiceSettings.panVariationRateScale = *options.panVariationRateScale;

  if(options.portamentoTimeAtCC5MinSec)
    voiceSettings.portamentoTimeAtCC5MinSec = *options.portamentoTimeAtCC5MinSec;

  if(options.portamentoTimeAtCC5MaxSec)
    voiceSettings.portamentoTimeAtCC5MaxSec = *options.portamentoTimeAtCC5MaxSec;

  if(options.drive)
    effectsSettings.drive = *options.drive;

  if(options.tone)
    effectsSettings.tone = *options.tone;

  if(options.chorus)
    effectsSettings.chorus = *options.chorus;

  if(options.reverb)
    effectsSettings.reverb = *options.reverb;

  engine.mGlobalVoiceSettings = global_settings::Sanitize(voiceSettings);
  engine.mEffectsSettings = effects_settings::Sanitize(effectsSettings);
  engine.mTransposeSemitones = static_cast<double>(options.transposeSemitones.value_or(0));
  engine.Reset(static_cast<double>(options.sampleRate), kRenderBlockSize);
  engine.SetCompoundPreset(document.compoundPreset);
}

bool ValidateCommonRenderOptions(const HeadlessRenderOptions& options, std::string* errorMessage)
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

  if(options.levelScale && !ValidateFiniteRange("Level", *options.levelScale, 0.0, 10.0, errorMessage))
    return false;

  if(options.attackScale && !ValidateFiniteRange("Attack", *options.attackScale, 0.0, 100.0, errorMessage))
    return false;

  if(options.releaseScale && !ValidateFiniteRange("Release", *options.releaseScale, 0.0, 100.0, errorMessage))
    return false;

  if(options.pitchOffsetCents
     && !ValidateFiniteRange("Pitch offset", *options.pitchOffsetCents, -50.0, 50.0, errorMessage))
  {
    return false;
  }

  if(options.panOffset && !ValidateFiniteRange("Pan offset", *options.panOffset, -1.0, 1.0, errorMessage))
    return false;

  if(options.intensityVariationAmplitudeScale
     && !ValidateFiniteRange(
       "Level variation amount",
       *options.intensityVariationAmplitudeScale,
       0.0,
       100.0,
       errorMessage))
  {
    return false;
  }

  if(options.intensityVariationRateScale
     && !ValidateFiniteRange(
       "Level variation rate",
       *options.intensityVariationRateScale,
       0.0,
       100.0,
       errorMessage))
  {
    return false;
  }

  if(options.panVariationAmplitudeScale
     && !ValidateFiniteRange(
       "Pan variation amount",
       *options.panVariationAmplitudeScale,
       0.0,
       100.0,
       errorMessage))
  {
    return false;
  }

  if(options.panVariationRateScale
     && !ValidateFiniteRange(
       "Pan variation rate",
       *options.panVariationRateScale,
       0.0,
       100.0,
       errorMessage))
  {
    return false;
  }

  if(options.pitchVariationAmplitudeScale
     && !ValidateFiniteRange(
       "Pitch variation amount",
       *options.pitchVariationAmplitudeScale,
       0.0,
       100.0,
       errorMessage))
  {
    return false;
  }

  if(options.pitchVariationRateScale
     && !ValidateFiniteRange(
       "Pitch variation rate",
       *options.pitchVariationRateScale,
       0.0,
       100.0,
       errorMessage))
  {
    return false;
  }

  if(!ValidateOptionalNonNegative("Portamento minimum", options.portamentoTimeAtCC5MinSec, errorMessage))
    return false;

  if(!ValidateOptionalNonNegative("Portamento maximum", options.portamentoTimeAtCC5MaxSec, errorMessage))
    return false;

  if(options.drive && !ValidateFiniteRange("Drive", *options.drive, 0.0, 100.0, errorMessage))
    return false;

  if(options.tone && !ValidateFiniteRange("Tone", *options.tone, -1.0, 1.0, errorMessage))
    return false;

  if(options.chorus && !ValidateFiniteRange("Chorus", *options.chorus, 0.0, 100.0, errorMessage))
    return false;

  if(options.reverb && !ValidateFiniteRange("Reverb", *options.reverb, 0.0, 100.0, errorMessage))
    return false;

  return true;
}

bool RenderScheduledEventsToWav(const HeadlessRenderOptions& options,
                                const std::vector<ScheduledMidiEvent>& events,
                                int64_t lastEventFrame,
                                const DetectedBreathCCSources* breathCCSources,
                                std::string* errorMessage)
{
  preset_io::PresetDocument document;
  if(!preset_io::LoadPresetFromFile(options.presetPath, document, errorMessage))
    return false;

  SynthEngine engine;
  ApplyPresetAndOverrides(engine, document, options);
  if(breathCCSources)
  {
    for(int channel = 0; channel < 16; ++channel)
    {
      const auto& source = (*breathCCSources)[static_cast<std::size_t>(channel)];
      if(source.has_value())
        engine.SetBreathCCSourceForChannel(channel, *source);
    }
  }

  const int64_t requiredSilentFrames = std::max<int64_t>(
    1,
    static_cast<int64_t>(std::llround(kRequiredSilentTailSeconds * options.sampleRate)));
  const int64_t maxTailFrames = std::max<int64_t>(
    0,
    static_cast<int64_t>(std::llround(kMaxTailSeconds * options.sampleRate)));

  std::vector<double> renderedSamples;
  renderedSamples.reserve(
    static_cast<std::size_t>((lastEventFrame + maxTailFrames + kRenderBlockSize) * options.numOutputChannels));

  std::vector<iplug::sample> leftBlock(static_cast<std::size_t>(kRenderBlockSize), 0.0);
  std::vector<iplug::sample> rightBlock(static_cast<std::size_t>(kRenderBlockSize), 0.0);
  iplug::sample* outputs[2] = {leftBlock.data(), rightBlock.data()};

  std::size_t nextEventIndex = 0;
  int64_t renderedFrames = 0;
  int64_t silentFramesAfterPlayback = 0;
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
      const double left = static_cast<double>(leftBlock[static_cast<std::size_t>(frame)]);
      const double right = static_cast<double>(rightBlock[static_cast<std::size_t>(frame)]);

      blockPeak = std::max(blockPeak, std::max(std::abs(left), std::abs(right)));

      if(options.numOutputChannels == 1)
        renderedSamples.push_back(0.5 * (left + right));
      else
      {
        renderedSamples.push_back(left);
        renderedSamples.push_back(right);
      }
    }

    renderedFrames += blockFrames;

    if(renderedFrames <= lastEventFrame)
      continue;

    tailFramesRendered += blockFrames;
    if(blockPeak <= kSilenceThreshold)
      silentFramesAfterPlayback += blockFrames;
    else
      silentFramesAfterPlayback = 0;

    if(silentFramesAfterPlayback >= requiredSilentFrames)
      break;

    if(tailFramesRendered >= maxTailFrames)
      break;
  }

  return WriteWaveFile(
    options.outputPath,
    renderedSamples,
    options.sampleRate,
    options.numOutputChannels,
    options.waveFileFormat,
    errorMessage);
}
} // namespace

bool RenderPresetNoteToWav(const HeadlessRenderOptions& options, std::string* errorMessage)
{
  if(!ValidateCommonRenderOptions(options, errorMessage))
    return false;

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

  const int64_t noteDurationFrames = std::max<int64_t>(
    1,
    static_cast<int64_t>(std::llround(options.durationSeconds * options.sampleRate)));
  const std::vector<ScheduledMidiEvent> events = BuildSingleNoteEvents(options, noteDurationFrames);
  return RenderScheduledEventsToWav(options, events, noteDurationFrames, nullptr, errorMessage);
}

bool RenderMidiFileToWav(const HeadlessRenderOptions& options,
                         std::string_view midiPath,
                         std::string* errorMessage)
{
  if(!ValidateCommonRenderOptions(options, errorMessage))
    return false;

  if(midiPath.empty())
  {
    SetErrorMessage(errorMessage, "MIDI path is required");
    return false;
  }

  midi_file::ParsedFile parsedFile;
  if(!midi_file::ParseFile(midiPath, parsedFile, errorMessage))
    return false;

  DetectedBreathCCSources breathCCSources;
  if(!DetectMidiFileBreathCCSources(parsedFile, breathCCSources, errorMessage))
    return false;

  int64_t lastEventFrame = 0;
  const std::vector<ScheduledMidiEvent> events = BuildMidiFileEvents(parsedFile, options.sampleRate, lastEventFrame);
  return RenderScheduledEventsToWav(options, events, lastEventFrame, &breathCCSources, errorMessage);
}

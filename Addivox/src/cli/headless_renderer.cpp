#include "headless_renderer.h"

#include "IPlugConstants.h"
#include "IPlugMidi.h"
#include "midi/breath_control.h"
#include "midi_file_parser.h"
#include "settings/effects.h"
#include "settings/global.h"
#include "settings/patch_io.h"
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

namespace {
constexpr int kRenderBlockSize = 64;
constexpr double kSilenceThreshold = 1.0e-5;
constexpr double kRequiredSilentTailSeconds = 0.25;
constexpr double kMaxTailSeconds = 8.0;

struct ScheduledMidiEvent {
  int64_t sampleOffset{0};
  iplug::IMidiMsg message;
};

using DetectedBreathCCSources = std::array<std::optional<BreathCCSource>, 16>;

struct WaveFormatDescriptor {
  uint16_t formatTag{0};
  uint16_t bitsPerSample{0};
  uint16_t bytesPerSample{0};
  bool isFloatingPoint{false};
};

bool Fail(std::string* errorMessage, std::string message) {
  if (errorMessage) *errorMessage = std::move(message);
  return false;
}

constexpr double kNoMaximum = std::numeric_limits<double>::infinity();

// A command-line option that replaces one of the patch's settings.
template <typename Settings> struct SettingOverride {
  const char* label;
  std::optional<double> HeadlessRenderOptions::* option;
  double Settings::* setting;
  double minimum;
  double maximum;
};

// Options are validated in this order, so the first invalid one is the one reported.
constexpr SettingOverride<GlobalVoiceSettings> kVoiceSettingOverrides[]{
    {"Level",                  &HeadlessRenderOptions::levelScale,                   &GlobalVoiceSettings::levelScale,                     0.0, 10.0},
    {"Formants",               &HeadlessRenderOptions::formantsScale,                &GlobalVoiceSettings::formantsScale,                  0.0, 3.0},
    {"Resonance",              &HeadlessRenderOptions::resonanceScale,               &GlobalVoiceSettings::resonanceScale,                 0.0, 3.0},
    {"Attack",                 &HeadlessRenderOptions::attackScale,                  &GlobalVoiceSettings::attackScale,                    0.0, 100.0},
    {"Release",                &HeadlessRenderOptions::releaseScale,                 &GlobalVoiceSettings::releaseScale,                   0.0, 100.0},
    {"Tuning",                 &HeadlessRenderOptions::tuningCents,                  &GlobalVoiceSettings::tuningCents,                  -50.0, 50.0},
    {"Pan offset",             &HeadlessRenderOptions::panOffset,                    &GlobalVoiceSettings::panOffset,                     -1.0, 1.0},
    {"Level variation amount", &HeadlessRenderOptions::levelVariationAmplitudeScale, &GlobalVoiceSettings::levelVariationAmplitudeScale,   0.0, 100.0},
    {"Level variation rate",   &HeadlessRenderOptions::levelVariationRateScale,      &GlobalVoiceSettings::levelVariationRateScale,        0.0, 100.0},
    {"Pan variation amount",   &HeadlessRenderOptions::panVariationAmplitudeScale,   &GlobalVoiceSettings::panVariationAmplitudeScale,     0.0, 100.0},
    {"Pan variation rate",     &HeadlessRenderOptions::panVariationRateScale,        &GlobalVoiceSettings::panVariationRateScale,          0.0, 100.0},
    {"Pitch variation amount", &HeadlessRenderOptions::pitchVariationAmplitudeScale, &GlobalVoiceSettings::pitchVariationAmplitudeScale,   0.0, 100.0},
    {"Pitch variation rate",   &HeadlessRenderOptions::pitchVariationRateScale,      &GlobalVoiceSettings::pitchVariationRateScale,        0.0, 100.0},
    {"Portamento minimum",     &HeadlessRenderOptions::portamentoTimeAtCC5MinSec,    &GlobalVoiceSettings::portamentoTimeAtCC5MinSec,      0.0, kNoMaximum},
    {"Portamento maximum",     &HeadlessRenderOptions::portamentoTimeAtCC5MaxSec,    &GlobalVoiceSettings::portamentoTimeAtCC5MaxSec,      0.0, kNoMaximum},
};

constexpr SettingOverride<EffectsSettings> kEffectsSettingOverrides[]{
    {"Drive",  &HeadlessRenderOptions::drive,  &EffectsSettings::drive,   0.0, 100.0},
    {"Tone",   &HeadlessRenderOptions::tone,   &EffectsSettings::tone,   -1.0, 1.0},
    {"Chorus", &HeadlessRenderOptions::chorus, &EffectsSettings::chorus,  0.0, 100.0},
    {"Reverb", &HeadlessRenderOptions::reverb, &EffectsSettings::reverb,  0.0, 100.0},
};

template <typename Settings, std::size_t N>
bool ValidateOverrides(const HeadlessRenderOptions& options, const SettingOverride<Settings> (&overrides)[N], std::string* errorMessage) {
  for (const auto& [labelText, option, setting, minimum, maximum] : overrides) {
    const std::optional<double>& value = options.*option;
    const std::string label{labelText};
    if (!value) continue;
    if (!std::isfinite(*value)) return Fail(errorMessage, label + " must be a finite number");
    if (*value >= minimum && *value <= maximum) continue;
    if (maximum == kNoMaximum) return Fail(errorMessage, label + " must be non-negative");
    return Fail(errorMessage, label + " must be in the range " + patch_io::detail::FormatDouble(minimum) + " to " + patch_io::detail::FormatDouble(maximum));
  }
  return true;
}

template <typename Settings, std::size_t N>
void ApplyOverrides(const HeadlessRenderOptions& options, const SettingOverride<Settings> (&overrides)[N], Settings& settings) {
  for (const auto& override : overrides)
    if (const std::optional<double>& value = options.*(override.option)) settings.*(override.setting) = *value;
}

template <std::size_t NumBytes> void WriteLittleEndian(std::ofstream& stream, uint64_t value) {
  std::array<char, NumBytes> bytes{};
  for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<char>((value >> (8 * i)) & 0xFFu);
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteFloat32LE(std::ofstream& stream, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  WriteLittleEndian<4>(stream, bits);
}

void WriteFloat64LE(std::ofstream& stream, double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  WriteLittleEndian<8>(stream, bits);
}

WaveFormatDescriptor DescribeWaveFileFormat(WaveFileFormat waveFileFormat) {
  constexpr uint16_t kWaveFormatPcm = 1;
  constexpr uint16_t kWaveFormatIeeeFloat = 3;

  switch (waveFileFormat) {
  case WaveFileFormat::Pcm16:   return {kWaveFormatPcm, 16, 2, false};
  case WaveFileFormat::Pcm24:   return {kWaveFormatPcm, 24, 3, false};
  case WaveFileFormat::Pcm32:   return {kWaveFormatPcm, 32, 4, false};
  case WaveFileFormat::Float32: return {kWaveFormatIeeeFloat, 32, 4, true};
  case WaveFileFormat::Float64: return {kWaveFormatIeeeFloat, 64, 8, true};
  }

  return {};
}

int32_t QuantizePcmSample(double sample, int bitsPerSample) {
  const int32_t maxValue = (bitsPerSample == 32) ? std::numeric_limits<int32_t>::max() : ((static_cast<int32_t>(1) << (bitsPerSample - 1)) - 1);
  const int32_t minValue = (bitsPerSample == 32) ? std::numeric_limits<int32_t>::min() : (-maxValue - 1);

  if (sample <= -1.0) return minValue;
  if (sample >= 1.0) return maxValue;
  return static_cast<int32_t>(std::llround(sample * static_cast<double>(maxValue)));
}

bool WriteWaveSampleData(std::ofstream& stream, const std::vector<double>& samples, WaveFileFormat waveFileFormat) {
  switch (waveFileFormat) {
  case WaveFileFormat::Pcm16:
    for (const double sample : samples) WriteLittleEndian<2>(stream, static_cast<uint16_t>(QuantizePcmSample(sample, 16)));
    return stream.good();

  case WaveFileFormat::Pcm24:
    for (const double sample : samples) WriteLittleEndian<3>(stream, QuantizePcmSample(sample, 24));
    return stream.good();

  case WaveFileFormat::Pcm32:
    for (const double sample : samples) WriteLittleEndian<4>(stream, static_cast<uint32_t>(QuantizePcmSample(sample, 32)));
    return stream.good();

  case WaveFileFormat::Float32:
    for (const double sample : samples) WriteFloat32LE(stream, static_cast<float>(sample));
    return stream.good();

  case WaveFileFormat::Float64:
    for (const double sample : samples) WriteFloat64LE(stream, sample);
    return stream.good();
  }

  return false;
}

bool WriteWaveFile(std::string_view path, const std::vector<double>& samples, int sampleRate, int numChannels, WaveFileFormat waveFileFormat,
                   std::string* errorMessage) {
  if (sampleRate <= 0) return Fail(errorMessage, "Sample rate must be positive");

  if (numChannels <= 0) return Fail(errorMessage, "Channel count must be positive");

  if (samples.size() % static_cast<std::size_t>(numChannels) != 0) return Fail(errorMessage, "Rendered sample buffer is not aligned to the channel count");

  const WaveFormatDescriptor format = DescribeWaveFileFormat(waveFileFormat);
  const std::string parentPath = patch_io::detail::ParentPath(path);
  if (!parentPath.empty() && !patch_io::detail::EnsureDirectoryExists(parentPath)) return Fail(errorMessage, "Could not create output directory");

  std::ofstream stream{std::string{path}, std::ios::binary};
  if (!stream.is_open()) return Fail(errorMessage, "Could not open output WAV file");

  const uint64_t dataChunkSize64 = static_cast<uint64_t>(samples.size()) * format.bytesPerSample;
  const uint64_t factChunkSize64 = format.isFloatingPoint ? 12u : 0u;
  const uint32_t dataChunkPadding = static_cast<uint32_t>(dataChunkSize64 & 1u);
  const uint64_t riffChunkSize64 = 4u + (8u + 16u) + factChunkSize64 + (8u + dataChunkSize64 + dataChunkPadding);
  const uint32_t sampleFrames = static_cast<uint32_t>(samples.size() / static_cast<std::size_t>(numChannels));
  if (dataChunkSize64 > std::numeric_limits<uint32_t>::max() || riffChunkSize64 > std::numeric_limits<uint32_t>::max())
    return Fail(errorMessage, "Output WAV file would exceed 4 GB");

  const uint32_t dataChunkSize = static_cast<uint32_t>(dataChunkSize64);
  const uint32_t byteRate = static_cast<uint32_t>(sampleRate * numChannels) * format.bytesPerSample;
  const uint16_t blockAlign = static_cast<uint16_t>(numChannels * format.bytesPerSample);

  stream.write("RIFF", 4);
  WriteLittleEndian<4>(stream, static_cast<uint32_t>(riffChunkSize64));
  stream.write("WAVE", 4);

  stream.write("fmt ", 4);
  WriteLittleEndian<4>(stream, 16u);
  WriteLittleEndian<2>(stream, format.formatTag);
  WriteLittleEndian<2>(stream, static_cast<uint16_t>(numChannels));
  WriteLittleEndian<4>(stream, static_cast<uint32_t>(sampleRate));
  WriteLittleEndian<4>(stream, byteRate);
  WriteLittleEndian<2>(stream, blockAlign);
  WriteLittleEndian<2>(stream, format.bitsPerSample);

  if (format.isFloatingPoint) {
    stream.write("fact", 4);
    WriteLittleEndian<4>(stream, 4u);
    WriteLittleEndian<4>(stream, sampleFrames);
  }

  stream.write("data", 4);
  WriteLittleEndian<4>(stream, dataChunkSize);
  if (!WriteWaveSampleData(stream, samples, waveFileFormat)) return Fail(errorMessage, "Failed while writing the WAV file");

  if (dataChunkPadding != 0) stream.put('\0');

  if (!stream.good()) return Fail(errorMessage, "Failed while writing the WAV file");

  return true;
}

std::vector<ScheduledMidiEvent> BuildSingleNoteEvents(const HeadlessRenderOptions& options, int64_t noteDurationFrames) {
  std::vector<ScheduledMidiEvent> events;
  events.reserve(3);

  iplug::IMidiMsg breathMessage;
  breathMessage.MakeControlChangeMsg(iplug::IMidiMsg::kBreathController, static_cast<double>(options.breathMidiValue) / 127.0, 0, 0);
  events.push_back({0, breathMessage});

  iplug::IMidiMsg noteOnMessage;
  noteOnMessage.MakeNoteOnMsg(options.note, 127, 0, 0);
  events.push_back({0, noteOnMessage});

  iplug::IMidiMsg noteOffMessage;
  noteOffMessage.MakeNoteOffMsg(options.note, 0, 0);
  events.push_back({noteDurationFrames, noteOffMessage});

  return events;
}

std::vector<ScheduledMidiEvent> BuildMidiFileEvents(const midi_file::ParsedFile& parsedFile, int sampleRate, int64_t& lastEventFrame) {
  std::vector<ScheduledMidiEvent> events;
  events.reserve(parsedFile.events.size());

  lastEventFrame = 0;
  for (const midi_file::TimedEvent& timedEvent : parsedFile.events) {
    const int64_t sampleOffset = std::max<int64_t>(0, static_cast<int64_t>(std::llround(timedEvent.timeSeconds * static_cast<double>(sampleRate))));
    lastEventFrame = std::max(lastEventFrame, sampleOffset);
    events.push_back({sampleOffset, timedEvent.message});
  }

  return events;
}

bool DetectMidiFileBreathCCSources(const midi_file::ParsedFile& parsedFile, DetectedBreathCCSources& breathCCSources, std::string* errorMessage) {
  std::array<std::array<bool, 128>, 16> seenControllers{};
  breathCCSources.fill(std::nullopt);

  for (const midi_file::TimedEvent& timedEvent : parsedFile.events) {
    const iplug::IMidiMsg& message = timedEvent.message;
    if (message.StatusMsg() != iplug::IMidiMsg::kControlChange) continue;

    const int controller = std::clamp(static_cast<int>(message.mData1), 0, 127);
    if (!IsSupportedBreathCCController(controller)) continue;

    seenControllers[static_cast<std::size_t>(std::clamp(message.Channel(), 0, 15))][controller] = true;
  }

  bool foundBreathCCSource = false;
  for (int channel = 0; channel < 16; ++channel) {
    breathCCSources[channel] = DetectBreathCCSource(seenControllers[channel]);
    foundBreathCCSource = foundBreathCCSource || breathCCSources[channel].has_value();
  }

  if (foundBreathCCSource) return true;

  return Fail(errorMessage, "MIDI file contains no supported breath controllers; expected CC2/34, CC2, CC11/43, CC11, CC7/39, CC7, or CC1");
}

void ApplyPatchAndOverrides(SynthEngine& engine, const patch_io::PatchDocument& document, const HeadlessRenderOptions& options) {
  GlobalVoiceSettings voiceSettings = global_settings::Sanitize(document.voiceSettings);
  EffectsSettings effectsSettings = effects_settings::Sanitize(document.effectsSettings);

  ApplyOverrides(options, kVoiceSettingOverrides, voiceSettings);
  ApplyOverrides(options, kEffectsSettingOverrides, effectsSettings);
  engine.mGlobalVoiceSettings = global_settings::Sanitize(voiceSettings);
  engine.mEffectsSettings = effects_settings::Sanitize(effectsSettings);
  engine.mTransposeSemitones = static_cast<double>(options.transposeSemitones.value_or(0));
  engine.Reset(static_cast<double>(options.sampleRate), kRenderBlockSize);
  engine.SetCompoundPatch(document.compoundPatch);
}

bool ValidateCommonRenderOptions(const HeadlessRenderOptions& options, std::string* errorMessage) {
  if (options.patchPath.empty()) return Fail(errorMessage, "Patch path is required");

  if (options.outputPath.empty()) return Fail(errorMessage, "Output path is required");

  if (options.sampleRate <= 0) return Fail(errorMessage, "Sample rate must be positive");

  if (options.numOutputChannels != 1 && options.numOutputChannels != 2) return Fail(errorMessage, "Output channel count must be 1 or 2");

  if (options.transposeSemitones && (*options.transposeSemitones < -36 || *options.transposeSemitones > 36))
    return Fail(errorMessage, "Transpose must be in the range -36 to 36 semitones");

  return ValidateOverrides(options, kVoiceSettingOverrides, errorMessage) && ValidateOverrides(options, kEffectsSettingOverrides, errorMessage);
}

using RenderBlock = std::array<iplug::sample, kRenderBlockSize>;

// Appends a rendered block to the interleaved (or mono) output samples and returns the block's peak level.
double AppendBlock(const RenderBlock& leftBlock, const RenderBlock& rightBlock, int numOutputChannels, std::vector<double>& samples) {
  double blockPeak = 0.0;
  for (std::size_t frame = 0; frame < leftBlock.size(); ++frame) {
    const double left = static_cast<double>(leftBlock[frame]);
    const double right = static_cast<double>(rightBlock[frame]);
    blockPeak = std::max(blockPeak, std::max(std::abs(left), std::abs(right)));

    if (numOutputChannels == 1) samples.push_back(0.5 * (left + right));
    else
      samples.insert(samples.end(), {left, right});
  }
  return blockPeak;
}

bool RenderScheduledEventsToWav(const HeadlessRenderOptions& options, const std::vector<ScheduledMidiEvent>& events, int64_t lastEventFrame,
                                const DetectedBreathCCSources* breathCCSources, std::string* errorMessage) {
  patch_io::PatchDocument document;
  if (!patch_io::LoadPatchFromFile(options.patchPath, document, errorMessage)) return false;

  SynthEngine engine;
  ApplyPatchAndOverrides(engine, document, options);
  if (breathCCSources) {
    for (int channel = 0; channel < 16; ++channel) {
      const auto& source = (*breathCCSources)[channel];
      if (source.has_value()) engine.SetBreathCCSourceForChannel(channel, *source);
    }
  }

  const int64_t requiredSilentFrames = std::max<int64_t>(1, static_cast<int64_t>(std::llround(kRequiredSilentTailSeconds * options.sampleRate)));
  const int64_t maxTailFrames = std::max<int64_t>(0, static_cast<int64_t>(std::llround(kMaxTailSeconds * options.sampleRate)));

  std::vector<double> renderedSamples;
  renderedSamples.reserve(static_cast<std::size_t>((lastEventFrame + maxTailFrames + kRenderBlockSize) * options.numOutputChannels));

  RenderBlock leftBlock{};
  RenderBlock rightBlock{};
  iplug::sample* outputs[2] = {leftBlock.data(), rightBlock.data()};

  std::size_t nextEventIndex = 0;
  int64_t renderedFrames = 0;
  int64_t silentFramesAfterPlayback = 0;
  int64_t tailFramesRendered = 0;

  do {
    // Events are in time order, and all those falling in this block are sent before it is rendered.
    for (; nextEventIndex < events.size() && events[nextEventIndex].sampleOffset < renderedFrames + kRenderBlockSize; ++nextEventIndex) {
      iplug::IMidiMsg message = events[nextEventIndex].message;
      message.mOffset = static_cast<int>(events[nextEventIndex].sampleOffset - renderedFrames);
      engine.ProcessMidiMsg(message);
    }

    engine.ProcessBlock(outputs, kRenderBlockSize);
    const double blockPeak = AppendBlock(leftBlock, rightBlock, options.numOutputChannels, renderedSamples);
    renderedFrames += kRenderBlockSize;
    if (renderedFrames <= lastEventFrame) continue;

    tailFramesRendered += kRenderBlockSize;
    silentFramesAfterPlayback = blockPeak <= kSilenceThreshold ? silentFramesAfterPlayback + kRenderBlockSize : 0;
  } while (renderedFrames <= lastEventFrame || (silentFramesAfterPlayback < requiredSilentFrames && tailFramesRendered < maxTailFrames));

  return WriteWaveFile(options.outputPath, renderedSamples, options.sampleRate, options.numOutputChannels, options.waveFileFormat, errorMessage);
}
} // namespace

bool RenderPatchNoteToWav(const HeadlessRenderOptions& options, std::string* errorMessage) {
  if (!ValidateCommonRenderOptions(options, errorMessage)) return false;

  if (options.note < 0 || options.note > 127) return Fail(errorMessage, "Note must be in the MIDI range 0-127");

  if (!(options.durationSeconds > 0.0) || !std::isfinite(options.durationSeconds)) return Fail(errorMessage, "Duration must be a positive number of seconds");

  if (options.breathMidiValue < 0 || options.breathMidiValue > 127) return Fail(errorMessage, "Breath must be in the MIDI CC range 0-127");

  const int64_t noteDurationFrames = std::max<int64_t>(1, static_cast<int64_t>(std::llround(options.durationSeconds * options.sampleRate)));
  const std::vector<ScheduledMidiEvent> events = BuildSingleNoteEvents(options, noteDurationFrames);
  return RenderScheduledEventsToWav(options, events, noteDurationFrames, nullptr, errorMessage);
}

bool RenderMidiFileToWav(const HeadlessRenderOptions& options, std::string_view midiPath, std::string* errorMessage) {
  if (!ValidateCommonRenderOptions(options, errorMessage)) return false;

  if (midiPath.empty()) return Fail(errorMessage, "MIDI path is required");

  midi_file::ParsedFile parsedFile;
  if (!midi_file::ParseFile(midiPath, parsedFile, errorMessage)) return false;

  DetectedBreathCCSources breathCCSources;
  if (!DetectMidiFileBreathCCSources(parsedFile, breathCCSources, errorMessage)) return false;

  int64_t lastEventFrame = 0;
  const std::vector<ScheduledMidiEvent> events = BuildMidiFileEvents(parsedFile, options.sampleRate, lastEventFrame);
  return RenderScheduledEventsToWav(options, events, lastEventFrame, &breathCCSources, errorMessage);
}

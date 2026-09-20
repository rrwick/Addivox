#include "headless_renderer.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace {
void PrintUsage(std::ostream& stream) {
  stream << "\n"
         << "Addivox CLI tool\n"
         << "\n"
         << "Usage examples:\n"
         << "  addivox --patch brass.toml --note 60 --seconds 5 --breath 104 "
            "-o brass_C4_ff.wav\n"
         << "  addivox --patch brass.toml --midi song.mid -o song.wav\n"
         << "\n"
         << "Required:\n"
         << "  -p, --patch PATH        Patch TOML file to load\n"
         << "  -o, --output PATH        Output WAV file\n"
         << "\n"
         << "Single-note playback:\n"
         << "      --note N             MIDI note number to render\n"
         << "      --seconds N          Note duration in seconds\n"
         << "      --breath N           Breath CC value 0-127 sent before "
            "note-on\n"
         << "\n"
         << "MIDI playback:\n"
         << "      --midi PATH          MIDI file to render\n"
         << "                           Breath source is auto-detected from "
            "CC2/34, CC2, CC11/43, CC11, CC7/39, CC7, or CC1\n"
         << "\n"
         << "Envelope:\n"
         << "      --attack N           Attack scaling, 0-100\n"
         << "      --release N          Release scaling, 0-100\n"
         << "\n"
         << "Tuning:\n"
         << "      --transpose N        Transpose in semitones, -36 to 36 "
            "(default: 0)\n"
         << "      --tuning N           Tuning offset in cents, -50 to 50 "
            "(default: 0)\n"
         << "      --port_min N         Portamento minimum in seconds\n"
         << "      --port_max N         Portamento maximum in seconds\n"
         << "\n"
         << "Output:\n"
         << "      --level N            Level scaling, 0-10\n"
         << "      --pan N              Pan offset, -1 to 1 (default: 0)\n"
         << "\n"
         << "Variation:\n"
         << "      --lvl_var_amt N      Level variation amount, 0 to 100\n"
         << "      --lvl_var_rate N     Level variation rate, 0 to 100\n"
         << "      --pan_var_amt N      Pan variation amount, 0 to 100\n"
         << "      --pan_var_rate N     Pan variation rate, 0 to 100\n"
         << "      --pch_var_amt N      Pitch variation amount, 0 to 100\n"
         << "      --pch_var_rate N     Pitch variation rate, 0 to 100\n"
         << "\n"
         << "Effects:\n"
         << "      --drive N            Drive amount, 0-100\n"
         << "      --tone N             Tone amount, -1 to 1\n"
         << "      --chorus N           Chorus amount, 0-100\n"
         << "      --reverb N           Reverb amount, 0-100 (default: 0)\n"
         << "\n"
         << "Audio:\n"
         << "      --sample_rate N      Output sample rate in Hz (default: "
            "48000)\n"
         << "      --wav_format FORMAT  WAV sample format: pcm16, pcm24, "
            "pcm32, f32, f64 (default: pcm24)\n"
         << "      --mono               Write a mono WAV file\n"
         << "      --stereo             Write a stereo WAV file (default)\n"
         << "\n"
         << "Other:\n"
         << "  -h, --help               Show this help message\n"
         << "\n";
}

bool ParseIntArgument(std::string_view text, int& value) {
  if (text.empty()) return false;

  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(std::string{text}.c_str(), &end, 10);
  if (errno != 0 || !end || *end != '\0' || parsed < static_cast<long>(std::numeric_limits<int>::min()) ||
      parsed > static_cast<long>(std::numeric_limits<int>::max())) {
    return false;
  }

  value = static_cast<int>(parsed);
  return true;
}

bool ParseDoubleArgument(std::string_view text, double& value) {
  if (text.empty()) return false;

  errno = 0;
  char* end = nullptr;
  const double parsed = std::strtod(std::string{text}.c_str(), &end);
  if (errno != 0 || !end || *end != '\0') return false;

  value = parsed;
  return true;
}

bool ParseWaveFormatArgument(std::string_view text, WaveFileFormat& value) {
  std::string normalized;
  normalized.reserve(text.size());
  for (const char ch : text) normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

  if (normalized == "pcm16") value = WaveFileFormat::Pcm16;
  else if (normalized == "pcm24")
    value = WaveFileFormat::Pcm24;
  else if (normalized == "pcm32")
    value = WaveFileFormat::Pcm32;
  else if (normalized == "f32")
    value = WaveFileFormat::Float32;
  else if (normalized == "f64")
    value = WaveFileFormat::Float64;
  else
    return false;

  return true;
}

bool ReadStringValue(int argc, char** argv, int& index, std::string& value, std::string& errorMessage) {
  if (index + 1 >= argc) {
    errorMessage = "Missing value for option " + std::string{argv[index]};
    return false;
  }

  ++index;
  value = argv[index];
  return true;
}

template <typename ValueT, typename ParseFunc>
bool ReadParsedValue(int argc, char** argv, int& index, ValueT& value, ParseFunc&& parse, std::string_view label, std::string& errorMessage) {
  std::string text;
  if (!ReadStringValue(argc, argv, index, text, errorMessage)) return false;

  if (!parse(text, value)) {
    errorMessage = "Invalid value for " + std::string{label} + ": " + text;
    return false;
  }

  return true;
}

template <typename ValueT, typename ParseFunc>
bool ReadParsedValue(int argc, char** argv, int& index, std::optional<ValueT>& value, ParseFunc&& parse, std::string_view label,
                     std::string& errorMessage) {
  ValueT parsed{};
  if (!ReadParsedValue(argc, argv, index, parsed, std::forward<ParseFunc>(parse), label, errorMessage)) return false;
  value = parsed;
  return true;
}

std::string_view CanonicalOption(std::string_view option) {
  if (option == "--tuning-cents" || option == "--pitch" || option == "--pitch-offset") return "--tuning";
  if (option == "--pan-offset") return "--pan";
  if (option == "--sample-rate") return "--sample_rate";
  if (option == "--wav-format") return "--wav_format";
  return option;
}

std::optional<double>* FindDoubleOption(HeadlessRenderOptions& options, std::string_view name) {
  struct Option {
    std::string_view name;
    std::optional<double> HeadlessRenderOptions::* member;
  };
  static constexpr Option kOptions[]{
      {"--reverb",       &HeadlessRenderOptions::reverb},
      {"--drive",        &HeadlessRenderOptions::drive},
      {"--tone",         &HeadlessRenderOptions::tone},
      {"--chorus",       &HeadlessRenderOptions::chorus},
      {"--attack",       &HeadlessRenderOptions::attackScale},
      {"--release",      &HeadlessRenderOptions::releaseScale},
      {"--level",        &HeadlessRenderOptions::levelScale},
      {"--tuning",       &HeadlessRenderOptions::tuningCents},
      {"--pan",          &HeadlessRenderOptions::panOffset},
      {"--port_min",     &HeadlessRenderOptions::portamentoTimeAtCC5MinSec},
      {"--port_max",     &HeadlessRenderOptions::portamentoTimeAtCC5MaxSec},
      {"--lvl_var_amt",  &HeadlessRenderOptions::levelVariationAmplitudeScale},
      {"--lvl_var_rate", &HeadlessRenderOptions::levelVariationRateScale},
      {"--pan_var_amt",  &HeadlessRenderOptions::panVariationAmplitudeScale},
      {"--pan_var_rate", &HeadlessRenderOptions::panVariationRateScale},
      {"--pch_var_amt",  &HeadlessRenderOptions::pitchVariationAmplitudeScale},
      {"--pch_var_rate", &HeadlessRenderOptions::pitchVariationRateScale},
  };
  for (const auto& option : kOptions)
    if (option.name == name) return &(options.*option.member);
  return nullptr;
}

struct PlaybackArguments {
  std::string midiPath;
  std::optional<int> note;
  std::optional<double> seconds;
  std::optional<int> breath;
  bool showHelp{false};
};

bool ParseArguments(int argc, char** argv, HeadlessRenderOptions& options, PlaybackArguments& playback, std::string& errorMessage) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = CanonicalOption(argv[index]);

    if (argument == "-h" || argument == "--help") {
      playback.showHelp = true;
      return true;
    }
    if (argument == "-p" || argument == "--patch") {
      if (!ReadStringValue(argc, argv, index, options.patchPath, errorMessage)) break;
      continue;
    }
    if (argument == "-o" || argument == "--output") {
      if (!ReadStringValue(argc, argv, index, options.outputPath, errorMessage)) break;
      continue;
    }
    if (argument == "--midi") {
      if (!ReadStringValue(argc, argv, index, playback.midiPath, errorMessage)) break;
      continue;
    }
    if (argument == "--note") {
      if (!ReadParsedValue(argc, argv, index, playback.note, ParseIntArgument, argument, errorMessage)) break;
      continue;
    }
    if (argument == "--seconds") {
      if (!ReadParsedValue(argc, argv, index, playback.seconds, ParseDoubleArgument, argument, errorMessage)) break;
      continue;
    }
    if (argument == "--breath") {
      if (!ReadParsedValue(argc, argv, index, playback.breath, ParseIntArgument, argument, errorMessage)) break;
      continue;
    }
    if (argument == "--sample_rate") {
      if (!ReadParsedValue(argc, argv, index, options.sampleRate, ParseIntArgument, "--sample_rate", errorMessage)) break;
      continue;
    }
    if (argument == "--wav_format") {
      if (!ReadParsedValue(argc, argv, index, options.waveFileFormat, ParseWaveFormatArgument, "--wav_format", errorMessage)) {
        break;
      }
      continue;
    }
    if (argument == "--mono") {
      options.numOutputChannels = 1;
      continue;
    }
    if (argument == "--stereo") {
      options.numOutputChannels = 2;
      continue;
    }
    if (argument == "--transpose") {
      if (!ReadParsedValue(argc, argv, index, options.transposeSemitones, ParseIntArgument, argument, errorMessage)) break;
      continue;
    }
    if (auto* value = FindDoubleOption(options, argument)) {
      if (!ReadParsedValue(argc, argv, index, *value, ParseDoubleArgument, argument, errorMessage)) break;
      continue;
    }

    errorMessage = "Unknown option: " + std::string{argument};
    break;
  }

  return errorMessage.empty();
}
} // namespace

int main(int argc, char** argv) {
  HeadlessRenderOptions options;
  PlaybackArguments playback;
  std::string errorMessage;
  if (!ParseArguments(argc, argv, options, playback, errorMessage)) {
    std::cerr << errorMessage << "\n\n";
    PrintUsage(std::cerr);
    return 1;
  }
  if (playback.showHelp) {
    PrintUsage(std::cout);
    return 0;
  }

  if (options.patchPath.empty()) {
    std::cerr << "Missing required option --patch\n\n";
    PrintUsage(std::cerr);
    return 1;
  }

  if (options.outputPath.empty()) {
    std::cerr << "Missing required option --output\n\n";
    PrintUsage(std::cerr);
    return 1;
  }

  const int singleNoteOptionCount =
      static_cast<int>(playback.note.has_value()) + static_cast<int>(playback.seconds.has_value()) + static_cast<int>(playback.breath.has_value());
  const bool hasMidi = !playback.midiPath.empty();

  if (hasMidi && singleNoteOptionCount > 0) {
    std::cerr << "Cannot combine --midi with --note, --seconds, or --breath\n";
    return 1;
  }

  if (!hasMidi && singleNoteOptionCount == 0) {
    std::cerr << "Must provide either --midi or all of --note, --seconds, and "
                 "--breath\n";
    return 1;
  }

  if (!hasMidi && singleNoteOptionCount != 3) {
    std::cerr << "Single-note playback requires --note, --seconds, and --breath\n";
    return 1;
  }

  if (hasMidi) {
    if (!RenderMidiFileToWav(options, playback.midiPath, &errorMessage)) {
      std::cerr << errorMessage << '\n';
      return 1;
    }

    return 0;
  }

  options.note = *playback.note;
  options.durationSeconds = *playback.seconds;
  options.breathMidiValue = *playback.breath;

  if (!RenderPatchNoteToWav(options, &errorMessage)) {
    std::cerr << errorMessage << '\n';
    return 1;
  }

  return 0;
}

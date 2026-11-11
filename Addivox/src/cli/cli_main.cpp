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
         << "  addivox --patch brass.toml --note 60 --seconds 5 --breath 104 -o brass_C4_ff.wav\n"
         << "  addivox --patch brass.toml --midi song.mid -o song.wav\n"
         << "\n"
         << "Required:\n"
         << "  -p, --patch PATH        Patch TOML file to load\n"
         << "  -o, --output PATH        Output WAV file\n"
         << "\n"
         << "Single-note playback:\n"
         << "      --note N             MIDI note number to render\n"
         << "      --seconds N          Note duration in seconds\n"
         << "      --breath N           Breath CC value 0-127 sent before note-on\n"
         << "\n"
         << "MIDI playback:\n"
         << "      --midi PATH          MIDI file to render\n"
         << "                           Breath source is auto-detected from CC2/34, CC2, CC11/43, CC11, CC7/39, CC7, or CC1\n"
         << "\n"
         << "Envelope:\n"
         << "      --attack N           Attack scaling, 0-100\n"
         << "      --release N          Release scaling, 0-100\n"
         << "\n"
         << "Tuning:\n"
         << "      --transpose N        Transpose in semitones, -36 to 36 (default: 0)\n"
         << "      --tuning N           Tuning offset in cents, -50 to 50 (default: 0)\n"
         << "      --port_min N         Portamento minimum in seconds\n"
         << "      --port_max N         Portamento maximum in seconds\n"
         << "\n"
         << "EQ:\n"
         << "      --formants N         Formants curve scaling, 0-3\n"
         << "      --resonance N        Resonance curve scaling, 0-3\n"
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
         << "      --sample_rate N      Output sample rate in Hz (default: 48000)\n"
         << "      --wav_format FORMAT  WAV sample format: pcm16, pcm24, pcm32, f32, f64 (default: pcm24)\n"
         << "      --mono               Write a mono WAV file\n"
         << "      --stereo             Write a stereo WAV file (default)\n"
         << "\n"
         << "Other:\n"
         << "  -h, --help               Show this help message\n"
         << "\n";
}

bool ParseIntArgument(std::string_view text, int& value) {
  if (text.empty()) return false;

  const std::string numericText{text};
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(numericText.c_str(), &end, 10);
  if (errno != 0 || !end || *end != '\0' || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;

  value = static_cast<int>(parsed);
  return true;
}

bool ParseDoubleArgument(std::string_view text, double& value) {
  if (text.empty()) return false;

  const std::string numericText{text};
  errno = 0;
  char* end = nullptr;
  const double parsed = std::strtod(numericText.c_str(), &end);
  if (errno != 0 || !end || *end != '\0') return false;

  value = parsed;
  return true;
}

bool ParseWaveFormatArgument(std::string_view text, WaveFileFormat& value) {
  static constexpr std::pair<std::string_view, WaveFileFormat> kFormats[]{
      {"pcm16", WaveFileFormat::Pcm16}, {"pcm24", WaveFileFormat::Pcm24}, {"pcm32", WaveFileFormat::Pcm32},
      {"f32", WaveFileFormat::Float32}, {"f64", WaveFileFormat::Float64},
  };

  std::string lowerCaseText;
  for (const char ch : text) lowerCaseText.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

  for (const auto& [name, format] : kFormats) {
    if (lowerCaseText != name) continue;
    value = format;
    return true;
  }
  return false;
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
      {"--formants",     &HeadlessRenderOptions::formantsScale},
      {"--resonance",    &HeadlessRenderOptions::resonanceScale},
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

// Reads the option at argv[index], along with its value if it takes one.
bool ParseOption(int argc, char** argv, int& index, HeadlessRenderOptions& options, PlaybackArguments& playback, std::string& errorMessage) {
  const std::string_view option = CanonicalOption(argv[index]);
  const auto readString = [&](std::string& value) { return ReadStringValue(argc, argv, index, value, errorMessage); };
  const auto read = [&](auto& value, auto&& parse) { return ReadParsedValue(argc, argv, index, value, parse, option, errorMessage); };

  if (option == "-p" || option == "--patch") return readString(options.patchPath);
  if (option == "-o" || option == "--output") return readString(options.outputPath);
  if (option == "--midi") return readString(playback.midiPath);
  if (option == "--note") return read(playback.note, ParseIntArgument);
  if (option == "--seconds") return read(playback.seconds, ParseDoubleArgument);
  if (option == "--breath") return read(playback.breath, ParseIntArgument);
  if (option == "--sample_rate") return read(options.sampleRate, ParseIntArgument);
  if (option == "--wav_format") return read(options.waveFileFormat, ParseWaveFormatArgument);
  if (option == "--transpose") return read(options.transposeSemitones, ParseIntArgument);
  if (auto* value = FindDoubleOption(options, option)) return read(*value, ParseDoubleArgument);

  const bool isHelp = option == "-h" || option == "--help";
  const bool isChannelCount = option == "--mono" || option == "--stereo";
  if (isHelp) playback.showHelp = true;
  if (isChannelCount) options.numOutputChannels = option == "--mono" ? 1 : 2;
  if (!isHelp && !isChannelCount) errorMessage = "Unknown option: " + std::string{option};
  return isHelp || isChannelCount;
}

// Anything after a help option is ignored.
bool ParseArguments(int argc, char** argv, HeadlessRenderOptions& options, PlaybackArguments& playback, std::string& errorMessage) {
  for (int index = 1; index < argc && !playback.showHelp; ++index)
    if (!ParseOption(argc, argv, index, options, playback, errorMessage)) return false;
  return true;
}
} // namespace

int main(int argc, char** argv) {
  const auto failWithUsage = [](std::string_view message) {
    std::cerr << message << "\n\n";
    PrintUsage(std::cerr);
    return 1;
  };
  const auto fail = [](std::string_view message) {
    std::cerr << message << '\n';
    return 1;
  };

  HeadlessRenderOptions options;
  PlaybackArguments playback;
  std::string errorMessage;
  if (!ParseArguments(argc, argv, options, playback, errorMessage)) return failWithUsage(errorMessage);
  if (playback.showHelp) {
    PrintUsage(std::cout);
    return 0;
  }

  if (options.patchPath.empty()) return failWithUsage("Missing required option --patch");
  if (options.outputPath.empty()) return failWithUsage("Missing required option --output");

  const int singleNoteOptionCount = playback.note.has_value() + playback.seconds.has_value() + playback.breath.has_value();
  const bool hasMidi = !playback.midiPath.empty();
  if (hasMidi && singleNoteOptionCount > 0) return fail("Cannot combine --midi with --note, --seconds, or --breath");
  if (!hasMidi && singleNoteOptionCount == 0) return fail("Must provide either --midi or all of --note, --seconds, and --breath");
  if (!hasMidi && singleNoteOptionCount != 3) return fail("Single-note playback requires --note, --seconds, and --breath");

  if (hasMidi) return RenderMidiFileToWav(options, playback.midiPath, &errorMessage) ? 0 : fail(errorMessage);

  options.note = *playback.note;
  options.durationSeconds = *playback.seconds;
  options.breathMidiValue = *playback.breath;
  return RenderPatchNoteToWav(options, &errorMessage) ? 0 : fail(errorMessage);
}

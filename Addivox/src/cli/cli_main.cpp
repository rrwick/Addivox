#include "headless_renderer.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace
{
void PrintUsage(std::ostream& stream)
{
  stream
    << "\n"
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

bool ParseIntArgument(std::string_view text, int& value)
{
  if(text.empty())
    return false;

  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(std::string{text}.c_str(), &end, 10);
  if(errno != 0 || !end || *end != '\0'
     || parsed < static_cast<long>(std::numeric_limits<int>::min())
     || parsed > static_cast<long>(std::numeric_limits<int>::max()))
  {
    return false;
  }

  value = static_cast<int>(parsed);
  return true;
}

bool ParseDoubleArgument(std::string_view text, double& value)
{
  if(text.empty())
    return false;

  errno = 0;
  char* end = nullptr;
  const double parsed = std::strtod(std::string{text}.c_str(), &end);
  if(errno != 0 || !end || *end != '\0')
    return false;

  value = parsed;
  return true;
}

bool ParseWaveFormatArgument(std::string_view text, WaveFileFormat& value)
{
  std::string normalized;
  normalized.reserve(text.size());
  for(const char ch : text)
    normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

  if(normalized == "pcm16")
    value = WaveFileFormat::Pcm16;
  else if(normalized == "pcm24")
    value = WaveFileFormat::Pcm24;
  else if(normalized == "pcm32")
    value = WaveFileFormat::Pcm32;
  else if(normalized == "f32")
    value = WaveFileFormat::Float32;
  else if(normalized == "f64")
    value = WaveFileFormat::Float64;
  else
    return false;

  return true;
}

bool ReadStringValue(int argc, char** argv, int& index, std::string& value, std::string& errorMessage)
{
  if(index + 1 >= argc)
  {
    errorMessage = "Missing value for option " + std::string{argv[index]};
    return false;
  }

  ++index;
  value = argv[index];
  return true;
}

template <typename ValueT, typename ParseFunc>
bool ReadParsedValue(int argc,
                     char** argv,
                     int& index,
                     ValueT& value,
                     ParseFunc&& parse,
                     std::string_view label,
                     std::string& errorMessage)
{
  std::string text;
  if(!ReadStringValue(argc, argv, index, text, errorMessage))
    return false;

  if(!parse(text, value))
  {
    errorMessage = "Invalid value for " + std::string{label} + ": " + text;
    return false;
  }

  return true;
}
} // namespace

int main(int argc, char** argv)
{
  HeadlessRenderOptions options;
  std::string errorMessage;
  std::string midiPath;
  std::optional<int> note;
  std::optional<double> seconds;
  std::optional<int> breath;

  for(int index = 1; index < argc; ++index)
  {
    const std::string_view argument = argv[index];

    if(argument == "-h" || argument == "--help")
    {
      PrintUsage(std::cout);
      return 0;
    }
    if(argument == "-p" || argument == "--patch")
    {
      if(!ReadStringValue(argc, argv, index, options.patchPath, errorMessage))
        break;
      continue;
    }
    if(argument == "-o" || argument == "--output")
    {
      if(!ReadStringValue(argc, argv, index, options.outputPath, errorMessage))
        break;
      continue;
    }
    if(argument == "--midi")
    {
      if(!ReadStringValue(argc, argv, index, midiPath, errorMessage))
        break;
      continue;
    }
    if(argument == "--note")
    {
      int value = 0;
      if(!ReadParsedValue(argc, argv, index, value, ParseIntArgument, "--note", errorMessage))
        break;
      note = value;
      continue;
    }
    if(argument == "--seconds")
    {
      double value = 0.0;
      if(!ReadParsedValue(
           argc,
           argv,
           index,
           value,
           ParseDoubleArgument,
           "--seconds",
           errorMessage))
      {
        break;
      }
      seconds = value;
      continue;
    }
    if(argument == "--breath")
    {
      int value = 0;
      if(!ReadParsedValue(argc, argv, index, value, ParseIntArgument, "--breath", errorMessage))
        break;
      breath = value;
      continue;
    }
    if(argument == "--sample_rate" || argument == "--sample-rate")
    {
      if(!ReadParsedValue(argc, argv, index, options.sampleRate, ParseIntArgument, "--sample_rate", errorMessage))
        break;
      continue;
    }
    if(argument == "--wav_format" || argument == "--wav-format")
    {
      if(!ReadParsedValue(
           argc,
           argv,
           index,
           options.waveFileFormat,
           ParseWaveFormatArgument,
           "--wav_format",
           errorMessage))
      {
        break;
      }
      continue;
    }
    if(argument == "--mono")
    {
      options.numOutputChannels = 1;
      continue;
    }
    if(argument == "--stereo")
    {
      options.numOutputChannels = 2;
      continue;
    }
    if(argument == "--reverb")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--reverb", errorMessage))
        break;
      options.reverb = value;
      continue;
    }
    if(argument == "--drive")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--drive", errorMessage))
        break;
      options.drive = value;
      continue;
    }
    if(argument == "--tone")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--tone", errorMessage))
        break;
      options.tone = value;
      continue;
    }
    if(argument == "--chorus")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--chorus", errorMessage))
        break;
      options.chorus = value;
      continue;
    }
    if(argument == "--attack")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--attack", errorMessage))
        break;
      options.attackScale = value;
      continue;
    }
    if(argument == "--release")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--release", errorMessage))
        break;
      options.releaseScale = value;
      continue;
    }
    if(argument == "--level")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--level", errorMessage))
        break;
      options.levelScale = value;
      continue;
    }
    if(argument == "--tuning" || argument == "--tuning-cents" || argument == "--pitch" || argument == "--pitch-offset")
    {
      double value = 0.0;
      if(!ReadParsedValue(
           argc,
           argv,
           index,
           value,
           ParseDoubleArgument,
           "--tuning",
           errorMessage))
      {
        break;
      }
      options.tuningCents = value;
      continue;
    }
    if(argument == "--pan" || argument == "--pan-offset")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--pan", errorMessage))
        break;
      options.panOffset = value;
      continue;
    }
    if(argument == "--port_min")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--port_min", errorMessage))
        break;
      options.portamentoTimeAtCC5MinSec = value;
      continue;
    }
    if(argument == "--port_max")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--port_max", errorMessage))
        break;
      options.portamentoTimeAtCC5MaxSec = value;
      continue;
    }
    if(argument == "--transpose")
    {
      int value = 0;
      if(!ReadParsedValue(argc, argv, index, value, ParseIntArgument, "--transpose", errorMessage))
        break;
      options.transposeSemitones = value;
      continue;
    }
    if(argument == "--lvl_var_amt")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--lvl_var_amt", errorMessage))
        break;
      options.levelVariationAmplitudeScale = value;
      continue;
    }
    if(argument == "--lvl_var_rate")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--lvl_var_rate", errorMessage))
        break;
      options.levelVariationRateScale = value;
      continue;
    }
    if(argument == "--pan_var_amt")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--pan_var_amt", errorMessage))
        break;
      options.panVariationAmplitudeScale = value;
      continue;
    }
    if(argument == "--pan_var_rate")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--pan_var_rate", errorMessage))
        break;
      options.panVariationRateScale = value;
      continue;
    }
    if(argument == "--pch_var_amt")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--pch_var_amt", errorMessage))
        break;
      options.pitchVariationAmplitudeScale = value;
      continue;
    }
    if(argument == "--pch_var_rate")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--pch_var_rate", errorMessage))
        break;
      options.pitchVariationRateScale = value;
      continue;
    }

    errorMessage = "Unknown option: " + std::string{argument};
    break;
  }

  if(!errorMessage.empty())
  {
    std::cerr << errorMessage << "\n\n";
    PrintUsage(std::cerr);
    return 1;
  }

  if(options.patchPath.empty())
  {
    std::cerr << "Missing required option --patch\n\n";
    PrintUsage(std::cerr);
    return 1;
  }

  if(options.outputPath.empty())
  {
    std::cerr << "Missing required option --output\n\n";
    PrintUsage(std::cerr);
    return 1;
  }

  const int singleNoteOptionCount = static_cast<int>(note.has_value())
    + static_cast<int>(seconds.has_value())
    + static_cast<int>(breath.has_value());
  const bool hasMidi = !midiPath.empty();

  if(hasMidi && singleNoteOptionCount > 0)
  {
    std::cerr << "Cannot combine --midi with --note, --seconds, or --breath\n";
    return 1;
  }

  if(!hasMidi && singleNoteOptionCount == 0)
  {
    std::cerr << "Must provide either --midi or all of --note, --seconds, and --breath\n";
    return 1;
  }

  if(!hasMidi && singleNoteOptionCount != 3)
  {
    std::cerr << "Single-note playback requires --note, --seconds, and --breath\n";
    return 1;
  }

  if(hasMidi)
  {
    if(!RenderMidiFileToWav(options, midiPath, &errorMessage))
    {
      std::cerr << errorMessage << '\n';
      return 1;
    }

    return 0;
  }

  options.note = *note;
  options.durationSeconds = *seconds;
  options.breathMidiValue = *breath;

  if(!RenderPatchNoteToWav(options, &errorMessage))
  {
    std::cerr << errorMessage << '\n';
    return 1;
  }

  return 0;
}

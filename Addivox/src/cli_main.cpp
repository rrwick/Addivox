#include "headless_renderer.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace
{
void PrintUsage(std::ostream& stream, std::string_view executableName)
{
  stream
    << "Usage: " << executableName << " -p PRESET.toml -o OUTPUT.wav [options]\n"
    << "\n"
    << "Required options:\n"
    << "  -p, --preset PATH        Preset TOML file to load\n"
    << "  -o, --output PATH        Output WAV file\n"
    << "\n"
    << "Render options:\n"
    << "      --note N            MIDI note number to render (default: 60)\n"
    << "      --seconds N         Note duration in seconds (default: 1.0)\n"
    << "      --breath N          Breath CC value 0-127 sent before note-on (default: 127)\n"
    << "      --sample-rate N     Output sample rate in Hz (default: 48000)\n"
    << "      --mono              Write a mono WAV file\n"
    << "      --stereo            Write a stereo WAV file (default)\n"
    << "\n"
    << "Overrides:\n"
    << "      --reverb N          Reverb amount 0-100\n"
    << "      --pitch-offset N    Global pitch offset in cents\n"
    << "      --pan-offset N      Global pan offset -1 to 1\n"
    << "      --transpose N       Global transpose in semitones (-36 to 36)\n"
    << "\n"
    << "Other:\n"
    << "  -h, --help               Show this help message\n"
    << "\n"
    << "The renderer keeps writing tail audio until the output is silent, up to 8 seconds.\n";
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

  for(int index = 1; index < argc; ++index)
  {
    const std::string_view argument = argv[index];

    if(argument == "-h" || argument == "--help")
    {
      PrintUsage(std::cout, argc > 0 ? std::string_view{argv[0]} : std::string_view{"addivox"});
      return 0;
    }
    if(argument == "-p" || argument == "--preset")
    {
      if(!ReadStringValue(argc, argv, index, options.presetPath, errorMessage))
        break;
      continue;
    }
    if(argument == "-o" || argument == "--output")
    {
      if(!ReadStringValue(argc, argv, index, options.outputPath, errorMessage))
        break;
      continue;
    }
    if(argument == "--note")
    {
      if(!ReadParsedValue(argc, argv, index, options.note, ParseIntArgument, "--note", errorMessage))
        break;
      continue;
    }
    if(argument == "--seconds")
    {
      if(!ReadParsedValue(
           argc,
           argv,
           index,
           options.durationSeconds,
           ParseDoubleArgument,
           "--seconds",
           errorMessage))
      {
        break;
      }
      continue;
    }
    if(argument == "--breath")
    {
      if(!ReadParsedValue(argc, argv, index, options.breathMidiValue, ParseIntArgument, "--breath", errorMessage))
        break;
      continue;
    }
    if(argument == "--sample-rate")
    {
      if(!ReadParsedValue(argc, argv, index, options.sampleRate, ParseIntArgument, "--sample-rate", errorMessage))
        break;
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
    if(argument == "--pitch-offset")
    {
      double value = 0.0;
      if(!ReadParsedValue(
           argc,
           argv,
           index,
           value,
           ParseDoubleArgument,
           "--pitch-offset",
           errorMessage))
      {
        break;
      }
      options.pitchOffsetCents = value;
      continue;
    }
    if(argument == "--pan-offset")
    {
      double value = 0.0;
      if(!ReadParsedValue(argc, argv, index, value, ParseDoubleArgument, "--pan-offset", errorMessage))
        break;
      options.panOffset = value;
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

    errorMessage = "Unknown option: " + std::string{argument};
    break;
  }

  if(!errorMessage.empty())
  {
    std::cerr << errorMessage << "\n\n";
    PrintUsage(std::cerr, argc > 0 ? std::string_view{argv[0]} : std::string_view{"addivox"});
    return 1;
  }

  if(!RenderPresetNoteToWav(options, &errorMessage))
  {
    std::cerr << errorMessage << '\n';
    return 1;
  }

  return 0;
}

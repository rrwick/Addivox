#pragma once

#include "IPlugMidi.h"

#include <string>
#include <string_view>
#include <vector>

namespace midi_file
{
struct TimedEvent
{
  double timeSeconds{0.0};
  iplug::IMidiMsg message{};
};

struct ParsedFile
{
  std::vector<TimedEvent> events;
};

bool ParseFile(std::string_view path, ParsedFile& parsedFile, std::string* errorMessage = nullptr);
} // namespace midi_file

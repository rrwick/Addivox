#pragma once

#include "settings_global.h"
#include "settings_oscillator.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <vector>

namespace preset_io
{
struct PresetDocument
{
  std::string name;
  GlobalVoiceSettings voiceSettings{};
  CompoundPreset compoundPreset{};
};

inline constexpr int kFormatVersion = 1;

namespace detail
{
using OscillatorParameter = OscillatorSettings::Parameter;

struct GlobalVoiceSettingDescriptor
{
  const char* key;
  double GlobalVoiceSettings::* member;
};

struct OscillatorParameterDescriptor
{
  const char* key;
  OscillatorParameter parameter;
};

inline constexpr std::array<GlobalVoiceSettingDescriptor, 13> kGlobalVoiceSettingDescriptors{{
  {"levelScale", &GlobalVoiceSettings::levelScale},
  {"attackScale", &GlobalVoiceSettings::attackScale},
  {"releaseScale", &GlobalVoiceSettings::releaseScale},
  {"pitchOffsetCents", &GlobalVoiceSettings::pitchOffsetCents},
  {"panOffset", &GlobalVoiceSettings::panOffset},
  {"intensityVariationAmplitudeScale", &GlobalVoiceSettings::intensityVariationAmplitudeScale},
  {"intensityVariationRateScale", &GlobalVoiceSettings::intensityVariationRateScale},
  {"pitchVariationAmplitudeScale", &GlobalVoiceSettings::pitchVariationAmplitudeScale},
  {"pitchVariationRateScale", &GlobalVoiceSettings::pitchVariationRateScale},
  {"panVariationAmplitudeScale", &GlobalVoiceSettings::panVariationAmplitudeScale},
  {"panVariationRateScale", &GlobalVoiceSettings::panVariationRateScale},
  {"portamentoTimeAtCC5MinSec", &GlobalVoiceSettings::portamentoTimeAtCC5MinSec},
  {"portamentoTimeAtCC5MaxSec", &GlobalVoiceSettings::portamentoTimeAtCC5MaxSec},
}};

inline constexpr std::array<OscillatorParameterDescriptor, OscillatorSettings::kNumParameters>
  kOscillatorParameterDescriptors{{
    {"intensity", OscillatorParameter::intensity},
    {"breath_power", OscillatorParameter::breath_power},
    {"attack", OscillatorParameter::attack},
    {"release", OscillatorParameter::release},
    {"pitch", OscillatorParameter::pitch},
    {"pan", OscillatorParameter::pan},
    {"intensity_variation_amplitude", OscillatorParameter::intensity_variation_amplitude},
    {"intensity_variation_rate", OscillatorParameter::intensity_variation_rate},
    {"pitch_variation_amplitude", OscillatorParameter::pitch_variation_amplitude},
    {"pitch_variation_rate", OscillatorParameter::pitch_variation_rate},
    {"pan_variation_amplitude", OscillatorParameter::pan_variation_amplitude},
    {"pan_variation_rate", OscillatorParameter::pan_variation_rate},
  }};

inline std::string_view Trim(std::string_view text)
{
  std::size_t start = 0;
  while(start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
    ++start;

  std::size_t end = text.size();
  while(end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
    --end;

  return text.substr(start, end - start);
}

inline std::string StripComment(std::string_view line)
{
  bool inString = false;
  bool escaping = false;
  for(std::size_t i = 0; i < line.size(); ++i)
  {
    const char c = line[i];
    if(c == '"' && !escaping)
      inString = !inString;

    if(c == '#' && !inString)
      return std::string{Trim(line.substr(0, i))};

    escaping = (c == '\\' && !escaping);
    if(c != '\\')
      escaping = false;
  }

  return std::string{Trim(line)};
}

inline bool ParseInteger(std::string_view text, int& value)
{
  const std::string trimmed{Trim(text)};
  if(trimmed.empty())
    return false;

  char* endPtr = nullptr;
  errno = 0;
  const long parsed = std::strtol(trimmed.c_str(), &endPtr, 10);
  if(errno != 0 || !endPtr || *endPtr != '\0')
    return false;

  value = static_cast<int>(parsed);
  return true;
}

inline bool ParseDouble(std::string_view text, double& value)
{
  const std::string trimmed{Trim(text)};
  if(trimmed.empty())
    return false;

  char* endPtr = nullptr;
  errno = 0;
  const double parsed = std::strtod(trimmed.c_str(), &endPtr);
  if(errno != 0 || !endPtr || *endPtr != '\0')
    return false;

  value = parsed;
  return true;
}

inline bool ParseQuotedString(std::string_view text, std::string& value)
{
  const std::string_view trimmed = Trim(text);
  if(trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"')
    return false;

  value.clear();
  value.reserve(trimmed.size() - 2);
  bool escaping = false;
  for(std::size_t i = 1; i + 1 < trimmed.size(); ++i)
  {
    const char c = trimmed[i];
    if(escaping)
    {
      switch(c)
      {
        case '\\':
        case '"':
          value.push_back(c);
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          return false;
      }
      escaping = false;
    }
    else if(c == '\\')
      escaping = true;
    else
      value.push_back(c);
  }

  return !escaping;
}

inline bool ParseDoubleArray(std::string_view text, std::vector<double>& values)
{
  const std::string_view trimmed = Trim(text);
  if(trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
    return false;

  values.clear();
  std::size_t start = 1;
  while(start < trimmed.size() - 1)
  {
    const std::size_t commaPos = trimmed.find(',', start);
    const std::size_t end = (commaPos == std::string_view::npos || commaPos >= trimmed.size() - 1)
      ? trimmed.size() - 1
      : commaPos;
    const std::string_view item = Trim(trimmed.substr(start, end - start));
    if(!item.empty())
    {
      double value = 0.0;
      if(!ParseDouble(item, value))
        return false;
      values.push_back(value);
    }

    if(commaPos == std::string_view::npos || commaPos >= trimmed.size() - 1)
      break;

    start = commaPos + 1;
  }

  return true;
}

inline const GlobalVoiceSettingDescriptor* FindGlobalVoiceSettingDescriptor(std::string_view key)
{
  const auto it = std::find_if(
    kGlobalVoiceSettingDescriptors.begin(),
    kGlobalVoiceSettingDescriptors.end(),
    [key](const auto& descriptor) { return key == descriptor.key; });
  return it == kGlobalVoiceSettingDescriptors.end() ? nullptr : &(*it);
}

inline const OscillatorParameterDescriptor* FindOscillatorParameterDescriptor(std::string_view key)
{
  const auto it = std::find_if(
    kOscillatorParameterDescriptors.begin(),
    kOscillatorParameterDescriptors.end(),
    [key](const auto& descriptor) { return key == descriptor.key; });
  return it == kOscillatorParameterDescriptors.end() ? nullptr : &(*it);
}

inline SimplePreset MakeDefaultKeyNotePreset()
{
  SimplePreset::OscillatorArray oscillatorSettings{};
  oscillatorSettings.fill(OscillatorSettings{0.0});
  return SimplePreset{oscillatorSettings};
}

inline void SetOscillatorParameterValues(SimplePreset& preset,
                                         OscillatorParameter parameter,
                                         const std::vector<double>& values)
{
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    preset.SetOscillatorParameter(
      oscillatorIndex,
      parameter,
      values[static_cast<std::size_t>(oscillatorIndex)]);
  }
}

inline int ChooseDefaultSelectedMidiNote(const CompoundPreset& compoundPreset, int preferredMidiNote = 60)
{
  const auto& keyNotePresets = compoundPreset.GetKeyNotePresets();
  if(keyNotePresets.empty())
    return preferredMidiNote;

  auto upper = keyNotePresets.lower_bound(preferredMidiNote);
  if(upper == keyNotePresets.begin())
    return upper->first;
  if(upper == keyNotePresets.end())
    return std::prev(upper)->first;

  const auto lower = std::prev(upper);
  return (std::abs(preferredMidiNote - lower->first) <= std::abs(upper->first - preferredMidiNote))
    ? lower->first
    : upper->first;
}

inline std::string FormatDouble(double value)
{
  if(std::abs(value) < 1.0e-15)
    value = 0.0;

  std::ostringstream stream;
  stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  std::string result = stream.str();

  if(result.find_first_of(".eE") == std::string::npos)
    result += ".0";

  return result;
}

inline std::string MidiNoteToName(int midiNote)
{
  static constexpr std::array<const char*, 12> kNoteNames{
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
  };

  const int clampedMidiNote = std::clamp(midiNote, CompoundPreset::kMinMidiNote, CompoundPreset::kMaxMidiNote);
  const int noteClass = clampedMidiNote % 12;
  const int octave = (clampedMidiNote / 12) - 1;

  std::ostringstream stream;
  stream << kNoteNames[static_cast<std::size_t>(noteClass)] << octave;
  return stream.str();
}

inline std::string EscapeTomlString(std::string_view text)
{
  std::string escaped;
  escaped.reserve(text.size());
  for(const char c : text)
  {
    switch(c)
    {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(c);
        break;
    }
  }

  return escaped;
}

inline void AppendOscillatorParameterArray(std::ostringstream& stream,
                                           const SimplePreset& preset,
                                           const OscillatorParameterDescriptor& descriptor)
{
  stream << descriptor.key << " = [\n";

  constexpr int kValuesPerLine = 8;
  const auto& oscillatorSettings = preset.GetOscillatorSettingsArray();
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    if((oscillatorIndex % kValuesPerLine) == 0)
      stream << "  ";

    stream << FormatDouble(
      oscillatorSettings[static_cast<std::size_t>(oscillatorIndex)].GetParameter(descriptor.parameter));

    const bool lastValue = oscillatorIndex == (SimplePreset::kNumOscillators - 1);
    if(!lastValue)
      stream << ", ";

    if(((oscillatorIndex + 1) % kValuesPerLine) == 0 || lastValue)
      stream << '\n';
  }

  stream << "]\n";
}

inline std::string JoinPath(std::string_view lhs, std::string_view rhs)
{
  if(lhs.empty())
    return std::string{rhs};
  if(rhs.empty())
    return std::string{lhs};

  std::string joined{lhs};
  if(joined.back() != '/')
    joined.push_back('/');

  if(rhs.front() == '/')
    joined.append(rhs.substr(1));
  else
    joined.append(rhs);

  return joined;
}

inline std::string ParentPath(std::string_view path)
{
  const std::size_t slashPos = std::string{path}.find_last_of('/');
  if(slashPos == std::string::npos)
    return {};
  if(slashPos == 0)
    return "/";
  return std::string{path.substr(0, slashPos)};
}

inline std::string FileStem(std::string_view path)
{
  const std::size_t slashPos = std::string{path}.find_last_of('/');
  const std::string_view fileName = slashPos == std::string::npos ? path : path.substr(slashPos + 1);
  const std::size_t dotPos = std::string{fileName}.find_last_of('.');
  return dotPos == std::string::npos ? std::string{fileName} : std::string{fileName.substr(0, dotPos)};
}

inline bool StatPath(std::string_view path, struct stat& info)
{
  return ::stat(std::string{path}.c_str(), &info) == 0;
}

inline bool IsDirectory(std::string_view path)
{
  struct stat info{};
  return StatPath(path, info) && S_ISDIR(info.st_mode);
}

inline bool EnsureDirectoryExists(std::string_view path)
{
  if(path.empty() || path == "/")
    return true;

  if(IsDirectory(path))
    return true;

  const std::string parent = ParentPath(path);
  if(!parent.empty() && !EnsureDirectoryExists(parent))
    return false;

  return ::mkdir(std::string{path}.c_str(), 0755) == 0 || errno == EEXIST;
}

inline bool ReadTextFile(std::string_view path, std::string& text)
{
  std::ifstream stream(std::string{path}, std::ios::binary);
  if(!stream)
    return false;

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  text = buffer.str();
  return true;
}

inline bool WriteTextFile(std::string_view path, const std::string& text)
{
  const std::string parent = ParentPath(path);
  if(!parent.empty())
  {
    if(!EnsureDirectoryExists(parent))
      return false;
  }

  std::ofstream stream(std::string{path}, std::ios::binary);
  if(!stream)
    return false;

  stream << text;
  return stream.good();
}
} // namespace detail

inline std::string SerializePresetToToml(const PresetDocument& document)
{
  std::ostringstream stream;
  stream << "format_version = " << kFormatVersion << '\n';
  stream << "name = \"" << detail::EscapeTomlString(document.name.empty() ? "Preset" : document.name) << "\"\n\n";

  stream << "[voice_settings]\n";
  for(const auto& descriptor : detail::kGlobalVoiceSettingDescriptors)
  {
    stream << descriptor.key << " = "
           << detail::FormatDouble(document.voiceSettings.*(descriptor.member))
           << '\n';
  }

  for(const auto& [midiNote, preset] : document.compoundPreset.GetKeyNotePresets())
  {
    stream << "\n[[key_notes]]\n";
    stream << "midi_note = " << midiNote << '\n';
    stream << "note_name = \"" << detail::EscapeTomlString(detail::MidiNoteToName(midiNote)) << "\"\n";
    for(const auto& descriptor : detail::kOscillatorParameterDescriptors)
      detail::AppendOscillatorParameterArray(stream, preset, descriptor);
  }

  return stream.str();
}

inline bool ParsePresetToml(const std::string& toml, PresetDocument& document, std::string* errorMessage = nullptr)
{
  enum class Section
  {
    Root,
    VoiceSettings,
    KeyNote,
    Ignored
  };

  struct ParsedKeyNote
  {
    int midiNote = 60;
    bool hasMidiNote = false;
    SimplePreset preset = detail::MakeDefaultKeyNotePreset();
  };

  const auto fail = [errorMessage](const std::string& message) {
    if(errorMessage)
      *errorMessage = message;
    return false;
  };

  document = PresetDocument{};

  Section currentSection = Section::Root;
  std::vector<ParsedKeyNote> keyNotes;
  ParsedKeyNote* currentKeyNote = nullptr;
  bool sawFormatVersion = false;

  std::istringstream input{toml};
  std::string rawLine;
  std::string pendingKey;
  std::string pendingValue;
  Section pendingSection = Section::Root;
  ParsedKeyNote* pendingKeyNote = nullptr;
  int lineNumber = 0;

  const auto parseAssignment = [&](std::string_view key,
                                   std::string_view value,
                                   Section section,
                                   ParsedKeyNote* keyNote,
                                   int assignmentLine) {
    if(section == Section::Root)
    {
      if(key == "format_version")
      {
        int formatVersion = 0;
        if(!detail::ParseInteger(value, formatVersion))
          return fail("Invalid format_version on line " + std::to_string(assignmentLine));
        if(formatVersion != kFormatVersion)
          return fail("Unsupported format_version on line " + std::to_string(assignmentLine));
        sawFormatVersion = true;
      }
      else if(key == "name")
      {
        if(!detail::ParseQuotedString(value, document.name))
          return fail("Invalid preset name on line " + std::to_string(assignmentLine));
      }

      return true;
    }

    if(section == Section::VoiceSettings)
    {
      const auto* descriptor = detail::FindGlobalVoiceSettingDescriptor(key);
      if(!descriptor)
        return true;

      double parsedValue = 0.0;
      if(!detail::ParseDouble(value, parsedValue))
        return fail("Invalid voice setting on line " + std::to_string(assignmentLine));

      document.voiceSettings.*(descriptor->member) = parsedValue;
      return true;
    }

    if(section == Section::KeyNote)
    {
      if(!keyNote)
        return fail("Key-note data found before [[key_notes]] on line " + std::to_string(assignmentLine));

      if(key == "midi_note")
      {
        int midiNote = 0;
        if(!detail::ParseInteger(value, midiNote))
          return fail("Invalid midi_note on line " + std::to_string(assignmentLine));
        keyNote->midiNote = std::clamp(midiNote, CompoundPreset::kMinMidiNote, CompoundPreset::kMaxMidiNote);
        keyNote->hasMidiNote = true;
        return true;
      }

      if(key == "note_name")
      {
        std::string ignored;
        if(!detail::ParseQuotedString(value, ignored))
          return fail("Invalid note_name on line " + std::to_string(assignmentLine));
        return true;
      }

      const auto* descriptor = detail::FindOscillatorParameterDescriptor(key);
      if(!descriptor)
        return true;

      std::vector<double> values;
      if(!detail::ParseDoubleArray(value, values))
        return fail("Invalid oscillator array on line " + std::to_string(assignmentLine));
      if(static_cast<int>(values.size()) != SimplePreset::kNumOscillators)
      {
        return fail(
          "Oscillator array must contain "
          + std::to_string(SimplePreset::kNumOscillators)
          + " values on line "
          + std::to_string(assignmentLine));
      }

      detail::SetOscillatorParameterValues(keyNote->preset, descriptor->parameter, values);
    }

    return true;
  };

  while(std::getline(input, rawLine))
  {
    ++lineNumber;
    const std::string line = detail::StripComment(rawLine);
    if(line.empty())
      continue;

    if(!pendingKey.empty())
    {
      pendingValue += line;
      if(line.find(']') == std::string::npos)
        continue;

      if(!parseAssignment(
           pendingKey,
           pendingValue,
           pendingSection,
           pendingKeyNote,
           lineNumber))
      {
        return false;
      }

      pendingKey.clear();
      pendingValue.clear();
      pendingSection = Section::Root;
      pendingKeyNote = nullptr;
      continue;
    }

    const std::string_view trimmedLine = detail::Trim(line);
    if(trimmedLine == "[voice_settings]")
    {
      currentSection = Section::VoiceSettings;
      currentKeyNote = nullptr;
      continue;
    }

    if(trimmedLine == "[[key_notes]]")
    {
      keyNotes.emplace_back();
      currentKeyNote = &keyNotes.back();
      currentSection = Section::KeyNote;
      continue;
    }

    if(trimmedLine.front() == '[' && trimmedLine.back() == ']')
    {
      currentSection = Section::Ignored;
      currentKeyNote = nullptr;
      continue;
    }

    const std::size_t equalsPos = trimmedLine.find('=');
    if(equalsPos == std::string_view::npos)
      return fail("Invalid TOML assignment on line " + std::to_string(lineNumber));

    const std::string key{detail::Trim(trimmedLine.substr(0, equalsPos))};
    const std::string value{detail::Trim(trimmedLine.substr(equalsPos + 1))};
    if(key.empty() || value.empty())
      return fail("Invalid TOML assignment on line " + std::to_string(lineNumber));

    if(!value.empty() && value.front() == '[' && value.find(']') == std::string::npos)
    {
      pendingKey = key;
      pendingValue = value;
      pendingSection = currentSection;
      pendingKeyNote = currentKeyNote;
      continue;
    }

    if(!parseAssignment(key, value, currentSection, currentKeyNote, lineNumber))
      return false;
  }

  if(!pendingKey.empty())
    return fail("Unterminated array at end of file");

  if(!sawFormatVersion)
    return fail("Missing format_version");

  CompoundPreset compoundPreset;
  compoundPreset.ClearKeyNotePresets();
  for(const auto& keyNote : keyNotes)
  {
    if(!keyNote.hasMidiNote)
      return fail("Each [[key_notes]] table must define midi_note");
    compoundPreset.SetKeyNotePreset(keyNote.midiNote, keyNote.preset);
  }

  document.voiceSettings = global_settings::Sanitize(document.voiceSettings);
  document.compoundPreset = compoundPreset;
  return true;
}

inline bool LoadPresetFromFile(std::string_view path,
                               PresetDocument& document,
                               std::string* errorMessage = nullptr)
{
  std::string toml;
  if(!detail::ReadTextFile(path, toml))
  {
    if(errorMessage)
      *errorMessage = "Could not read preset file";
    return false;
  }

  if(!ParsePresetToml(toml, document, errorMessage))
    return false;

  if(document.name.empty())
    document.name = detail::FileStem(path);

  return true;
}

inline bool SavePresetToFile(std::string_view path,
                             const PresetDocument& document,
                             std::string* errorMessage = nullptr)
{
  if(!detail::WriteTextFile(path, SerializePresetToToml(document)))
  {
    if(errorMessage)
      *errorMessage = "Could not write preset file";
    return false;
  }

  return true;
}

inline void FindPresetFilesRecursive(std::string_view directory, std::vector<std::string>& paths)
{
  DIR* dir = opendir(std::string{directory}.c_str());
  if(!dir)
    return;

  while(const dirent* entry = readdir(dir))
  {
    const std::string_view entryName = entry->d_name;
    if(entryName == "." || entryName == "..")
      continue;

    const std::string childPath = detail::JoinPath(directory, entryName);
    struct stat info{};
    if(!detail::StatPath(childPath, info))
      continue;

    if(S_ISDIR(info.st_mode))
    {
      FindPresetFilesRecursive(childPath, paths);
      continue;
    }

    if(S_ISREG(info.st_mode) && childPath.size() >= 5 && childPath.substr(childPath.size() - 5) == ".toml")
      paths.push_back(childPath);
  }

  closedir(dir);
}

inline std::vector<std::string> FindPresetFiles(std::string_view directory)
{
  std::vector<std::string> paths;
  FindPresetFilesRecursive(directory, paths);
  std::sort(paths.begin(), paths.end());
  return paths;
}
} // namespace preset_io

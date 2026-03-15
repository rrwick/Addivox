#pragma once

#include "IPlugPlatform.h"
#include "IPlugUtilities.h"

#if !defined(OS_WIN)
#include <strings.h>
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#endif

#include "dirscan.h"

#include "effects.h"
#include "global.h"
#include "oscillator.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <vector>

#if defined(OS_WIN)
#include <direct.h>
#endif

namespace preset_io
{
struct PresetDocument
{
  std::string name;
  GlobalVoiceSettings voiceSettings{};
  EffectsSettings effectsSettings{};
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

struct EffectsSettingDescriptor
{
  const char* key;
  double EffectsSettings::* member;
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

inline constexpr std::array<EffectsSettingDescriptor, 4> kEffectsSettingDescriptors{{
  {"drive", &EffectsSettings::drive},
  {"tone", &EffectsSettings::tone},
  {"chorus", &EffectsSettings::chorus},
  {"reverb", &EffectsSettings::reverb},
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

inline const EffectsSettingDescriptor* FindEffectsSettingDescriptor(std::string_view key)
{
  const auto it = std::find_if(
    kEffectsSettingDescriptors.begin(),
    kEffectsSettingDescriptors.end(),
    [key](const auto& descriptor) { return key == descriptor.key; });
  return it == kEffectsSettingDescriptors.end() ? nullptr : &(*it);
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
  stream << std::fixed << std::setprecision(12) << value;
  std::string result = stream.str();

  const std::size_t decimalPos = result.find('.');
  if(decimalPos != std::string::npos)
  {
    while(!result.empty() && result.back() == '0')
      result.pop_back();

    if(!result.empty() && result.back() == '.')
      result += '0';
  }

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
  stream << descriptor.key << " = [";
  const auto& oscillatorSettings = preset.GetOscillatorSettingsArray();
  for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
  {
    stream << FormatDouble(
      oscillatorSettings[static_cast<std::size_t>(oscillatorIndex)].GetParameter(descriptor.parameter));

    const bool lastValue = oscillatorIndex == (SimplePreset::kNumOscillators - 1);
    if(!lastValue)
      stream << ", ";
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
  if(joined.back() != '/' && joined.back() != '\\')
  {
#if defined(OS_WIN)
    joined.push_back('\\');
#else
    joined.push_back('/');
#endif
  }

  std::size_t rhsOffset = 0;
  while(rhsOffset < rhs.size() && (rhs[rhsOffset] == '/' || rhs[rhsOffset] == '\\'))
    ++rhsOffset;

  joined.append(rhs.substr(rhsOffset));

  return joined;
}

inline bool IsRootPath(std::string_view path)
{
  if(path == "/" || path == "\\")
    return true;

#if defined(OS_WIN)
  return path.size() == 3
    && std::isalpha(static_cast<unsigned char>(path[0]))
    && path[1] == ':'
    && (path[2] == '/' || path[2] == '\\');
#else
  return false;
#endif
}

inline std::string_view TrimTrailingPathSeparators(std::string_view path)
{
  while(path.size() > 1 && !IsRootPath(path) && (path.back() == '/' || path.back() == '\\'))
    path.remove_suffix(1);
  return path;
}

inline std::string_view FileNameView(std::string_view path)
{
  const std::size_t slashPos = path.find_last_of("/\\");
  return slashPos == std::string_view::npos ? path : path.substr(slashPos + 1);
}

inline bool HasExtension(std::string_view path, std::string_view extension)
{
  const std::string_view fileName = FileNameView(path);
  if(fileName.size() < extension.size())
    return false;

  const std::string_view suffix = fileName.substr(fileName.size() - extension.size());
  for(std::size_t i = 0; i < extension.size(); ++i)
  {
    if(std::tolower(static_cast<unsigned char>(suffix[i]))
       != std::tolower(static_cast<unsigned char>(extension[i])))
    {
      return false;
    }
  }

  return true;
}

inline std::string ParentPath(std::string_view path)
{
  const std::string_view trimmedPath = TrimTrailingPathSeparators(path);
  if(trimmedPath.empty() || IsRootPath(trimmedPath))
    return std::string{trimmedPath};

  const std::size_t slashPos = trimmedPath.find_last_of("/\\");
  if(slashPos == std::string::npos)
    return {};
  if(slashPos == 0)
    return "/";

#if defined(OS_WIN)
  if(slashPos == 2
     && std::isalpha(static_cast<unsigned char>(trimmedPath[0]))
     && trimmedPath[1] == ':')
  {
    return std::string{trimmedPath.substr(0, 3)};
  }
#endif

  return std::string{trimmedPath.substr(0, slashPos)};
}

inline std::string FileStem(std::string_view path)
{
  const std::string_view fileName = FileNameView(path);
  const std::size_t dotPos = fileName.find_last_of('.');
  return dotPos == std::string::npos ? std::string{fileName} : std::string{fileName.substr(0, dotPos)};
}

inline bool PathExists(std::string_view path)
{
#if defined(OS_WIN)
  struct _stat info{};
  return _wstat(UTF8AsUTF16(std::string{path}.c_str()).Get(), &info) == 0;
#else
  struct stat info{};
  return ::stat(std::string{path}.c_str(), &info) == 0;
#endif
}

inline bool IsDirectory(std::string_view path)
{
#if defined(OS_WIN)
  struct _stat info{};
  return _wstat(UTF8AsUTF16(std::string{path}.c_str()).Get(), &info) == 0
    && (info.st_mode & _S_IFDIR) != 0;
#else
  struct stat info{};
  return ::stat(std::string{path}.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

inline bool EnsureDirectoryExists(std::string_view path)
{
  const std::string_view trimmedPath = TrimTrailingPathSeparators(path);
  if(trimmedPath.empty() || IsRootPath(trimmedPath))
    return true;

  if(IsDirectory(trimmedPath))
    return true;

  const std::string parent = ParentPath(trimmedPath);
  if(!parent.empty() && parent != std::string{trimmedPath} && !EnsureDirectoryExists(parent))
    return false;

  errno = 0;
#if defined(OS_WIN)
  return _wmkdir(UTF8AsUTF16(std::string{trimmedPath}.c_str()).Get()) == 0 || errno == EEXIST;
#else
  return ::mkdir(std::string{trimmedPath}.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

inline bool ReadTextFile(std::string_view path, std::string& text)
{
  text.clear();

#if defined(OS_WIN)
  FILE* stream = _wfopen(UTF8AsUTF16(std::string{path}.c_str()).Get(), L"rb");
#else
  FILE* stream = std::fopen(std::string{path}.c_str(), "rb");
#endif
  if(!stream)
    return false;

  std::array<char, 4096> buffer{};
  while(const std::size_t bytesRead = std::fread(buffer.data(), 1, buffer.size(), stream))
    text.append(buffer.data(), bytesRead);

  const bool success = std::ferror(stream) == 0;
  std::fclose(stream);
  return success;
}

inline bool WriteTextFile(std::string_view path, const std::string& text)
{
  const std::string parent = ParentPath(path);
  if(!parent.empty())
  {
    if(!EnsureDirectoryExists(parent))
      return false;
  }

#if defined(OS_WIN)
  FILE* stream = _wfopen(UTF8AsUTF16(std::string{path}.c_str()).Get(), L"wb");
#else
  FILE* stream = std::fopen(std::string{path}.c_str(), "wb");
#endif
  if(!stream)
    return false;

  const std::size_t bytesWritten = std::fwrite(text.data(), 1, text.size(), stream);
  const bool success = bytesWritten == text.size() && std::ferror(stream) == 0;
  std::fclose(stream);
  return success;
}

inline bool DeleteFile(std::string_view path)
{
#if defined(OS_WIN)
  return _wremove(UTF8AsUTF16(std::string{path}.c_str()).Get()) == 0;
#else
  return std::remove(std::string{path}.c_str()) == 0;
#endif
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

  stream << "\n[effects_settings]\n";
  for(const auto& descriptor : detail::kEffectsSettingDescriptors)
  {
    stream << descriptor.key << " = "
           << detail::FormatDouble(document.effectsSettings.*(descriptor.member))
           << '\n';
  }

  bool wroteAllKeyNotes = false;
  for(const auto& descriptor : detail::kOscillatorParameterDescriptors)
  {
    if(!document.compoundPreset.IsAllKeyNotesEnabled(descriptor.parameter))
      continue;

    if(!wroteAllKeyNotes)
    {
      stream << "\n[all_key_notes]\n";
      wroteAllKeyNotes = true;
    }

    stream << descriptor.key << " = [";
    const auto& values = document.compoundPreset.GetAllKeyNotesValues(descriptor.parameter);
    for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
    {
      stream << detail::FormatDouble(values[static_cast<std::size_t>(oscillatorIndex)]);
      if(oscillatorIndex != (SimplePreset::kNumOscillators - 1))
        stream << ", ";
    }

    stream << "]\n";
  }

  for(const auto& [midiNote, preset] : document.compoundPreset.GetKeyNotePresets())
  {
    stream << "\n[[key_notes]]\n";
    stream << "midi_note = " << midiNote << '\n';
    stream << "note_name = \"" << detail::EscapeTomlString(detail::MidiNoteToName(midiNote)) << "\"\n";
    for(const auto& descriptor : detail::kOscillatorParameterDescriptors)
    {
      if(document.compoundPreset.IsAllKeyNotesEnabled(descriptor.parameter))
        continue;

      detail::AppendOscillatorParameterArray(stream, preset, descriptor);
    }
  }

  return stream.str();
}

inline bool ParsePresetToml(const std::string& toml, PresetDocument& document, std::string* errorMessage = nullptr)
{
  enum class Section
  {
    Root,
    VoiceSettings,
    EffectsSettings,
    AllKeyNotes,
    KeyNote,
    Ignored
  };

  struct ParsedKeyNote
  {
    int midiNote = 60;
    bool hasMidiNote = false;
    SimplePreset preset = detail::MakeDefaultKeyNotePreset();
  };

  struct ParsedAllKeyNotesParameter
  {
    bool present = false;
    CompoundPreset::OscillatorParameterValues values{};
  };

  const auto fail = [errorMessage](const std::string& message) {
    if(errorMessage)
      *errorMessage = message;
    return false;
  };

  document = PresetDocument{};

  Section currentSection = Section::Root;
  std::vector<ParsedKeyNote> keyNotes;
  std::array<ParsedAllKeyNotesParameter, OscillatorSettings::kNumParameters> allKeyNotesParameters{};
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

    if(section == Section::EffectsSettings)
    {
      const auto* descriptor = detail::FindEffectsSettingDescriptor(key);
      if(!descriptor)
        return true;

      double parsedValue = 0.0;
      if(!detail::ParseDouble(value, parsedValue))
        return fail("Invalid effects setting on line " + std::to_string(assignmentLine));

      if(descriptor->member == &EffectsSettings::tone && std::abs(parsedValue) > 1.0)
        parsedValue *= 0.01;

      document.effectsSettings.*(descriptor->member) = parsedValue;
      return true;
    }

    if(section == Section::AllKeyNotes)
    {
      const auto* descriptor = detail::FindOscillatorParameterDescriptor(key);
      if(!descriptor)
        return true;

      std::vector<double> values;
      if(!detail::ParseDoubleArray(value, values))
        return fail("Invalid all_key_notes array on line " + std::to_string(assignmentLine));
      if(static_cast<int>(values.size()) != SimplePreset::kNumOscillators)
      {
        return fail(
          "All-key-notes array must contain "
          + std::to_string(SimplePreset::kNumOscillators)
          + " values on line "
          + std::to_string(assignmentLine));
      }

      auto& parsedParameter = allKeyNotesParameters[static_cast<std::size_t>(descriptor->parameter)];
      parsedParameter.present = true;
      for(int oscillatorIndex = 0; oscillatorIndex < SimplePreset::kNumOscillators; ++oscillatorIndex)
      {
        parsedParameter.values[static_cast<std::size_t>(oscillatorIndex)] =
          values[static_cast<std::size_t>(oscillatorIndex)];
      }

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

    if(trimmedLine == "[effects_settings]")
    {
      currentSection = Section::EffectsSettings;
      currentKeyNote = nullptr;
      continue;
    }

    if(trimmedLine == "[all_key_notes]")
    {
      currentSection = Section::AllKeyNotes;
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

  for(const auto& descriptor : detail::kOscillatorParameterDescriptors)
  {
    const auto& parsedParameter = allKeyNotesParameters[static_cast<std::size_t>(descriptor.parameter)];
    if(parsedParameter.present)
      compoundPreset.EnableAllKeyNotes(descriptor.parameter, parsedParameter.values);
  }

  document.voiceSettings = global_settings::Sanitize(document.voiceSettings);
  document.effectsSettings = effects_settings::Sanitize(document.effectsSettings);
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
  WDL_DirScan scan;
  if(scan.First(std::string{directory}.c_str()) != 0)
    return;

  do
  {
    const char* entryName = scan.GetCurrentFN();
    if(!entryName || std::strcmp(entryName, ".") == 0 || std::strcmp(entryName, "..") == 0)
      continue;

    WDL_FastString childPath;
    scan.GetCurrentFullFN(&childPath);
    const int directoryState = scan.GetCurrentIsDirectory();
    if(directoryState != 0 && directoryState != 4)
    {
      FindPresetFilesRecursive(childPath.Get(), paths);
      continue;
    }

    if(detail::HasExtension(childPath.Get(), ".toml"))
      paths.push_back(childPath.Get());
  }
  while(scan.Next() == 0);
}

inline std::vector<std::string> FindPresetFiles(std::string_view directory)
{
  std::vector<std::string> paths;
  FindPresetFilesRecursive(directory, paths);
  std::sort(paths.begin(), paths.end());
  return paths;
}
} // namespace preset_io

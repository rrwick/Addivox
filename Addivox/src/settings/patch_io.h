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
#include <cctype>
#include <cerrno>
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

namespace patch_io {
struct PatchDocument {
  std::string name;
  GlobalVoiceSettings voiceSettings{};
  EffectsSettings effectsSettings{};
  CompoundPatch compoundPatch{};
};

inline constexpr int kFormatVersion = 1;

namespace detail {
using OscillatorParameter = OscillatorSettings::Parameter;

struct GlobalVoiceSettingDescriptor {
  const char* key;
  double GlobalVoiceSettings::* member;
};

struct EffectsSettingDescriptor {
  const char* key;
  double EffectsSettings::* member;
};

struct OscillatorParameterDescriptor {
  const char* key;
  OscillatorParameter parameter;
};

inline constexpr std::array<GlobalVoiceSettingDescriptor, 11> kGlobalVoiceSettingDescriptors{{
    {"portamentoTimeAtCC5MinSec", &GlobalVoiceSettings::portamentoTimeAtCC5MinSec},
    {"portamentoTimeAtCC5MaxSec", &GlobalVoiceSettings::portamentoTimeAtCC5MaxSec},
    {"attackScale", &GlobalVoiceSettings::attackScale},
    {"releaseScale", &GlobalVoiceSettings::releaseScale},
    {"levelVariationAmplitudeScale", &GlobalVoiceSettings::levelVariationAmplitudeScale},
    {"levelVariationRateScale", &GlobalVoiceSettings::levelVariationRateScale},
    {"panVariationAmplitudeScale", &GlobalVoiceSettings::panVariationAmplitudeScale},
    {"panVariationRateScale", &GlobalVoiceSettings::panVariationRateScale},
    {"pitchVariationAmplitudeScale", &GlobalVoiceSettings::pitchVariationAmplitudeScale},
    {"pitchVariationRateScale", &GlobalVoiceSettings::pitchVariationRateScale},
    {"levelScale", &GlobalVoiceSettings::levelScale},
}};

inline constexpr std::array<EffectsSettingDescriptor, 3> kEffectsSettingDescriptors{{
    {"drive", &EffectsSettings::drive},
    {"tone", &EffectsSettings::tone},
    {"chorus", &EffectsSettings::chorus},
}};

inline constexpr std::array<OscillatorParameterDescriptor, OscillatorSettings::kNumParameters> kOscillatorParameterDescriptors{{
    {"level", OscillatorParameter::level},
    {"breath_power", OscillatorParameter::breath_power},
    {"attack", OscillatorParameter::attack},
    {"release", OscillatorParameter::release},
    {"pitch", OscillatorParameter::pitch},
    {"pan", OscillatorParameter::pan},
    {"level_variation_amplitude", OscillatorParameter::level_variation_amplitude},
    {"level_variation_rate", OscillatorParameter::level_variation_rate},
    {"pan_variation_amplitude", OscillatorParameter::pan_variation_amplitude},
    {"pan_variation_rate", OscillatorParameter::pan_variation_rate},
    {"pitch_variation_amplitude", OscillatorParameter::pitch_variation_amplitude},
    {"pitch_variation_rate", OscillatorParameter::pitch_variation_rate},
}};

inline std::string_view Trim(std::string_view text) {
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) ++start;

  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;

  return text.substr(start, end - start);
}

inline std::string_view StripComment(std::string_view line) {
  bool inString = false;
  bool escaping = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '"' && !escaping) inString = !inString;

    if (c == '#' && !inString) return Trim(line.substr(0, i));

    escaping = (c == '\\' && !escaping);
  }

  return Trim(line);
}

inline bool ParseInteger(std::string_view text, int& value) {
  const std::string trimmed{Trim(text)};
  if (trimmed.empty()) return false;

  char* endPtr = nullptr;
  errno = 0;
  const long parsed = std::strtol(trimmed.c_str(), &endPtr, 10);
  if (errno != 0 || !endPtr || *endPtr != '\0') return false;

  value = static_cast<int>(parsed);
  return true;
}

inline bool ParseDouble(std::string_view text, double& value) {
  const std::string trimmed{Trim(text)};
  if (trimmed.empty()) return false;

  char* endPtr = nullptr;
  errno = 0;
  const double parsed = std::strtod(trimmed.c_str(), &endPtr);
  if (errno != 0 || !endPtr || *endPtr != '\0') return false;

  value = parsed;
  return true;
}

inline bool ParseQuotedString(std::string_view text, std::string& value) {
  const std::string_view trimmed = Trim(text);
  if (trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"') return false;

  value.clear();
  value.reserve(trimmed.size() - 2);
  bool escaping = false;
  for (std::size_t i = 1; i + 1 < trimmed.size(); ++i) {
    const char c = trimmed[i];
    if (escaping) {
      switch (c) {
      case '\\':
      case '"':  value.push_back(c); break;
      case 'n':  value.push_back('\n'); break;
      case 't':  value.push_back('\t'); break;
      default:   return false;
      }
      escaping = false;
    } else if (c == '\\')
      escaping = true;
    else
      value.push_back(c);
  }

  return !escaping;
}

inline bool ParseDoubleArray(std::string_view text, std::vector<double>& values) {
  const std::string_view trimmed = Trim(text);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') return false;

  values.clear();
  std::size_t start = 1;
  while (start < trimmed.size() - 1) {
    const std::size_t commaPos = trimmed.find(',', start);
    const std::size_t end = (commaPos == std::string_view::npos || commaPos >= trimmed.size() - 1) ? trimmed.size() - 1 : commaPos;
    const std::string_view item = Trim(trimmed.substr(start, end - start));
    if (!item.empty()) {
      double value = 0.0;
      if (!ParseDouble(item, value)) return false;
      values.push_back(value);
    }

    if (commaPos == std::string_view::npos || commaPos >= trimmed.size() - 1) break;

    start = commaPos + 1;
  }

  return true;
}

// Bad or unsupported advisory metadata must never prevent loading the sound.
inline bool ParseMacroSettings(std::string_view text, MacroSettings& macros) {
  std::vector<double> values;
  if (!ParseDoubleArray(text, values) || values.size() < 2 || !std::isfinite(values[0]) || values[0] < 1.0 ||
      values[0] > std::numeric_limits<int>::max() || std::floor(values[0]) != values[0])
    return false;
  macros = MacroSettings{static_cast<int>(values[0]), {values.begin() + 1, values.end()}};
  return macros.IsValid();
}

template <typename Descriptor, std::size_t N>
inline const Descriptor* FindDescriptor(const std::array<Descriptor, N>& descriptors, std::string_view key) {
  for (const auto& descriptor : descriptors)
    if (key == descriptor.key) return &descriptor;
  return nullptr;
}

inline SimplePatch MakeDefaultKeyNotePatch() {
  SimplePatch::OscillatorArray oscillatorSettings{};
  oscillatorSettings.fill(OscillatorSettings{0.0});
  return SimplePatch{oscillatorSettings};
}

inline void SetOscillatorParameterValues(SimplePatch& patch, OscillatorParameter parameter, const std::vector<double>& values) {
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    patch.SetOscillatorParameter(oscillatorIndex, parameter, values[static_cast<std::size_t>(oscillatorIndex)]);
  }
}

inline int ChooseDefaultSelectedMidiNote(const CompoundPatch& compoundPatch, int preferredMidiNote = 60) {
  const auto& keyNotePatches = compoundPatch.GetKeyNotePatches();
  if (keyNotePatches.empty()) return preferredMidiNote;

  auto upper = keyNotePatches.lower_bound(preferredMidiNote);
  if (upper == keyNotePatches.begin()) return upper->first;
  if (upper == keyNotePatches.end()) return std::prev(upper)->first;

  const auto lower = std::prev(upper);
  return (std::abs(preferredMidiNote - lower->first) <= std::abs(upper->first - preferredMidiNote)) ? lower->first : upper->first;
}

inline std::string FormatDouble(double value) {
  if (std::abs(value) < 1.0e-15) value = 0.0;

  std::ostringstream stream;
  stream << std::fixed << std::setprecision(12) << value;
  std::string result = stream.str();

  const std::size_t decimalPos = result.find('.');
  if (decimalPos != std::string::npos) {
    while (!result.empty() && result.back() == '0') result.pop_back();

    if (!result.empty() && result.back() == '.') result += '0';
  }

  if (result.find_first_of(".eE") == std::string::npos) result += ".0";

  return result;
}

inline std::string MidiNoteToName(int midiNote) {
  static constexpr std::array<const char*, 12> kNoteNames{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

  const int clampedMidiNote = std::clamp(midiNote, CompoundPatch::kMinMidiNote, CompoundPatch::kMaxMidiNote);
  const int noteClass = clampedMidiNote % 12;
  const int octave = (clampedMidiNote / 12) - 1;

  std::ostringstream stream;
  stream << kNoteNames[static_cast<std::size_t>(noteClass)] << octave;
  return stream.str();
}

inline std::string EscapeTomlString(std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (const char c : text) {
    switch (c) {
    case '\\': escaped += "\\\\"; break;
    case '"':  escaped += "\\\""; break;
    case '\n': escaped += "\\n"; break;
    case '\t': escaped += "\\t"; break;
    default:   escaped.push_back(c); break;
    }
  }

  return escaped;
}

inline void AppendOscillatorParameterArray(std::ostringstream& stream, const SimplePatch& patch, const OscillatorParameterDescriptor& descriptor) {
  stream << descriptor.key << " = [";
  const auto& oscillatorSettings = patch.GetOscillatorSettingsArray();
  for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
    stream << FormatDouble(oscillatorSettings[static_cast<std::size_t>(oscillatorIndex)].GetParameter(descriptor.parameter));

    const bool lastValue = oscillatorIndex == (SimplePatch::kNumOscillators - 1);
    if (!lastValue) stream << ", ";
  }

  stream << "]\n";
}

// The first element versions the tab's generator and knob mapping; the rest are normalized positions.
inline void AppendMacroSettings(std::ostringstream& stream, std::string_view key, const MacroSettings& macros) {
  if (!macros.IsValid()) return;
  stream << key << "_macros = [" << macros.version << std::setprecision(std::numeric_limits<double>::max_digits10);
  for (double position : macros.positions) stream << ", " << position;
  stream << "]\n";
}

inline void AppendAlignedStringArray(std::ostringstream& stream, std::string_view key, std::size_t prefixWidth, const std::vector<std::string>& values,
                                     const std::vector<std::size_t>& columnWidths) {
  const std::string prefix = std::string{key} + " = ";
  stream << prefix;
  if (prefixWidth > prefix.size()) stream << std::string(prefixWidth - prefix.size(), ' ');

  stream << '[';
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) stream << ", ";

    const std::size_t width = (i < columnWidths.size()) ? columnWidths[i] : values[i].size();
    if (width > values[i].size()) stream << std::string(width - values[i].size(), ' ');

    stream << values[i];
  }

  stream << "]\n";
}

inline void AppendEqCurveArrays(std::ostringstream& stream, const EqCurve& curve) {
  std::vector<std::string> frequenciesHz;
  std::vector<std::string> gainsDb;
  frequenciesHz.reserve(curve.GetPoints().size());
  gainsDb.reserve(curve.GetPoints().size());

  for (const auto& point : curve.GetPoints()) {
    frequenciesHz.push_back(FormatDouble(point.frequencyHz));
    gainsDb.push_back(FormatDouble(point.gainDb));
  }

  std::vector<std::size_t> columnWidths;
  columnWidths.reserve(frequenciesHz.size());
  for (std::size_t i = 0; i < frequenciesHz.size(); ++i) {
    columnWidths.push_back(std::max(frequenciesHz[i].size(), gainsDb[i].size()));
  }

  constexpr std::string_view kEqFreqKey = "eq_freq_hz";
  constexpr std::string_view kEqDbKey = "eq_db";
  const std::size_t prefixWidth = kEqFreqKey.size() + 3; // "key = "
  AppendAlignedStringArray(stream, kEqFreqKey, prefixWidth, frequenciesHz, columnWidths);
  AppendAlignedStringArray(stream, kEqDbKey, prefixWidth, gainsDb, columnWidths);
}

inline std::string JoinPath(std::string_view lhs, std::string_view rhs) {
  if (lhs.empty()) return std::string{rhs};
  if (rhs.empty()) return std::string{lhs};

  std::string joined{lhs};
  if (joined.back() != '/' && joined.back() != '\\') {
#if defined(OS_WIN)
    joined.push_back('\\');
#else
    joined.push_back('/');
#endif
  }

  std::size_t rhsOffset = 0;
  while (rhsOffset < rhs.size() && (rhs[rhsOffset] == '/' || rhs[rhsOffset] == '\\')) ++rhsOffset;

  joined.append(rhs.substr(rhsOffset));

  return joined;
}

inline bool IsRootPath(std::string_view path) {
  if (path == "/" || path == "\\") return true;

#if defined(OS_WIN)
  return path.size() == 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && (path[2] == '/' || path[2] == '\\');
#else
  return false;
#endif
}

inline std::string_view TrimTrailingPathSeparators(std::string_view path) {
  while (path.size() > 1 && !IsRootPath(path) && (path.back() == '/' || path.back() == '\\')) path.remove_suffix(1);
  return path;
}

inline std::string_view FileNameView(std::string_view path) {
  const std::size_t slashPos = path.find_last_of("/\\");
  return slashPos == std::string_view::npos ? path : path.substr(slashPos + 1);
}

inline bool HasExtension(std::string_view path, std::string_view extension) {
  const std::string_view fileName = FileNameView(path);
  if (fileName.size() < extension.size()) return false;

  const std::string_view suffix = fileName.substr(fileName.size() - extension.size());
  for (std::size_t i = 0; i < extension.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(suffix[i])) != std::tolower(static_cast<unsigned char>(extension[i]))) {
      return false;
    }
  }

  return true;
}

inline std::string ParentPath(std::string_view path) {
  const std::string_view trimmedPath = TrimTrailingPathSeparators(path);
  if (trimmedPath.empty() || IsRootPath(trimmedPath)) return std::string{trimmedPath};

  const std::size_t slashPos = trimmedPath.find_last_of("/\\");
  if (slashPos == std::string::npos) return {};
  if (slashPos == 0) return "/";

#if defined(OS_WIN)
  if (slashPos == 2 && std::isalpha(static_cast<unsigned char>(trimmedPath[0])) && trimmedPath[1] == ':') {
    return std::string{trimmedPath.substr(0, 3)};
  }
#endif

  return std::string{trimmedPath.substr(0, slashPos)};
}

inline std::string FileStem(std::string_view path) {
  const std::string_view fileName = FileNameView(path);
  const std::size_t dotPos = fileName.find_last_of('.');
  return dotPos == std::string::npos ? std::string{fileName} : std::string{fileName.substr(0, dotPos)};
}

inline bool PathExists(std::string_view path) {
#if defined(OS_WIN)
  struct _stat info{};
  return _wstat(UTF8AsUTF16(std::string{path}.c_str()).Get(), &info) == 0;
#else
  struct stat info{};
  return ::stat(std::string{path}.c_str(), &info) == 0;
#endif
}

inline bool IsDirectory(std::string_view path) {
#if defined(OS_WIN)
  struct _stat info{};
  return _wstat(UTF8AsUTF16(std::string{path}.c_str()).Get(), &info) == 0 && (info.st_mode & _S_IFDIR) != 0;
#else
  struct stat info{};
  return ::stat(std::string{path}.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

inline bool EnsureDirectoryExists(std::string_view path) {
  const std::string_view trimmedPath = TrimTrailingPathSeparators(path);
  if (trimmedPath.empty() || IsRootPath(trimmedPath)) return true;

  if (IsDirectory(trimmedPath)) return true;

  const std::string parent = ParentPath(trimmedPath);
  if (!parent.empty() && parent != std::string{trimmedPath} && !EnsureDirectoryExists(parent)) return false;

  errno = 0;
#if defined(OS_WIN)
  return _wmkdir(UTF8AsUTF16(std::string{trimmedPath}.c_str()).Get()) == 0 || errno == EEXIST;
#else
  return ::mkdir(std::string{trimmedPath}.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

inline bool ReadTextFile(std::string_view path, std::string& text) {
  text.clear();

#if defined(OS_WIN)
  FILE* stream = _wfopen(UTF8AsUTF16(std::string{path}.c_str()).Get(), L"rb");
#else
  FILE* stream = std::fopen(std::string{path}.c_str(), "rb");
#endif
  if (!stream) return false;

  std::array<char, 4096> buffer{};
  while (const std::size_t bytesRead = std::fread(buffer.data(), 1, buffer.size(), stream)) text.append(buffer.data(), bytesRead);

  const bool success = std::ferror(stream) == 0;
  std::fclose(stream);
  return success;
}

inline bool WriteTextFile(std::string_view path, const std::string& text) {
  const std::string parent = ParentPath(path);
  if (!parent.empty() && !EnsureDirectoryExists(parent)) return false;

#if defined(OS_WIN)
  FILE* stream = _wfopen(UTF8AsUTF16(std::string{path}.c_str()).Get(), L"wb");
#else
  FILE* stream = std::fopen(std::string{path}.c_str(), "wb");
#endif
  if (!stream) return false;

  const std::size_t bytesWritten = std::fwrite(text.data(), 1, text.size(), stream);
  const bool success = bytesWritten == text.size() && std::ferror(stream) == 0;
  std::fclose(stream);
  return success;
}

inline bool DeleteFile(std::string_view path) {
#if defined(OS_WIN)
  return _wremove(UTF8AsUTF16(std::string{path}.c_str()).Get()) == 0;
#else
  return std::remove(std::string{path}.c_str()) == 0;
#endif
}

struct ParsedEqCurve {
  bool hasFreqHz = false;
  bool hasDb = false;
  std::vector<double> freqHz;
  std::vector<double> db;
};

struct ParsedKeyNote {
  int midiNote = 60;
  bool hasMidiNote = false;
  SimplePatch patch = MakeDefaultKeyNotePatch();
  std::array<MacroSettings, OscillatorSettings::kNumParameters> macros{};
  ParsedEqCurve eqCurve;
};

struct ParsedAllKeyNotesParameter {
  bool present = false;
  CompoundPatch::OscillatorParameterValues values{};
  MacroSettings macros;
};

inline bool BuildCompoundPatch(const std::vector<ParsedKeyNote>& keyNotes,
                               const std::array<ParsedAllKeyNotesParameter, OscillatorSettings::kNumParameters>& allKeyNotesParameters,
                               const ParsedEqCurve& allKeyNotesEqCurve, CompoundPatch& compoundPatch, std::string* errorMessage) {
  const auto fail = [errorMessage](const std::string& message) {
    if (errorMessage) *errorMessage = message;
    return false;
  };

  compoundPatch.ClearKeyNotePatches();
  const auto buildEqCurve = [&](const ParsedEqCurve& parsed, const char* contextLabel, EqCurve& curve) {
    if (parsed.freqHz.size() != parsed.db.size()) return fail(std::string{contextLabel} + " EQ frequency and gain arrays must have the same length");

    EqCurve::PointList points;
    points.reserve(parsed.freqHz.size());
    for (std::size_t i = 0; i < parsed.freqHz.size(); ++i) points.push_back({parsed.freqHz[i], parsed.db[i]});

    curve.SetPoints(std::move(points));
    return true;
  };

  for (const auto& keyNote : keyNotes) {
    if (!keyNote.hasMidiNote) return fail("Each [[key_notes]] table must define midi_note");

    if (keyNote.eqCurve.hasFreqHz != keyNote.eqCurve.hasDb)
      return fail("Each [[key_notes]] EQ definition must include both "
                  "eq_freq_hz and eq_db");

    compoundPatch.SetKeyNotePatch(keyNote.midiNote, keyNote.patch);
    for (auto parameter : OscillatorSettings::AllParameters())
      compoundPatch.SetMacroSettings(keyNote.midiNote, parameter, keyNote.macros[static_cast<std::size_t>(parameter)]);

    if (keyNote.eqCurve.hasFreqHz) {
      EqCurve eqCurve;
      if (!buildEqCurve(keyNote.eqCurve, "[[key_notes]]", eqCurve)) return false;

      if (!compoundPatch.SetKeyNoteEqCurve(keyNote.midiNote, eqCurve))
        return fail("Could not apply EQ curve for midi_note " + std::to_string(keyNote.midiNote));
    }
  }

  for (const auto& descriptor : kOscillatorParameterDescriptors) {
    const auto& parsedParameter = allKeyNotesParameters[static_cast<std::size_t>(descriptor.parameter)];
    if (parsedParameter.present) compoundPatch.EnableAllKeyNotes(descriptor.parameter, parsedParameter.values, parsedParameter.macros);
  }

  if (allKeyNotesEqCurve.hasFreqHz != allKeyNotesEqCurve.hasDb) return fail("[all_key_notes] EQ definition must include both eq_freq_hz and eq_db");

  if (allKeyNotesEqCurve.hasFreqHz) {
    EqCurve eqCurve;
    if (!buildEqCurve(allKeyNotesEqCurve, "[all_key_notes]", eqCurve)) return false;

    compoundPatch.EnableAllKeyNotesEq(eqCurve);
  }

  return true;
}

class PatchParser {
public:
  PatchParser(PatchDocument& document, std::string* errorMessage) : mDocument(document), mErrorMessage(errorMessage) {}

  bool Parse(const std::string& toml) {
    mDocument = PatchDocument{};
    if (!ReadAssignments(toml)) return false;
    if (!mSawFormatVersion) return Fail("Missing format_version");

    CompoundPatch compoundPatch;
    if (!BuildCompoundPatch(mKeyNotes, mAllKeyNotesParameters, mAllKeyNotesEqCurve, compoundPatch, mErrorMessage)) return false;

    mDocument.voiceSettings = global_settings::Sanitize(mDocument.voiceSettings);
    mDocument.effectsSettings = effects_settings::Sanitize(mDocument.effectsSettings);
    mDocument.compoundPatch = std::move(compoundPatch);
    return true;
  }

private:
  enum class Section { Root, VoiceSettings, EffectsSettings, AllKeyNotes, KeyNote, Ignored };

  bool Fail(const std::string& message) const {
    if (mErrorMessage) *mErrorMessage = message;
    return false;
  }

  bool ReadAssignments(const std::string& toml) {
    std::istringstream input{toml};
    std::string rawLine;
    std::string pendingKey;
    std::string pendingValue;
    while (std::getline(input, rawLine)) {
      ++mLineNumber;
      const std::string_view line = StripComment(rawLine);
      if (line.empty()) continue;

      if (!pendingKey.empty()) {
        pendingValue += line;
        if (line.find(']') == std::string::npos) continue;

        if (!ParseAssignment(pendingKey, pendingValue)) return false;

        pendingKey.clear();
        pendingValue.clear();
        continue;
      }

      if (line.front() == '[' && line.back() == ']') {
        mCurrentKeyNote = nullptr;
        if (line == "[voice_settings]") mSection = Section::VoiceSettings;
        else if (line == "[effects_settings]")
          mSection = Section::EffectsSettings;
        else if (line == "[all_key_notes]")
          mSection = Section::AllKeyNotes;
        else if (line == "[[key_notes]]") {
          mKeyNotes.emplace_back();
          mCurrentKeyNote = &mKeyNotes.back();
          mSection = Section::KeyNote;
        } else
          mSection = Section::Ignored;
        continue;
      }

      const std::size_t equalsPos = line.find('=');
      if (equalsPos == std::string_view::npos) return Fail("Invalid TOML assignment on line " + std::to_string(mLineNumber));

      const std::string_view key = Trim(line.substr(0, equalsPos));
      const std::string_view value = Trim(line.substr(equalsPos + 1));
      if (key.empty() || value.empty()) return Fail("Invalid TOML assignment on line " + std::to_string(mLineNumber));

      if (value.front() == '[' && value.find(']') == std::string::npos) {
        pendingKey = key;
        pendingValue = value;
        continue;
      }

      if (!ParseAssignment(key, value)) return false;
    }
    if (!pendingKey.empty()) return Fail("Unterminated array at end of file");
    return true;
  }

  bool ParseAssignment(std::string_view key, std::string_view value) {
    switch (mSection) {
    case Section::Root:            return ParseRootAssignment(key, value);
    case Section::VoiceSettings:   return ParseVoiceSetting(key, value);
    case Section::EffectsSettings: return ParseEffectsSetting(key, value);
    case Section::AllKeyNotes:
    case Section::KeyNote:         return ParseNoteAssignment(key, value);
    case Section::Ignored:         return true;
    }
    return true;
  }

  bool ParseRootAssignment(std::string_view key, std::string_view value) {
    if (key == "format_version") {
      int formatVersion = 0;
      if (!ParseInteger(value, formatVersion)) return Fail("Invalid format_version on line " + std::to_string(mLineNumber));
      if (formatVersion > kFormatVersion) return Fail("Unsupported format_version on line " + std::to_string(mLineNumber));
      mSawFormatVersion = true;
    } else if (key == "name") {
      if (!ParseQuotedString(value, mDocument.name)) return Fail("Invalid patch name on line " + std::to_string(mLineNumber));
    }

    return true;
  }

  bool ParseVoiceSetting(std::string_view key, std::string_view value) {
    const auto* descriptor = FindDescriptor(kGlobalVoiceSettingDescriptors, key);
    if (!descriptor) return true;

    double parsedValue = 0.0;
    if (!ParseDouble(value, parsedValue)) return Fail("Invalid voice setting on line " + std::to_string(mLineNumber));

    mDocument.voiceSettings.*(descriptor->member) = parsedValue;
    return true;
  }

  bool ParseEffectsSetting(std::string_view key, std::string_view value) {
    const auto* descriptor = FindDescriptor(kEffectsSettingDescriptors, key);
    if (!descriptor) return true;

    double parsedValue = 0.0;
    if (!ParseDouble(value, parsedValue)) return Fail("Invalid effects setting on line " + std::to_string(mLineNumber));

    if (descriptor->member == &EffectsSettings::tone && std::abs(parsedValue) > 1.0) parsedValue *= 0.01;

    mDocument.effectsSettings.*(descriptor->member) = parsedValue;
    return true;
  }

  bool ParseNoteAssignment(std::string_view key, std::string_view value) {
    constexpr std::string_view macroSuffix = "_macros";
    if (key.size() > macroSuffix.size() && key.substr(key.size() - macroSuffix.size()) == macroSuffix) {
      const auto* descriptor = FindDescriptor(kOscillatorParameterDescriptors, key.substr(0, key.size() - macroSuffix.size()));
      if (!descriptor) return true;
      MacroSettings macros;
      if (!ParseMacroSettings(value, macros)) return true;
      const auto index = static_cast<std::size_t>(descriptor->parameter);
      if (mSection == Section::AllKeyNotes) mAllKeyNotesParameters[index].macros = std::move(macros);
      else if (mCurrentKeyNote)
        mCurrentKeyNote->macros[index] = std::move(macros);
      return true;
    }

    if (mSection == Section::KeyNote) {
      if (!mCurrentKeyNote) return Fail("Key-note data found before [[key_notes]] on line " + std::to_string(mLineNumber));

      if (key == "midi_note") {
        int midiNote = 0;
        if (!ParseInteger(value, midiNote)) return Fail("Invalid midi_note on line " + std::to_string(mLineNumber));
        mCurrentKeyNote->midiNote = std::clamp(midiNote, CompoundPatch::kMinMidiNote, CompoundPatch::kMaxMidiNote);
        mCurrentKeyNote->hasMidiNote = true;
        return true;
      }

      if (key == "note_name") {
        std::string ignored;
        if (!ParseQuotedString(value, ignored)) return Fail("Invalid note_name on line " + std::to_string(mLineNumber));
        return true;
      }
    }

    auto& eqCurve = mSection == Section::AllKeyNotes ? mAllKeyNotesEqCurve : mCurrentKeyNote->eqCurve;
    if (key == "eq_freq_hz" || key == "eq_db") {
      const bool isFrequency = key == "eq_freq_hz";
      if (!ParseDoubleArray(value, isFrequency ? eqCurve.freqHz : eqCurve.db))
        return Fail(std::string{isFrequency ? "Invalid EQ frequency array on line " : "Invalid EQ gain array on line "} + std::to_string(mLineNumber));
      (isFrequency ? eqCurve.hasFreqHz : eqCurve.hasDb) = true;
      return true;
    }

    return ParseOscillatorArray(key, value);
  }

  bool ParseOscillatorArray(std::string_view key, std::string_view value) {
    const auto* descriptor = FindDescriptor(kOscillatorParameterDescriptors, key);
    if (!descriptor) return true;

    const bool allKeyNotes = mSection == Section::AllKeyNotes;
    std::vector<double> values;
    if (!ParseDoubleArray(value, values))
      return Fail(std::string{allKeyNotes ? "Invalid all_key_notes array on line " : "Invalid oscillator array on line "} + std::to_string(mLineNumber));
    if (static_cast<int>(values.size()) != SimplePatch::kNumOscillators)
      return Fail(std::string{allKeyNotes ? "All-key-notes array must contain " : "Oscillator array must contain "} +
                  std::to_string(SimplePatch::kNumOscillators) + " values on line " + std::to_string(mLineNumber));

    if (allKeyNotes) {
      auto& parsedParameter = mAllKeyNotesParameters[static_cast<std::size_t>(descriptor->parameter)];
      parsedParameter.present = true;
      std::copy(values.begin(), values.end(), parsedParameter.values.begin());
    } else
      SetOscillatorParameterValues(mCurrentKeyNote->patch, descriptor->parameter, values);

    return true;
  }

  PatchDocument& mDocument;
  std::string* mErrorMessage;
  Section mSection{Section::Root};
  std::vector<ParsedKeyNote> mKeyNotes;
  std::array<ParsedAllKeyNotesParameter, OscillatorSettings::kNumParameters> mAllKeyNotesParameters{};
  ParsedEqCurve mAllKeyNotesEqCurve{};
  ParsedKeyNote* mCurrentKeyNote{nullptr};
  bool mSawFormatVersion{false};
  int mLineNumber{0};
};
} // namespace detail

inline std::string SerializePatchToToml(const PatchDocument& document, bool includeName = true) {
  std::ostringstream stream;
  stream << "format_version = " << kFormatVersion << '\n';
  if (includeName) stream << "name = \"" << detail::EscapeTomlString(document.name.empty() ? "Patch" : document.name) << "\"\n";
  stream << '\n';

  stream << "[voice_settings]\n";
  for (const auto& descriptor : detail::kGlobalVoiceSettingDescriptors) {
    stream << descriptor.key << " = " << detail::FormatDouble(document.voiceSettings.*(descriptor.member)) << '\n';
  }

  stream << "\n[effects_settings]\n";
  for (const auto& descriptor : detail::kEffectsSettingDescriptors) {
    stream << descriptor.key << " = " << detail::FormatDouble(document.effectsSettings.*(descriptor.member)) << '\n';
  }

  bool wroteAllKeyNotes = false;
  for (const auto& descriptor : detail::kOscillatorParameterDescriptors) {
    if (!document.compoundPatch.IsAllKeyNotesEnabled(descriptor.parameter)) continue;

    if (!wroteAllKeyNotes) {
      stream << "\n[all_key_notes]\n";
      wroteAllKeyNotes = true;
    }

    stream << descriptor.key << " = [";
    const auto& values = document.compoundPatch.GetAllKeyNotesValues(descriptor.parameter);
    for (int oscillatorIndex = 0; oscillatorIndex < SimplePatch::kNumOscillators; ++oscillatorIndex) {
      stream << detail::FormatDouble(values[static_cast<std::size_t>(oscillatorIndex)]);
      if (oscillatorIndex != (SimplePatch::kNumOscillators - 1)) stream << ", ";
    }

    stream << "]\n";
    const auto& notes = document.compoundPatch.GetKeyNotePatches();
    if (!notes.empty())
      detail::AppendMacroSettings(stream, descriptor.key, document.compoundPatch.GetMacroSettings(notes.begin()->first, descriptor.parameter));
  }

  if (document.compoundPatch.IsAllKeyNotesEqEnabled()) {
    if (!wroteAllKeyNotes) {
      stream << "\n[all_key_notes]\n";
      wroteAllKeyNotes = true;
    }

    detail::AppendEqCurveArrays(stream, document.compoundPatch.GetAllKeyNotesEqCurve());
  }

  for (const auto& [midiNote, patch] : document.compoundPatch.GetKeyNotePatches()) {
    stream << "\n[[key_notes]]\n";
    stream << "midi_note = " << midiNote << '\n';
    stream << "note_name = \"" << detail::EscapeTomlString(detail::MidiNoteToName(midiNote)) << "\"\n";
    for (const auto& descriptor : detail::kOscillatorParameterDescriptors) {
      if (document.compoundPatch.IsAllKeyNotesEnabled(descriptor.parameter)) continue;

      detail::AppendOscillatorParameterArray(stream, patch, descriptor);
      detail::AppendMacroSettings(stream, descriptor.key, document.compoundPatch.GetMacroSettings(midiNote, descriptor.parameter));
    }

    if (!document.compoundPatch.IsAllKeyNotesEqEnabled()) {
      if (const auto* eqCurve = document.compoundPatch.GetKeyNoteEqCurve(midiNote)) detail::AppendEqCurveArrays(stream, *eqCurve);
    }
  }

  return stream.str();
}

inline bool ParsePatchToml(const std::string& toml, PatchDocument& document, std::string* errorMessage = nullptr) {
  return detail::PatchParser{document, errorMessage}.Parse(toml);
}

inline bool LoadPatchFromFile(std::string_view path, PatchDocument& document, std::string* errorMessage = nullptr) {
  std::string toml;
  if (!detail::ReadTextFile(path, toml)) {
    if (errorMessage) *errorMessage = "Could not read patch file";
    return false;
  }

  if (!ParsePatchToml(toml, document, errorMessage)) return false;

  if (document.name.empty()) document.name = detail::FileStem(path);

  return true;
}

inline bool SavePatchToFile(std::string_view path, const PatchDocument& document, std::string* errorMessage = nullptr) {
  if (!detail::WriteTextFile(path, SerializePatchToToml(document, false))) {
    if (errorMessage) *errorMessage = "Could not write patch file";
    return false;
  }

  return true;
}

inline void FindPatchFilesRecursive(std::string_view directory, std::vector<std::string>& paths) {
  WDL_DirScan scan;
  if (scan.First(std::string{directory}.c_str()) != 0) return;

  do {
    const char* entryName = scan.GetCurrentFN();
    if (!entryName || std::strcmp(entryName, ".") == 0 || std::strcmp(entryName, "..") == 0) continue;

    WDL_FastString childPath;
    scan.GetCurrentFullFN(&childPath);
    const int directoryState = scan.GetCurrentIsDirectory();
    if (directoryState != 0 && directoryState != 4) {
      FindPatchFilesRecursive(childPath.Get(), paths);
      continue;
    }

    if (detail::HasExtension(childPath.Get(), ".toml")) paths.push_back(childPath.Get());
  } while (scan.Next() == 0);
}

inline std::vector<std::string> FindPatchFiles(std::string_view directory) {
  std::vector<std::string> paths;
  FindPatchFilesRecursive(directory, paths);
  std::sort(paths.begin(), paths.end());
  return paths;
}
} // namespace patch_io

#include "plugin.h"
#include "IPlugPaths.h"
#include "IPlug_include_in_plug_src.h"
#include "settings/effects.h"
#include "settings/params.h"
#include "settings/patch_io.h"
#include "ui/editor_messages.h"
#include "ui/layout.h"
#include "ui/transformations.h"

#if defined APP_API
#include "standalone/audio_midi_settings.h"
#endif
#if defined OS_IOS
#include "ios/host_view_ios.h"
#endif
#if defined OS_MAC
#include <CoreGraphics/CoreGraphics.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <utility>

namespace {
constexpr uint32_t kPluginStateSettingsMagic = 0x42524343u; // Legacy plugin-state settings block magic.
constexpr int kDefaultPitchBendRange = 2;
constexpr int kMaxPitchBendRange = 96;
constexpr bool kDefaultHarmonicVisualizerEnabled = true;
constexpr int64_t kStandaloneStateSaveDebounceMs = 250;
#if defined OS_WIN
constexpr const char* kFactoryPatchResourcePrefix = "FACTORY_PATCH_";
#endif
#if defined(OS_IOS)
constexpr double kIOSStandaloneDefaultReverb = 50.0;
constexpr float kIOSMinDrawScale = 0.2f;
#endif

int SanitizePitchBendRange(int pitchBendRange) { return std::clamp(pitchBendRange, 0, kMaxPitchBendRange); }

int64_t GetMonotonicMilliseconds() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

#if defined OS_WIN
BOOL CALLBACK AddFactoryPatchResourceName(HMODULE, LPCWSTR, LPWSTR resourceName, LONG_PTR userData) {
  if (IS_INTRESOURCE(resourceName)) return TRUE;

  std::vector<std::string>& resourceNames = *reinterpret_cast<std::vector<std::string>*>(userData);
  const std::string name = UTF16AsUTF8(resourceName).Get();
  if (name.rfind(kFactoryPatchResourcePrefix, 0) == 0) resourceNames.push_back(name);
  return TRUE;
}

std::vector<std::string> FindEmbeddedFactoryPatchNames() {
  std::vector<std::string> resourceNames;
  EnumResourceNamesW(static_cast<HMODULE>(gHINSTANCE), L"TOML", AddFactoryPatchResourceName, reinterpret_cast<LONG_PTR>(&resourceNames));
  std::sort(resourceNames.begin(), resourceNames.end());
  return resourceNames;
}
#endif

GlobalVoiceSettings GetGlobalVoiceSettingsFromParams(const Addivox& plugin) {
  GlobalVoiceSettings settings{};
  for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) global_settings::ApplyParam(paramIdx, plugin.GetParam(paramIdx)->Value(), settings);

  return global_settings::Sanitize(settings);
}

EffectsSettings GetEffectsSettingsFromParams(const Addivox& plugin) {
  EffectsSettings settings{};
  for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) effects_settings::ApplyParam(paramIdx, plugin.GetParam(paramIdx)->Value(), settings);

  return effects_settings::Sanitize(settings);
}

bool IsPatchOwnedParam(int paramIdx) {
  // These controls persist across patch changes and retain their constructor defaults.
  switch (paramIdx) {
  case kParamGlobalTuning:
  case kParamGlobalPanShift:
  case kParamEffectsReverb:
  case kParamTranspose:      return false;
  default:                   return paramIdx >= 0 && paramIdx < kNumParams;
  }
}

// Looks a parameter's value up in the sanitized settings. False for a parameter that neither settings struct holds (transpose).
bool GetSettingsParamValue(int paramIdx, const GlobalVoiceSettings& voiceSettings, const EffectsSettings& effectsSettings, double& value) {
  return global_settings::GetParamValue(paramIdx, voiceSettings, value) || effects_settings::GetParamValue(paramIdx, effectsSettings, value);
}

void SetPatchOwnedParams(Addivox& plugin, const patch_io::PatchDocument& document) {
  const GlobalVoiceSettings voiceSettings = global_settings::Sanitize(document.voiceSettings);
  const EffectsSettings effectsSettings = effects_settings::Sanitize(document.effectsSettings);
  for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) {
    double value = 0.0;
    if (IsPatchOwnedParam(paramIdx) && GetSettingsParamValue(paramIdx, voiceSettings, effectsSettings, value)) plugin.GetParam(paramIdx)->Set(value);
  }
}

void SyncPatchOwnedParamDefaultsToCurrentValues(Addivox& plugin) {
  for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) {
    if (!IsPatchOwnedParam(paramIdx)) continue;

    IParam* const param = plugin.GetParam(paramIdx);
    param->SetDefault(param->Value());
  }
}

void ResetNonPatchOwnedParamsToDefaults(Addivox& plugin) {
  for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) {
    if (IsPatchOwnedParam(paramIdx)) continue;

    IParam* const param = plugin.GetParam(paramIdx);
    if (param->Value() == param->GetDefault()) continue;

    param->SetToDefault();
#if IPLUG_DSP
    plugin.OnParamChange(paramIdx);
#endif
    plugin.SendParameterValueFromDelegate(paramIdx, param->GetNormalized(), true);
  }
}

// A factory preset chunk holds the patch TOML, then the values of the parameters up to kParamEffectsTone in index order. Later parameters are not stored:
// restoring the preset takes them from the TOML (see RestoreStateParamsFromChunk).
bool BuildPatchChunk(const patch_io::PatchDocument& document, IByteChunk& chunk) {
  const GlobalVoiceSettings voiceSettings = global_settings::Sanitize(document.voiceSettings);
  const EffectsSettings effectsSettings = effects_settings::Sanitize(document.effectsSettings);
  if (chunk.PutStr(patch_io::SerializePatchToToml(document).c_str()) <= 0) return false;

  for (int paramIdx = 0; paramIdx <= kParamEffectsTone; ++paramIdx) {
    double value = 0.0;
    if (!GetSettingsParamValue(paramIdx, voiceSettings, effectsSettings, value) || chunk.Put(&value) <= 0) return false;
  }
  return true;
}

// The settings block that follows the parameter values in a saved state.
struct PluginStateSettings {
  BreathCCSource breathCCSource{kDefaultBreathCCSource};
  bool harmonicVisualizerEnabled{kDefaultHarmonicVisualizerEnabled};
  int pitchBendRange{kDefaultPitchBendRange};
  bool patchDirty{false};
  std::string patchCleanSnapshot;
  int patchSource{0};
  int factoryPatchIdx{-1};
  std::string patchPath;
  std::string patchGroupKey;
  int portamentoCC{5};
};

bool AppendPluginStateSettingsChunk(IByteChunk& chunk, const PluginStateSettings& settings) {
  const uint32_t magic = kPluginStateSettingsMagic;
  const int32_t rawBreathCCSource = static_cast<int32_t>(settings.breathCCSource);
  const int32_t rawHarmonicVisualizerEnabled = settings.harmonicVisualizerEnabled ? 1 : 0;
  const int32_t rawPitchBendRange = static_cast<int32_t>(SanitizePitchBendRange(settings.pitchBendRange));
  const int32_t rawPatchDirty = settings.patchDirty ? 1 : 0;
  const int32_t rawPatchSource = static_cast<int32_t>(settings.patchSource);
  const int32_t rawFactoryPatchIdx = static_cast<int32_t>(settings.factoryPatchIdx);
  const int32_t rawPortamentoCC = settings.portamentoCC;
  return chunk.Put(&magic) > 0 && chunk.Put(&rawBreathCCSource) > 0 && chunk.Put(&rawHarmonicVisualizerEnabled) > 0 && chunk.Put(&rawPitchBendRange) > 0 &&
         chunk.Put(&rawPatchDirty) > 0 && chunk.PutStr(settings.patchCleanSnapshot.c_str()) > 0 && chunk.Put(&rawPatchSource) > 0 &&
         chunk.Put(&rawFactoryPatchIdx) > 0 && chunk.PutStr(settings.patchPath.c_str()) > 0 && chunk.PutStr(settings.patchGroupKey.c_str()) > 0 &&
         chunk.Put(&rawPortamentoCC) > 0;
}

// Fields that are absent (older states) keep the values that settings came in with.
bool ReadPluginStateSettingsChunk(const IByteChunk& chunk, int startPos, PluginStateSettings& settings) {
  uint32_t magic = 0;
  int position = chunk.Get(&magic, startPos);
  if (position < 0 || magic != kPluginStateSettingsMagic) return false;

  int32_t rawBreathCCSource = static_cast<int32_t>(kDefaultBreathCCSource);
  position = chunk.Get(&rawBreathCCSource, position);
  if (position < 0) return false;

  settings.breathCCSource = SanitizeBreathCCSource(rawBreathCCSource);

  // Optional fields leave the cursor unchanged when absent, preserving older state chunks.
  const auto readInt = [&](int32_t& value) {
    if (position < 0) return false;
    const int nextPosition = chunk.Get(&value, position);
    if (nextPosition < 0) return false;
    position = nextPosition;
    return true;
  };
  const auto readString = [&](std::string& value) {
    if (position < 0) return false;
    WDL_String text;
    const int nextPosition = chunk.GetStr(text, position);
    if (nextPosition < 0) return false;
    value = text.Get();
    position = nextPosition;
    return true;
  };

  int32_t rawValue = 0;
  position = chunk.Get(&rawValue, position);
  if (position >= 0) settings.harmonicVisualizerEnabled = (rawValue != 0);

  if (readInt(rawValue)) settings.pitchBendRange = SanitizePitchBendRange(rawValue);
  if (readInt(rawValue)) settings.patchDirty = (rawValue != 0);
  readString(settings.patchCleanSnapshot);
  if (readInt(rawValue)) settings.patchSource = rawValue;
  if (readInt(rawValue)) settings.factoryPatchIdx = rawValue;
  readString(settings.patchPath);
  if (readString(settings.patchGroupKey) && readInt(rawValue)) settings.portamentoCC = rawValue == 65 ? 65 : 5;

  return true;
}

int GetSevenBitControllerValue(double value) { return std::clamp(static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 127.0)), 0, 127); }

int GetHighResolutionControllerValue(double value) { return std::clamp(static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 16383.0)), 0, 16383); }

bool SetParamFromChunkValue(Addivox& plugin, const IByteChunk& chunk, int paramIdx, int& position, bool applyValue = true) {
  double value = 0.0;
  const int nextPos = chunk.Get(&value, position);
  if (nextPos < 0) return false;

  if (applyValue) {
    if (paramIdx == kParamEffectsTone && std::abs(value) > 1.0) value *= 0.01;

    plugin.GetParam(paramIdx)->Set(value);
  }
  position = nextPos;
  return true;
}

int RestoreStateParamsFromChunk(Addivox& plugin, const IByteChunk& chunk, int startPos, bool applyNonPatchOwnedParams) {
  int position = startPos;
  for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) {
    // States saved before a parameter was added have fewer values, followed directly by the settings block.
    uint32_t magic = 0;
    if (chunk.Get(&magic, position) >= 0 && magic == kPluginStateSettingsMagic) break;

    const bool applyValue = applyNonPatchOwnedParams || IsPatchOwnedParam(paramIdx);
    if (!SetParamFromChunkValue(plugin, chunk, paramIdx, position, applyValue)) break;
  }

  return position;
}

std::string GetFullFileDialogPath(const WDL_String& fileName) { return fileName.GetLength() > 0 ? std::string{fileName.Get()} : std::string{}; }

std::string EnsureTomlExtension(std::string path) {
  if (patch_io::detail::HasExtension(path, ".toml")) return path;

  path += ".toml";
  return path;
}

std::string SanitizePatchFileName(std::string_view patchName) {
  std::string sanitized;
  sanitized.reserve(patchName.size());

  for (const char c : patchName) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) || c == ' ' || c == '-' || c == '_') sanitized.push_back(c);
    else
      sanitized.push_back('_');
  }

  while (!sanitized.empty() && (sanitized.back() == ' ' || sanitized.back() == '.')) sanitized.pop_back();

  if (sanitized.empty()) sanitized = "Patch";

  return EnsureTomlExtension(sanitized);
}

// The app-group folder that the iOS app mirrors its user patches into, for the AUv3 extension to read. Empty on other platforms.
std::string GetSharedUserPatchMirrorDirectory() {
#if defined(OS_IOS)
  WDL_String appGroupPath;
  AppGroupContainerPath(appGroupPath, APP_GROUP_ID);
  if (appGroupPath.GetLength() == 0) return {};

  return patch_io::detail::JoinPath(patch_io::detail::JoinPath(appGroupPath.Get(), BUNDLE_NAME), "Patches");
#else
  return {};
#endif
}

std::string GetDefaultUserPatchDirectory() {
#if defined(OS_IOS)
  if (IsOOPAuv3AppExtension()) return GetSharedUserPatchMirrorDirectory();

  const char* home = std::getenv("HOME");
  if (!home || home[0] == '\0') return {};

  return patch_io::detail::JoinPath(patch_io::detail::JoinPath(home, "Documents"), "Addivox Patches");
#else
  WDL_String appSupportPath;
  AppSupportPath(appSupportPath, false);
  if (appSupportPath.GetLength() == 0) return {};

  return patch_io::detail::JoinPath(patch_io::detail::JoinPath(appSupportPath.Get(), BUNDLE_NAME), "Patches");
#endif
}

bool IsReadOnlyUserPatchLibrary() {
#if defined(OS_IOS)
  return IsOOPAuv3AppExtension();
#else
  return false;
#endif
}

std::string EnsureDefaultUserPatchDirectory() {
  const std::string directory = GetDefaultUserPatchDirectory();
  return (!directory.empty() && patch_io::detail::EnsureDirectoryExists(directory)) ? directory : std::string{};
}

// Where a patch file dialog opens: the last folder the user chose, or the user patch folder.
std::string GetPatchDialogDirectory(const std::string& lastUsedDirectory) {
  return lastUsedDirectory.empty() ? EnsureDefaultUserPatchDirectory() : lastUsedDirectory;
}

std::string GetStandaloneSettingsDirectory() {
  WDL_String appSupportPath;
  AppSupportPath(appSupportPath, false);
  if (appSupportPath.GetLength() == 0) return {};

  return patch_io::detail::JoinPath(appSupportPath.Get(), BUNDLE_NAME);
}

std::string GetStandaloneSettingsPath(const char* fileName) {
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  return settingsDirectory.empty() ? std::string{} : patch_io::detail::JoinPath(settingsDirectory, fileName);
}

bool LoadStandaloneIntegerSetting(const char* fileName, int& value) {
  const std::string path = GetStandaloneSettingsPath(fileName);
  if (path.empty()) return false;

  std::ifstream stream(path);
  return static_cast<bool>(stream >> value);
}

bool LoadStandaloneStateChunk(IByteChunk& chunk) {
  const std::string path = GetStandaloneSettingsPath("standalone_state.chunk");
  if (path.empty()) return false;

  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream.is_open()) return false;

  const std::streamsize size = stream.tellg();
  if (size <= 0) return false;

  chunk.Clear();
  chunk.Resize(static_cast<int>(size));
  stream.seekg(0, std::ios::beg);
  if (!stream.read(reinterpret_cast<char*>(chunk.GetData()), size)) {
    chunk.Clear();
    return false;
  }

  return true;
}

bool SaveStandaloneStateChunk(const IByteChunk& chunk) {
  const std::string path = GetStandaloneSettingsPath("standalone_state.chunk");
  if (path.empty()) return false;

  const std::string parentPath = patch_io::detail::ParentPath(path);
  if (!parentPath.empty() && !patch_io::detail::EnsureDirectoryExists(parentPath)) return false;

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) return false;

  stream.write(reinterpret_cast<const char*>(chunk.GetData()), static_cast<std::streamsize>(chunk.Size()));
  return stream.good();
}

void DeleteStandaloneSettingsFile(const std::string& path) {
  if (!path.empty() && patch_io::detail::PathExists(path)) patch_io::detail::DeleteFile(path);
}

void DeleteStandaloneStateFile() { DeleteStandaloneSettingsFile(GetStandaloneSettingsPath("standalone_state.chunk")); }

void DeleteStandaloneLegacySettingsFiles() {
  DeleteStandaloneSettingsFile(GetStandaloneSettingsPath("breath_cc_source.txt"));
  DeleteStandaloneSettingsFile(GetStandaloneSettingsPath("pitch_bend_range.txt"));
  DeleteStandaloneSettingsFile(GetStandaloneSettingsPath("harmonic_visualizer_enabled.txt"));
}

std::string GetTemporaryDirectoryPath() {
#if defined(OS_WIN)
  const char* candidates[] = {std::getenv("TEMP"), std::getenv("TMP")};
#else
  const char* candidates[] = {std::getenv("TMPDIR"), std::getenv("TEMP"), std::getenv("TMP")};
#endif

  for (const char* candidate : candidates) {
    if (candidate && candidate[0] != '\0') return std::string{candidate};
  }

#if defined(OS_WIN)
  const std::string fallbackPath = GetDefaultUserPatchDirectory();
  return fallbackPath.empty() ? "." : fallbackPath;
#else
  return "/tmp";
#endif
}

void ShowPatchFileError(IGraphics* ui, const char* title, const std::string& message) {
  if (ui) ui->ShowMessageBox(message.c_str(), title, kMB_OK);
}

std::string TrimTrailingSeparators(std::string_view path) { return std::string{patch_io::detail::TrimTrailingPathSeparators(path)}; }

std::string NormalizeMenuPath(std::string path) {
  for (char& c : path) {
    if (c == '\\') c = '/';
  }
  return path;
}

std::vector<std::string> SplitMenuPath(std::string_view path) {
  std::vector<std::string> parts;
  std::string normalized = NormalizeMenuPath(std::string{path});
  std::size_t start = 0;
  while (start < normalized.size()) {
    const std::size_t slash = normalized.find('/', start);
    const std::size_t end = slash == std::string::npos ? normalized.size() : slash;
    if (end > start) parts.push_back(normalized.substr(start, end - start));
    if (slash == std::string::npos) break;
    start = slash + 1;
  }
  return parts;
}

std::string MakeGroupKey(const std::vector<std::string>& menuPath) {
  std::string key;
  for (const auto& part : menuPath) {
    if (!key.empty()) key.push_back('/');
    key += part;
  }
  return key.empty() ? "Factory" : key;
}

// Windows paths are case-insensitive, so a prefix match must be too; other platforms keep exact case matching.
bool PathPrefixMatches(const std::string& full, const std::string& base) {
#if defined(OS_WIN)
  return _strnicmp(full.data(), base.data(), base.size()) == 0;
#else
  return full.compare(0, base.size(), base) == 0;
#endif
}

std::string RelativePathFromDirectory(std::string_view directory, std::string_view path) {
  const std::string base = TrimTrailingSeparators(directory);
  const std::string full{path};
  if (base.empty() || full.size() <= base.size()) return full;

  if (!PathPrefixMatches(full, base)) return full;
  if (base.back() != '/' && base.back() != '\\' && full[base.size()] != '/' && full[base.size()] != '\\') return full;

  std::size_t offset = base.size();
  while (offset < full.size() && (full[offset] == '/' || full[offset] == '\\')) ++offset;
  return full.substr(offset);
}

bool IsAbsolutePath(std::string_view path) {
  return path.size() > 0 && (path[0] == '/' || path[0] == '\\' || (path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':'));
}

// "User", followed by the patch's subfolders within the user patch directory.
std::vector<std::string> UserPatchMenuPath(std::string_view userPatchDirectory, std::string_view patchPath) {
  std::vector<std::string> menuPath{"User"};
  const std::vector<std::string> subfolders = SplitMenuPath(patch_io::detail::ParentPath(RelativePathFromDirectory(userPatchDirectory, patchPath)));
  menuPath.insert(menuPath.end(), subfolders.begin(), subfolders.end());
  return menuPath;
}

std::string UserPatchGroupKeyForPath(std::string_view userPatchDirectory, std::string_view patchPath) {
  return MakeGroupKey(UserPatchMenuPath(userPatchDirectory, patchPath));
}

std::string GetUniqueChildDirectory(std::string_view parentDirectory, std::string_view preferredName) {
  std::string sanitized = SanitizePatchFileName(preferredName);
  if (patch_io::detail::HasExtension(sanitized, ".toml")) sanitized.resize(sanitized.size() - 5);
  if (sanitized.empty()) sanitized = "Collection";

  std::string candidate = patch_io::detail::JoinPath(parentDirectory, sanitized);
  int suffix = 2;
  while (patch_io::detail::PathExists(candidate)) {
    candidate = patch_io::detail::JoinPath(parentDirectory, sanitized + " " + std::to_string(suffix));
    ++suffix;
  }
  return candidate;
}

std::string GetUniquePatchFilePath(std::string_view parentDirectory, std::string_view preferredName) {
  std::string sanitized = SanitizePatchFileName(patch_io::detail::FileStem(preferredName));
  if (sanitized.empty()) sanitized = "Patch.toml";

  std::string candidate = patch_io::detail::JoinPath(parentDirectory, sanitized);
  int suffix = 2;
  while (patch_io::detail::PathExists(candidate)) {
    std::string stem = patch_io::detail::FileStem(sanitized);
    if (stem.empty()) stem = "Patch";

    candidate = patch_io::detail::JoinPath(parentDirectory, EnsureTomlExtension(stem + " " + std::to_string(suffix)));
    ++suffix;
  }
  return candidate;
}

bool CopyFileBytes(const std::string& sourcePath, const std::string& destinationPath) {
  const std::string destinationParent = patch_io::detail::ParentPath(destinationPath);
  if (!destinationParent.empty() && !patch_io::detail::EnsureDirectoryExists(destinationParent)) return false;

  std::ifstream input(sourcePath, std::ios::binary);
  if (!input.is_open()) return false;

  std::ofstream output(destinationPath, std::ios::binary);
  if (!output.is_open()) return false;

  output << input.rdbuf();
  return !input.bad() && output.good();
}

bool RemoveDirectoryTree(std::string_view path) {
  if (!patch_io::detail::PathExists(path)) return true;
  if (!patch_io::detail::IsDirectory(path)) return patch_io::detail::DeleteFile(path);

  const bool removedContents = patch_io::detail::ForEachDirectoryEntry(path, [](const char* childPath, bool isDirectory) {
    return isDirectory ? RemoveDirectoryTree(childPath) : patch_io::detail::DeleteFile(childPath);
  });
  return removedContents && std::remove(std::string{path}.c_str()) == 0;
}

bool CopyDirectoryTree(std::string_view sourceDirectory, std::string_view destinationDirectory) {
  if (!patch_io::detail::IsDirectory(sourceDirectory)) return false;
  if (!patch_io::detail::EnsureDirectoryExists(destinationDirectory)) return false;

  return patch_io::detail::ForEachDirectoryEntry(sourceDirectory, [&](const char* childPath, bool isDirectory) {
    const std::string destinationPath = patch_io::detail::JoinPath(destinationDirectory, RelativePathFromDirectory(sourceDirectory, childPath));
    return isDirectory ? CopyDirectoryTree(childPath, destinationPath) : CopyFileBytes(childPath, destinationPath);
  });
}

bool SyncUserPatchDirectoryToSharedMirror() {
#if defined(OS_IOS)
  if (IsReadOnlyUserPatchLibrary()) return true;

  const std::string userPatchDirectory = GetDefaultUserPatchDirectory();
  const std::string mirrorDirectory = GetSharedUserPatchMirrorDirectory();
  if (userPatchDirectory.empty() || mirrorDirectory.empty()) return false;
  if (!patch_io::detail::EnsureDirectoryExists(userPatchDirectory)) return false;
  if (!RemoveDirectoryTree(mirrorDirectory)) return false;
  return CopyDirectoryTree(userPatchDirectory, mirrorDirectory);
#else
  return true;
#endif
}

bool CopyPatchCollection(std::string_view sourceDirectory, std::string_view destinationDirectory, std::string* errorMessage = nullptr) {
  const std::vector<std::string> patchPaths = patch_io::FindPatchFiles(sourceDirectory);
  if (patchPaths.empty()) {
    if (errorMessage) *errorMessage = "No TOML patches were found in that collection.";
    return false;
  }

  for (const auto& patchPath : patchPaths) {
    const std::string relativePath = RelativePathFromDirectory(sourceDirectory, patchPath);
    const std::string destinationPath = patch_io::detail::JoinPath(destinationDirectory, relativePath);
    if (!CopyFileBytes(patchPath, destinationPath)) {
      if (errorMessage) *errorMessage = "Could not copy patch: " + patchPath;
      return false;
    }
  }

  return true;
}

bool IsPathInDirectory(std::string_view directory, std::string_view path) {
  const std::string base = TrimTrailingSeparators(directory);
  const std::string full{path};
  if (base.empty() || full.size() < base.size()) return false;
  if (!PathPrefixMatches(full, base)) return false;
  return full.size() == base.size() || full[base.size()] == '/' || full[base.size()] == '\\';
}

// A message's payload, or nullptr if the message is not the expected one or its data is not that payload's size.
template <typename Payload> const Payload* GetPayload(bool isExpectedMessage, int dataSize, const void* pData) {
  return (isExpectedMessage && pData && dataSize == sizeof(Payload)) ? static_cast<const Payload*>(pData) : nullptr;
}

bool ShouldTrackPatchDirtyForParamSource(EParamSource source) {
  switch (source) {
  case kPresetRecall:
  case kDelegate:
  case kHost:
  case kRecompile:    return false;
  default:            return true;
  }
}

std::string CanonicalizePatchSnapshotToml(const std::string& snapshot) {
  patch_io::PatchDocument document;
  if (snapshot.empty() || !patch_io::ParsePatchToml(snapshot, document)) return snapshot;

  return patch_io::SerializePatchToToml(document);
}

std::string CanonicalizePatchDirtySnapshotToml(const std::string& snapshot) {
  patch_io::PatchDocument document;
  if (snapshot.empty() || !patch_io::ParsePatchToml(snapshot, document)) return snapshot;

  document.name.clear();
  document.voiceSettings.tuningCents = 0.0;
  document.voiceSettings.panOffset = 0.0;
  document.effectsSettings.reverb = 0.0;
  return patch_io::SerializePatchToToml(document);
}
} // namespace

Addivox::Addivox(const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(kNumParams, kMaxFactoryPatches)), mEditorState(std::make_shared<plugin_ui::EditorState>()) {
  InitializeParams();

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() { return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT)); };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachSVGBackground("background.svg");
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);
#if defined OS_IOS
    pGraphics->SetScaleConstraints(kIOSMinDrawScale, DEFAULT_MAX_DRAW_SCALE); // Hosts such as GarageBand offer less than half the UI's natural height.
#endif

#ifdef OS_WEB
    pGraphics->AttachPopupMenuControl();
#endif

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Roboto-Bold", ROBOTO_BOLD_FN);
    pGraphics->LoadFont("Roboto-Black", ROBOTO_BLACK_FN);
    mEditorContext = plugin_ui::AttachMainControls(pGraphics, mEditorState, kCtrlTagHarmonicVisualizer, kCtrlTagEditorTabs, kCtrlTagKeyboard, kCtrlTagBender,
                                                   kCtrlTagBreathMeter, kCtrlTagMeter, mPitchBendRange);

    pGraphics->SetKeyHandlerFunc([this, pGraphics](const IKeyPress& key, bool isUp) { return HandleKeyPress(pGraphics, key, isUp); });
  };
#endif

  LoadBuiltInPatches();
  RebuildPatchCatalog();
  RestoreFactoryPatch(0);
  if (mActivePatchDisplayName.empty() && NPresets() > 0) mActivePatchDisplayName = GetPresetName(GetCurrentPresetIdx());

  SetPitchBendRange(mPitchBendRange);
  SetBreathCCSource(kDefaultBreathCCSource);
  SetPortamentoCC(5);
}

void Addivox::InitializeParams() {
  const auto formatPseudoLogScaleDisplay = [](double value, WDL_String& str) {
    int decimals = 2;
    if (value >= 10.0) decimals = 1;
    if (value >= 100.0) decimals = 0;
    const double scale = (decimals == 2 ? 100.0 : (decimals == 1 ? 10.0 : 1.0));
    const double rounded = std::round(value * scale) / scale;
    str.SetFormatted(32, "%.*f\xC3\x97", decimals, rounded);
  };

  const auto initPseudoLogScale = [this, formatPseudoLogScaleDisplay](int paramIdx, const char* name, double defaultValue = 1.0) {
    GetParam(paramIdx)->InitDouble(name, defaultValue, 0., 100., 0.01, "", 0, "", transformations::GetGlobalPseudoLogShape(), iplug::IParam::kUnitCustom,
                                   formatPseudoLogScaleDisplay);
  };
  const auto formatPercentDisplay = [](double value, WDL_String& str) { str.SetFormatted(32, "%.1f%%", value); };
  const auto formatSignedCentsDisplay = [](double value, WDL_String& str) {
    const double roundedValue = std::round(value);
    const double normalizedValue = (roundedValue == 0.0) ? 0.0 : roundedValue;
    if (normalizedValue > 0.0) str.SetFormatted(32, "+%.0f\xC2\xA2", normalizedValue);
    else
      str.SetFormatted(32, "%.0f\xC2\xA2", normalizedValue);
  };
  const auto formatSignedUnitDisplay = [](double value, WDL_String& str) {
    const double roundedValue = std::round(value * 100.0) / 100.0;
    const double normalized = (roundedValue == 0.0) ? 0.0 : roundedValue;
    if (normalized > 0.0) str.SetFormatted(32, "+%.2f", normalized);
    else
      str.SetFormatted(32, "%.2f", normalized);
  };

  GetParam(kParamGlobalLevel)
      ->InitDouble("Level", 1.0, 0., 10.0, 0.01, "", 0, "", transformations::GetLevelPseudoLogShape(), iplug::IParam::kUnitCustom, formatPseudoLogScaleDisplay);
  GetParam(kParamFormantsScale)
      ->InitDouble("Formants", 1.0, 0., 3.0, 0.01, "", 0, "", transformations::GetEqAmountPseudoLogShape(), iplug::IParam::kUnitCustom,
                   formatPseudoLogScaleDisplay);
  GetParam(kParamResonanceScale)
      ->InitDouble("Resonance", 0.0, 0., 3.0, 0.01, "", 0, "", transformations::GetEqAmountPseudoLogShape(), iplug::IParam::kUnitCustom,
                   formatPseudoLogScaleDisplay);
  initPseudoLogScale(kParamGlobalAttackScale, "Attack");
  initPseudoLogScale(kParamGlobalReleaseScale, "Release");
  GetParam(kParamGlobalTuning)
      ->InitDouble("Tuning", 0., -50., 50., 1.0, "", iplug::IParam::kFlagStepped, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCents,
                   formatSignedCentsDisplay);
  GetParam(kParamGlobalPanShift)
      ->InitDouble("Pan Shift", 0., -1., 1., 0.01, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatSignedUnitDisplay);
  initPseudoLogScale(kParamGlobalLevelVariationAmplitudeScale, "Level Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalLevelVariationRateScale, "Level Variation Rate");
  initPseudoLogScale(kParamGlobalPitchVariationAmplitudeScale, "Pitch Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPitchVariationRateScale, "Pitch Variation Rate");
  initPseudoLogScale(kParamGlobalPanVariationAmplitudeScale, "Pan Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPanVariationRateScale, "Pan Variation Rate");
  const auto& portamentoShape = transformations::GetPortamentoPseudoLogShape();
  GetParam(kParamPortamentoAtCC5Min)->InitDouble("Portamento (min)", 0.001, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
  GetParam(kParamPortamentoAtCC5Max)->InitDouble("Portamento (max)", 0.025, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
  GetParam(kParamEffectsDrive)
      ->InitDouble("Drive", 0., 0., 100.0, 0.1, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatPercentDisplay);
  GetParam(kParamEffectsTone)
      ->InitDouble("Tone", 0., -1.0, 1.0, 0.01, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatSignedUnitDisplay);
  GetParam(kParamTranspose)->InitInt("Transpose", 0, -36, 36, "st", iplug::IParam::kFlagSignDisplay);
  GetParam(kParamEffectsChorus)
      ->InitDouble("Chorus", 0., 0., 100.0, 0.1, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatPercentDisplay);
  GetParam(kParamEffectsReverb)
      ->InitDouble("Reverb", effects_settings::kDefaultReverb, 0., 100.0, 0.1, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom,
                   formatPercentDisplay);
}

#if IPLUG_EDITOR
void Addivox::SendQwertyMidi(IGraphics* pGraphics, const IMidiMsg& msg) {
  SendMidiMsgFromUI(msg);
  plugin_ui::HandleQwertyMidi(pGraphics, kCtrlTagKeyboard, mLastQwertyMIDINote, msg);
}

void Addivox::ReleaseHeldQwertyNotes(IGraphics* pGraphics) {
  IMidiMsg msg;
  for (int pitch = 0; pitch < static_cast<int>(mQwertyMidiKeysDown.size()); ++pitch) {
    if (!mQwertyMidiKeysDown[static_cast<std::size_t>(pitch)]) continue;

    msg.MakeNoteOffMsg(pitch, 0);
    mQwertyMidiKeysDown[static_cast<std::size_t>(pitch)] = false;
    SendQwertyMidi(pGraphics, msg);
  }
}

// macOS doesn't deliver key-up events while the window is being dragged, which would leave a QWERTY note stuck on.
void Addivox::ReleaseQwertyNotesIfKeysReleased() {
#if defined OS_MAC
  IGraphics* pGraphics = GetUI();
  if (mLastQwertyMIDINote < 0 || !pGraphics) return;

  constexpr CGKeyCode kNumKeyCodes = 128;
  for (CGKeyCode keyCode = 0; keyCode < kNumKeyCodes; ++keyCode) {
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, keyCode)) return;
  }
  ReleaseHeldQwertyNotes(pGraphics);
#endif
}

bool Addivox::HandleKeyPress(IGraphics* pGraphics, const IKeyPress& key, bool isUp) {
  const bool editMode = mEditorState && mEditorState->editMode;
  if (editMode && !mWasQwertyKeyboardInEditMode) ReleaseHeldQwertyNotes(pGraphics);
  mWasQwertyKeyboardInEditMode = editMode;

  const bool popupExpanded = pGraphics->GetPopupMenuControl() && pGraphics->GetPopupMenuControl()->GetExpanded();
  const bool textEntryActive = pGraphics->IsInPlatformTextEntry() || (pGraphics->GetControlInTextEntry() != nullptr);
  const bool modifiersActive = key.S || key.C || key.A;

  if (!isUp && !popupExpanded && !textEntryActive && !modifiersActive) {
    if (key.VK == kVK_LEFT) {
      CyclePatchInCurrentGroup(-1);
      return true;
    }
    if (key.VK == kVK_RIGHT) {
      CyclePatchInCurrentGroup(1);
      return true;
    }
  }

  if (editMode) {
    if (!isUp && !popupExpanded && !textEntryActive && !modifiersActive) plugin_ui::editor::ApplyKeyboardActionToSelectedTab(mEditorContext, key.VK);

    return true;
  }

  const int noteOffset = plugin_ui::editor::GetQwertyMidiNoteOffset(key.VK);
  IMidiMsg msg;

  if (noteOffset >= 0) {
    const int pitch = std::clamp(mQwertyMidiBaseNote + noteOffset, 0, 127);
    const auto pitchIndex = static_cast<std::size_t>(pitch);

    if (!isUp) {
      if (!mQwertyMidiKeysDown[pitchIndex]) {
        msg.MakeNoteOnMsg(pitch, 127, 0);
        mQwertyMidiKeysDown[pitchIndex] = true;
        SendQwertyMidi(pGraphics, msg);
      }
    } else if (mQwertyMidiKeysDown[pitchIndex]) {
      msg.MakeNoteOffMsg(pitch, 0);
      mQwertyMidiKeysDown[pitchIndex] = false;
      SendQwertyMidi(pGraphics, msg);
    }

    return true;
  }

  if (!isUp && (key.VK == kVK_Z || key.VK == kVK_X)) {
    const int octaveShift = key.VK == kVK_Z ? -12 : 12;
    mQwertyMidiBaseNote = std::clamp(mQwertyMidiBaseNote + octaveShift, 24, 96);
    ReleaseHeldQwertyNotes(pGraphics);
  }

  return true;
}
#endif

void Addivox::OnHostIdentified() { EnsureStandaloneStateInitialized(); }

bool Addivox::LoadStandaloneState() {
  IByteChunk chunk;
  if (!LoadStandaloneStateChunk(chunk)) return false;

  int position = 0;
  IByteChunk::GetIPlugVerFromChunk(chunk, position);

  mSuppressStandaloneStatePersistence = true;
  const int result = UnserializeState(chunk, position);
  if (result >= 0) OnRestoreState();
  mSuppressStandaloneStatePersistence = false;

  if (result < 0) return false;

  DeleteStandaloneLegacySettingsFiles();
  mStandaloneStateSavedRevision = mStandaloneStateRevision;
  return true;
}

bool Addivox::SaveStandaloneState() const {
  if (GetHost() != kHostStandalone) return false;

  IByteChunk chunk;
  IByteChunk::InitChunkWithIPlugVer(chunk);
  if (!SerializeState(chunk)) return false;

  if (!SaveStandaloneStateChunk(chunk)) return false;

  DeleteStandaloneLegacySettingsFiles();
  return true;
}

void Addivox::SaveStandaloneStateIfNeeded(bool force) {
  if (GetHost() != kHostStandalone || !mStandaloneStateInitialized || mSuppressStandaloneStatePersistence) return;

  if (mStandaloneStateRevision == mStandaloneStateSavedRevision) return;

  const int64_t now = GetMonotonicMilliseconds();
  if (!force && (now - mStandaloneStateLastDirtyMillis) < kStandaloneStateSaveDebounceMs) return;

  const uint64_t revisionBeforeSave = mStandaloneStateRevision;
  if (!SaveStandaloneState()) return;

  if (mStandaloneStateRevision == revisionBeforeSave) mStandaloneStateSavedRevision = revisionBeforeSave;
}

void Addivox::MarkStandaloneStateDirty() {
  if (GetHost() != kHostStandalone || !mStandaloneStateInitialized || mSuppressStandaloneStatePersistence) return;

  mStandaloneStateLastDirtyMillis = GetMonotonicMilliseconds();
  ++mStandaloneStateRevision;
}

void Addivox::EnsureStandaloneStateInitialized() {
  if (mStandaloneStateInitialized || GetHost() != kHostStandalone) return;

  mStandaloneStateInitialized = true;

  if (LoadStandaloneState()) return;

  DeleteStandaloneStateFile();

  bool migratedLegacyState = false;
  mSuppressStandaloneStatePersistence = true;

  int persistedPitchBendRange = kDefaultPitchBendRange;
  if (LoadStandaloneIntegerSetting("pitch_bend_range.txt", persistedPitchBendRange)) {
    SetPitchBendRange(SanitizePitchBendRange(persistedPitchBendRange));
    migratedLegacyState = true;
  }

  int persistedSource = static_cast<int>(kDefaultBreathCCSource);
  if (LoadStandaloneIntegerSetting("breath_cc_source.txt", persistedSource)) {
    SetBreathCCSource(SanitizeBreathCCSource(persistedSource));
    migratedLegacyState = true;
  }

  int persistedEnabled = kDefaultHarmonicVisualizerEnabled ? 1 : 0;
  if (LoadStandaloneIntegerSetting("harmonic_visualizer_enabled.txt", persistedEnabled)) {
    SetHarmonicVisualizerEnabled(persistedEnabled != 0);
    migratedLegacyState = true;
  }

  mSuppressStandaloneStatePersistence = false;

  if (migratedLegacyState) MarkStandaloneStateDirty();
}

void Addivox::ResetStandaloneStateToDefaults() {
  EnsureStandaloneStateInitialized();

  mSuppressStandaloneStatePersistence = true;
  DeleteStandaloneStateFile();
  DeleteStandaloneLegacySettingsFiles();
  mStandaloneStateRevision = 0;
  mStandaloneStateSavedRevision = 0;
  mStandaloneStateLastDirtyMillis = 0;

  mActiveUIMIDINotes.fill(false);
  mNumActiveUIMIDINotes = 0;

  if (NPresets() > 0) RestoreFactoryPatch(0);
  ResetNonPatchOwnedParamsToDefaults(*this);
#if defined(OS_IOS)
  if (GetParam(kParamEffectsReverb)->Value() != kIOSStandaloneDefaultReverb) {
    GetParam(kParamEffectsReverb)->Set(kIOSStandaloneDefaultReverb);
#if IPLUG_DSP
    OnParamChange(kParamEffectsReverb);
#endif
    SendParameterValueFromDelegate(kParamEffectsReverb, GetParam(kParamEffectsReverb)->GetNormalized(), true);
  }
#endif

  SetPitchBendRange(kDefaultPitchBendRange);
  SetBreathCCSource(kDefaultBreathCCSource);
  SetPortamentoCC(5);
  SetHarmonicVisualizerEnabled(kDefaultHarmonicVisualizerEnabled);

#if IPLUG_DSP
  OnReset();
#endif

  mSuppressStandaloneStatePersistence = false;
  RefreshEditorUI(true);
}

void Addivox::SetPitchBendRange(int pitchBendRange) {
  const int sanitizedPitchBendRange = SanitizePitchBendRange(pitchBendRange);
  const bool changed = sanitizedPitchBendRange != mPitchBendRange;
  mPitchBendRange = sanitizedPitchBendRange;
  if (mEditorState) mEditorState->pitchBendRange = mPitchBendRange;

#if IPLUG_DSP
  mDSP.SetPitchBendRange(mPitchBendRange);
#endif

  SyncPitchBendRangeUI();
  if (changed) MarkStandaloneStateDirty();
}

void Addivox::SetPortamentoCC(int controller) {
  const int sanitizedController = controller == 65 ? 65 : 5;
  const bool changed = sanitizedController != mPortamentoCC;
  mPortamentoCC = sanitizedController;
  if (mEditorState) mEditorState->portamentoCC = mPortamentoCC;
#if IPLUG_DSP
  mDSP.SetPortamentoCC(mPortamentoCC);
#endif
  if (changed) MarkStandaloneStateDirty();
}

void Addivox::SetBreathCCSource(BreathCCSource source) {
  const BreathCCSource sanitizedSource = SanitizeBreathCCSource(static_cast<int>(source));
  const bool changed = sanitizedSource != mBreathCCSource;
  mBreathCCSource = sanitizedSource;
  if (mEditorState) mEditorState->breathCCSource = mBreathCCSource;

#if IPLUG_DSP
  mDSP.SetBreathCCSource(mBreathCCSource);
  mBreathCCInputTracker.Reset();
#endif
  if (changed) MarkStandaloneStateDirty();
}

void Addivox::SetHarmonicVisualizerEnabled(bool enabled) {
  const bool changed = enabled != mHarmonicVisualizerEnabled.load(std::memory_order_relaxed);
  mHarmonicVisualizerEnabled.store(enabled, std::memory_order_relaxed);
  mHarmonicVisualizerBlankPending.store(!enabled, std::memory_order_relaxed);
  if (mEditorState) mEditorState->harmonicVisualizerEnabled = enabled;

#if IPLUG_DSP
  mHarmonicVisualizerSender.ClearQueuedData();
  mHarmonicVisualizerSender.Reset(GetSampleRate(), PLUG_FPS);
#endif
  if (changed) MarkStandaloneStateDirty();
}

void Addivox::SendBreathControlFromUI(double value, int channel, int offset) {
  const BreathCCSourceInfo& info = GetBreathCCSourceInfo(mBreathCCSource);
  if (info.IsHighResolution()) {
    const int rawValue = GetHighResolutionControllerValue(value);
    IPlugAPIBase::SendMidiMsgFromUI(MakeControlChangeMsg(info.msbController, rawValue / 128, channel, offset));
    IPlugAPIBase::SendMidiMsgFromUI(MakeControlChangeMsg(info.lsbController, rawValue % 128, channel, offset));
    return;
  }

  IPlugAPIBase::SendMidiMsgFromUI(MakeControlChangeMsg(info.msbController, GetSevenBitControllerValue(value), channel, offset));
}

void Addivox::ApplyPatchDocumentToState(const patch_io::PatchDocument& document) {
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    mEditorState->compoundPatch = document.compoundPatch;
    mEditorState->selectedMidiNote = patch_io::detail::ChooseDefaultSelectedMidiNote(document.compoundPatch, mEditorState->selectedMidiNote);
  }

#if IPLUG_DSP
  mDSP.SetCompoundPatch(document.compoundPatch);
#endif

  SetPatchOwnedParams(*this, document);
}

void Addivox::FinalizePatchRecall(bool syncControlValues) {
  OnParamReset(kPresetRecall);
  SyncPatchOwnedParamDefaultsToCurrentValues(*this);
  if (syncControlValues) SendCurrentParamValuesFromDelegate();
}

void Addivox::PromptLoadPatchFromFile() {
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if (!ui) return;

  WDL_String fileName;
  WDL_String directory{GetPatchDialogDirectory(mUserPatchDirectory).c_str()};

  ui->PromptForFile(fileName, directory, EFileAction::Open, "toml", [this](const WDL_String& selectedFileName, const WDL_String&) {
    const std::string fullPath = GetFullFileDialogPath(selectedFileName);
    if (fullPath.empty()) return;

    LoadUserPatchByPath(fullPath);
  });
#endif
}

void Addivox::PromptSavePatchToFile() {
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if (!ui) return;

  patch_io::PatchDocument document;
  std::string defaultFileName;
  std::string initialDirectory;
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    document.name = mActivePatchDisplayName.empty() ? "Patch" : mActivePatchDisplayName;
    document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
    document.effectsSettings = GetEffectsSettingsFromParams(*this);
    document.compoundPatch = mEditorState->compoundPatch;

    defaultFileName = (mActivePatchSource == PatchSource::User && !mActivePatchPath.empty()) ? std::string{patch_io::detail::FileNameView(mActivePatchPath)}
                                                                                             : SanitizePatchFileName(document.name);
    initialDirectory = (mActivePatchSource == PatchSource::User && !mActivePatchPath.empty())
                           ? patch_io::detail::ParentPath(mActivePatchPath)
                           : GetPatchDialogDirectory(mUserPatchDirectory);
  }

  WDL_String fileName;
  WDL_String directory;
  if (!initialDirectory.empty()) patch_io::detail::EnsureDirectoryExists(initialDirectory);

#if defined(OS_IOS)
  const std::string exportPath = patch_io::detail::JoinPath(GetTemporaryDirectoryPath(), defaultFileName);
  std::string errorMessage;
  if (!patch_io::SavePatchToFile(exportPath, document, &errorMessage)) {
    ShowPatchFileError(ui, "Patch Save Failed", errorMessage);
    return;
  }

  fileName.Set(defaultFileName.c_str());
  directory.Set(exportPath.c_str());
  ui->PromptForFile(fileName, directory, EFileAction::Save, "toml", [this, exportPath, document](const WDL_String& selectedFileName, const WDL_String&) {
    patch_io::detail::DeleteFile(exportPath);

    std::string destinationPath = GetFullFileDialogPath(selectedFileName);
    if (destinationPath.empty()) return;
    destinationPath = EnsureTomlExtension(std::move(destinationPath));

    mUserPatchDirectory = patch_io::detail::ParentPath(destinationPath);

    const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
    if (IsPathInDirectory(userPatchDirectory, destinationPath)) {
      const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
      SetActiveUserPatch(destinationPath);
      mActivePatchDisplayName = patch_io::detail::FileStem(destinationPath);
      SetActivePatchCleanSnapshotFromCurrentState();
    }

    RebuildPatchCatalog();
    RefreshEditorUI();
  });
#else
  fileName.Set(defaultFileName.c_str());
  if (!initialDirectory.empty()) directory.Set(initialDirectory.c_str());

  ui->PromptForFile(fileName, directory, EFileAction::Save, "toml", [this, document](const WDL_String& selectedFileName, const WDL_String&) {
    std::string fullPath = GetFullFileDialogPath(selectedFileName);
    if (fullPath.empty()) return;

    fullPath = EnsureTomlExtension(std::move(fullPath));

    std::string errorMessage;
    if (!patch_io::SavePatchToFile(fullPath, document, &errorMessage)) {
      ShowPatchFileError(GetUI(), "Patch Save Failed", errorMessage);
      return;
    }

    mUserPatchDirectory = patch_io::detail::ParentPath(fullPath);
    {
      const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
      SetActiveUserPatch(fullPath);
      mActivePatchDisplayName = patch_io::detail::FileStem(fullPath);
      SetActivePatchCleanSnapshotFromCurrentState();
    }
    RebuildPatchCatalog();
    RefreshEditorUI();
  });
#endif
#endif
}

void Addivox::SendMidiMsgFromUI(const IMidiMsg& msg) {
  const IMidiMsg::EStatusMsg status = msg.StatusMsg();
  const bool isNoteOn = (status == IMidiMsg::kNoteOn) && (msg.Velocity() > 0);
  const bool isNoteOff = (status == IMidiMsg::kNoteOff) || ((status == IMidiMsg::kNoteOn) && (msg.Velocity() == 0));

  if (!isNoteOn && !isNoteOff) {
    IPlugAPIBase::SendMidiMsgFromUI(msg);
    return;
  }

  const int midiNote = msg.NoteNumber();
  if (midiNote < 0 || midiNote >= static_cast<int>(mActiveUIMIDINotes.size())) {
    IPlugAPIBase::SendMidiMsgFromUI(msg);
    return;
  }

  const auto sendBreathFromUI = [this, &msg](double value) { SendBreathControlFromUI(value, msg.Channel(), msg.mOffset); };

  if (isNoteOn) {
    if (!mActiveUIMIDINotes[static_cast<size_t>(midiNote)]) {
      if (mNumActiveUIMIDINotes == 0) sendBreathFromUI(1.0);

      mActiveUIMIDINotes[static_cast<size_t>(midiNote)] = true;
      ++mNumActiveUIMIDINotes;
    }

    IPlugAPIBase::SendMidiMsgFromUI(msg);
    return;
  }

  bool releasedActiveUINote = false;
  if (mActiveUIMIDINotes[static_cast<size_t>(midiNote)]) {
    mActiveUIMIDINotes[static_cast<size_t>(midiNote)] = false;
    --mNumActiveUIMIDINotes;
    releasedActiveUINote = true;
  }

  IPlugAPIBase::SendMidiMsgFromUI(msg);

  if (releasedActiveUINote && mNumActiveUIMIDINotes == 0) sendBreathFromUI(0.0);
}

bool Addivox::SerializeState(IByteChunk& chunk) const {
  // Hosts may call this from any thread (AUv3 XPC, VST3), so snapshot the patch state under the lock, then
  // serialize from the locals. SerializeParams can take iPlug2's params mutex and must stay outside the lock.
  patch_io::PatchDocument document;
  PluginStateSettings settings;
  settings.breathCCSource = mBreathCCSource;
  settings.harmonicVisualizerEnabled = mHarmonicVisualizerEnabled.load(std::memory_order_relaxed);
  settings.pitchBendRange = mPitchBendRange;
  settings.portamentoCC = mPortamentoCC;
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    document.name =
        mActivePatchDisplayName.empty() ? GetPresetName(mActiveFactoryPatchIdx >= 0 ? mActiveFactoryPatchIdx : GetCurrentPresetIdx()) : mActivePatchDisplayName;
    document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
    document.effectsSettings = GetEffectsSettingsFromParams(*this);
    document.compoundPatch = mEditorState->compoundPatch;

    settings.patchPath = mActivePatchPath;
    if (mActivePatchSource == PatchSource::User) {
      const std::string userPatchDirectory = GetDefaultUserPatchDirectory();
      if (!userPatchDirectory.empty()) settings.patchPath = RelativePathFromDirectory(userPatchDirectory, mActivePatchPath);
    }

    settings.patchDirty = mActivePatchDirty;
    settings.patchCleanSnapshot = mActivePatchCleanSnapshot;
    settings.patchSource = static_cast<int>(mActivePatchSource);
    settings.factoryPatchIdx = mActiveFactoryPatchIdx;
    settings.patchGroupKey = mActivePatchGroupKey;
  }

  return chunk.PutStr(patch_io::SerializePatchToToml(document).c_str()) > 0 && SerializeParams(chunk) && AppendPluginStateSettingsChunk(chunk, settings);
}

int Addivox::UnserializeState(const IByteChunk& chunk, int startPos) {
  WDL_String patchToml;
  int position = chunk.GetStr(patchToml, startPos);
  if (position < 0) return position;

  patch_io::PatchDocument document;
  if (!patch_io::ParsePatchToml(patchToml.Get(), document)) return -1;

  mSuppressPatchDirtyTracking = true;
  ApplyPatchDocumentToState(document);
  mPendingRestoredStatePatchName = document.name;

  int pos = position;
  ENTER_PARAMS_MUTEX
  pos = RestoreStateParamsFromChunk(*this, chunk, pos, mRestoringFactoryPatchIdx < 0);
  FinalizePatchRecall(false);
  LEAVE_PARAMS_MUTEX
  mSuppressPatchDirtyTracking = false;

  PluginStateSettings restored;
  restored.pitchBendRange = mPitchBendRange;
  restored.harmonicVisualizerEnabled = mHarmonicVisualizerEnabled.load(std::memory_order_relaxed);
  if (ReadPluginStateSettingsChunk(chunk, pos, restored)) {
    SetPitchBendRange(restored.pitchBendRange);
    SetBreathCCSource(restored.breathCCSource);
    SetHarmonicVisualizerEnabled(restored.harmonicVisualizerEnabled);
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    mPendingRestoredPatchDirty = restored.patchDirty;
    mPendingRestoredPatchHasDirtyState = true;
    mPendingRestoredPatchSource =
        static_cast<PatchSource>(std::clamp(restored.patchSource, static_cast<int>(PatchSource::Unknown), static_cast<int>(PatchSource::User)));
    mPendingRestoredFactoryPatchIdx = restored.factoryPatchIdx;
    mPendingRestoredPatchPath = std::move(restored.patchPath);
    mPendingRestoredPatchGroupKey = std::move(restored.patchGroupKey);
    mPendingRestoredPatchHasIdentity = mPendingRestoredPatchSource != PatchSource::Unknown;
  }
  if (mRestoringFactoryPatchIdx < 0) SetPortamentoCC(restored.portamentoCC);
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    mPendingRestoredPatchCleanSnapshot = std::move(restored.patchCleanSnapshot);
    mPendingRestoredPatchHasCleanSnapshot = !mPendingRestoredPatchCleanSnapshot.empty();
  }

  return pos;
}

bool Addivox::HasPendingRestoredState() const {
  const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
  return !mPendingRestoredStatePatchName.empty() || mPendingRestoredPatchHasIdentity || mPendingRestoredPatchHasCleanSnapshot || mRestoringFactoryPatchIdx >= 0;
}

void Addivox::ApplyPendingRestoredState() {
  const std::unique_lock<std::recursive_mutex> patchLock(mEditorState->patchMutex);

  if (!mPendingRestoredStatePatchName.empty()) {
    mActivePatchDisplayName = mPendingRestoredStatePatchName;
    mPendingRestoredStatePatchName.clear();
  } else if (NPresets() > 0 && GetCurrentPresetIdx() >= 0 && GetCurrentPresetIdx() < NPresets()) {
    mActivePatchDisplayName = GetPresetName(GetCurrentPresetIdx());
  }

  if (mPendingRestoredPatchHasIdentity) {
    mActivePatchSource = mPendingRestoredPatchSource;
    mActiveFactoryPatchIdx = mPendingRestoredFactoryPatchIdx;
    mActivePatchPath = std::move(mPendingRestoredPatchPath);
    mActivePatchGroupKey = std::move(mPendingRestoredPatchGroupKey);

    if (mActivePatchSource == PatchSource::Factory) {
      SetActiveFactoryPatch(mActiveFactoryPatchIdx);
    } else if (mActivePatchSource == PatchSource::User) {
      const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
      if (!mActivePatchPath.empty() && !IsAbsolutePath(mActivePatchPath) && !userPatchDirectory.empty())
        mActivePatchPath = patch_io::detail::JoinPath(userPatchDirectory, mActivePatchPath);
      if (mActivePatchGroupKey.empty() && !mActivePatchPath.empty() && !userPatchDirectory.empty())
        mActivePatchGroupKey = UserPatchGroupKeyForPath(userPatchDirectory, mActivePatchPath);
    }
  } else if (mRestoringFactoryPatchIdx >= 0) {
    SetActiveFactoryPatch(mRestoringFactoryPatchIdx);
    mRestoringFactoryPatchIdx = -1;
    mPendingRestoredPatchCleanSnapshot.clear();
    mPendingRestoredPatchHasCleanSnapshot = false;
  } else {
    mActivePatchSource = PatchSource::Unknown;
    mActiveFactoryPatchIdx = -1;
    mActivePatchPath.clear();
    mActivePatchGroupKey = "Factory";
  }
  mPendingRestoredPatchHasIdentity = false;
  mPendingRestoredPatchSource = PatchSource::Unknown;
  mPendingRestoredFactoryPatchIdx = -1;
  mPendingRestoredPatchPath.clear();
  mPendingRestoredPatchGroupKey.clear();
  mRestoringFactoryPatchIdx = -1;

  if (mPendingRestoredPatchHasCleanSnapshot) {
    mActivePatchCleanSnapshot = CanonicalizePatchSnapshotToml(mPendingRestoredPatchCleanSnapshot);
    const bool patchOwnedStateDiffers =
        CanonicalizePatchDirtySnapshotToml(SerializeCurrentPatchSnapshot()) != CanonicalizePatchDirtySnapshotToml(mActivePatchCleanSnapshot);
    mActivePatchDirty = mPendingRestoredPatchHasDirtyState ? (mPendingRestoredPatchDirty && patchOwnedStateDiffers) : patchOwnedStateDiffers;
  } else {
    SetActivePatchCleanSnapshotFromCurrentState();
  }
  mPendingRestoredPatchCleanSnapshot.clear();
  mPendingRestoredPatchHasCleanSnapshot = false;
  mPendingRestoredPatchDirty = false;
  mPendingRestoredPatchHasDirtyState = false;
  RebuildPatchCatalog();
  RefreshEditorUI(true);
  MarkStandaloneStateDirty();
}

void Addivox::OnRestoreState() {
  mSuppressPatchDirtyTracking = true;
  IEditorDelegate::OnRestoreState();
  mSuppressPatchDirtyTracking = false;

  ApplyPendingRestoredState();
}

void Addivox::OnUIOpen() {
  EnsureStandaloneStateInitialized();
  mLastSentBreathLevel = -1.;
  mSuppressPatchDirtyTracking = true;
  IEditorDelegate::OnUIOpen();
  mSuppressPatchDirtyTracking = false;
  if (HasPendingRestoredState()) {
    ApplyPendingRestoredState();
  } else {
    RebuildPatchCatalog();
    RefreshEditorUI();
  }
}

#if defined OS_IOS
// iOS hosts decide how much space the UI gets (e.g. GarageBand's strip above its keyboard), so scale the whole UI to fit that space and centre it.
void Addivox::OnParentWindowResize(int width, int height) {
  IGraphics* ui = GetUI();
  if (!ui || width <= 0 || height <= 0) return;

  const float scale = std::min(static_cast<float>(width) / PLUG_WIDTH, static_cast<float>(height) / PLUG_HEIGHT);
  ui->Resize(PLUG_WIDTH, PLUG_HEIGHT, scale);
  addivox_ios::CenterViewInSuperview(ui->GetWindow());
}
#endif

void Addivox::OnUIClose() {
  SaveStandaloneStateIfNeeded(true);
  mEditorContext.reset();
  ResetQwertyKeyboard();
}

void Addivox::ResetQwertyKeyboard() {
  mQwertyMidiKeysDown.fill(false);
  mQwertyMidiBaseNote = 48;
  mWasQwertyKeyboardInEditMode = false;
  mLastQwertyMIDINote = -1;
}

bool Addivox::OnHostRequestingAboutBox() { return ShowAboutBox(); }

bool Addivox::OnHostRequestingProductHelp() { return OpenOnlineDocs(); }

void Addivox::OnParamChangeUI(int paramIdx, EParamSource source) {
  if (ShouldTrackPatchDirtyForParamSource(source) && IsPatchOwnedParam(paramIdx)) MarkActivePatchDirty();

  MarkStandaloneStateDirty();
}

bool Addivox::ShowAboutBox() { return plugin_ui::layout::ShowAboutBox(GetUI(), kCtrlTagAboutBox); }

bool Addivox::OpenOnlineDocs() {
  if (auto* ui = GetUI()) return ui->OpenURL(PLUG_URL_STR);

  return false;
}

#if IPLUG_DSP
void Addivox::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {
  (void)inputs;
  ENTER_PARAMS_MUTEX
  mDSP.ProcessBlock(outputs, nFrames);
  LEAVE_PARAMS_MUTEX
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
  if (mHarmonicVisualizerEnabled.load(std::memory_order_relaxed)) {
    mHarmonicVisualizerSender.ProcessBlock(nFrames, kCtrlTagHarmonicVisualizer, [this](VisualizerFrame& frame) { mDSP.GetVisualizerFrame(frame); });
  }
}

void Addivox::OnIdle() {
  mMeterSender.TransmitData(*this);
  if (mHarmonicVisualizerEnabled.load(std::memory_order_relaxed)) {
    mHarmonicVisualizerSender.TransmitData(*this);
  } else {
    mHarmonicVisualizerSender.ClearQueuedData();
    if (mHarmonicVisualizerBlankPending.exchange(false, std::memory_order_relaxed)) {
      mHarmonicVisualizerSender.PushFrame(kCtrlTagHarmonicVisualizer, VisualizerFrame{});
      mHarmonicVisualizerSender.TransmitData(*this);
    }
  }

  const double breathLevel = mBreathLevel.load(std::memory_order_relaxed);
  if (breathLevel != mLastSentBreathLevel) {
    SendControlValueFromDelegate(kCtrlTagBreathMeter, breathLevel);
    mLastSentBreathLevel = breathLevel;
  }

#if IPLUG_EDITOR
  ReleaseQwertyNotesIfKeysReleased();
#endif
  SaveStandaloneStateIfNeeded();
}

void Addivox::OnReset() {
  EnsureStandaloneStateInitialized();
  mDSP.Reset(GetSampleRate(), GetBlockSize());
  mDSP.mSynth.SetPitchBendRange(mPitchBendRange);
  mDSP.mSynth.SetPortamentoCC(mPortamentoCC);
  mDSP.SetBreathCCSource(mBreathCCSource);
  mBreathCCInputTracker.Reset();

  mMeterSender.Reset(GetSampleRate());

  mHarmonicVisualizerSender.Reset(GetSampleRate(), PLUG_FPS);
  mHarmonicVisualizerBlankPending.store(!mHarmonicVisualizerEnabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
  ResetQwertyKeyboard();
  mBreathLevel.store(0.0, std::memory_order_relaxed);
  mLastSentBreathLevel = -1.;
}

void Addivox::ProcessMidiMsg(const IMidiMsg& msg) {
  const BreathCCValueUpdate breathUpdate = mBreathCCInputTracker.HandleMessage(mBreathCCSource, msg);
  if (breathUpdate.hasValue) mBreathLevel.store(breathUpdate.value, std::memory_order_relaxed);

  mDSP.ProcessMidiMsg(msg);
  SendMidiMsg(msg);
}

void Addivox::OnParamChange(int paramIdx) { mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value()); }

bool Addivox::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) {
  using namespace editor_messages;

  if (msgTag == kMsgTagPromptLoadPatchFromFile) {
    PromptLoadPatchFromFile();
    return true;
  }

  if (msgTag == kMsgTagPromptSavePatchToFile) {
    PromptSavePatchToFile();
    return true;
  }

  if (const auto* payload = GetPayload<PatchManagerActionPayload>(msgTag == kMsgTagPatchManagerAction, dataSize, pData)) {
    HandlePatchManagerAction(payload->action, payload->patchId);
    return true;
  }

  if (const auto* payload = GetPayload<SetPortamentoCCPayload>(msgTag == kMsgTagSetPortamentoCC, dataSize, pData)) {
    SetPortamentoCC(payload->controller);
    return true;
  }

  if (const auto* payload = GetPayload<SetBreathCCSourcePayload>(msgTag == kMsgTagSetBreathCCSource, dataSize, pData)) {
    SetBreathCCSource(SanitizeBreathCCSource(payload->source));
    return true;
  }

  if (const auto* payload = GetPayload<SetPitchBendRangePayload>(msgTag == kMsgTagSetPitchBendRange, dataSize, pData)) {
    SetPitchBendRange(payload->semitones);
    return true;
  }

  if (const auto* payload = GetPayload<SetHarmonicVisualizerEnabledPayload>(msgTag == kMsgTagSetHarmonicVisualizerEnabled, dataSize, pData)) {
    SetHarmonicVisualizerEnabled(payload->enabled != 0);
    return true;
  }

  if (msgTag == kMsgTagResetStandaloneStateToDefaults) {
    ResetStandaloneStateToDefaults();
    return true;
  }

  if (msgTag == kMsgTagOpenAudioMidiSettings) {
#if defined APP_API
    return addivox_standalone::OpenAudioMidiSettingsDialog();
#else
    return false;
#endif
  }

  const auto applyPatchEdit = [this](auto&& edit) {
    const bool updated = mDSP.EditCompoundPatch(std::forward<decltype(edit)>(edit));
    if (updated) {
      MarkActivePatchDirty();
      MarkStandaloneStateDirty();
    }
    return updated;
  };
  const auto isValidParameter = [](int parameter) { return parameter >= 0 && parameter < OscillatorSettings::kNumParameters; };
  const bool fromEditorTabs = ctrlTag == kCtrlTagEditorTabs;

  if (const auto* payload =
          GetPayload<SetKeyNoteOscillatorParameterPayload>(fromEditorTabs && msgTag == kMsgTagSetKeyNoteOscillatorParameter, dataSize, pData)) {
    if (!isValidParameter(payload->parameter)) return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    return applyPatchEdit([&](CompoundPatch& patch) {
      return patch.SetKeyNoteOscillatorParameter(payload->midiNote, payload->oscillatorIndex, parameter, payload->value);
    });
  }

  if (const auto* payload =
          GetPayload<SetKeyNoteOscillatorParameterValuesPayload>(fromEditorTabs && msgTag == kMsgTagSetKeyNoteOscillatorParameterValues, dataSize, pData)) {
    if (!isValidParameter(payload->parameter)) return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    return applyPatchEdit([&](CompoundPatch& patch) { return patch.SetKeyNoteOscillatorParameterValues(payload->midiNote, parameter, payload->values); });
  }

  if (fromEditorTabs && msgTag == kMsgTagSetKeyNoteEqCurve && dataSize > 0 && pData) {
    int midiNote = 0;
    EqCurve curve;
    if (!DeserializeKeyNoteEqCurvePayload(dataSize, pData, midiNote, curve)) return false;

    return applyPatchEdit([&](CompoundPatch& patch) { return patch.SetKeyNoteEqCurve(midiNote, curve); });
  }

  if (fromEditorTabs && msgTag == kMsgTagSetKeyNoteResonance && dataSize > 0 && pData) {
    int midiNote = 0;
    ResonanceSettings resonance;
    if (!DeserializeKeyNoteResonancePayload(dataSize, pData, midiNote, resonance)) return false;

    return applyPatchEdit([&](CompoundPatch& patch) { return patch.SetKeyNoteResonance(midiNote, resonance); });
  }

  if (const auto* payload = GetPayload<SetAllKeyNotesEnabledPayload>(fromEditorTabs && msgTag == kMsgTagSetAllKeyNotesEnabled, dataSize, pData)) {
    if (!isValidParameter(payload->parameter)) return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    return applyPatchEdit([&](CompoundPatch& patch) {
      patch.SetAllKeyNotesEnabled(parameter, payload->enabled != 0, payload->midiNote);
      return true;
    });
  }

  if (const auto* payload = GetPayload<SetAllKeyNotesEqEnabledPayload>(fromEditorTabs && msgTag == kMsgTagSetAllKeyNotesEqEnabled, dataSize, pData)) {
    return applyPatchEdit([&](CompoundPatch& patch) {
      patch.SetAllKeyNotesEqEnabled(payload->enabled != 0);
      return true;
    });
  }

  if (const auto* payload =
          GetPayload<SetAllKeyNotesResonanceEnabledPayload>(fromEditorTabs && msgTag == kMsgTagSetAllKeyNotesResonanceEnabled, dataSize, pData)) {
    return applyPatchEdit([&](CompoundPatch& patch) {
      patch.SetAllKeyNotesResonanceEnabled(payload->enabled != 0, payload->midiNote);
      return true;
    });
  }

  const bool isKeyNotePatchMessage = fromEditorTabs && (msgTag == kMsgTagAddKeyNotePatch || msgTag == kMsgTagRemoveKeyNotePatch);
  if (const auto* payload = GetPayload<KeyNotePatchPayload>(isKeyNotePatchMessage, dataSize, pData)) {
    return applyPatchEdit([&](CompoundPatch& patch) {
      return msgTag == kMsgTagAddKeyNotePatch ? patch.AddKeyNotePatch(payload->midiNote)
                                              : patch.RemoveKeyNotePatch(static_cast<int>(std::lround(payload->midiNote)));
    });
  }

  const bool isBenderRangeMessage = ctrlTag == kCtrlTagBender && msgTag == plugin_ui::layout::PitchBendWheelControl::kMessageTagSetPitchBendRange;
  if (const auto* bendRange = GetPayload<int>(isBenderRangeMessage, dataSize, pData)) {
    SetPitchBendRange(*bendRange);
    return true;
  }

  return false;
}
#endif

void Addivox::RebuildPatchCatalog() {
  const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);

  mPatchCatalog.clear();
  int nextId = 0;

  for (int patchIdx = 0; patchIdx < NPresets(); ++patchIdx) {
    PatchCatalogEntry entry;
    entry.id = nextId++;
    entry.source = PatchSource::Factory;
    entry.factoryIndex = patchIdx;
    entry.name = GetPresetName(patchIdx);
    if (entry.name.empty()) entry.name = "Patch " + std::to_string(patchIdx + 1);
    entry.path = FactoryPatchPath(patchIdx);
    entry.menuPath = {"Factory"};
    entry.groupKey = "Factory";
    mPatchCatalog.push_back(std::move(entry));
  }

  const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
  SyncUserPatchDirectoryToSharedMirror();
  const std::vector<std::string> userPatchPaths = patch_io::FindPatchFiles(userPatchDirectory);
  for (const auto& patchPath : userPatchPaths) {
    patch_io::PatchDocument document;
    patch_io::LoadPatchFromFile(patchPath, document);

    PatchCatalogEntry entry;
    entry.id = nextId++;
    entry.source = PatchSource::User;
    entry.path = patchPath;
    entry.name = document.name.empty() ? patch_io::detail::FileStem(patchPath) : document.name;

    entry.menuPath = UserPatchMenuPath(userPatchDirectory, patchPath);
    entry.groupKey = MakeGroupKey(entry.menuPath);
    mPatchCatalog.push_back(std::move(entry));
  }

  ReconcileActivePatchIdentity();
}

void Addivox::ReconcileActivePatchIdentity() {
  const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);

  if (mActivePatchSource == PatchSource::Factory) {
    // Factory patches can move when new ones are inserted. Saved states retain the patch name as well as its old index.
    if (!mActivePatchDisplayName.empty() &&
        (mActiveFactoryPatchIdx < 0 || mActiveFactoryPatchIdx >= NPresets() || mActivePatchDisplayName != GetPresetName(mActiveFactoryPatchIdx))) {
      mActiveFactoryPatchIdx = -1;
      for (int patchIdx = 0; patchIdx < NPresets(); ++patchIdx) {
        if (mActivePatchDisplayName == GetPresetName(patchIdx)) {
          mActiveFactoryPatchIdx = patchIdx;
          break;
        }
      }
    }
    if (mActiveFactoryPatchIdx >= 0 && mActiveFactoryPatchIdx < NPresets()) {
      SetCurrentPresetIdx(mActiveFactoryPatchIdx);
      SetActiveFactoryPatch(mActiveFactoryPatchIdx);
      return;
    }
    mActivePatchSource = PatchSource::Unknown;
    mActiveFactoryPatchIdx = -1;
    mActivePatchPath.clear();
  }

  if (mActivePatchSource == PatchSource::User) {
    const auto entryIt = std::find_if(mPatchCatalog.begin(), mPatchCatalog.end(),
                                      [this](const PatchCatalogEntry& entry) { return entry.source == PatchSource::User && entry.path == mActivePatchPath; });
    if (entryIt != mPatchCatalog.end()) mActivePatchGroupKey = entryIt->groupKey;
    else if (mActivePatchGroupKey.empty())
      mActivePatchGroupKey = "Factory";
    return;
  }

  const std::string snapshot = !mActivePatchCleanSnapshot.empty() ? mActivePatchCleanSnapshot : SerializeCurrentPatchSnapshot();
  if (snapshot.empty()) return;

  for (const auto& entry : mPatchCatalog) {
    if (entry.path.empty()) continue;

    patch_io::PatchDocument document;
    if (!patch_io::LoadPatchFromFile(entry.path, document)) continue;
    if (CanonicalizePatchSnapshotToml(patch_io::SerializePatchToToml(document)) != snapshot) continue;

    mActivePatchSource = entry.source;
    mActiveFactoryPatchIdx = entry.factoryIndex;
    mActivePatchPath = entry.path;
    mActivePatchGroupKey = entry.groupKey;
    if (mActivePatchDisplayName.empty()) mActivePatchDisplayName = entry.name;
    return;
  }
}

void Addivox::RestoreFactoryPatch(int patchIdx) {
  if (patchIdx < 0 || patchIdx >= NPresets()) return;

  mRestoringFactoryPatchIdx = patchIdx;
  if (!RestorePreset(patchIdx)) {
    mRestoringFactoryPatchIdx = -1;
    return;
  }

  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    SetActiveFactoryPatch(patchIdx);
    mActivePatchDisplayName = GetPresetName(patchIdx);
    SetActivePatchCleanSnapshotFromCurrentState();
  }
  RefreshEditorUI(true);
}

void Addivox::LoadUserPatchByPath(const std::string& path) {
  patch_io::PatchDocument document;
  std::string errorMessage;
  if (!patch_io::LoadPatchFromFile(path, document, &errorMessage)) {
    ShowPatchFileError(GetUI(), "Patch Load Failed", errorMessage);
    return;
  }

  mUserPatchDirectory = patch_io::detail::ParentPath(path);
  mSuppressPatchDirtyTracking = true;
  ApplyPatchDocumentToState(document);
  FinalizePatchRecall(true);
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    if (!document.name.empty()) mActivePatchDisplayName = document.name;
    else if (mActivePatchDisplayName.empty())
      mActivePatchDisplayName = "Patch";
  }
  mSuppressPatchDirtyTracking = false;

  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    SetActiveUserPatch(path);
    SetActivePatchCleanSnapshotFromCurrentState();
  }
  RebuildPatchCatalog();
  RefreshEditorUI(true);
  MarkStandaloneStateDirty();
}

void Addivox::PromptImportPatchFromFile() {
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if (!ui) return;

  WDL_String fileName;
  WDL_String directory{GetPatchDialogDirectory(mUserPatchDirectory).c_str()};

  ui->PromptForFile(fileName, directory, EFileAction::Open, "toml", [this](const WDL_String& selectedFileName, const WDL_String&) {
    const std::string sourcePath = GetFullFileDialogPath(selectedFileName);
    if (sourcePath.empty()) return;

    patch_io::PatchDocument document;
    std::string errorMessage;
    if (!patch_io::LoadPatchFromFile(sourcePath, document, &errorMessage)) {
      ShowPatchFileError(GetUI(), "Import Failed", errorMessage);
      return;
    }

    const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
    if (userPatchDirectory.empty()) {
      ShowPatchFileError(GetUI(), "Import Failed", "Could not locate the user patch directory.");
      return;
    }

    const std::string destinationPath = GetUniquePatchFilePath(userPatchDirectory, patch_io::detail::FileNameView(sourcePath));
    if (!CopyFileBytes(sourcePath, destinationPath)) {
      ShowPatchFileError(GetUI(), "Import Failed", "Could not copy patch: " + sourcePath);
      return;
    }

    mUserPatchDirectory = userPatchDirectory;
    RebuildPatchCatalog();
    LoadUserPatchByPath(destinationPath);
  });
#endif
}

void Addivox::LoadPatchById(int patchId) {
  // Copy the entry under the lock; the load/restore calls below take iPlug2's params mutex and must run unlocked.
  PatchSource source = PatchSource::Unknown;
  int factoryIndex = -1;
  std::string path;
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    const auto entryIt = std::find_if(mPatchCatalog.begin(), mPatchCatalog.end(), [patchId](const PatchCatalogEntry& entry) { return entry.id == patchId; });
    if (entryIt == mPatchCatalog.end()) return;

    source = entryIt->source;
    factoryIndex = entryIt->factoryIndex;
    path = entryIt->path;
  }

  if (source == PatchSource::Factory) RestoreFactoryPatch(factoryIndex);
  else if (source == PatchSource::User)
    LoadUserPatchByPath(path);
}

// These two set which catalog patch is active. Callers hold the patch mutex.
void Addivox::SetActiveFactoryPatch(int patchIdx) {
  mActivePatchSource = PatchSource::Factory;
  mActiveFactoryPatchIdx = patchIdx;
  mActivePatchPath = FactoryPatchPath(patchIdx);
  mActivePatchGroupKey = "Factory";
}

void Addivox::SetActiveUserPatch(const std::string& path) {
  mActivePatchSource = PatchSource::User;
  mActiveFactoryPatchIdx = -1;
  mActivePatchPath = path;
  mActivePatchGroupKey = UserPatchGroupKeyForPath(EnsureDefaultUserPatchDirectory(), path);
}

std::string Addivox::FactoryPatchPath(int patchIdx) const {
  const bool hasPath = patchIdx >= 0 && patchIdx < static_cast<int>(mFactoryPatchPaths.size());
  return hasPath ? mFactoryPatchPaths[patchIdx] : std::string{};
}

bool Addivox::IsActivePatch(const PatchCatalogEntry& entry) const {
  return (entry.source == PatchSource::Factory && mActivePatchSource == PatchSource::Factory && entry.factoryIndex == mActiveFactoryPatchIdx) ||
         (entry.source == PatchSource::User && mActivePatchSource == PatchSource::User && entry.path == mActivePatchPath);
}

void Addivox::CyclePatchInCurrentGroup(int direction) {
  if (mPatchCatalog.empty()) RebuildPatchCatalog();

  // Resolve the next patch id under the lock; LoadPatchById re-finds by id and must run unlocked (see LoadPatchById).
  int nextPatchId = -1;
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);

    const auto entriesInGroup = [this](const std::string& groupKey) {
      std::vector<const PatchCatalogEntry*> entries;
      for (const auto& entry : mPatchCatalog)
        if (entry.groupKey == groupKey) entries.push_back(&entry);
      return entries;
    };

    std::vector<const PatchCatalogEntry*> groupEntries = entriesInGroup(mActivePatchGroupKey);
    if (groupEntries.empty()) groupEntries = entriesInGroup("Factory");
    if (groupEntries.empty()) return;

    // With no active patch in the group, stepping forwards lands on the first patch and stepping backwards on the last.
    const int numEntries = static_cast<int>(groupEntries.size());
    const auto activeIt = std::find_if(groupEntries.begin(), groupEntries.end(), [this](const PatchCatalogEntry* entry) { return IsActivePatch(*entry); });
    const int currentIndex = activeIt != groupEntries.end() ? static_cast<int>(activeIt - groupEntries.begin()) : (direction >= 0 ? -1 : 0);
    const int nextIndex = (currentIndex + (direction >= 0 ? 1 : -1) + numEntries) % numEntries;

    nextPatchId = groupEntries[static_cast<std::size_t>(nextIndex)]->id;
  }

  LoadPatchById(nextPatchId);
}

void Addivox::PromptImportPatchCollection() {
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if (!ui) return;

  WDL_String directory{GetPatchDialogDirectory(mUserPatchDirectory).c_str()};

  ui->PromptForDirectory(directory, [this](const WDL_String&, const WDL_String& selectedDirectory) {
    if (selectedDirectory.GetLength() == 0) return;

    const std::string sourceDirectory = TrimTrailingSeparators(selectedDirectory.Get());
    const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
    if (userPatchDirectory.empty()) {
      ShowPatchFileError(GetUI(), "Import Failed", "Could not locate the user patch directory.");
      return;
    }

    const std::string collectionName{patch_io::detail::FileNameView(sourceDirectory)};
    const std::string destinationDirectory = GetUniqueChildDirectory(userPatchDirectory, collectionName);
    std::string errorMessage;
    if (!CopyPatchCollection(sourceDirectory, destinationDirectory, &errorMessage)) {
      ShowPatchFileError(GetUI(), "Import Failed", errorMessage);
      return;
    }

    mUserPatchDirectory = destinationDirectory;
    RebuildPatchCatalog();
    RefreshEditorUI();
  });
#endif
}

void Addivox::ShowActivePatchInFileBrowser() {
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if (!ui) return;

  std::string activePatchPath;
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    if (mActivePatchSource == PatchSource::User) activePatchPath = mActivePatchPath;
  }

  WDL_String path;
  if (!activePatchPath.empty() && patch_io::detail::PathExists(activePatchPath)) {
    path.Set(activePatchPath.c_str());
    ui->RevealPathInExplorerOrFinder(path, true);
    return;
  }

  const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
  if (userPatchDirectory.empty()) return;

  path.Set(userPatchDirectory.c_str());
  ui->RevealPathInExplorerOrFinder(path, false);
#endif
}

void Addivox::HandlePatchManagerAction(int action, int patchId) {
  using editor_messages::PatchManagerAction;
  switch (static_cast<PatchManagerAction>(action)) {
  case PatchManagerAction::SelectPatch:            LoadPatchById(patchId); break;
  case PatchManagerAction::PreviousPatch:          CyclePatchInCurrentGroup(-1); break;
  case PatchManagerAction::NextPatch:              CyclePatchInCurrentGroup(1); break;
  case PatchManagerAction::SavePatch:
    if (!IsReadOnlyUserPatchLibrary()) PromptSavePatchToFile();
    break;
  case PatchManagerAction::ImportPatch:
    if (!IsReadOnlyUserPatchLibrary()) PromptImportPatchFromFile();
    break;
  case PatchManagerAction::ImportCollection:
    if (!IsReadOnlyUserPatchLibrary()) PromptImportPatchCollection();
    break;
  case PatchManagerAction::ShowPatchInFileBrowser: ShowActivePatchInFileBrowser(); break;
  case PatchManagerAction::RefreshPatches:
    RebuildPatchCatalog();
    RefreshEditorUI();
    break;
  }
}

std::string Addivox::SerializeCurrentPatchSnapshot() const {
  const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);

  patch_io::PatchDocument document;
  document.name =
      mActivePatchDisplayName.empty()
          ? (NPresets() > 0 ? std::string{GetPresetName(mActiveFactoryPatchIdx >= 0 ? mActiveFactoryPatchIdx : GetCurrentPresetIdx())} : std::string{"Patch"})
          : mActivePatchDisplayName;
  document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
  document.effectsSettings = GetEffectsSettingsFromParams(*this);
  document.compoundPatch = mEditorState ? mEditorState->compoundPatch : CompoundPatch{};
  return patch_io::SerializePatchToToml(document);
}

void Addivox::SetActivePatchCleanSnapshotFromCurrentState() {
  const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
  mActivePatchCleanSnapshot = SerializeCurrentPatchSnapshot();
  mActivePatchDirty = false;
}

void Addivox::MarkActivePatchDirty() {
  {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    if (mSuppressPatchDirtyTracking || mActivePatchDirty) return;

    if (!mActivePatchCleanSnapshot.empty() &&
        CanonicalizePatchDirtySnapshotToml(SerializeCurrentPatchSnapshot()) == CanonicalizePatchDirtySnapshotToml(mActivePatchCleanSnapshot))
      return;

    mActivePatchDirty = true;
  }
  RefreshEditorUI();
}

void Addivox::LoadBuiltInPatches() {
  mFactoryPatchPaths.clear();

  const auto addPatch = [this](const patch_io::PatchDocument& document, const std::string& path) {
    IByteChunk chunk;
    if (!BuildPatchChunk(document, chunk)) return false;

    const std::string patchName = document.name.empty() ? patch_io::detail::FileStem(path) : document.name;
    MakePresetFromChunk(patchName.c_str(), chunk);
    mFactoryPatchPaths.push_back(path);
    return true;
  };

  int numLoadedPatches = 0;
#if defined OS_WIN
  for (const std::string& resourceName : FindEmbeddedFactoryPatchNames()) {
    int resourceSize = 0;
    const void* resourceData = LoadWinResource(resourceName.c_str(), "toml", resourceSize, gHINSTANCE);
    if (!resourceData || resourceSize <= 0) continue;

    patch_io::PatchDocument document;
    const std::string toml(static_cast<const char*>(resourceData), static_cast<std::size_t>(resourceSize));
    if (!patch_io::ParsePatchToml(toml, document) || !addPatch(document, {})) continue;

    ++numLoadedPatches;
    if (numLoadedPatches >= kMaxFactoryPatches) break;
  }
#else
  WDL_String resourcePath;
  BundleResourcePath(resourcePath, GetBundleID());
  if (resourcePath.GetLength() > 0) {
    const auto patchPaths = patch_io::FindPatchFiles(patch_io::detail::JoinPath(resourcePath.Get(), "factory_patches"));
    for (const auto& patchPath : patchPaths) {
      patch_io::PatchDocument document;
      if (!patch_io::LoadPatchFromFile(patchPath, document) || !addPatch(document, patchPath)) continue;
      ++numLoadedPatches;

      if (numLoadedPatches >= kMaxFactoryPatches) break;
    }
  }
#endif

  if (numLoadedPatches == 0) {
    patch_io::PatchDocument fallbackDocument;
    fallbackDocument.name = "Init";
    addPatch(fallbackDocument, {});
  }

  PruneUninitializedPresets();
}

void Addivox::RefreshEditorUI(bool resetOscillatorRestoreStates) {
#if IPLUG_EDITOR
  if (!GetUI() || !mEditorContext) return;

  if (mEditorContext->patchManagerControl) {
    if (auto* patchManager = dynamic_cast<plugin_ui::layout::PatchManagerControl*>(mEditorContext->patchManagerControl)) {
      plugin_ui::layout::PatchMenuModel model;
      {
        const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
        std::string label = mActivePatchDisplayName.empty()
                                ? (NPresets() > 0 ? std::string{GetPresetName(mActiveFactoryPatchIdx >= 0 ? mActiveFactoryPatchIdx : GetCurrentPresetIdx())}
                                                  : std::string{"Choose Patch..."})
                                : mActivePatchDisplayName;
        if (mActivePatchDirty) label += "*";

        model.label = label;
        if (IsReadOnlyUserPatchLibrary()) {
          model.canSavePatch = false;
          model.canImportPatch = false;
          model.canImportCollection = false;
        }
        const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
        model.canShowInFileBrowser = (mActivePatchSource == PatchSource::User && !mActivePatchPath.empty() && patch_io::detail::PathExists(mActivePatchPath)) ||
                                     (!userPatchDirectory.empty() && patch_io::detail::IsDirectory(userPatchDirectory));

        for (const auto& entry : mPatchCatalog) {
          plugin_ui::layout::PatchMenuEntry menuEntry;
          menuEntry.id = entry.id;
          menuEntry.name = entry.name;
          menuEntry.groupPath = entry.menuPath;
          menuEntry.checked = IsActivePatch(entry);
          model.entries.push_back(std::move(menuEntry));
        }
      }

      patchManager->SetPatchMenuModel(std::move(model));
    }
  }

  if (auto* keyboard = plugin_ui::layout::GetKeyboardControl(GetUI(), kCtrlTagKeyboard)) {
    const std::lock_guard<std::recursive_mutex> patchLock(mEditorState->patchMutex);
    plugin_ui::layout::RefreshKeyboardKeyNoteHighlights(keyboard, mEditorState->compoundPatch);
    keyboard->SetSelectedMidiNote(mEditorState->selectedMidiNote);
  }

  SyncPitchBendRangeUI();

  if (resetOscillatorRestoreStates) mEditorContext->ResetOscillatorRestoreStates();

  mEditorContext->RefreshOscillatorTabs();
  mEditorContext->RefreshEditorActionButtons();
  GetUI()->SetAllControlsDirty();
#endif
}

void Addivox::SyncPitchBendRangeUI() {
#if IPLUG_EDITOR
  if (mEditorState) mEditorState->pitchBendRange = mPitchBendRange;
  if (!GetUI()) return;

  if (auto* control = GetUI()->GetControlWithTag(kCtrlTagBender)) {
    if (auto* wheelControl = control->As<plugin_ui::layout::PitchBendWheelControl>()) wheelControl->SetPitchBendRange(mPitchBendRange);
  }
#endif
}

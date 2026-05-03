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

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>

namespace {
constexpr uint32_t kPluginStateSettingsMagic = 0x42524343u; // Legacy plugin-state settings block magic.
constexpr int kDefaultPitchBendRange = 2;
constexpr int kMaxPitchBendRange = 96;
constexpr bool kDefaultHarmonicVisualizerEnabled = true;
constexpr int64_t kStandaloneStateSaveDebounceMs = 250;

int SanitizePitchBendRange(int pitchBendRange) { return std::clamp(pitchBendRange, 0, kMaxPitchBendRange); }

int64_t GetMonotonicMilliseconds() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

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

void SetGlobalVoiceSettingsParams(Addivox& plugin, const GlobalVoiceSettings& voiceSettings, bool includeTuning = true, bool includePanShift = true) {
  const GlobalVoiceSettings sanitizedVoiceSettings = global_settings::Sanitize(voiceSettings);
  plugin.GetParam(kParamGlobalLevel)->Set(sanitizedVoiceSettings.levelScale);
  plugin.GetParam(kParamGlobalAttackScale)->Set(sanitizedVoiceSettings.attackScale);
  plugin.GetParam(kParamGlobalReleaseScale)->Set(sanitizedVoiceSettings.releaseScale);
  if (includeTuning) plugin.GetParam(kParamGlobalTuning)->Set(sanitizedVoiceSettings.tuningCents);
  if (includePanShift) plugin.GetParam(kParamGlobalPanShift)->Set(sanitizedVoiceSettings.panOffset);
  plugin.GetParam(kParamGlobalLevelVariationAmplitudeScale)->Set(sanitizedVoiceSettings.levelVariationAmplitudeScale);
  plugin.GetParam(kParamGlobalLevelVariationRateScale)->Set(sanitizedVoiceSettings.levelVariationRateScale);
  plugin.GetParam(kParamGlobalPitchVariationAmplitudeScale)->Set(sanitizedVoiceSettings.pitchVariationAmplitudeScale);
  plugin.GetParam(kParamGlobalPitchVariationRateScale)->Set(sanitizedVoiceSettings.pitchVariationRateScale);
  plugin.GetParam(kParamGlobalPanVariationAmplitudeScale)->Set(sanitizedVoiceSettings.panVariationAmplitudeScale);
  plugin.GetParam(kParamGlobalPanVariationRateScale)->Set(sanitizedVoiceSettings.panVariationRateScale);
  plugin.GetParam(kParamPortamentoAtCC5Min)->Set(sanitizedVoiceSettings.portamentoTimeAtCC5MinSec);
  plugin.GetParam(kParamPortamentoAtCC5Max)->Set(sanitizedVoiceSettings.portamentoTimeAtCC5MaxSec);
}

void SetEffectsSettingsParams(Addivox& plugin, const EffectsSettings& effectsSettings, bool includeReverb = true) {
  const EffectsSettings sanitizedEffectsSettings = effects_settings::Sanitize(effectsSettings);
  plugin.GetParam(kParamEffectsDrive)->Set(sanitizedEffectsSettings.drive);
  plugin.GetParam(kParamEffectsTone)->Set(sanitizedEffectsSettings.tone);
  plugin.GetParam(kParamEffectsChorus)->Set(sanitizedEffectsSettings.chorus);
  if (includeReverb) plugin.GetParam(kParamEffectsReverb)->Set(sanitizedEffectsSettings.reverb);
}

bool IsPatchOwnedParam(int paramIdx) {
  // These controls intentionally persist across patch changes, so reset should
  // keep their constructor defaults instead of following the last recalled
  // patch.
  switch (paramIdx) {
  case kParamGlobalTuning:
  case kParamGlobalPanShift:
  case kParamEffectsReverb:
  case kParamTranspose:      return false;
  default:                   return paramIdx >= 0 && paramIdx < kNumParams;
  }
}

void SyncPatchOwnedParamDefaultsToCurrentValues(Addivox& plugin) {
  for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) {
    if (!IsPatchOwnedParam(paramIdx)) continue;

    IParam* const param = plugin.GetParam(paramIdx);
    param->SetDefault(param->Value());
  }
}

bool BuildPatchChunk(const patch_io::PatchDocument& document, IByteChunk& chunk) {
  const std::string toml = patch_io::SerializePatchToToml(document);
  const GlobalVoiceSettings voiceSettings = global_settings::Sanitize(document.voiceSettings);
  const EffectsSettings effectsSettings = effects_settings::Sanitize(document.effectsSettings);
  return chunk.PutStr(toml.c_str()) > 0 && chunk.Put(&voiceSettings.levelScale) > 0 && chunk.Put(&voiceSettings.attackScale) > 0 &&
         chunk.Put(&voiceSettings.releaseScale) > 0 && chunk.Put(&voiceSettings.tuningCents) > 0 && chunk.Put(&voiceSettings.panOffset) > 0 &&
         chunk.Put(&voiceSettings.levelVariationAmplitudeScale) > 0 && chunk.Put(&voiceSettings.levelVariationRateScale) > 0 &&
         chunk.Put(&voiceSettings.pitchVariationAmplitudeScale) > 0 && chunk.Put(&voiceSettings.pitchVariationRateScale) > 0 &&
         chunk.Put(&voiceSettings.panVariationAmplitudeScale) > 0 && chunk.Put(&voiceSettings.panVariationRateScale) > 0 &&
         chunk.Put(&voiceSettings.portamentoTimeAtCC5MinSec) > 0 && chunk.Put(&voiceSettings.portamentoTimeAtCC5MaxSec) > 0 &&
         chunk.Put(&effectsSettings.reverb) > 0 && chunk.Put(&effectsSettings.drive) > 0 && chunk.Put(&effectsSettings.chorus) > 0 &&
         chunk.Put(&effectsSettings.tone) > 0;
}

bool AppendPluginStateSettingsChunk(IByteChunk& chunk, BreathCCSource breathCCSource, bool harmonicVisualizerEnabled, int pitchBendRange, bool patchDirty,
                                    const std::string& patchCleanSnapshot, int activePatchSource, int activeFactoryPatchIdx, const std::string& activePatchPath,
                                    const std::string& activePatchGroupKey) {
  const uint32_t magic = kPluginStateSettingsMagic;
  const int32_t rawBreathCCSource = static_cast<int32_t>(breathCCSource);
  const int32_t rawHarmonicVisualizerEnabled = harmonicVisualizerEnabled ? 1 : 0;
  const int32_t rawPitchBendRange = static_cast<int32_t>(SanitizePitchBendRange(pitchBendRange));
  const int32_t rawPatchDirty = patchDirty ? 1 : 0;
  const int32_t rawPatchSource = static_cast<int32_t>(activePatchSource);
  const int32_t rawFactoryPatchIdx = static_cast<int32_t>(activeFactoryPatchIdx);
  return chunk.Put(&magic) > 0 && chunk.Put(&rawBreathCCSource) > 0 && chunk.Put(&rawHarmonicVisualizerEnabled) > 0 && chunk.Put(&rawPitchBendRange) > 0 &&
         chunk.Put(&rawPatchDirty) > 0 && chunk.PutStr(patchCleanSnapshot.c_str()) > 0 && chunk.Put(&rawPatchSource) > 0 &&
         chunk.Put(&rawFactoryPatchIdx) > 0 && chunk.PutStr(activePatchPath.c_str()) > 0 && chunk.PutStr(activePatchGroupKey.c_str()) > 0;
}

bool ReadPluginStateSettingsChunk(const IByteChunk& chunk, int startPos, BreathCCSource& breathCCSource, bool& harmonicVisualizerEnabled, int& pitchBendRange,
                                  bool& patchDirty, std::string& patchCleanSnapshot, int& activePatchSource, int& activeFactoryPatchIdx,
                                  std::string& activePatchPath, std::string& activePatchGroupKey) {
  uint32_t magic = 0;
  int position = chunk.Get(&magic, startPos);
  if (position < 0 || magic != kPluginStateSettingsMagic) return false;

  int32_t rawBreathCCSource = static_cast<int32_t>(kDefaultBreathCCSource);
  position = chunk.Get(&rawBreathCCSource, position);
  if (position < 0) return false;

  breathCCSource = SanitizeBreathCCSource(rawBreathCCSource);

  int32_t rawHarmonicVisualizerEnabled = harmonicVisualizerEnabled ? 1 : 0;
  position = chunk.Get(&rawHarmonicVisualizerEnabled, position);
  if (position >= 0) harmonicVisualizerEnabled = (rawHarmonicVisualizerEnabled != 0);

  int32_t rawPitchBendRange = static_cast<int32_t>(pitchBendRange);
  if (position >= 0) {
    const int nextPosition = chunk.Get(&rawPitchBendRange, position);
    if (nextPosition >= 0) {
      pitchBendRange = SanitizePitchBendRange(rawPitchBendRange);
      position = nextPosition;
    }
  }

  int32_t rawPatchDirty = patchDirty ? 1 : 0;
  if (position >= 0) {
    const int nextPosition = chunk.Get(&rawPatchDirty, position);
    if (nextPosition >= 0) {
      patchDirty = (rawPatchDirty != 0);
      position = nextPosition;
    }
  }

  WDL_String rawPatchCleanSnapshot;
  if (position >= 0) {
    const int nextPosition = chunk.GetStr(rawPatchCleanSnapshot, position);
    if (nextPosition >= 0) {
      patchCleanSnapshot = rawPatchCleanSnapshot.Get();
      position = nextPosition;
    }
  }

  int32_t rawPatchSource = activePatchSource;
  if (position >= 0) {
    const int nextPosition = chunk.Get(&rawPatchSource, position);
    if (nextPosition >= 0) {
      activePatchSource = rawPatchSource;
      position = nextPosition;
    }
  }

  int32_t rawFactoryPatchIdx = activeFactoryPatchIdx;
  if (position >= 0) {
    const int nextPosition = chunk.Get(&rawFactoryPatchIdx, position);
    if (nextPosition >= 0) {
      activeFactoryPatchIdx = rawFactoryPatchIdx;
      position = nextPosition;
    }
  }

  WDL_String rawPatchPath;
  if (position >= 0) {
    const int nextPosition = chunk.GetStr(rawPatchPath, position);
    if (nextPosition >= 0) {
      activePatchPath = rawPatchPath.Get();
      position = nextPosition;
    }
  }

  WDL_String rawPatchGroupKey;
  if (position >= 0) {
    const int nextPosition = chunk.GetStr(rawPatchGroupKey, position);
    if (nextPosition >= 0) activePatchGroupKey = rawPatchGroupKey.Get();
  }

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

int RestoreStateParamsFromChunk(Addivox& plugin, const IByteChunk& chunk, int startPos) {
  const int remainingBytes = chunk.Size() - startPos;
  if (remainingBytes <= 0) return startPos;

  const int bytesPerValue = static_cast<int>(sizeof(double));
  const bool hasWholeDoublePayload = (remainingBytes % bytesPerValue) == 0;
  const int remainingValues = hasWholeDoublePayload ? (remainingBytes / bytesPerValue) : -1;
  int position = startPos;

  if (remainingValues == kNumParams) {
    for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) {
      if (!SetParamFromChunkValue(plugin, chunk, paramIdx, position)) break;
    }
    return position;
  }

  constexpr int kPatchChunkParamCountWithReverb = kParamEffectsTone + 1;
  constexpr int kPatchChunkParamCountWithoutReverb = kPatchChunkParamCountWithReverb - 1;
  if (remainingValues == kPatchChunkParamCountWithReverb || remainingValues == kPatchChunkParamCountWithoutReverb) {
    for (int paramIdx = 0; paramIdx <= kParamPortamentoAtCC5Max; ++paramIdx) {
      const bool applyValue = (paramIdx != kParamGlobalTuning) && (paramIdx != kParamGlobalPanShift);
      if (!SetParamFromChunkValue(plugin, chunk, paramIdx, position, applyValue)) return position;
    }

    if (remainingValues == kPatchChunkParamCountWithReverb) {
      if (!SetParamFromChunkValue(plugin, chunk, kParamEffectsReverb, position, false)) return position;
    }

    for (int paramIdx = kParamEffectsDrive; paramIdx <= kParamEffectsTone; ++paramIdx) {
      if (!SetParamFromChunkValue(plugin, chunk, paramIdx, position)) break;
    }

    return position;
  }

  for (int paramIdx = 0; paramIdx < kNumParams; ++paramIdx) {
    if (!SetParamFromChunkValue(plugin, chunk, paramIdx, position)) break;
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

std::string GetDefaultUserPatchDirectory() {
  WDL_String appSupportPath;
  AppSupportPath(appSupportPath, false);
  if (appSupportPath.GetLength() == 0) return {};

  return patch_io::detail::JoinPath(patch_io::detail::JoinPath(appSupportPath.Get(), BUNDLE_NAME), "Patches");
}

std::string EnsureDefaultUserPatchDirectory() {
  const std::string directory = GetDefaultUserPatchDirectory();
  if (directory.empty()) return {};

  if (!patch_io::detail::EnsureDirectoryExists(directory)) return {};

  return directory;
}

std::string GetStandaloneSettingsDirectory() {
  WDL_String appSupportPath;
  AppSupportPath(appSupportPath, false);
  if (appSupportPath.GetLength() == 0) return {};

  return patch_io::detail::JoinPath(appSupportPath.Get(), BUNDLE_NAME);
}

std::string GetStandaloneBreathCCSourcePath() {
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  if (settingsDirectory.empty()) return {};

  return patch_io::detail::JoinPath(settingsDirectory, "breath_cc_source.txt");
}

std::string GetStandaloneStatePath() {
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  if (settingsDirectory.empty()) return {};

  return patch_io::detail::JoinPath(settingsDirectory, "standalone_state.chunk");
}

std::string GetStandalonePitchBendRangePath() {
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  if (settingsDirectory.empty()) return {};

  return patch_io::detail::JoinPath(settingsDirectory, "pitch_bend_range.txt");
}

std::string GetStandaloneHarmonicVisualizerEnabledPath() {
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  if (settingsDirectory.empty()) return {};

  return patch_io::detail::JoinPath(settingsDirectory, "harmonic_visualizer_enabled.txt");
}

bool LoadStandaloneBreathCCSource(BreathCCSource& source) {
  const std::string path = GetStandaloneBreathCCSourcePath();
  if (path.empty()) return false;

  std::ifstream stream(path);
  if (!stream.is_open()) return false;

  int rawValue = static_cast<int>(kDefaultBreathCCSource);
  stream >> rawValue;
  if (!stream) return false;

  source = SanitizeBreathCCSource(rawValue);
  return true;
}

bool LoadStandalonePitchBendRange(int& pitchBendRange) {
  const std::string path = GetStandalonePitchBendRangePath();
  if (path.empty()) return false;

  std::ifstream stream(path);
  if (!stream.is_open()) return false;

  int rawValue = kDefaultPitchBendRange;
  stream >> rawValue;
  if (!stream) return false;

  pitchBendRange = SanitizePitchBendRange(rawValue);
  return true;
}

bool LoadStandaloneStateChunk(IByteChunk& chunk) {
  const std::string path = GetStandaloneStatePath();
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
  const std::string path = GetStandaloneStatePath();
  if (path.empty()) return false;

  const std::string parentPath = patch_io::detail::ParentPath(path);
  if (!parentPath.empty() && !patch_io::detail::EnsureDirectoryExists(parentPath)) return false;

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) return false;

  stream.write(reinterpret_cast<const char*>(chunk.GetData()), static_cast<std::streamsize>(chunk.Size()));
  return stream.good();
}

bool LoadStandaloneHarmonicVisualizerEnabled(bool& enabled) {
  const std::string path = GetStandaloneHarmonicVisualizerEnabledPath();
  if (path.empty()) return false;

  std::ifstream stream(path);
  if (!stream.is_open()) return false;

  int rawValue = enabled ? 1 : 0;
  stream >> rawValue;
  if (!stream) return false;

  enabled = (rawValue != 0);
  return true;
}

void DeleteStandaloneSettingsFile(const std::string& path) {
  if (path.empty()) return;

  if (!patch_io::detail::PathExists(path)) return;

  patch_io::detail::DeleteFile(path);
}

void DeleteStandaloneStateFile() { DeleteStandaloneSettingsFile(GetStandaloneStatePath()); }

void DeleteStandaloneLegacySettingsFiles() {
  DeleteStandaloneSettingsFile(GetStandaloneBreathCCSourcePath());
  DeleteStandaloneSettingsFile(GetStandalonePitchBendRangePath());
  DeleteStandaloneSettingsFile(GetStandaloneHarmonicVisualizerEnabledPath());
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

std::string TrimTrailingSeparators(std::string path) {
  const std::string_view trimmed = patch_io::detail::TrimTrailingPathSeparators(path);
  return std::string{trimmed};
}

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

std::string RelativePathFromDirectory(std::string_view directory, std::string_view path) {
  const std::string base = TrimTrailingSeparators(std::string{directory});
  const std::string full{path};
  if (base.empty() || full.size() <= base.size()) return full;

  if (full.compare(0, base.size(), base) != 0) return full;

  std::size_t offset = base.size();
  while (offset < full.size() && (full[offset] == '/' || full[offset] == '\\')) ++offset;
  return full.substr(offset);
}

bool IsAbsolutePath(std::string_view path) {
  return path.size() > 0 && (path[0] == '/' || path[0] == '\\' || (path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':'));
}

std::string DirectoryRelativeToRoot(std::string_view rootDirectory, std::string_view filePath) {
  const std::string relativePath = RelativePathFromDirectory(rootDirectory, filePath);
  return patch_io::detail::ParentPath(relativePath);
}

std::string UserPatchGroupKeyForPath(std::string_view userPatchDirectory, std::string_view patchPath) {
  std::vector<std::string> menuPath{"User"};
  const std::string relativeDirectory = DirectoryRelativeToRoot(userPatchDirectory, patchPath);
  const std::vector<std::string> relativeParts = SplitMenuPath(relativeDirectory);
  menuPath.insert(menuPath.end(), relativeParts.begin(), relativeParts.end());
  return MakeGroupKey(menuPath);
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

bool ShouldTrackPatchDirtyForParamSource(EParamSource source) {
  switch (source) {
  case kPresetRecall:
  case kDelegate:
  case kRecompile:    return false;
  default:            return true;
  }
}

std::string CanonicalizePatchSnapshotToml(const std::string& snapshot) {
  patch_io::PatchDocument document;
  if (snapshot.empty() || !patch_io::ParsePatchToml(snapshot, document)) return snapshot;

  return patch_io::SerializePatchToToml(document);
}
} // namespace

Addivox::Addivox(const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(kNumParams, kMaxFactoryPatches)), mEditorState(std::make_shared<plugin_ui::EditorState>()) {
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

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() { return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT)); };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachSVGBackground("background.svg");
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

#ifdef OS_WEB
    pGraphics->AttachPopupMenuControl();
#endif

    // pGraphics->EnableLiveEdit(true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Roboto-Bold", ROBOTO_BOLD_FN);
    pGraphics->LoadFont("Roboto-Black", ROBOTO_BLACK_FN);
    mEditorContext = plugin_ui::AttachMainControls(pGraphics, mEditorState, kCtrlTagHarmonicVisualizer, kCtrlTagEditorTabs, kCtrlTagKeyboard, kCtrlTagBender,
                                                   kCtrlTagBreathMeter, kCtrlTagMeter, mPitchBendRange);

    pGraphics->SetKeyHandlerFunc([this, pGraphics](const IKeyPress& key, bool isUp) {
      const auto sendQwertyMidi = [this, pGraphics](const IMidiMsg& msg) {
        SendMidiMsgFromUI(msg);
        plugin_ui::HandleQwertyMidi(pGraphics, kCtrlTagKeyboard, mLastQwertyMIDINote, msg);
      };

      const auto releaseHeldQwertyMidiNotes = [&]() {
        IMidiMsg msg;
        for (int pitch = 0; pitch < static_cast<int>(mQwertyMidiKeysDown.size()); ++pitch) {
          if (!mQwertyMidiKeysDown[static_cast<std::size_t>(pitch)]) continue;

          msg.MakeNoteOffMsg(pitch, 0);
          mQwertyMidiKeysDown[static_cast<std::size_t>(pitch)] = false;
          sendQwertyMidi(msg);
        }
      };

      const bool editMode = mEditorState && mEditorState->editMode;
      if (editMode && !mWasQwertyKeyboardInEditMode) releaseHeldQwertyMidiNotes();
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

        if (plugin_ui::editor::IsEditorActionShortcutKey(key.VK) || plugin_ui::editor::IsQwertyMidiKeyboardKey(key.VK)) return true;

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
            sendQwertyMidi(msg);
          }
        } else if (mQwertyMidiKeysDown[pitchIndex]) {
          msg.MakeNoteOffMsg(pitch, 0);
          mQwertyMidiKeysDown[pitchIndex] = false;
          sendQwertyMidi(msg);
        }

        return true;
      }

      if (key.VK == kVK_Z) {
        if (!isUp) {
          mQwertyMidiBaseNote = std::clamp(mQwertyMidiBaseNote - 12, 24, 96);
          releaseHeldQwertyMidiNotes();
        }
        return true;
      }

      if (key.VK == kVK_X) {
        if (!isUp) {
          mQwertyMidiBaseNote = std::clamp(mQwertyMidiBaseNote + 12, 24, 96);
          releaseHeldQwertyMidiNotes();
        }
        return true;
      }

      return true;
    });
  };
#endif

  LoadBuiltInPatches();
  RebuildPatchCatalog();
  RestoreFactoryPatch(0);
  if (mActivePatchDisplayName.empty() && NPresets() > 0) mActivePatchDisplayName = GetPresetName(GetCurrentPresetIdx());

  SetPitchBendRange(mPitchBendRange);
  SetBreathCCSource(kDefaultBreathCCSource);
}

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
  if (LoadStandalonePitchBendRange(persistedPitchBendRange)) {
    SetPitchBendRange(persistedPitchBendRange);
    migratedLegacyState = true;
  }

  BreathCCSource persistedSource = kDefaultBreathCCSource;
  if (LoadStandaloneBreathCCSource(persistedSource)) {
    SetBreathCCSource(persistedSource);
    migratedLegacyState = true;
  }

  bool persistedEnabled = kDefaultHarmonicVisualizerEnabled;
  if (LoadStandaloneHarmonicVisualizerEnabled(persistedEnabled)) {
    SetHarmonicVisualizerEnabled(persistedEnabled);
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

  SetPitchBendRange(kDefaultPitchBendRange);
  SetBreathCCSource(kDefaultBreathCCSource);
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
  mDSP.mSynth.SetPitchBendRange(mPitchBendRange);
#endif

  SyncPitchBendRangeUI();
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
  if (IsHighResolutionBreathCCSource(mBreathCCSource)) {
    const int rawValue = GetHighResolutionControllerValue(value);
    Plugin::SendMidiMsgFromUI(MakeControlChangeMsg(GetBreathCCSourceMSBController(mBreathCCSource), rawValue / 128, channel, offset));
    Plugin::SendMidiMsgFromUI(MakeControlChangeMsg(GetBreathCCSourceLSBController(mBreathCCSource), rawValue % 128, channel, offset));
    return;
  }

  Plugin::SendMidiMsgFromUI(MakeControlChangeMsg(GetBreathCCSourceMSBController(mBreathCCSource), GetSevenBitControllerValue(value), channel, offset));
}

void Addivox::ApplyPatchDocument(const patch_io::PatchDocument& document) {
  mSuppressPatchDirtyTracking = true;

  mEditorState->compoundPatch = document.compoundPatch;
  mEditorState->selectedMidiNote = patch_io::detail::ChooseDefaultSelectedMidiNote(document.compoundPatch, mEditorState->selectedMidiNote);

#if IPLUG_DSP
  mDSP.SetCompoundPatch(document.compoundPatch);
#endif

  SetGlobalVoiceSettingsParams(*this, document.voiceSettings, false, false);
  SetEffectsSettingsParams(*this, document.effectsSettings, false);
  OnParamReset(kPresetRecall);
  SyncPatchOwnedParamDefaultsToCurrentValues(*this);

  if (!document.name.empty()) mActivePatchDisplayName = document.name;
  else if (mActivePatchDisplayName.empty())
    mActivePatchDisplayName = "Patch";

  mSuppressPatchDirtyTracking = false;
  SetActivePatchCleanSnapshotFromCurrentState();
  RefreshEditorUI(true);
  MarkStandaloneStateDirty();
}

void Addivox::PromptLoadPatchFromFile() {
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if (!ui) return;

  WDL_String fileName;
  WDL_String directory;
  const std::string initialDirectory = mUserPatchDirectory.empty() ? EnsureDefaultUserPatchDirectory() : mUserPatchDirectory;
  if (!initialDirectory.empty()) directory.Set(initialDirectory.c_str());

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
  document.name = mActivePatchDisplayName.empty() ? "Patch" : mActivePatchDisplayName;
  document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
  document.effectsSettings = GetEffectsSettingsFromParams(*this);
  document.compoundPatch = mEditorState->compoundPatch;

  WDL_String fileName;
  WDL_String directory;
  const std::string defaultFileName = (mActivePatchSource == PatchSource::User && !mActivePatchPath.empty())
                                          ? std::string{patch_io::detail::FileNameView(mActivePatchPath)}
                                          : SanitizePatchFileName(document.name);
  const std::string initialDirectory = (mActivePatchSource == PatchSource::User && !mActivePatchPath.empty())
                                           ? patch_io::detail::ParentPath(mActivePatchPath)
                                           : (mUserPatchDirectory.empty() ? EnsureDefaultUserPatchDirectory() : mUserPatchDirectory);
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
  ui->PromptForFile(fileName, directory, EFileAction::Save, "toml", [this, exportPath](const WDL_String& selectedFileName, const WDL_String&) {
    patch_io::detail::DeleteFile(exportPath);

    const std::string destinationPath = GetFullFileDialogPath(selectedFileName);
    if (destinationPath.empty()) return;

    mUserPatchDirectory = patch_io::detail::ParentPath(destinationPath);
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
    mActivePatchSource = PatchSource::User;
    mActiveFactoryPatchIdx = -1;
    mActivePatchPath = fullPath;
    mActivePatchDisplayName = patch_io::detail::FileStem(fullPath);
    mActivePatchGroupKey = UserPatchGroupKeyForPath(EnsureDefaultUserPatchDirectory(), fullPath);
    SetActivePatchCleanSnapshotFromCurrentState();
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
    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  const int midiNote = msg.NoteNumber();
  if (midiNote < 0 || midiNote >= static_cast<int>(mActiveUIMIDINotes.size())) {
    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  const auto sendBreathFromUI = [this, &msg](double value) { SendBreathControlFromUI(value, msg.Channel(), msg.mOffset); };

  if (isNoteOn) {
    if (!mActiveUIMIDINotes[static_cast<size_t>(midiNote)]) {
      if (mNumActiveUIMIDINotes == 0) sendBreathFromUI(1.0);

      mActiveUIMIDINotes[static_cast<size_t>(midiNote)] = true;
      ++mNumActiveUIMIDINotes;
    }

    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  bool releasedActiveUINote = false;
  if (mActiveUIMIDINotes[static_cast<size_t>(midiNote)]) {
    mActiveUIMIDINotes[static_cast<size_t>(midiNote)] = false;
    --mNumActiveUIMIDINotes;
    releasedActiveUINote = true;
  }

  Plugin::SendMidiMsgFromUI(msg);

  if (releasedActiveUINote && mNumActiveUIMIDINotes == 0) sendBreathFromUI(0.0);
}

bool Addivox::SerializeState(IByteChunk& chunk) const {
  patch_io::PatchDocument document;
  document.name =
      mActivePatchDisplayName.empty() ? GetPresetName(mActiveFactoryPatchIdx >= 0 ? mActiveFactoryPatchIdx : GetCurrentPresetIdx()) : mActivePatchDisplayName;
  document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
  document.effectsSettings = GetEffectsSettingsFromParams(*this);
  document.compoundPatch = mEditorState->compoundPatch;

  std::string statePatchPath = mActivePatchPath;
  if (mActivePatchSource == PatchSource::User) {
    const std::string userPatchDirectory = GetDefaultUserPatchDirectory();
    if (!userPatchDirectory.empty()) statePatchPath = RelativePathFromDirectory(userPatchDirectory, mActivePatchPath);
  }

  return chunk.PutStr(patch_io::SerializePatchToToml(document).c_str()) > 0 && SerializeParams(chunk) &&
         AppendPluginStateSettingsChunk(chunk, mBreathCCSource, mHarmonicVisualizerEnabled.load(std::memory_order_relaxed), mPitchBendRange, mActivePatchDirty,
                                        mActivePatchCleanSnapshot, static_cast<int>(mActivePatchSource), mActiveFactoryPatchIdx, statePatchPath,
                                        mActivePatchGroupKey);
}

int Addivox::UnserializeState(const IByteChunk& chunk, int startPos) {
  WDL_String patchToml;
  int position = chunk.GetStr(patchToml, startPos);
  if (position < 0) return position;

  patch_io::PatchDocument document;
  if (!patch_io::ParsePatchToml(patchToml.Get(), document)) return -1;

  mSuppressPatchDirtyTracking = true;
  mEditorState->compoundPatch = document.compoundPatch;
  mEditorState->selectedMidiNote = patch_io::detail::ChooseDefaultSelectedMidiNote(document.compoundPatch, mEditorState->selectedMidiNote);
  mPendingRestoredStatePatchName = document.name;
  SetGlobalVoiceSettingsParams(*this, document.voiceSettings, false, false);
  SetEffectsSettingsParams(*this, document.effectsSettings, false);

#if IPLUG_DSP
  mDSP.SetCompoundPatch(document.compoundPatch);
#endif

  int pos = position;
  ENTER_PARAMS_MUTEX
  pos = RestoreStateParamsFromChunk(*this, chunk, pos);
  OnParamReset(kPresetRecall);
  SyncPatchOwnedParamDefaultsToCurrentValues(*this);
  LEAVE_PARAMS_MUTEX
  mSuppressPatchDirtyTracking = false;

  int restoredPitchBendRange = mPitchBendRange;
  BreathCCSource restoredBreathCCSource = kDefaultBreathCCSource;
  bool restoredHarmonicVisualizerEnabled = mHarmonicVisualizerEnabled.load(std::memory_order_relaxed);
  bool restoredPatchDirty = false;
  std::string restoredPatchCleanSnapshot;
  int restoredPatchSource = static_cast<int>(PatchSource::Unknown);
  int restoredFactoryPatchIdx = -1;
  std::string restoredPatchPath;
  std::string restoredPatchGroupKey;
  if (ReadPluginStateSettingsChunk(chunk, pos, restoredBreathCCSource, restoredHarmonicVisualizerEnabled, restoredPitchBendRange, restoredPatchDirty,
                                   restoredPatchCleanSnapshot, restoredPatchSource, restoredFactoryPatchIdx, restoredPatchPath, restoredPatchGroupKey)) {
    SetPitchBendRange(restoredPitchBendRange);
    SetBreathCCSource(restoredBreathCCSource);
    SetHarmonicVisualizerEnabled(restoredHarmonicVisualizerEnabled);
    mPendingRestoredPatchSource =
        static_cast<PatchSource>(std::clamp(restoredPatchSource, static_cast<int>(PatchSource::Unknown), static_cast<int>(PatchSource::User)));
    mPendingRestoredFactoryPatchIdx = restoredFactoryPatchIdx;
    mPendingRestoredPatchPath = std::move(restoredPatchPath);
    mPendingRestoredPatchGroupKey = std::move(restoredPatchGroupKey);
    mPendingRestoredPatchHasIdentity = mPendingRestoredPatchSource != PatchSource::Unknown;
  }
  (void)restoredPatchDirty;
  mPendingRestoredPatchCleanSnapshot = std::move(restoredPatchCleanSnapshot);
  mPendingRestoredPatchHasCleanSnapshot = !mPendingRestoredPatchCleanSnapshot.empty();

  return pos;
}

void Addivox::OnRestoreState() {
  mSuppressPatchDirtyTracking = true;
  Plugin::OnRestoreState();
  mSuppressPatchDirtyTracking = false;

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
      mActivePatchPath = (mActiveFactoryPatchIdx >= 0 && mActiveFactoryPatchIdx < static_cast<int>(mFactoryPatchPaths.size()))
                             ? mFactoryPatchPaths[static_cast<std::size_t>(mActiveFactoryPatchIdx)]
                             : std::string{};
      mActivePatchGroupKey = "Factory";
    } else if (mActivePatchSource == PatchSource::User) {
      const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
      if (!mActivePatchPath.empty() && !IsAbsolutePath(mActivePatchPath) && !userPatchDirectory.empty())
        mActivePatchPath = patch_io::detail::JoinPath(userPatchDirectory, mActivePatchPath);
      if (mActivePatchGroupKey.empty() && !mActivePatchPath.empty() && !userPatchDirectory.empty())
        mActivePatchGroupKey = UserPatchGroupKeyForPath(userPatchDirectory, mActivePatchPath);
    }
  } else if (mRestoringFactoryPatchIdx >= 0) {
    mActivePatchSource = PatchSource::Factory;
    mActiveFactoryPatchIdx = mRestoringFactoryPatchIdx;
    mActivePatchPath = (mRestoringFactoryPatchIdx < static_cast<int>(mFactoryPatchPaths.size()))
                           ? mFactoryPatchPaths[static_cast<std::size_t>(mRestoringFactoryPatchIdx)]
                           : std::string{};
    mActivePatchGroupKey = "Factory";
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
    mActivePatchDirty = SerializeCurrentPatchSnapshot() != mActivePatchCleanSnapshot;
  } else {
    SetActivePatchCleanSnapshotFromCurrentState();
  }
  mPendingRestoredPatchCleanSnapshot.clear();
  mPendingRestoredPatchHasCleanSnapshot = false;
  RebuildPatchCatalog();
  RefreshEditorUI(true);
  MarkStandaloneStateDirty();
}

void Addivox::OnUIOpen() {
  EnsureStandaloneStateInitialized();
  mSuppressPatchDirtyTracking = true;
  Plugin::OnUIOpen();
  mSuppressPatchDirtyTracking = false;
  RebuildPatchCatalog();
  RefreshEditorUI();
}

void Addivox::OnUIClose() {
  SaveStandaloneStateIfNeeded(true);
  mEditorContext.reset();
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
  mDSP.ProcessBlock(outputs, nFrames);
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

  SaveStandaloneStateIfNeeded();
}

void Addivox::OnReset() {
  EnsureStandaloneStateInitialized();
  mDSP.Reset(GetSampleRate(), GetBlockSize());
  mDSP.mSynth.SetPitchBendRange(mPitchBendRange);
  mDSP.SetBreathCCSource(mBreathCCSource);
  mBreathCCInputTracker.Reset();

  // Make the meter respond more quickly to changes in level.
  mMeterSender.Reset(GetSampleRate());
  mMeterSender.SetDecayTimeMs(50.0, GetSampleRate());
  mMeterSender.SetPeakHoldTimeMs(250.0, GetSampleRate());

  mHarmonicVisualizerSender.Reset(GetSampleRate(), PLUG_FPS);
  if (!mHarmonicVisualizerEnabled.load(std::memory_order_relaxed)) mHarmonicVisualizerBlankPending.store(true, std::memory_order_relaxed);
  else
    mHarmonicVisualizerBlankPending.store(false, std::memory_order_relaxed);
  mQwertyMidiKeysDown.fill(false);
  mQwertyMidiBaseNote = 48;
  mWasQwertyKeyboardInEditMode = false;
  mLastQwertyMIDINote = -1;
  mBreathLevel.store(1.0, std::memory_order_relaxed);
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
  if (msgTag == editor_messages::kMsgTagPromptLoadPatchFromFile) {
    PromptLoadPatchFromFile();
    return true;
  }

  if (msgTag == editor_messages::kMsgTagPromptSavePatchToFile) {
    PromptSavePatchToFile();
    return true;
  }

  if (msgTag == editor_messages::kMsgTagPatchManagerAction && dataSize == sizeof(editor_messages::PatchManagerActionPayload) && pData) {
    const auto* payload = static_cast<const editor_messages::PatchManagerActionPayload*>(pData);
    HandlePatchManagerAction(payload->action, payload->patchId);
    return true;
  }

  if (msgTag == editor_messages::kMsgTagSetBreathCCSource && dataSize == sizeof(editor_messages::SetBreathCCSourcePayload) && pData) {
    const auto* payload = static_cast<const editor_messages::SetBreathCCSourcePayload*>(pData);
    SetBreathCCSource(SanitizeBreathCCSource(payload->source));
    return true;
  }

  if (msgTag == editor_messages::kMsgTagSetPitchBendRange && dataSize == sizeof(editor_messages::SetPitchBendRangePayload) && pData) {
    const auto* payload = static_cast<const editor_messages::SetPitchBendRangePayload*>(pData);
    SetPitchBendRange(payload->semitones);
    return true;
  }

  if (msgTag == editor_messages::kMsgTagSetHarmonicVisualizerEnabled && dataSize == sizeof(editor_messages::SetHarmonicVisualizerEnabledPayload) && pData) {
    const auto* payload = static_cast<const editor_messages::SetHarmonicVisualizerEnabledPayload*>(pData);
    SetHarmonicVisualizerEnabled(payload->enabled != 0);
    return true;
  }

  if (msgTag == editor_messages::kMsgTagResetStandaloneStateToDefaults) {
    ResetStandaloneStateToDefaults();
    return true;
  }

  if (msgTag == editor_messages::kMsgTagOpenAudioMidiSettings) {
#if defined APP_API
    return addivox_standalone::OpenAudioMidiSettingsDialog();
#else
    return false;
#endif
  }

  if (ctrlTag == kCtrlTagEditorTabs && msgTag == editor_messages::kMsgTagSetKeyNoteOscillatorParameter &&
      dataSize == sizeof(editor_messages::SetKeyNoteOscillatorParameterPayload) && pData) {
    const auto* payload = static_cast<const editor_messages::SetKeyNoteOscillatorParameterPayload*>(pData);
    if (payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters) return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    const bool updated = mDSP.mSynth.GetVoice().SetKeyNoteOscillatorParameter(payload->midiNote, payload->oscillatorIndex, parameter, payload->value);
    if (updated) {
      MarkActivePatchDirty();
      MarkStandaloneStateDirty();
    }
    return updated;
  }

  if (ctrlTag == kCtrlTagEditorTabs && msgTag == editor_messages::kMsgTagSetKeyNoteOscillatorParameterValues &&
      dataSize == sizeof(editor_messages::SetKeyNoteOscillatorParameterValuesPayload) && pData) {
    const auto* payload = static_cast<const editor_messages::SetKeyNoteOscillatorParameterValuesPayload*>(pData);
    if (payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters) return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    const bool updated = mDSP.mSynth.GetVoice().SetKeyNoteOscillatorParameterValues(payload->midiNote, parameter, payload->values);
    if (updated) {
      MarkActivePatchDirty();
      MarkStandaloneStateDirty();
    }
    return updated;
  }

  if (ctrlTag == kCtrlTagEditorTabs && msgTag == editor_messages::kMsgTagSetKeyNoteEqCurve && dataSize > 0 && pData) {
    int midiNote = 0;
    EqCurve curve;
    if (!editor_messages::DeserializeKeyNoteEqCurvePayload(dataSize, pData, midiNote, curve)) return false;

    const bool updated = mDSP.mSynth.GetVoice().SetKeyNoteEqCurve(midiNote, curve);
    if (updated) {
      MarkActivePatchDirty();
      MarkStandaloneStateDirty();
    }
    return updated;
  }

  if (ctrlTag == kCtrlTagEditorTabs && msgTag == editor_messages::kMsgTagSetAllKeyNotesEnabled &&
      dataSize == sizeof(editor_messages::SetAllKeyNotesEnabledPayload) && pData) {
    const auto* payload = static_cast<const editor_messages::SetAllKeyNotesEnabledPayload*>(pData);
    if (payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters) return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    const bool updated = mDSP.mSynth.GetVoice().SetAllKeyNotesEnabled(parameter, payload->enabled != 0, payload->midiNote);
    if (updated) {
      MarkActivePatchDirty();
      MarkStandaloneStateDirty();
    }
    return updated;
  }

  if (ctrlTag == kCtrlTagEditorTabs && msgTag == editor_messages::kMsgTagSetAllKeyNotesEqEnabled &&
      dataSize == sizeof(editor_messages::SetAllKeyNotesEqEnabledPayload) && pData) {
    const auto* payload = static_cast<const editor_messages::SetAllKeyNotesEqEnabledPayload*>(pData);
    const bool updated = mDSP.mSynth.GetVoice().SetAllKeyNotesEqEnabled(payload->enabled != 0);
    if (updated) {
      MarkActivePatchDirty();
      MarkStandaloneStateDirty();
    }
    return updated;
  }

  if (ctrlTag == kCtrlTagEditorTabs && msgTag == editor_messages::kMsgTagAddKeyNotePatch && dataSize == sizeof(editor_messages::KeyNotePatchPayload) && pData) {
    const auto* payload = static_cast<const editor_messages::KeyNotePatchPayload*>(pData);
    const bool updated = mDSP.mSynth.GetVoice().AddKeyNotePatch(payload->midiNote);
    if (updated) {
      MarkActivePatchDirty();
      MarkStandaloneStateDirty();
    }
    return updated;
  }

  if (ctrlTag == kCtrlTagEditorTabs && msgTag == editor_messages::kMsgTagRemoveKeyNotePatch && dataSize == sizeof(editor_messages::KeyNotePatchPayload) &&
      pData) {
    const auto* payload = static_cast<const editor_messages::KeyNotePatchPayload*>(pData);
    const bool updated = mDSP.mSynth.GetVoice().RemoveKeyNotePatch(payload->midiNote);
    if (updated) {
      MarkActivePatchDirty();
      MarkStandaloneStateDirty();
    }
    return updated;
  }

  if (ctrlTag == kCtrlTagBender && msgTag == plugin_ui::layout::PitchBendWheelControl::kMessageTagSetPitchBendRange) {
    const int bendRange = *static_cast<const int*>(pData);
    SetPitchBendRange(bendRange);
    return true;
  }

  return false;
}
#endif

void Addivox::RebuildPatchCatalog() {
  mPatchCatalog.clear();
  int nextId = 0;

  for (int patchIdx = 0; patchIdx < NPresets(); ++patchIdx) {
    PatchCatalogEntry entry;
    entry.id = nextId++;
    entry.source = PatchSource::Factory;
    entry.factoryIndex = patchIdx;
    entry.name = GetPresetName(patchIdx);
    if (entry.name.empty()) entry.name = "Patch " + std::to_string(patchIdx + 1);
    if (patchIdx < static_cast<int>(mFactoryPatchPaths.size())) entry.path = mFactoryPatchPaths[static_cast<std::size_t>(patchIdx)];
    entry.menuPath = {"Factory"};
    entry.groupKey = "Factory";
    mPatchCatalog.push_back(std::move(entry));
  }

  const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
  const std::vector<std::string> userPatchPaths = patch_io::FindPatchFiles(userPatchDirectory);
  for (const auto& patchPath : userPatchPaths) {
    patch_io::PatchDocument document;
    const bool loaded = patch_io::LoadPatchFromFile(patchPath, document);

    PatchCatalogEntry entry;
    entry.id = nextId++;
    entry.source = PatchSource::User;
    entry.path = patchPath;
    entry.name = loaded && !document.name.empty() ? document.name : patch_io::detail::FileStem(patchPath);

    entry.menuPath = {"User"};
    const std::string relativeDirectory = DirectoryRelativeToRoot(userPatchDirectory, patchPath);
    const std::vector<std::string> relativeParts = SplitMenuPath(relativeDirectory);
    entry.menuPath.insert(entry.menuPath.end(), relativeParts.begin(), relativeParts.end());
    entry.groupKey = MakeGroupKey(entry.menuPath);
    mPatchCatalog.push_back(std::move(entry));
  }

  ReconcileActivePatchIdentity();
}

void Addivox::ReconcileActivePatchIdentity() {
  if (mActivePatchSource == PatchSource::Factory) {
    if (mActiveFactoryPatchIdx >= 0 && mActiveFactoryPatchIdx < NPresets()) {
      mActivePatchPath = (mActiveFactoryPatchIdx < static_cast<int>(mFactoryPatchPaths.size()))
                             ? mFactoryPatchPaths[static_cast<std::size_t>(mActiveFactoryPatchIdx)]
                             : std::string{};
      mActivePatchGroupKey = "Factory";
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

  mActivePatchSource = PatchSource::Factory;
  mActiveFactoryPatchIdx = patchIdx;
  mActivePatchPath = (patchIdx < static_cast<int>(mFactoryPatchPaths.size())) ? mFactoryPatchPaths[static_cast<std::size_t>(patchIdx)] : std::string{};
  mActivePatchGroupKey = "Factory";
  SetActivePatchCleanSnapshotFromCurrentState();
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
  ApplyPatchDocument(document);
  mActivePatchSource = PatchSource::User;
  mActiveFactoryPatchIdx = -1;
  mActivePatchPath = path;

  mActivePatchGroupKey = UserPatchGroupKeyForPath(EnsureDefaultUserPatchDirectory(), path);
  SetActivePatchCleanSnapshotFromCurrentState();
  RebuildPatchCatalog();
  RefreshEditorUI(true);
}

void Addivox::PromptImportPatchFromFile() {
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if (!ui) return;

  WDL_String fileName;
  WDL_String directory;
  const std::string initialDirectory = mUserPatchDirectory.empty() ? EnsureDefaultUserPatchDirectory() : mUserPatchDirectory;
  if (!initialDirectory.empty()) directory.Set(initialDirectory.c_str());

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
  const auto entryIt = std::find_if(mPatchCatalog.begin(), mPatchCatalog.end(), [patchId](const PatchCatalogEntry& entry) { return entry.id == patchId; });
  if (entryIt == mPatchCatalog.end()) return;

  if (entryIt->source == PatchSource::Factory) RestoreFactoryPatch(entryIt->factoryIndex);
  else if (entryIt->source == PatchSource::User)
    LoadUserPatchByPath(entryIt->path);
}

void Addivox::CyclePatchInCurrentGroup(int direction) {
  if (mPatchCatalog.empty()) RebuildPatchCatalog();

  std::vector<const PatchCatalogEntry*> groupEntries;
  for (const auto& entry : mPatchCatalog) {
    if (entry.groupKey == mActivePatchGroupKey) groupEntries.push_back(&entry);
  }

  if (groupEntries.empty()) {
    for (const auto& entry : mPatchCatalog) {
      if (entry.groupKey == "Factory") groupEntries.push_back(&entry);
    }
  }

  if (groupEntries.empty()) return;

  int currentIndex = -1;
  for (int i = 0; i < static_cast<int>(groupEntries.size()); ++i) {
    const PatchCatalogEntry* entry = groupEntries[static_cast<std::size_t>(i)];
    const bool matches =
        (entry->source == PatchSource::Factory && mActivePatchSource == PatchSource::Factory && entry->factoryIndex == mActiveFactoryPatchIdx) ||
        (entry->source == PatchSource::User && mActivePatchSource == PatchSource::User && entry->path == mActivePatchPath);
    if (matches) {
      currentIndex = i;
      break;
    }
  }

  if (currentIndex < 0) currentIndex = direction >= 0 ? -1 : 0;

  int nextIndex = currentIndex + (direction >= 0 ? 1 : -1);
  if (nextIndex < 0) nextIndex = static_cast<int>(groupEntries.size()) - 1;
  if (nextIndex >= static_cast<int>(groupEntries.size())) nextIndex = 0;

  LoadPatchById(groupEntries[static_cast<std::size_t>(nextIndex)]->id);
}

void Addivox::PromptImportPatchCollection() {
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if (!ui) return;

  WDL_String directory;
  const std::string initialDirectory = mUserPatchDirectory.empty() ? EnsureDefaultUserPatchDirectory() : mUserPatchDirectory;
  if (!initialDirectory.empty()) directory.Set(initialDirectory.c_str());

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

  WDL_String path;
  if (mActivePatchSource == PatchSource::User && !mActivePatchPath.empty() && patch_io::detail::PathExists(mActivePatchPath)) {
    path.Set(mActivePatchPath.c_str());
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
  const auto patchAction = static_cast<editor_messages::PatchManagerAction>(action);
  switch (patchAction) {
  case editor_messages::PatchManagerAction::SelectPatch:            LoadPatchById(patchId); break;
  case editor_messages::PatchManagerAction::PreviousPatch:          CyclePatchInCurrentGroup(-1); break;
  case editor_messages::PatchManagerAction::NextPatch:              CyclePatchInCurrentGroup(1); break;
  case editor_messages::PatchManagerAction::SavePatch:              PromptSavePatchToFile(); break;
  case editor_messages::PatchManagerAction::ImportPatch:            PromptImportPatchFromFile(); break;
  case editor_messages::PatchManagerAction::ImportCollection:       PromptImportPatchCollection(); break;
  case editor_messages::PatchManagerAction::ShowPatchInFileBrowser: ShowActivePatchInFileBrowser(); break;
  case editor_messages::PatchManagerAction::RefreshPatches:
    RebuildPatchCatalog();
    RefreshEditorUI();
    break;
  }
}

std::string Addivox::SerializeCurrentPatchSnapshot() const {
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
  mActivePatchCleanSnapshot = SerializeCurrentPatchSnapshot();
  mActivePatchDirty = false;
}

void Addivox::MarkActivePatchDirty() {
  if (mSuppressPatchDirtyTracking || mActivePatchDirty) return;

  if (!mActivePatchCleanSnapshot.empty() && SerializeCurrentPatchSnapshot() == mActivePatchCleanSnapshot) return;

  mActivePatchDirty = true;
  RefreshEditorUI();
}

void Addivox::ClearActivePatchDirty() { SetActivePatchCleanSnapshotFromCurrentState(); }

void Addivox::LoadBuiltInPatches() {
  mFactoryPatchPaths.clear();

  WDL_String resourcePath;
  BundleResourcePath(resourcePath, GetBundleID());

  int numLoadedPatches = 0;
  if (resourcePath.GetLength() > 0) {
    const auto patchPaths = patch_io::FindPatchFiles(patch_io::detail::JoinPath(resourcePath.Get(), "factory_patches"));
    for (const auto& patchPath : patchPaths) {
      patch_io::PatchDocument document;
      if (!patch_io::LoadPatchFromFile(patchPath, document)) continue;

      IByteChunk chunk;
      if (!BuildPatchChunk(document, chunk)) continue;

      const std::string patchName = document.name.empty() ? patch_io::detail::FileStem(patchPath) : document.name;
      MakePresetFromChunk(patchName.c_str(), chunk);
      mFactoryPatchPaths.push_back(patchPath);
      ++numLoadedPatches;

      if (numLoadedPatches >= kMaxFactoryPatches) break;
    }
  }

  if (numLoadedPatches == 0) {
    patch_io::PatchDocument fallbackDocument;
    fallbackDocument.name = "Init";
    fallbackDocument.voiceSettings = GlobalVoiceSettings{};
    fallbackDocument.effectsSettings = EffectsSettings{};
    fallbackDocument.compoundPatch = CompoundPatch{};

    IByteChunk chunk;
    if (BuildPatchChunk(fallbackDocument, chunk)) {
      MakePresetFromChunk(fallbackDocument.name.c_str(), chunk);
      mFactoryPatchPaths.push_back({});
    }
  }

  PruneUninitializedPresets();
}

void Addivox::RefreshEditorUI(bool resetOscillatorRestoreStates) {
#if IPLUG_EDITOR
  if (!GetUI() || !mEditorContext) return;

  if (mEditorContext->title.patchManagerControl && *mEditorContext->title.patchManagerControl) {
    if (auto* patchManager = dynamic_cast<plugin_ui::layout::PatchManagerControl*>(*mEditorContext->title.patchManagerControl)) {
      std::string label = mActivePatchDisplayName.empty()
                              ? (NPresets() > 0 ? std::string{GetPresetName(mActiveFactoryPatchIdx >= 0 ? mActiveFactoryPatchIdx : GetCurrentPresetIdx())}
                                                : std::string{"Choose Patch..."})
                              : mActivePatchDisplayName;
      if (mActivePatchDirty) label += "*";

      plugin_ui::layout::PatchMenuModel model;
      model.label = label;
      const std::string userPatchDirectory = EnsureDefaultUserPatchDirectory();
      model.canShowInFileBrowser = (mActivePatchSource == PatchSource::User && !mActivePatchPath.empty() && patch_io::detail::PathExists(mActivePatchPath)) ||
                                   (!userPatchDirectory.empty() && patch_io::detail::IsDirectory(userPatchDirectory));

      for (const auto& entry : mPatchCatalog) {
        plugin_ui::layout::PatchMenuEntry menuEntry;
        menuEntry.id = entry.id;
        menuEntry.name = entry.name;
        menuEntry.groupPath = entry.menuPath;
        menuEntry.checked =
            (entry.source == PatchSource::Factory && mActivePatchSource == PatchSource::Factory && entry.factoryIndex == mActiveFactoryPatchIdx) ||
            (entry.source == PatchSource::User && mActivePatchSource == PatchSource::User && entry.path == mActivePatchPath);
        model.entries.push_back(std::move(menuEntry));
      }

      patchManager->SetPatchMenuModel(std::move(model));
    }
  }

  if (auto* keyboard = plugin_ui::layout::GetKeyboardControl(GetUI(), kCtrlTagKeyboard)) {
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

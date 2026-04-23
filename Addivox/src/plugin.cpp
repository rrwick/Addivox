#include "plugin.h"
#include "IPlug_include_in_plug_src.h"
#include "IPlugPaths.h"
#include "settings/effects.h"
#include "settings/params.h"
#include "settings/preset_io.h"
#include "ui/editor_messages.h"
#include "ui/transformations.h"
#include "ui/layout.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>

namespace
{
constexpr uint32_t kPluginStateSettingsMagic = 0x42524343u; // Legacy plugin-state settings block magic.
constexpr int kDefaultPitchBendRange = 2;
constexpr int kMaxPitchBendRange = 96;
constexpr bool kDefaultHarmonicVisualizerEnabled = true;
constexpr int64_t kStandaloneStateSaveDebounceMs = 250;

int SanitizePitchBendRange(int pitchBendRange)
{
  return std::clamp(pitchBendRange, 0, kMaxPitchBendRange);
}

int64_t GetMonotonicMilliseconds()
{
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

GlobalVoiceSettings GetGlobalVoiceSettingsFromParams(const Addivox& plugin)
{
  GlobalVoiceSettings settings{};
  for(int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
    global_settings::ApplyParam(paramIdx, plugin.GetParam(paramIdx)->Value(), settings);

  return global_settings::Sanitize(settings);
}

EffectsSettings GetEffectsSettingsFromParams(const Addivox& plugin)
{
  EffectsSettings settings{};
  for(int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
    effects_settings::ApplyParam(paramIdx, plugin.GetParam(paramIdx)->Value(), settings);

  return effects_settings::Sanitize(settings);
}

void SetGlobalVoiceSettingsParams(Addivox& plugin,
                                  const GlobalVoiceSettings& voiceSettings,
                                  bool includeTuning = true,
                                  bool includePanShift = true)
{
  const GlobalVoiceSettings sanitizedVoiceSettings = global_settings::Sanitize(voiceSettings);
  plugin.GetParam(kParamGlobalLevel)->Set(sanitizedVoiceSettings.levelScale);
  plugin.GetParam(kParamGlobalAttackScale)->Set(sanitizedVoiceSettings.attackScale);
  plugin.GetParam(kParamGlobalReleaseScale)->Set(sanitizedVoiceSettings.releaseScale);
  if(includeTuning)
    plugin.GetParam(kParamGlobalTuning)->Set(sanitizedVoiceSettings.tuningCents);
  if(includePanShift)
    plugin.GetParam(kParamGlobalPanShift)->Set(sanitizedVoiceSettings.panOffset);
  plugin.GetParam(kParamGlobalIntensityVariationAmplitudeScale)->Set(sanitizedVoiceSettings.intensityVariationAmplitudeScale);
  plugin.GetParam(kParamGlobalIntensityVariationRateScale)->Set(sanitizedVoiceSettings.intensityVariationRateScale);
  plugin.GetParam(kParamGlobalPitchVariationAmplitudeScale)->Set(sanitizedVoiceSettings.pitchVariationAmplitudeScale);
  plugin.GetParam(kParamGlobalPitchVariationRateScale)->Set(sanitizedVoiceSettings.pitchVariationRateScale);
  plugin.GetParam(kParamGlobalPanVariationAmplitudeScale)->Set(sanitizedVoiceSettings.panVariationAmplitudeScale);
  plugin.GetParam(kParamGlobalPanVariationRateScale)->Set(sanitizedVoiceSettings.panVariationRateScale);
  plugin.GetParam(kParamPortamentoAtCC5Min)->Set(sanitizedVoiceSettings.portamentoTimeAtCC5MinSec);
  plugin.GetParam(kParamPortamentoAtCC5Max)->Set(sanitizedVoiceSettings.portamentoTimeAtCC5MaxSec);
}

void SetEffectsSettingsParams(Addivox& plugin,
                              const EffectsSettings& effectsSettings,
                              bool includeReverb = true)
{
  const EffectsSettings sanitizedEffectsSettings = effects_settings::Sanitize(effectsSettings);
  plugin.GetParam(kParamEffectsDrive)->Set(sanitizedEffectsSettings.drive);
  plugin.GetParam(kParamEffectsTone)->Set(sanitizedEffectsSettings.tone);
  plugin.GetParam(kParamEffectsChorus)->Set(sanitizedEffectsSettings.chorus);
  if(includeReverb)
    plugin.GetParam(kParamEffectsReverb)->Set(sanitizedEffectsSettings.reverb);
}

bool IsPresetOwnedParam(int paramIdx)
{
  // These controls intentionally persist across preset changes, so reset should
  // keep their constructor defaults instead of following the last recalled preset.
  switch(paramIdx)
  {
    case kParamGlobalTuning:
    case kParamGlobalPanShift:
    case kParamEffectsReverb:
    case kParamTranspose:
      return false;
    default:
      return paramIdx >= 0 && paramIdx < kNumParams;
  }
}

void SyncPresetOwnedParamDefaultsToCurrentValues(Addivox& plugin)
{
  for(int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
  {
    if(!IsPresetOwnedParam(paramIdx))
      continue;

    IParam* const param = plugin.GetParam(paramIdx);
    param->SetDefault(param->Value());
  }
}

bool BuildPresetChunk(const preset_io::PresetDocument& document, IByteChunk& chunk)
{
  const std::string toml = preset_io::SerializePresetToToml(document);
  const GlobalVoiceSettings voiceSettings = global_settings::Sanitize(document.voiceSettings);
  const EffectsSettings effectsSettings = effects_settings::Sanitize(document.effectsSettings);
  return chunk.PutStr(toml.c_str()) > 0
    && chunk.Put(&voiceSettings.levelScale) > 0
    && chunk.Put(&voiceSettings.attackScale) > 0
    && chunk.Put(&voiceSettings.releaseScale) > 0
    && chunk.Put(&voiceSettings.tuningCents) > 0
    && chunk.Put(&voiceSettings.panOffset) > 0
    && chunk.Put(&voiceSettings.intensityVariationAmplitudeScale) > 0
    && chunk.Put(&voiceSettings.intensityVariationRateScale) > 0
    && chunk.Put(&voiceSettings.pitchVariationAmplitudeScale) > 0
    && chunk.Put(&voiceSettings.pitchVariationRateScale) > 0
    && chunk.Put(&voiceSettings.panVariationAmplitudeScale) > 0
    && chunk.Put(&voiceSettings.panVariationRateScale) > 0
    && chunk.Put(&voiceSettings.portamentoTimeAtCC5MinSec) > 0
    && chunk.Put(&voiceSettings.portamentoTimeAtCC5MaxSec) > 0
    && chunk.Put(&effectsSettings.reverb) > 0
    && chunk.Put(&effectsSettings.drive) > 0
    && chunk.Put(&effectsSettings.chorus) > 0
    && chunk.Put(&effectsSettings.tone) > 0;
}

bool AppendPluginStateSettingsChunk(IByteChunk& chunk,
                                    BreathCCSource breathCCSource,
                                    bool harmonicVisualizerEnabled,
                                    int pitchBendRange)
{
  const uint32_t magic = kPluginStateSettingsMagic;
  const int32_t rawBreathCCSource = static_cast<int32_t>(breathCCSource);
  const int32_t rawHarmonicVisualizerEnabled = harmonicVisualizerEnabled ? 1 : 0;
  const int32_t rawPitchBendRange = static_cast<int32_t>(SanitizePitchBendRange(pitchBendRange));
  return chunk.Put(&magic) > 0
    && chunk.Put(&rawBreathCCSource) > 0
    && chunk.Put(&rawHarmonicVisualizerEnabled) > 0
    && chunk.Put(&rawPitchBendRange) > 0;
}

bool ReadPluginStateSettingsChunk(const IByteChunk& chunk,
                                  int startPos,
                                  BreathCCSource& breathCCSource,
                                  bool& harmonicVisualizerEnabled,
                                  int& pitchBendRange)
{
  uint32_t magic = 0;
  int position = chunk.Get(&magic, startPos);
  if(position < 0 || magic != kPluginStateSettingsMagic)
    return false;

  int32_t rawBreathCCSource = static_cast<int32_t>(kDefaultBreathCCSource);
  position = chunk.Get(&rawBreathCCSource, position);
  if(position < 0)
    return false;

  breathCCSource = SanitizeBreathCCSource(rawBreathCCSource);

  int32_t rawHarmonicVisualizerEnabled = harmonicVisualizerEnabled ? 1 : 0;
  position = chunk.Get(&rawHarmonicVisualizerEnabled, position);
  if(position >= 0)
    harmonicVisualizerEnabled = (rawHarmonicVisualizerEnabled != 0);

  int32_t rawPitchBendRange = static_cast<int32_t>(pitchBendRange);
  if(position >= 0)
  {
    const int nextPosition = chunk.Get(&rawPitchBendRange, position);
    if(nextPosition >= 0)
      pitchBendRange = SanitizePitchBendRange(rawPitchBendRange);
  }

  return true;
}

int GetSevenBitControllerValue(double value)
{
  return std::clamp(static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 127.0)), 0, 127);
}

int GetHighResolutionControllerValue(double value)
{
  return std::clamp(static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 16383.0)), 0, 16383);
}

bool SetParamFromChunkValue(Addivox& plugin,
                            const IByteChunk& chunk,
                            int paramIdx,
                            int& position,
                            bool applyValue = true)
{
  double value = 0.0;
  const int nextPos = chunk.Get(&value, position);
  if(nextPos < 0)
    return false;

  if(applyValue)
  {
    if(paramIdx == kParamEffectsTone && std::abs(value) > 1.0)
      value *= 0.01;

    plugin.GetParam(paramIdx)->Set(value);
  }
  position = nextPos;
  return true;
}

int RestoreStateParamsFromChunk(Addivox& plugin,
                                const IByteChunk& chunk,
                                int startPos)
{
  const int remainingBytes = chunk.Size() - startPos;
  if(remainingBytes <= 0)
    return startPos;

  const int bytesPerValue = static_cast<int>(sizeof(double));
  const bool hasWholeDoublePayload = (remainingBytes % bytesPerValue) == 0;
  const int remainingValues = hasWholeDoublePayload ? (remainingBytes / bytesPerValue) : -1;
  int position = startPos;

  if(remainingValues == kNumParams)
  {
    for(int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
    {
      if(!SetParamFromChunkValue(plugin, chunk, paramIdx, position))
        break;
    }
    return position;
  }

  constexpr int kPresetChunkParamCountWithReverb = kParamEffectsTone + 1;
  constexpr int kPresetChunkParamCountWithoutReverb = kPresetChunkParamCountWithReverb - 1;
  if(remainingValues == kPresetChunkParamCountWithReverb
     || remainingValues == kPresetChunkParamCountWithoutReverb)
  {
    for(int paramIdx = 0; paramIdx <= kParamPortamentoAtCC5Max; ++paramIdx)
    {
      const bool applyValue = (paramIdx != kParamGlobalTuning) && (paramIdx != kParamGlobalPanShift);
      if(!SetParamFromChunkValue(plugin, chunk, paramIdx, position, applyValue))
        return position;
    }

    if(remainingValues == kPresetChunkParamCountWithReverb)
    {
      if(!SetParamFromChunkValue(plugin, chunk, kParamEffectsReverb, position, false))
        return position;
    }

    for(int paramIdx = kParamEffectsDrive; paramIdx <= kParamEffectsTone; ++paramIdx)
    {
      if(!SetParamFromChunkValue(plugin, chunk, paramIdx, position))
        break;
    }

    return position;
  }

  for(int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
  {
    if(!SetParamFromChunkValue(plugin, chunk, paramIdx, position))
      break;
  }

  return position;
}

std::string GetFullFileDialogPath(const WDL_String& fileName)
{
  return fileName.GetLength() > 0 ? std::string{fileName.Get()} : std::string{};
}

std::string EnsureTomlExtension(std::string path)
{
  if(preset_io::detail::HasExtension(path, ".toml"))
    return path;

  path += ".toml";
  return path;
}

std::string SanitizePresetFileName(std::string_view presetName)
{
  std::string sanitized;
  sanitized.reserve(presetName.size());

  for(const char c : presetName)
  {
    const unsigned char uc = static_cast<unsigned char>(c);
    if(std::isalnum(uc) || c == ' ' || c == '-' || c == '_')
      sanitized.push_back(c);
    else
      sanitized.push_back('_');
  }

  while(!sanitized.empty() && (sanitized.back() == ' ' || sanitized.back() == '.'))
    sanitized.pop_back();

  if(sanitized.empty())
    sanitized = "Preset";

  return EnsureTomlExtension(sanitized);
}

std::string GetDefaultUserPresetDirectory()
{
  WDL_String appSupportPath;
  AppSupportPath(appSupportPath, false);
  if(appSupportPath.GetLength() == 0)
    return {};

  return preset_io::detail::JoinPath(
    preset_io::detail::JoinPath(appSupportPath.Get(), BUNDLE_NAME),
    "Presets");
}

std::string GetStandaloneSettingsDirectory()
{
  WDL_String appSupportPath;
  AppSupportPath(appSupportPath, false);
  if(appSupportPath.GetLength() == 0)
    return {};

  return preset_io::detail::JoinPath(appSupportPath.Get(), BUNDLE_NAME);
}

std::string GetStandaloneBreathCCSourcePath()
{
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  if(settingsDirectory.empty())
    return {};

  return preset_io::detail::JoinPath(settingsDirectory, "breath_cc_source.txt");
}

std::string GetStandaloneStatePath()
{
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  if(settingsDirectory.empty())
    return {};

  return preset_io::detail::JoinPath(settingsDirectory, "standalone_state.chunk");
}

std::string GetStandalonePitchBendRangePath()
{
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  if(settingsDirectory.empty())
    return {};

  return preset_io::detail::JoinPath(settingsDirectory, "pitch_bend_range.txt");
}

std::string GetStandaloneHarmonicVisualizerEnabledPath()
{
  const std::string settingsDirectory = GetStandaloneSettingsDirectory();
  if(settingsDirectory.empty())
    return {};

  return preset_io::detail::JoinPath(settingsDirectory, "harmonic_visualizer_enabled.txt");
}

bool LoadStandaloneBreathCCSource(BreathCCSource& source)
{
  const std::string path = GetStandaloneBreathCCSourcePath();
  if(path.empty())
    return false;

  std::ifstream stream(path);
  if(!stream.is_open())
    return false;

  int rawValue = static_cast<int>(kDefaultBreathCCSource);
  stream >> rawValue;
  if(!stream)
    return false;

  source = SanitizeBreathCCSource(rawValue);
  return true;
}

bool LoadStandalonePitchBendRange(int& pitchBendRange)
{
  const std::string path = GetStandalonePitchBendRangePath();
  if(path.empty())
    return false;

  std::ifstream stream(path);
  if(!stream.is_open())
    return false;

  int rawValue = kDefaultPitchBendRange;
  stream >> rawValue;
  if(!stream)
    return false;

  pitchBendRange = SanitizePitchBendRange(rawValue);
  return true;
}

bool LoadStandaloneStateChunk(IByteChunk& chunk)
{
  const std::string path = GetStandaloneStatePath();
  if(path.empty())
    return false;

  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if(!stream.is_open())
    return false;

  const std::streamsize size = stream.tellg();
  if(size <= 0)
    return false;

  chunk.Clear();
  chunk.Resize(static_cast<int>(size));
  stream.seekg(0, std::ios::beg);
  if(!stream.read(reinterpret_cast<char*>(chunk.GetData()), size))
  {
    chunk.Clear();
    return false;
  }

  return true;
}

bool SaveStandaloneStateChunk(const IByteChunk& chunk)
{
  const std::string path = GetStandaloneStatePath();
  if(path.empty())
    return false;

  const std::string parentPath = preset_io::detail::ParentPath(path);
  if(!parentPath.empty() && !preset_io::detail::EnsureDirectoryExists(parentPath))
    return false;

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if(!stream.is_open())
    return false;

  stream.write(reinterpret_cast<const char*>(chunk.GetData()), static_cast<std::streamsize>(chunk.Size()));
  return stream.good();
}

bool LoadStandaloneHarmonicVisualizerEnabled(bool& enabled)
{
  const std::string path = GetStandaloneHarmonicVisualizerEnabledPath();
  if(path.empty())
    return false;

  std::ifstream stream(path);
  if(!stream.is_open())
    return false;

  int rawValue = enabled ? 1 : 0;
  stream >> rawValue;
  if(!stream)
    return false;

  enabled = (rawValue != 0);
  return true;
}

void DeleteStandaloneSettingsFile(const std::string& path)
{
  if(path.empty())
    return;

  if(!preset_io::detail::PathExists(path))
    return;

  preset_io::detail::DeleteFile(path);
}

void DeleteStandaloneStateFile()
{
  DeleteStandaloneSettingsFile(GetStandaloneStatePath());
}

void DeleteStandaloneLegacySettingsFiles()
{
  DeleteStandaloneSettingsFile(GetStandaloneBreathCCSourcePath());
  DeleteStandaloneSettingsFile(GetStandalonePitchBendRangePath());
  DeleteStandaloneSettingsFile(GetStandaloneHarmonicVisualizerEnabledPath());
}

std::string GetTemporaryDirectoryPath()
{
#if defined(OS_WIN)
  const char* candidates[] = {std::getenv("TEMP"), std::getenv("TMP")};
#else
  const char* candidates[] = {std::getenv("TMPDIR"), std::getenv("TEMP"), std::getenv("TMP")};
#endif

  for(const char* candidate : candidates)
  {
    if(candidate && candidate[0] != '\0')
      return std::string{candidate};
  }

#if defined(OS_WIN)
  const std::string fallbackPath = GetDefaultUserPresetDirectory();
  return fallbackPath.empty() ? "." : fallbackPath;
#else
  return "/tmp";
#endif
}

void ShowPresetFileError(IGraphics* ui, const char* title, const std::string& message)
{
  if(ui)
    ui->ShowMessageBox(message.c_str(), title, kMB_OK);
}
} // namespace

Addivox::Addivox(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kMaxFactoryPresets))
, mEditorState(std::make_shared<plugin_ui::EditorState>())
{
  const auto formatPseudoLogScaleDisplay = [](double value, WDL_String& str) {
    int decimals = 2;
    if (value >= 10.0)  decimals = 1;
    if (value >= 100.0) decimals = 0;
    const double scale = (decimals == 2 ? 100.0 : (decimals == 1 ? 10.0 : 1.0));
    const double rounded = std::round(value * scale) / scale;
    str.SetFormatted(32, "%.*f\xC3\x97", decimals, rounded);
  };

  const auto initPseudoLogScale = [this, formatPseudoLogScaleDisplay](int paramIdx, const char* name, double defaultValue = 1.0) {
    GetParam(paramIdx)->InitDouble(name, defaultValue, 0., 100., 0.01, "", 0, "",
      transformations::GetGlobalPseudoLogShape(),
      iplug::IParam::kUnitCustom,
      formatPseudoLogScaleDisplay);
  };
  const auto formatPercentDisplay = [](double value, WDL_String& str) {
    str.SetFormatted(32, "%.1f%%", value);
  };
  const auto formatSignedCentsDisplay = [](double value, WDL_String& str) {
    const double roundedValue = std::round(value);
    const double normalizedValue = (roundedValue == 0.0) ? 0.0 : roundedValue;
    if(normalizedValue > 0.0)
      str.SetFormatted(32, "+%.0f\xC2\xA2", normalizedValue);
    else
      str.SetFormatted(32, "%.0f\xC2\xA2", normalizedValue);
  };
  const auto formatSignedUnitDisplay = [](double value, WDL_String& str) {
    const double roundedValue = std::round(value * 100.0) / 100.0;
    const double normalized = (roundedValue == 0.0) ? 0.0 : roundedValue;
    if(normalized > 0.0)
      str.SetFormatted(32, "+%.2f", normalized);
    else
      str.SetFormatted(32, "%.2f", normalized);
  };

  GetParam(kParamGlobalLevel)->InitDouble("Level", 1.0, 0., 10.0, 0.01, "", 0, "", transformations::GetLevelPseudoLogShape(), iplug::IParam::kUnitCustom, formatPseudoLogScaleDisplay);
  initPseudoLogScale(kParamGlobalAttackScale, "Attack");
  initPseudoLogScale(kParamGlobalReleaseScale, "Release");
  GetParam(kParamGlobalTuning)->InitDouble("Tuning", 0., -50., 50., 1.0, "", iplug::IParam::kFlagStepped, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCents, formatSignedCentsDisplay);
  GetParam(kParamGlobalPanShift)->InitDouble("Pan Shift", 0., -1., 1., 0.01, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatSignedUnitDisplay);
  initPseudoLogScale(kParamGlobalIntensityVariationAmplitudeScale, "Level Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalIntensityVariationRateScale, "Level Variation Rate");
  initPseudoLogScale(kParamGlobalPitchVariationAmplitudeScale, "Pitch Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPitchVariationRateScale, "Pitch Variation Rate");
  initPseudoLogScale(kParamGlobalPanVariationAmplitudeScale, "Pan Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPanVariationRateScale, "Pan Variation Rate");
  const auto& portamentoShape = transformations::GetPortamentoPseudoLogShape();
  GetParam(kParamPortamentoAtCC5Min)->InitDouble("Portamento (min)", 0.001, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
  GetParam(kParamPortamentoAtCC5Max)->InitDouble("Portamento (max)", 0.025, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
  GetParam(kParamEffectsDrive)->InitDouble("Drive", 0., 0., 100.0, 0.1, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatPercentDisplay);
  GetParam(kParamEffectsTone)->InitDouble("Tone", 0., -1.0, 1.0, 0.01, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatSignedUnitDisplay);
  GetParam(kParamTranspose)->InitInt("Transpose", 0, -36, 36, "st", iplug::IParam::kFlagSignDisplay);
  GetParam(kParamEffectsChorus)->InitDouble("Chorus", 0., 0., 100.0, 0.1, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatPercentDisplay);
  GetParam(kParamEffectsReverb)->InitDouble("Reverb", effects_settings::kDefaultReverb, 0., 100.0, 0.1, "", 0, "", iplug::IParam::ShapeLinear(), iplug::IParam::kUnitCustom, formatPercentDisplay);
    
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
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
    mEditorContext = plugin_ui::AttachMainControls(
      pGraphics,
      mEditorState,
      kCtrlTagHarmonicVisualizer,
      kCtrlTagEditorTabs,
      kCtrlTagKeyboard,
      kCtrlTagBender,
      kCtrlTagBreathMeter,
      kCtrlTagMeter,
      mPitchBendRange);
    
    pGraphics->SetKeyHandlerFunc([this, pGraphics](const IKeyPress& key, bool isUp) {
      const auto sendQwertyMidi = [this, pGraphics](const IMidiMsg& msg) {
        SendMidiMsgFromUI(msg);
        plugin_ui::HandleQwertyMidi(pGraphics, kCtrlTagKeyboard, mLastQwertyMIDINote, msg);
      };

      const auto releaseHeldQwertyMidiNotes = [&]() {
        IMidiMsg msg;
        for(int pitch = 0; pitch < static_cast<int>(mQwertyMidiKeysDown.size()); ++pitch)
        {
          if(!mQwertyMidiKeysDown[static_cast<std::size_t>(pitch)])
            continue;

          msg.MakeNoteOffMsg(pitch, 0);
          mQwertyMidiKeysDown[static_cast<std::size_t>(pitch)] = false;
          sendQwertyMidi(msg);
        }
      };

      const bool editMode = mEditorState && mEditorState->editMode;
      if(editMode && !mWasQwertyKeyboardInEditMode)
        releaseHeldQwertyMidiNotes();
      mWasQwertyKeyboardInEditMode = editMode;

      if(editMode)
      {
        const bool popupExpanded = pGraphics->GetPopupMenuControl() && pGraphics->GetPopupMenuControl()->GetExpanded();
        const bool textEntryActive = pGraphics->IsInPlatformTextEntry()
          || (pGraphics->GetControlInTextEntry() != nullptr);
        const bool modifiersActive = key.S || key.C || key.A;

        if(!isUp && !popupExpanded && !textEntryActive && !modifiersActive)
          plugin_ui::editor::ApplyKeyboardActionToSelectedTab(mEditorContext, key.VK);

        if(plugin_ui::editor::IsEditorActionShortcutKey(key.VK)
           || plugin_ui::editor::IsQwertyMidiKeyboardKey(key.VK))
          return true;

        return true;
      }

      const int noteOffset = plugin_ui::editor::GetQwertyMidiNoteOffset(key.VK);
      IMidiMsg msg;

      if(noteOffset >= 0)
      {
        const int pitch = std::clamp(mQwertyMidiBaseNote + noteOffset, 0, 127);
        const auto pitchIndex = static_cast<std::size_t>(pitch);

        if(!isUp)
        {
          if(!mQwertyMidiKeysDown[pitchIndex])
          {
            msg.MakeNoteOnMsg(pitch, 127, 0);
            mQwertyMidiKeysDown[pitchIndex] = true;
            sendQwertyMidi(msg);
          }
        }
        else if(mQwertyMidiKeysDown[pitchIndex])
        {
          msg.MakeNoteOffMsg(pitch, 0);
          mQwertyMidiKeysDown[pitchIndex] = false;
          sendQwertyMidi(msg);
        }

        return true;
      }

      if(key.VK == kVK_Z)
      {
        if(!isUp)
        {
          mQwertyMidiBaseNote = std::clamp(mQwertyMidiBaseNote - 12, 24, 96);
          releaseHeldQwertyMidiNotes();
        }
        return true;
      }

      if(key.VK == kVK_X)
      {
        if(!isUp)
        {
          mQwertyMidiBaseNote = std::clamp(mQwertyMidiBaseNote + 12, 24, 96);
          releaseHeldQwertyMidiNotes();
        }
        return true;
      }

      return true;
    });
  };
#endif

  LoadBuiltInPresets();
  RestorePreset(0);
  if(mActivePresetDisplayName.empty() && NPresets() > 0)
    mActivePresetDisplayName = GetPresetName(GetCurrentPresetIdx());

  SetPitchBendRange(mPitchBendRange);
  SetBreathCCSource(kDefaultBreathCCSource);
}

void Addivox::OnHostIdentified()
{
  EnsureStandaloneStateInitialized();
}

bool Addivox::LoadStandaloneState()
{
  IByteChunk chunk;
  if(!LoadStandaloneStateChunk(chunk))
    return false;

  int position = 0;
  IByteChunk::GetIPlugVerFromChunk(chunk, position);

  mSuppressStandaloneStatePersistence = true;
  const int result = UnserializeState(chunk, position);
  if(result >= 0)
    OnRestoreState();
  mSuppressStandaloneStatePersistence = false;

  if(result < 0)
    return false;

  DeleteStandaloneLegacySettingsFiles();
  mStandaloneStateSavedRevision = mStandaloneStateRevision;
  return true;
}

bool Addivox::SaveStandaloneState() const
{
  if(GetHost() != kHostStandalone)
    return false;

  IByteChunk chunk;
  IByteChunk::InitChunkWithIPlugVer(chunk);
  if(!SerializeState(chunk))
    return false;

  if(!SaveStandaloneStateChunk(chunk))
    return false;

  DeleteStandaloneLegacySettingsFiles();
  return true;
}

void Addivox::SaveStandaloneStateIfNeeded(bool force)
{
  if(GetHost() != kHostStandalone || !mStandaloneStateInitialized || mSuppressStandaloneStatePersistence)
    return;

  if(mStandaloneStateRevision == mStandaloneStateSavedRevision)
    return;

  const int64_t now = GetMonotonicMilliseconds();
  if(!force && (now - mStandaloneStateLastDirtyMillis) < kStandaloneStateSaveDebounceMs)
    return;

  const uint64_t revisionBeforeSave = mStandaloneStateRevision;
  if(!SaveStandaloneState())
    return;

  if(mStandaloneStateRevision == revisionBeforeSave)
    mStandaloneStateSavedRevision = revisionBeforeSave;
}

void Addivox::MarkStandaloneStateDirty()
{
  if(GetHost() != kHostStandalone || !mStandaloneStateInitialized || mSuppressStandaloneStatePersistence)
    return;

  mStandaloneStateLastDirtyMillis = GetMonotonicMilliseconds();
  ++mStandaloneStateRevision;
}

void Addivox::EnsureStandaloneStateInitialized()
{
  if(mStandaloneStateInitialized || GetHost() != kHostStandalone)
    return;

  mStandaloneStateInitialized = true;

  if(LoadStandaloneState())
    return;

  DeleteStandaloneStateFile();

  bool migratedLegacyState = false;
  mSuppressStandaloneStatePersistence = true;

  int persistedPitchBendRange = kDefaultPitchBendRange;
  if(LoadStandalonePitchBendRange(persistedPitchBendRange))
  {
    SetPitchBendRange(persistedPitchBendRange);
    migratedLegacyState = true;
  }

  BreathCCSource persistedSource = kDefaultBreathCCSource;
  if(LoadStandaloneBreathCCSource(persistedSource))
  {
    SetBreathCCSource(persistedSource);
    migratedLegacyState = true;
  }

  bool persistedEnabled = kDefaultHarmonicVisualizerEnabled;
  if(LoadStandaloneHarmonicVisualizerEnabled(persistedEnabled))
  {
    SetHarmonicVisualizerEnabled(persistedEnabled);
    migratedLegacyState = true;
  }

  mSuppressStandaloneStatePersistence = false;

  if(migratedLegacyState)
    MarkStandaloneStateDirty();
}

void Addivox::ResetStandaloneStateToDefaults()
{
  EnsureStandaloneStateInitialized();

  mSuppressStandaloneStatePersistence = true;
  DeleteStandaloneStateFile();
  DeleteStandaloneLegacySettingsFiles();
  mStandaloneStateRevision = 0;
  mStandaloneStateSavedRevision = 0;
  mStandaloneStateLastDirtyMillis = 0;

  mActiveUIMIDINotes.fill(false);
  mNumActiveUIMIDINotes = 0;

  if(NPresets() > 0)
    RestorePreset(0);

  SetPitchBendRange(kDefaultPitchBendRange);
  SetBreathCCSource(kDefaultBreathCCSource);
  SetHarmonicVisualizerEnabled(kDefaultHarmonicVisualizerEnabled);

#if IPLUG_DSP
  OnReset();
#endif

  mSuppressStandaloneStatePersistence = false;
  RefreshEditorUI(true);
}

void Addivox::SetPitchBendRange(int pitchBendRange)
{
  const int sanitizedPitchBendRange = SanitizePitchBendRange(pitchBendRange);
  const bool changed = sanitizedPitchBendRange != mPitchBendRange;
  mPitchBendRange = sanitizedPitchBendRange;

#if IPLUG_DSP
  mDSP.mSynth.SetPitchBendRange(mPitchBendRange);
#endif

  SyncPitchBendRangeUI();
  if(changed)
    MarkStandaloneStateDirty();
}

void Addivox::SetBreathCCSource(BreathCCSource source)
{
  const BreathCCSource sanitizedSource = SanitizeBreathCCSource(static_cast<int>(source));
  const bool changed = sanitizedSource != mBreathCCSource;
  mBreathCCSource = sanitizedSource;
  if(mEditorState)
    mEditorState->breathCCSource = mBreathCCSource;

#if IPLUG_DSP
  mDSP.SetBreathCCSource(mBreathCCSource);
  mBreathCCInputTracker.Reset();
#endif
  if(changed)
    MarkStandaloneStateDirty();
}

void Addivox::SetHarmonicVisualizerEnabled(bool enabled)
{
  const bool changed = enabled != mHarmonicVisualizerEnabled.load(std::memory_order_relaxed);
  mHarmonicVisualizerEnabled.store(enabled, std::memory_order_relaxed);
  mHarmonicVisualizerBlankPending.store(!enabled, std::memory_order_relaxed);
  if(mEditorState)
    mEditorState->harmonicVisualizerEnabled = enabled;

#if IPLUG_DSP
  mHarmonicVisualizerSender.ClearQueuedData();
  mHarmonicVisualizerSender.Reset(GetSampleRate(), PLUG_FPS);
#endif
  if(changed)
    MarkStandaloneStateDirty();
}

void Addivox::SendBreathControlFromUI(double value, int channel, int offset)
{
  if(IsHighResolutionBreathCCSource(mBreathCCSource))
  {
    const int rawValue = GetHighResolutionControllerValue(value);
    Plugin::SendMidiMsgFromUI(MakeControlChangeMsg(
      GetBreathCCSourceMSBController(mBreathCCSource),
      rawValue / 128,
      channel,
      offset));
    Plugin::SendMidiMsgFromUI(MakeControlChangeMsg(
      GetBreathCCSourceLSBController(mBreathCCSource),
      rawValue % 128,
      channel,
      offset));
    return;
  }

  Plugin::SendMidiMsgFromUI(MakeControlChangeMsg(
    GetBreathCCSourceMSBController(mBreathCCSource),
    GetSevenBitControllerValue(value),
    channel,
    offset));
}

void Addivox::ApplyPresetDocument(const preset_io::PresetDocument& document)
{
  mEditorState->compoundPreset = document.compoundPreset;
  mEditorState->selectedMidiNote = preset_io::detail::ChooseDefaultSelectedMidiNote(
    document.compoundPreset,
    mEditorState->selectedMidiNote);

#if IPLUG_DSP
  mDSP.SetCompoundPreset(document.compoundPreset);
#endif

  SetGlobalVoiceSettingsParams(*this, document.voiceSettings, false, false);
  SetEffectsSettingsParams(*this, document.effectsSettings, false);
  OnParamReset(kPresetRecall);
  SyncPresetOwnedParamDefaultsToCurrentValues(*this);

  if(!document.name.empty())
    mActivePresetDisplayName = document.name;
  else if(mActivePresetDisplayName.empty())
    mActivePresetDisplayName = "Preset";

  RefreshEditorUI(true);
  MarkStandaloneStateDirty();
}

void Addivox::PromptLoadPresetFromFile()
{
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if(!ui)
    return;

  WDL_String fileName;
  WDL_String directory;
  const std::string initialDirectory = mUserPresetDirectory.empty()
    ? GetDefaultUserPresetDirectory()
    : mUserPresetDirectory;
  if(!initialDirectory.empty())
    directory.Set(initialDirectory.c_str());

  ui->PromptForFile(
    fileName,
    directory,
    EFileAction::Open,
    "toml",
    [this](const WDL_String& selectedFileName, const WDL_String&) {
      const std::string fullPath = GetFullFileDialogPath(selectedFileName);
      if(fullPath.empty())
        return;

      preset_io::PresetDocument document;
      std::string errorMessage;
      if(!preset_io::LoadPresetFromFile(fullPath, document, &errorMessage))
      {
        ShowPresetFileError(GetUI(), "Preset Load Failed", errorMessage);
        return;
      }

      mUserPresetDirectory = preset_io::detail::ParentPath(fullPath);
      ApplyPresetDocument(document);
    });
#endif
}

void Addivox::PromptSavePresetToFile()
{
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if(!ui)
    return;

  preset_io::PresetDocument document;
  document.name = mActivePresetDisplayName.empty() ? "Preset" : mActivePresetDisplayName;
  document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
  document.effectsSettings = GetEffectsSettingsFromParams(*this);
  document.compoundPreset = mEditorState->compoundPreset;

  WDL_String fileName;
  WDL_String directory;
  const std::string defaultFileName = SanitizePresetFileName(document.name);
  const std::string initialDirectory = mUserPresetDirectory.empty()
    ? GetDefaultUserPresetDirectory()
    : mUserPresetDirectory;

#if defined(OS_IOS)
  const std::string exportPath = preset_io::detail::JoinPath(GetTemporaryDirectoryPath(), defaultFileName);
  std::string errorMessage;
  if(!preset_io::SavePresetToFile(exportPath, document, &errorMessage))
  {
    ShowPresetFileError(ui, "Preset Save Failed", errorMessage);
    return;
  }

  fileName.Set(defaultFileName.c_str());
  directory.Set(exportPath.c_str());
  ui->PromptForFile(
    fileName,
    directory,
    EFileAction::Save,
    "toml",
    [this, exportPath](const WDL_String& selectedFileName, const WDL_String&) {
      preset_io::detail::DeleteFile(exportPath);

      const std::string destinationPath = GetFullFileDialogPath(selectedFileName);
      if(destinationPath.empty())
        return;

      mUserPresetDirectory = preset_io::detail::ParentPath(destinationPath);
    });
#else
  fileName.Set(defaultFileName.c_str());
  if(!initialDirectory.empty())
    directory.Set(initialDirectory.c_str());

  ui->PromptForFile(
    fileName,
    directory,
    EFileAction::Save,
    "toml",
    [this, document](const WDL_String& selectedFileName, const WDL_String&) {
      std::string fullPath = GetFullFileDialogPath(selectedFileName);
      if(fullPath.empty())
        return;

      fullPath = EnsureTomlExtension(std::move(fullPath));

      std::string errorMessage;
      if(!preset_io::SavePresetToFile(fullPath, document, &errorMessage))
      {
        ShowPresetFileError(GetUI(), "Preset Save Failed", errorMessage);
        return;
      }

      mUserPresetDirectory = preset_io::detail::ParentPath(fullPath);
    });
#endif
#endif
}

void Addivox::SendMidiMsgFromUI(const IMidiMsg& msg)
{
  const IMidiMsg::EStatusMsg status = msg.StatusMsg();
  const bool isNoteOn = (status == IMidiMsg::kNoteOn) && (msg.Velocity() > 0);
  const bool isNoteOff = (status == IMidiMsg::kNoteOff) || ((status == IMidiMsg::kNoteOn) && (msg.Velocity() == 0));

  if(!isNoteOn && !isNoteOff)
  {
    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  const int midiNote = msg.NoteNumber();
  if(midiNote < 0 || midiNote >= static_cast<int>(mActiveUIMIDINotes.size()))
  {
    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  const auto sendBreathFromUI = [this, &msg](double value) {
    SendBreathControlFromUI(value, msg.Channel(), msg.mOffset);
  };

  if(isNoteOn)
  {
    if(!mActiveUIMIDINotes[static_cast<size_t>(midiNote)])
    {
      if(mNumActiveUIMIDINotes == 0)
        sendBreathFromUI(1.0);

      mActiveUIMIDINotes[static_cast<size_t>(midiNote)] = true;
      ++mNumActiveUIMIDINotes;
    }

    Plugin::SendMidiMsgFromUI(msg);
    return;
  }

  bool releasedActiveUINote = false;
  if(mActiveUIMIDINotes[static_cast<size_t>(midiNote)])
  {
    mActiveUIMIDINotes[static_cast<size_t>(midiNote)] = false;
    --mNumActiveUIMIDINotes;
    releasedActiveUINote = true;
  }

  Plugin::SendMidiMsgFromUI(msg);

  if(releasedActiveUINote && mNumActiveUIMIDINotes == 0)
    sendBreathFromUI(0.0);
}

bool Addivox::SerializeState(IByteChunk& chunk) const
{
  preset_io::PresetDocument document;
  document.name = mActivePresetDisplayName.empty() ? GetPresetName(GetCurrentPresetIdx()) : mActivePresetDisplayName;
  document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
  document.effectsSettings = GetEffectsSettingsFromParams(*this);
  document.compoundPreset = mEditorState->compoundPreset;

  return chunk.PutStr(preset_io::SerializePresetToToml(document).c_str()) > 0
    && SerializeParams(chunk)
    && AppendPluginStateSettingsChunk(
      chunk,
      mBreathCCSource,
      mHarmonicVisualizerEnabled.load(std::memory_order_relaxed),
      mPitchBendRange);
}

int Addivox::UnserializeState(const IByteChunk& chunk, int startPos)
{
  WDL_String presetToml;
  int position = chunk.GetStr(presetToml, startPos);
  if(position < 0)
    return position;

  preset_io::PresetDocument document;
  if(!preset_io::ParsePresetToml(presetToml.Get(), document))
    return -1;

  mEditorState->compoundPreset = document.compoundPreset;
  mEditorState->selectedMidiNote = preset_io::detail::ChooseDefaultSelectedMidiNote(
    document.compoundPreset,
    mEditorState->selectedMidiNote);
  mPendingRestoredStatePresetName = document.name;
  SetGlobalVoiceSettingsParams(*this, document.voiceSettings, false, false);
  SetEffectsSettingsParams(*this, document.effectsSettings, false);

#if IPLUG_DSP
  mDSP.SetCompoundPreset(document.compoundPreset);
#endif

  int pos = position;
  ENTER_PARAMS_MUTEX
  pos = RestoreStateParamsFromChunk(*this, chunk, pos);
  OnParamReset(kPresetRecall);
  SyncPresetOwnedParamDefaultsToCurrentValues(*this);
  LEAVE_PARAMS_MUTEX

  int restoredPitchBendRange = mPitchBendRange;
  BreathCCSource restoredBreathCCSource = kDefaultBreathCCSource;
  bool restoredHarmonicVisualizerEnabled = mHarmonicVisualizerEnabled.load(std::memory_order_relaxed);
  if(ReadPluginStateSettingsChunk(
       chunk,
       pos,
       restoredBreathCCSource,
       restoredHarmonicVisualizerEnabled,
       restoredPitchBendRange))
  {
    SetPitchBendRange(restoredPitchBendRange);
    SetBreathCCSource(restoredBreathCCSource);
    SetHarmonicVisualizerEnabled(restoredHarmonicVisualizerEnabled);
  }

  return pos;
}

void Addivox::OnRestoreState()
{
  Plugin::OnRestoreState();

  if(!mPendingRestoredStatePresetName.empty())
  {
    mActivePresetDisplayName = mPendingRestoredStatePresetName;
    mPendingRestoredStatePresetName.clear();
  }
  else if(NPresets() > 0 && GetCurrentPresetIdx() >= 0 && GetCurrentPresetIdx() < NPresets())
  {
    mActivePresetDisplayName = GetPresetName(GetCurrentPresetIdx());
  }

  RefreshEditorUI(true);
  MarkStandaloneStateDirty();
}

void Addivox::OnUIOpen()
{
  EnsureStandaloneStateInitialized();
  Plugin::OnUIOpen();
  RefreshEditorUI();
}

void Addivox::OnUIClose()
{
  SaveStandaloneStateIfNeeded(true);
  mEditorContext.reset();
  mQwertyMidiKeysDown.fill(false);
  mQwertyMidiBaseNote = 48;
  mWasQwertyKeyboardInEditMode = false;
  mLastQwertyMIDINote = -1;
}

bool Addivox::OnHostRequestingAboutBox()
{
  return ShowAboutBox();
}

bool Addivox::OnHostRequestingProductHelp()
{
  return OpenOnlineDocs();
}

void Addivox::OnParamChangeUI(int paramIdx, EParamSource source)
{
  (void) paramIdx;
  (void) source;
  MarkStandaloneStateDirty();
}

bool Addivox::ShowAboutBox()
{
  return plugin_ui::layout::ShowAboutBox(GetUI(), kCtrlTagAboutBox);
}

bool Addivox::OpenOnlineDocs()
{
  if(auto* ui = GetUI())
    return ui->OpenURL(PLUG_URL_STR);

  return false;
}

#if IPLUG_DSP
void Addivox::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  (void) inputs;
  mDSP.ProcessBlock(outputs, nFrames);
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
  if(mHarmonicVisualizerEnabled.load(std::memory_order_relaxed))
  {
    mHarmonicVisualizerSender.ProcessBlock(
      nFrames,
      kCtrlTagHarmonicVisualizer,
      [this](VisualizerFrame& frame) { mDSP.GetVisualizerFrame(frame); });
  }
}

void Addivox::OnIdle()
{
  mMeterSender.TransmitData(*this);
  if(mHarmonicVisualizerEnabled.load(std::memory_order_relaxed))
  {
    mHarmonicVisualizerSender.TransmitData(*this);
  }
  else
  {
    mHarmonicVisualizerSender.ClearQueuedData();
    if(mHarmonicVisualizerBlankPending.exchange(false, std::memory_order_relaxed))
    {
      mHarmonicVisualizerSender.PushFrame(kCtrlTagHarmonicVisualizer, VisualizerFrame{});
      mHarmonicVisualizerSender.TransmitData(*this);
    }
  }

  const double breathLevel = mBreathLevel.load(std::memory_order_relaxed);
  if(breathLevel != mLastSentBreathLevel)
  {
    SendControlValueFromDelegate(kCtrlTagBreathMeter, breathLevel);
    mLastSentBreathLevel = breathLevel;
  }

  SaveStandaloneStateIfNeeded();
}

void Addivox::OnReset()
{
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
  if(!mHarmonicVisualizerEnabled.load(std::memory_order_relaxed))
    mHarmonicVisualizerBlankPending.store(true, std::memory_order_relaxed);
  else
    mHarmonicVisualizerBlankPending.store(false, std::memory_order_relaxed);
  mQwertyMidiKeysDown.fill(false);
  mQwertyMidiBaseNote = 48;
  mWasQwertyKeyboardInEditMode = false;
  mLastQwertyMIDINote = -1;
  mBreathLevel.store(1.0, std::memory_order_relaxed);
  mLastSentBreathLevel = -1.;
}

void Addivox::ProcessMidiMsg(const IMidiMsg& msg)
{
  const BreathCCValueUpdate breathUpdate = mBreathCCInputTracker.HandleMessage(mBreathCCSource, msg);
  if(breathUpdate.hasValue)
    mBreathLevel.store(breathUpdate.value, std::memory_order_relaxed);

  mDSP.ProcessMidiMsg(msg);
  SendMidiMsg(msg);
}

void Addivox::OnParamChange(int paramIdx)
{
  mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

bool Addivox::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if(msgTag == editor_messages::kMsgTagPromptLoadPresetFromFile)
  {
    PromptLoadPresetFromFile();
    return true;
  }

  if(msgTag == editor_messages::kMsgTagPromptSavePresetToFile)
  {
    PromptSavePresetToFile();
    return true;
  }

  if(msgTag == editor_messages::kMsgTagSetBreathCCSource
    && dataSize == sizeof(editor_messages::SetBreathCCSourcePayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetBreathCCSourcePayload*>(pData);
    SetBreathCCSource(SanitizeBreathCCSource(payload->source));
    return true;
  }

  if(msgTag == editor_messages::kMsgTagSetHarmonicVisualizerEnabled
    && dataSize == sizeof(editor_messages::SetHarmonicVisualizerEnabledPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetHarmonicVisualizerEnabledPayload*>(pData);
    SetHarmonicVisualizerEnabled(payload->enabled != 0);
    return true;
  }

  if(msgTag == editor_messages::kMsgTagResetStandaloneStateToDefaults)
  {
    ResetStandaloneStateToDefaults();
    return true;
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagSetKeyNoteOscillatorParameter
    && dataSize == sizeof(editor_messages::SetKeyNoteOscillatorParameterPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetKeyNoteOscillatorParameterPayload*>(pData);
    if(payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters)
      return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    const bool updated = mDSP.mSynth.GetVoice().SetKeyNoteOscillatorParameter(
      payload->midiNote,
      payload->oscillatorIndex,
      parameter,
      payload->value);
    if(updated)
      MarkStandaloneStateDirty();
    return updated;
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagSetKeyNoteOscillatorParameterValues
    && dataSize == sizeof(editor_messages::SetKeyNoteOscillatorParameterValuesPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetKeyNoteOscillatorParameterValuesPayload*>(pData);
    if(payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters)
      return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    const bool updated = mDSP.mSynth.GetVoice().SetKeyNoteOscillatorParameterValues(
      payload->midiNote,
      parameter,
      payload->values);
    if(updated)
      MarkStandaloneStateDirty();
    return updated;
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagSetKeyNoteEqCurve
    && dataSize > 0
    && pData)
  {
    int midiNote = 0;
    EqCurve curve;
    if(!editor_messages::DeserializeKeyNoteEqCurvePayload(dataSize, pData, midiNote, curve))
      return false;

    const bool updated = mDSP.mSynth.GetVoice().SetKeyNoteEqCurve(midiNote, curve);
    if(updated)
      MarkStandaloneStateDirty();
    return updated;
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagSetAllKeyNotesEnabled
    && dataSize == sizeof(editor_messages::SetAllKeyNotesEnabledPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetAllKeyNotesEnabledPayload*>(pData);
    if(payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters)
      return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    const bool updated = mDSP.mSynth.GetVoice().SetAllKeyNotesEnabled(
      parameter,
      payload->enabled != 0,
      payload->midiNote);
    if(updated)
      MarkStandaloneStateDirty();
    return updated;
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagSetAllKeyNotesEqEnabled
    && dataSize == sizeof(editor_messages::SetAllKeyNotesEqEnabledPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetAllKeyNotesEqEnabledPayload*>(pData);
    const bool updated = mDSP.mSynth.GetVoice().SetAllKeyNotesEqEnabled(payload->enabled != 0);
    if(updated)
      MarkStandaloneStateDirty();
    return updated;
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagAddKeyNotePreset
    && dataSize == sizeof(editor_messages::KeyNotePresetPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::KeyNotePresetPayload*>(pData);
    const bool updated = mDSP.mSynth.GetVoice().AddKeyNotePreset(payload->midiNote);
    if(updated)
      MarkStandaloneStateDirty();
    return updated;
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagRemoveKeyNotePreset
    && dataSize == sizeof(editor_messages::KeyNotePresetPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::KeyNotePresetPayload*>(pData);
    const bool updated = mDSP.mSynth.GetVoice().RemoveKeyNotePreset(payload->midiNote);
    if(updated)
      MarkStandaloneStateDirty();
    return updated;
  }

  if(ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    SetPitchBendRange(bendRange);
    return true;
  }
  
  return false;
}
#endif

void Addivox::LoadBuiltInPresets()
{
  WDL_String resourcePath;
  BundleResourcePath(resourcePath, GetBundleID());

  int numLoadedPresets = 0;
  if(resourcePath.GetLength() > 0)
  {
    const auto presetPaths = preset_io::FindPresetFiles(preset_io::detail::JoinPath(resourcePath.Get(), "presets"));
    for(const auto& presetPath : presetPaths)
    {
      preset_io::PresetDocument document;
      if(!preset_io::LoadPresetFromFile(presetPath, document))
        continue;

      IByteChunk chunk;
      if(!BuildPresetChunk(document, chunk))
        continue;

      const std::string presetName = document.name.empty() ? preset_io::detail::FileStem(presetPath) : document.name;
      MakePresetFromChunk(presetName.c_str(), chunk);
      ++numLoadedPresets;

      if(numLoadedPresets >= kMaxFactoryPresets)
        break;
    }
  }

  if(numLoadedPresets == 0)
  {
    preset_io::PresetDocument fallbackDocument;
    fallbackDocument.name = "Init";
    fallbackDocument.voiceSettings = GlobalVoiceSettings{};
    fallbackDocument.effectsSettings = EffectsSettings{};
    fallbackDocument.compoundPreset = CompoundPreset{};

    IByteChunk chunk;
    if(BuildPresetChunk(fallbackDocument, chunk))
      MakePresetFromChunk(fallbackDocument.name.c_str(), chunk);
  }

  PruneUninitializedPresets();
}

void Addivox::RefreshEditorUI(bool resetOscillatorRestoreStates)
{
#if IPLUG_EDITOR
  if(!GetUI() || !mEditorContext)
    return;

  if(mEditorContext->title.presetManagerControl && *mEditorContext->title.presetManagerControl)
  {
    if(auto* presetManager =
         dynamic_cast<plugin_ui::layout::BakedPresetManagerControl*>(*mEditorContext->title.presetManagerControl))
    {
      const std::string label = mActivePresetDisplayName.empty()
        ? (NPresets() > 0 ? std::string{GetPresetName(GetCurrentPresetIdx())} : std::string{"Choose Preset..."})
        : mActivePresetDisplayName;
      presetManager->SetPresetLabel(label.c_str());
    }
  }

  if(auto* keyboard = plugin_ui::layout::GetKeyboardControl(GetUI(), kCtrlTagKeyboard))
  {
    plugin_ui::layout::RefreshKeyboardKeyNoteHighlights(keyboard, mEditorState->compoundPreset);
    keyboard->SetSelectedMidiNote(mEditorState->selectedMidiNote);
  }

  SyncPitchBendRangeUI();

  if(resetOscillatorRestoreStates)
    mEditorContext->ResetOscillatorRestoreStates();

  mEditorContext->RefreshOscillatorTabs();
  mEditorContext->RefreshEditorActionButtons();
  GetUI()->SetAllControlsDirty();
#endif
}

void Addivox::SyncPitchBendRangeUI()
{
#if IPLUG_EDITOR
  if(!GetUI())
    return;

  if(auto* control = GetUI()->GetControlWithTag(kCtrlTagBender))
  {
    if(auto* wheelControl = control->As<IWheelControl>())
      wheelControl->SetPitchBendRange(mPitchBendRange);
  }
#endif
}

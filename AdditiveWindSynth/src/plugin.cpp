#include "plugin.h"
#include "IPlug_include_in_plug_src.h"
#include "IPlugPaths.h"
#include "settings/params.h"
#include "settings/preset_io.h"
#include "editor_messages.h"
#include "ui/transformations.h"
#include "ui/layout.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace
{
GlobalVoiceSettings GetGlobalVoiceSettingsFromParams(const AdditiveWindSynth& plugin)
{
  GlobalVoiceSettings settings{};
  for(int paramIdx = 0; paramIdx < kNumParams; ++paramIdx)
    global_settings::ApplyParam(paramIdx, plugin.GetParam(paramIdx)->Value(), settings);

  return global_settings::Sanitize(settings);
}

void SetGlobalVoiceSettingsParams(AdditiveWindSynth& plugin,
                                  const GlobalVoiceSettings& voiceSettings)
{
  const GlobalVoiceSettings sanitizedVoiceSettings = global_settings::Sanitize(voiceSettings);
  plugin.GetParam(kParamGlobalLevel)->Set(sanitizedVoiceSettings.levelScale);
  plugin.GetParam(kParamGlobalAttackScale)->Set(sanitizedVoiceSettings.attackScale);
  plugin.GetParam(kParamGlobalReleaseScale)->Set(sanitizedVoiceSettings.releaseScale);
  plugin.GetParam(kParamGlobalPitchShift)->Set(sanitizedVoiceSettings.pitchOffsetCents);
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

int ChooseDefaultSelectedMidiNote(const CompoundPreset& compoundPreset, int preferredMidiNote)
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

bool BuildPresetChunk(const preset_io::PresetDocument& document, IByteChunk& chunk)
{
  const std::string toml = preset_io::SerializePresetToToml(document);
  const GlobalVoiceSettings voiceSettings = global_settings::Sanitize(document.voiceSettings);
  return chunk.PutStr(toml.c_str()) > 0
    && chunk.Put(&voiceSettings.levelScale) > 0
    && chunk.Put(&voiceSettings.attackScale) > 0
    && chunk.Put(&voiceSettings.releaseScale) > 0
    && chunk.Put(&voiceSettings.pitchOffsetCents) > 0
    && chunk.Put(&voiceSettings.panOffset) > 0
    && chunk.Put(&voiceSettings.intensityVariationAmplitudeScale) > 0
    && chunk.Put(&voiceSettings.intensityVariationRateScale) > 0
    && chunk.Put(&voiceSettings.pitchVariationAmplitudeScale) > 0
    && chunk.Put(&voiceSettings.pitchVariationRateScale) > 0
    && chunk.Put(&voiceSettings.panVariationAmplitudeScale) > 0
    && chunk.Put(&voiceSettings.panVariationRateScale) > 0
    && chunk.Put(&voiceSettings.portamentoTimeAtCC5MinSec) > 0
    && chunk.Put(&voiceSettings.portamentoTimeAtCC5MaxSec) > 0;
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

AdditiveWindSynth::AdditiveWindSynth(const InstanceInfo& info)
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

  GetParam(kParamGlobalLevel)->InitDouble("Level", 1.0, 0., 10.0, 0.01, "", 0, "", transformations::GetLevelPseudoLogShape(), iplug::IParam::kUnitCustom, formatPseudoLogScaleDisplay);
  initPseudoLogScale(kParamGlobalAttackScale, "Attack");
  initPseudoLogScale(kParamGlobalReleaseScale, "Release");
  GetParam(kParamGlobalPitchShift)->InitDouble("Pitch Shift", 0., -50., 50., 0.1, "cents");
  GetParam(kParamGlobalPanShift)->InitDouble("Pan Shift", 0., -1., 1., 0.01, "", iplug::IParam::kFlagSignDisplay);
  initPseudoLogScale(kParamGlobalIntensityVariationAmplitudeScale, "Level Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalIntensityVariationRateScale, "Level Variation Rate");
  initPseudoLogScale(kParamGlobalPitchVariationAmplitudeScale, "Pitch Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPitchVariationRateScale, "Pitch Variation Rate");
  initPseudoLogScale(kParamGlobalPanVariationAmplitudeScale, "Pan Variation Amount", 0.0);
  initPseudoLogScale(kParamGlobalPanVariationRateScale, "Pan Variation Rate");
  const auto& portamentoShape = transformations::GetPortamentoPseudoLogShape();
  GetParam(kParamPortamentoAtCC5Min)->InitDouble("Portamento (min)", 0.001, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
  GetParam(kParamPortamentoAtCC5Max)->InitDouble("Portamento (max)", 0.025, 0., 1.0, 0.0001, "s", 0, "", portamentoShape);
  GetParam(kParamEffectsDrive)->InitDouble(
    "Drive",
    0.,
    0.,
    100.0,
    0.1,
    "",
    0,
    "",
    iplug::IParam::ShapeLinear(),
    iplug::IParam::kUnitCustom,
    formatPercentDisplay);
  GetParam(kParamEffectsReverb)->InitDouble(
    "Reverb",
    0.,
    0.,
    100.0,
    0.1,
    "",
    0,
    "",
    iplug::IParam::ShapeLinear(),
    iplug::IParam::kUnitCustom,
    formatPercentDisplay);
    
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
      kCtrlTagMeter);
    
    pGraphics->SetQwertyMidiKeyHandlerFunc([this, pGraphics](const IMidiMsg& msg) {
      plugin_ui::HandleQwertyMidi(pGraphics, kCtrlTagKeyboard, mLastQwertyMIDINote, msg);
    });
  };
#endif

  LoadBuiltInPresets();
  RestorePreset(0);
  if(mActivePresetDisplayName.empty() && NPresets() > 0)
    mActivePresetDisplayName = GetPresetName(GetCurrentPresetIdx());
}

void AdditiveWindSynth::ApplyPresetDocument(const preset_io::PresetDocument& document)
{
  mEditorState->compoundPreset = document.compoundPreset;
  mEditorState->selectedMidiNote = ChooseDefaultSelectedMidiNote(
    document.compoundPreset,
    mEditorState->selectedMidiNote);

#if IPLUG_DSP
  mDSP.SetCompoundPreset(document.compoundPreset);
#endif

  SetGlobalVoiceSettingsParams(*this, document.voiceSettings);
  OnParamReset(kPresetRecall);

  if(!document.name.empty())
    mActivePresetDisplayName = document.name;
  else if(mActivePresetDisplayName.empty())
    mActivePresetDisplayName = "Preset";

  RefreshEditorUI(true);
}

void AdditiveWindSynth::PromptLoadPresetFromFile()
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

void AdditiveWindSynth::PromptSavePresetToFile()
{
#if IPLUG_EDITOR
  IGraphics* ui = GetUI();
  if(!ui)
    return;

  preset_io::PresetDocument document;
  document.name = mActivePresetDisplayName.empty() ? "Preset" : mActivePresetDisplayName;
  document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
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

void AdditiveWindSynth::SendMidiMsgFromUI(const IMidiMsg& msg)
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
    IMidiMsg breathMsg;
    breathMsg.MakeControlChangeMsg(IMidiMsg::kBreathController, value, msg.Channel(), msg.mOffset);
    Plugin::SendMidiMsgFromUI(breathMsg);
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

bool AdditiveWindSynth::SerializeState(IByteChunk& chunk) const
{
  preset_io::PresetDocument document;
  document.name = mActivePresetDisplayName.empty() ? GetPresetName(GetCurrentPresetIdx()) : mActivePresetDisplayName;
  document.voiceSettings = GetGlobalVoiceSettingsFromParams(*this);
  document.compoundPreset = mEditorState->compoundPreset;

  return chunk.PutStr(preset_io::SerializePresetToToml(document).c_str()) > 0
    && SerializeParams(chunk);
}

int AdditiveWindSynth::UnserializeState(const IByteChunk& chunk, int startPos)
{
  WDL_String presetToml;
  int position = chunk.GetStr(presetToml, startPos);
  if(position < 0)
    return position;

  preset_io::PresetDocument document;
  if(!preset_io::ParsePresetToml(presetToml.Get(), document))
    return -1;

  mEditorState->compoundPreset = document.compoundPreset;
  mEditorState->selectedMidiNote = ChooseDefaultSelectedMidiNote(
    document.compoundPreset,
    mEditorState->selectedMidiNote);
  mPendingRestoredStatePresetName = document.name;
  SetGlobalVoiceSettingsParams(*this, document.voiceSettings);

#if IPLUG_DSP
  mDSP.SetCompoundPreset(document.compoundPreset);
#endif

  int pos = position;
  ENTER_PARAMS_MUTEX
  for(int paramIdx = 0; paramIdx < NParams(); ++paramIdx)
  {
    double value = 0.0;
    const int nextPos = chunk.Get(&value, pos);
    if(nextPos < 0)
      break;

    GetParam(paramIdx)->Set(value);
    pos = nextPos;
  }
  OnParamReset(kPresetRecall);
  LEAVE_PARAMS_MUTEX

  return pos;
}

void AdditiveWindSynth::OnRestoreState()
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
}

void AdditiveWindSynth::OnUIOpen()
{
  Plugin::OnUIOpen();
  RefreshEditorUI();
}

void AdditiveWindSynth::OnUIClose()
{
  mEditorContext.reset();
}

#if IPLUG_DSP
void AdditiveWindSynth::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  (void) inputs;
  mDSP.ProcessBlock(outputs, nFrames);
  mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
  mHarmonicVisualizerSender.ProcessBlock(
    nFrames,
    kCtrlTagHarmonicVisualizer,
    [this](VisualizerFrame& frame) { mDSP.GetVisualizerFrame(frame); });
}

void AdditiveWindSynth::OnIdle()
{
  mMeterSender.TransmitData(*this);
  mHarmonicVisualizerSender.TransmitData(*this);
  const double breathLevel = mBreathLevel.load(std::memory_order_relaxed);
  if(breathLevel != mLastSentBreathLevel)
  {
    SendControlValueFromDelegate(kCtrlTagBreathMeter, breathLevel);
    mLastSentBreathLevel = breathLevel;
  }
}

void AdditiveWindSynth::OnReset()
{
  mDSP.Reset(GetSampleRate(), GetBlockSize());

  // Make the meter respond more quickly to changes in level.
  mMeterSender.Reset(GetSampleRate());
  mMeterSender.SetDecayTimeMs(50.0, GetSampleRate());
  mMeterSender.SetPeakHoldTimeMs(250.0, GetSampleRate());
  
  mHarmonicVisualizerSender.Reset(GetSampleRate(), PLUG_FPS);
  mLastQwertyMIDINote = -1;
  mBreathLevel.store(1.0, std::memory_order_relaxed);
  mLastSentBreathLevel = -1.;
}

void AdditiveWindSynth::ProcessMidiMsg(const IMidiMsg& msg)
{
  if(msg.StatusMsg() == IMidiMsg::kControlChange && msg.ControlChangeIdx() == IMidiMsg::kBreathController)
  {
    mBreathLevel.store(static_cast<double>(msg.ControlChange(IMidiMsg::kBreathController)), std::memory_order_relaxed);
  }

  mDSP.ProcessMidiMsg(msg);
  SendMidiMsg(msg);
}

void AdditiveWindSynth::OnParamChange(int paramIdx)
{
  mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

bool AdditiveWindSynth::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
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

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagSetKeyNoteOscillatorParameter
    && dataSize == sizeof(editor_messages::SetKeyNoteOscillatorParameterPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::SetKeyNoteOscillatorParameterPayload*>(pData);
    if(payload->parameter < 0 || payload->parameter >= OscillatorSettings::kNumParameters)
      return false;

    const auto parameter = static_cast<OscillatorSettings::Parameter>(payload->parameter);
    return mDSP.SetKeyNoteOscillatorParameter(payload->midiNote, payload->oscillatorIndex, parameter, payload->value);
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
    return mDSP.SetKeyNoteOscillatorParameterValues(payload->midiNote, parameter, payload->values);
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagAddKeyNotePreset
    && dataSize == sizeof(editor_messages::KeyNotePresetPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::KeyNotePresetPayload*>(pData);
    return mDSP.AddKeyNotePreset(payload->midiNote);
  }

  if(ctrlTag == kCtrlTagEditorTabs
    && msgTag == editor_messages::kMsgTagRemoveKeyNotePreset
    && dataSize == sizeof(editor_messages::KeyNotePresetPayload)
    && pData)
  {
    const auto* payload = static_cast<const editor_messages::KeyNotePresetPayload*>(pData);
    return mDSP.RemoveKeyNotePreset(payload->midiNote);
  }

  if(ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
  {
    const int bendRange = *static_cast<const int*>(pData);
    mDSP.mSynth.SetPitchBendRange(bendRange);
  }
  
  return false;
}
#endif

void AdditiveWindSynth::LoadBuiltInPresets()
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
    fallbackDocument.compoundPreset = CompoundPreset{};

    IByteChunk chunk;
    if(BuildPresetChunk(fallbackDocument, chunk))
      MakePresetFromChunk(fallbackDocument.name.c_str(), chunk);
  }

  PruneUninitializedPresets();
}

void AdditiveWindSynth::RefreshEditorUI(bool resetOscillatorRestoreStates)
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

  if(resetOscillatorRestoreStates)
    mEditorContext->ResetOscillatorRestoreStates();

  mEditorContext->RefreshOscillatorTabs();
  mEditorContext->RefreshEditorActionButtons();
  GetUI()->SetAllControlsDirty();
#endif
}

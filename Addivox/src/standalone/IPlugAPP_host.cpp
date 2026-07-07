/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "../../../iPlug2/IPlug/APP/IPlugAPP_host.h"
#include "audio_midi_settings.h"
#include "resource.h"

#ifdef OS_WIN
#include "win32_utf8.h"
#include <sys/stat.h>
#endif

#include "IPlugLogger.h"

#include <atomic>

#if defined OS_WIN
#include <cstdint>
#elif defined OS_MAC
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>

#include <cmath>
#endif

using namespace iplug;

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 2048
#endif

#define STRBUFSZ 100

std::unique_ptr<IPlugAPPHost> IPlugAPPHost::sInstance;
UINT gSCROLLMSG;

namespace {
std::optional<IPlugAPPHost::AppState> gLastWorkingAudioState;
std::unique_ptr<RtAudio>               gPreservedAudio;

constexpr UINT    kAudioRecoveryDelayMilliseconds = 100;
std::atomic<bool> gAudioRecoveryPending{false};

// Generous upper bound for one callback-driven fade-out; the longest buffer
// option (8192 frames at 44.1 kHz) takes under 200 ms.
constexpr int kCloseAudioTimeoutMilliseconds = 500;

#if defined OS_WIN
constexpr UINT_PTR kAudioRecoveryTimerID = 0xADD1;

void CALLBACK RecoverAudioAfterDriverReset(HWND window, UINT, UINT_PTR timerID, DWORD) {
  KillTimer(window, timerID);
  if (!gAudioRecoveryPending.exchange(false)) return;

  if (auto* appHost = IPlugAPPHost::sInstance.get()) appHost->TryToChangeAudio();
}

void ScheduleAudioRecovery() {
  if (gHWND && gAudioRecoveryPending.load()) SetTimer(gHWND, kAudioRecoveryTimerID, kAudioRecoveryDelayMilliseconds, RecoverAudioAfterDriverReset);
}
#elif defined OS_MAC
void ScheduleAudioRecovery() {
  // RtAudio reports device disconnection from a CoreAudio notification thread,
  // so hop to the main thread (after a short delay, giving the device topology
  // time to settle) before restarting audio.
  const dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, kAudioRecoveryDelayMilliseconds * static_cast<int64_t>(NSEC_PER_MSEC));
  dispatch_after_f(when, dispatch_get_main_queue(), nullptr, [](void*) {
    if (!gAudioRecoveryPending.exchange(false)) return;

    if (auto* appHost = IPlugAPPHost::sInstance.get()) appHost->TryToChangeAudio();
  });
}
#endif

#if defined OS_MAC
// RtAudio's CoreAudio backend never watches kAudioDevicePropertyNominalSampleRate,
// so if another application (a DAW, another Addivox instance, Audio MIDI Setup)
// changes the device rate while our stream is open, CoreAudio keeps the stream
// running at the new hardware rate while the plugin still renders for the old
// one — shifting every pitch by the rate ratio (48k -> 44.1k sounds ~147 cents
// flat). Watch the device ourselves and restart audio at the rate the hardware
// is actually running.
#if defined(MAC_OS_VERSION_12_0) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_VERSION_12_0)
constexpr AudioObjectPropertyElement kAudioPropertyElement = kAudioObjectPropertyElementMain;
#else
constexpr AudioObjectPropertyElement kAudioPropertyElement = kAudioObjectPropertyElementMaster;
#endif

constexpr AudioObjectPropertyAddress kNominalRateAddress = {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
                                                            kAudioPropertyElement};

// Only gRateCheckPending crosses threads (set from CoreAudio's notification
// thread); everything else is touched on the main thread only.
std::atomic<bool>       gRateCheckPending{false};
AudioObjectID           gRateListenerDevice = kAudioObjectUnknown;
double                  gActiveDeviceSampleRate = 0.0;
std::optional<uint32_t> gDeviceRateToAdopt;

double GetDeviceNominalSampleRate(AudioObjectID device) {
  Float64 rate = 0.0;
  UInt32 size = sizeof(rate);
  if (AudioObjectGetPropertyData(device, &kNominalRateAddress, 0, nullptr, &size, &rate) != noErr) return 0.0;
  return rate;
}

void HandleDeviceRateChangeOnMainThread(void*) {
  if (!gRateCheckPending.exchange(false)) return;

  auto* appHost = IPlugAPPHost::sInstance.get();
  if (!appHost || gRateListenerDevice == kAudioObjectUnknown) return;

  const double deviceRate = GetDeviceNominalSampleRate(gRateListenerDevice);
  if (deviceRate <= 0.0 || std::fabs(deviceRate - gActiveDeviceSampleRate) < 1.0) return;

  DBGMSG("Device sample rate changed externally to %f; restarting audio\n", deviceRate);
  gDeviceRateToAdopt = static_cast<uint32_t>(std::llround(deviceRate));
  appHost->TryToChangeAudio();
}

void ScheduleDeviceRateCheck() {
  if (gRateCheckPending.exchange(true)) return;

  // Like ScheduleAudioRecovery: rate changes are reported off the main thread
  // and often settle in bursts, so hop to the main thread after a short delay.
  const dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, kAudioRecoveryDelayMilliseconds * static_cast<int64_t>(NSEC_PER_MSEC));
  dispatch_after_f(when, dispatch_get_main_queue(), nullptr, HandleDeviceRateChangeOnMainThread);
}

OSStatus DeviceNominalRateChanged(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void*) {
  ScheduleDeviceRateCheck();
  return noErr;
}

void RemoveDeviceRateListener() {
  if (gRateListenerDevice == kAudioObjectUnknown) return;

  AudioObjectRemovePropertyListener(gRateListenerDevice, &kNominalRateAddress, DeviceNominalRateChanged, nullptr);
  gRateListenerDevice = kAudioObjectUnknown;
}

AudioObjectID FindCoreAudioDeviceByRtAudioName(const std::string& rtAudioName) {
  AudioObjectPropertyAddress address = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioPropertyElement};
  UInt32 dataSize = 0;
  if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &dataSize) != noErr) return kAudioObjectUnknown;

  std::vector<AudioObjectID> devices(dataSize / sizeof(AudioObjectID));
  if (devices.empty() || AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &dataSize, devices.data()) != noErr)
    return kAudioObjectUnknown;

  auto stringProperty = [](AudioObjectID device, AudioObjectPropertySelector selector) -> std::string {
    CFStringRef cfString = nullptr;
    UInt32 size = sizeof(cfString);
    AudioObjectPropertyAddress propertyAddress = {selector, kAudioObjectPropertyScopeGlobal, kAudioPropertyElement};
    if (AudioObjectGetPropertyData(device, &propertyAddress, 0, nullptr, &size, &cfString) != noErr || !cfString) return {};
    char buffer[256] = {};
    CFStringGetCString(cfString, buffer, sizeof(buffer), CFStringGetSystemEncoding());
    CFRelease(cfString);
    return buffer;
  };

  for (AudioObjectID device : devices) {
    // Match the exact "Manufacturer: Name" string RtApiCore::probeDeviceInfo builds.
    if (stringProperty(device, kAudioObjectPropertyManufacturer) + ": " + stringProperty(device, kAudioObjectPropertyName) == rtAudioName)
      return device;
  }

  return kAudioObjectUnknown;
}

void InstallDeviceRateListener(const std::string& rtAudioDeviceName, double streamSampleRate) {
  RemoveDeviceRateListener();

  const AudioObjectID device = FindCoreAudioDeviceByRtAudioName(rtAudioDeviceName);
  if (device == kAudioObjectUnknown) return;

  gActiveDeviceSampleRate = streamSampleRate;

  if (AudioObjectAddPropertyListener(device, &kNominalRateAddress, DeviceNominalRateChanged, nullptr) != noErr) return;
  gRateListenerDevice = device;

  // The device may already be running at a different rate than the stream was
  // opened for (it can change between RtAudio's rate check and now, or revert
  // an unsupported request without RtAudio noticing); verify once immediately.
  const double deviceRate = GetDeviceNominalSampleRate(device);
  if (deviceRate > 0.0 && std::fabs(deviceRate - streamSampleRate) >= 1.0) ScheduleDeviceRateCheck();
}
#endif // OS_MAC

void CopyAudioSettings(IPlugAPPHost::AppState& destination, const IPlugAPPHost::AppState& source) {
  destination.mAudioInDev.Set(source.mAudioInDev.Get());
  destination.mAudioOutDev.Set(source.mAudioOutDev.Get());
  destination.mAudioDriverType = source.mAudioDriverType;
  destination.mAudioSR = source.mAudioSR;
  destination.mBufferSize = source.mBufferSize;
  destination.mAudioInChanL = source.mAudioInChanL;
  destination.mAudioInChanR = source.mAudioInChanR;
  destination.mAudioOutChanL = source.mAudioOutChanL;
  destination.mAudioOutChanR = source.mAudioOutChanR;
}
} // namespace

namespace addivox_standalone {
#if defined OS_WIN
constexpr uint32_t kDeviceWASAPI = 2;
#endif

bool OpenAudioMidiSettingsDialog() {
  auto* appHost = IPlugAPPHost::sInstance.get();
  if (!appHost || !gHWND) return false;

#if defined OS_WIN
  const INT_PTR result = DialogBox(gHINSTANCE, MAKEINTRESOURCE(IDD_DIALOG_PREF), gHWND, IPlugAPPHost::PreferencesDlgProc);
#elif defined OS_MAC
  const INT_PTR result = DialogBox(nullptr, MAKEINTRESOURCE(IDD_DIALOG_PREF), gHWND, IPlugAPPHost::PreferencesDlgProc);
#else
#error NOT IMPLEMENTED
#endif

  if (result == IDOK) appHost->UpdateINI();

  return result != -1;
}
} // namespace addivox_standalone

IPlugAPPHost::IPlugAPPHost() : mIPlug(MakePlug(InstanceInfo{this})) {}

IPlugAPPHost::~IPlugAPPHost() {
  mExiting = true;

  gAudioRecoveryPending = false;
#if defined OS_WIN
  if (gHWND) KillTimer(gHWND, kAudioRecoveryTimerID);
#elif defined OS_MAC
  gRateCheckPending = false;
#endif

  if (gPreservedAudio) mDAC = std::move(gPreservedAudio);

  CloseAudio();

  if (mMidiIn) mMidiIn->cancelCallback();

  if (mMidiOut) mMidiOut->closePort();
}

// static
IPlugAPPHost* IPlugAPPHost::Create() {
  sInstance = std::make_unique<IPlugAPPHost>();
  return sInstance.get();
}

bool IPlugAPPHost::Init() {
  mIPlug->SetHost("standalone", mIPlug->GetPluginVersion(false));

  if (!InitState()) return false;

  TryToChangeAudioDriverType(); // will init RTAudio with an API type based on
                                // gState->mAudioDriverType
  ProbeAudioIO();               // find out what audio IO devs are available and put their IDs
                                // in the global variables gAudioInputDevs / gAudioOutputDevs
  InitMidi();                   // creates RTMidiIn and RTMidiOut objects
  ProbeMidiIO();                // find out what midi IO devs are available and put their names
                                // in the global variables gMidiInputDevs / gMidiOutputDevs
  SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get());
  SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get());

  mIPlug->OnParamReset(kReset);
  mIPlug->OnActivate(true);

  return true;
}

bool IPlugAPPHost::OpenWindow(HWND pParent) {
  if (pParent) SetWindowText(pParent, BUNDLE_NAME);
  const bool opened = mIPlug->OpenWindow(pParent) != nullptr;

#if defined OS_WIN
  if (opened) ScheduleAudioRecovery();
#endif

  return opened;
}

void IPlugAPPHost::CloseWindow() { mIPlug->CloseWindow(); }

bool IPlugAPPHost::InitState() {
#if defined OS_WIN
  char strPath[MAX_PATH_LEN];
  SHGetSpecialFolderPathUTF8(NULL, strPath, MAX_PATH_LEN, CSIDL_LOCAL_APPDATA, FALSE);
  mINIPath.SetFormatted(MAX_PATH_LEN, "%s\\%s\\", strPath, BUNDLE_NAME);
#elif defined OS_MAC
  mINIPath.SetFormatted(MAX_PATH_LEN, "%s/Library/Application Support/%s/", getenv("HOME"), BUNDLE_NAME);
#else
#error NOT IMPLEMENTED
#endif

  struct stat st;

  if (stat(mINIPath.Get(), &st) == 0) // if directory exists
  {
    mINIPath.Append("settings.ini"); // add file name to path

    char buf[STRBUFSZ];

    if (stat(mINIPath.Get(), &st) == 0) // if settings file exists read values into state
    {
      DBGMSG("Reading ini file from %s\n", mINIPath.Get());

      mState.mAudioDriverType = GetPrivateProfileInt("audio", "driver", 0, mINIPath.Get());

      GetPrivateProfileString("audio", "indev", "Built-in Input", buf, STRBUFSZ, mINIPath.Get());
      mState.mAudioInDev.Set(buf);
      GetPrivateProfileString("audio", "outdev", "Built-in Output", buf, STRBUFSZ, mINIPath.Get());
      mState.mAudioOutDev.Set(buf);

      // audio
      mState.mAudioInChanL = GetPrivateProfileInt("audio", "in1", 1, mINIPath.Get()); // 1 is first audio input
      mState.mAudioInChanR = GetPrivateProfileInt("audio", "in2", 2, mINIPath.Get());
      mState.mAudioOutChanL = GetPrivateProfileInt("audio", "out1", 1, mINIPath.Get()); // 1 is first audio output
      mState.mAudioOutChanR = GetPrivateProfileInt("audio", "out2", 2, mINIPath.Get());
      // mState.mAudioInIsMono = GetPrivateProfileInt("audio", "monoinput", 0,
      // mINIPath.Get());

      mState.mBufferSize = GetPrivateProfileInt("audio", "buffer", 512, mINIPath.Get());
      mState.mAudioSR = GetPrivateProfileInt("audio", "sr", 44100, mINIPath.Get());

      // midi
      GetPrivateProfileString("midi", "indev", "no input", buf, STRBUFSZ, mINIPath.Get());
      mState.mMidiInDev.Set(buf);
      GetPrivateProfileString("midi", "outdev", "no output", buf, STRBUFSZ, mINIPath.Get());
      mState.mMidiOutDev.Set(buf);

      mState.mMidiInChan = GetPrivateProfileInt("midi", "inchan", 0, mINIPath.Get());   // 0 is any
      mState.mMidiOutChan = GetPrivateProfileInt("midi", "outchan", 0, mINIPath.Get()); // 1 is first chan
    }

    // if settings file doesn't exist, populate with default values, otherwise
    // overwrite
    UpdateINI();
  } else // folder doesn't exist - make folder and make file
  {
#if defined OS_WIN
    // folder doesn't exist - make folder and make file
    CreateDirectory(mINIPath.Get(), NULL);
    mINIPath.Append("settings.ini");
    UpdateINI(); // will write file if doesn't exist
#elif defined OS_MAC
    mode_t process_mask = umask(0);
    int result_code = mkdir(mINIPath.Get(), S_IRWXU | S_IRWXG | S_IRWXO);
    umask(process_mask);

    if (!result_code) {
      mINIPath.Append("settings.ini");
      UpdateINI(); // will write file if doesn't exist
    } else {
      return false;
    }
#else
#error NOT IMPLEMENTED
#endif
  }

  return true;
}

void IPlugAPPHost::UpdateINI() {
  char buf[STRBUFSZ]; // temp buffer for writing integers to profile strings
  const char* ini = mINIPath.Get();

  sprintf(buf, "%u", mState.mAudioDriverType);
  WritePrivateProfileString("audio", "driver", buf, ini);

  WritePrivateProfileString("audio", "indev", mState.mAudioInDev.Get(), ini);
  WritePrivateProfileString("audio", "outdev", mState.mAudioOutDev.Get(), ini);

  sprintf(buf, "%u", mState.mAudioInChanL);
  WritePrivateProfileString("audio", "in1", buf, ini);
  sprintf(buf, "%u", mState.mAudioInChanR);
  WritePrivateProfileString("audio", "in2", buf, ini);
  sprintf(buf, "%u", mState.mAudioOutChanL);
  WritePrivateProfileString("audio", "out1", buf, ini);
  sprintf(buf, "%u", mState.mAudioOutChanR);
  WritePrivateProfileString("audio", "out2", buf, ini);
  // sprintf(buf, "%u", mState.mAudioInIsMono);
  // WritePrivateProfileString("audio", "monoinput", buf, ini);

  WDL_String str;
  str.SetFormatted(32, "%i", mState.mBufferSize);
  WritePrivateProfileString("audio", "buffer", str.Get(), ini);

  str.SetFormatted(32, "%i", mState.mAudioSR);
  WritePrivateProfileString("audio", "sr", str.Get(), ini);

  WritePrivateProfileString("midi", "indev", mState.mMidiInDev.Get(), ini);
  WritePrivateProfileString("midi", "outdev", mState.mMidiOutDev.Get(), ini);

  sprintf(buf, "%u", mState.mMidiInChan);
  WritePrivateProfileString("midi", "inchan", buf, ini);
  sprintf(buf, "%u", mState.mMidiOutChan);
  WritePrivateProfileString("midi", "outchan", buf, ini);
}

std::string IPlugAPPHost::GetAudioDeviceName(uint32_t deviceID) const {
  auto str = mDAC->getDeviceInfo(deviceID).name;
  std::size_t pos = str.find(':');

  if (pos == std::string::npos) return str;

  // RtAudio builds CoreAudio device names as "Manufacturer: Name". Drop the
  // whitespace along with the prefix: a leading space would end up in
  // settings.ini device names, where anything else writing the natural
  // spelling could never match.
  pos = str.find_first_not_of(" \t", pos + 1);
  return pos == std::string::npos ? std::string() : str.substr(pos);
}

std::optional<uint32_t> IPlugAPPHost::GetAudioDeviceID(const char* deviceNameToTest) const {
  auto deviceIDs = mDAC->getDeviceIds();

  for (auto deviceID : deviceIDs) {
    auto name = GetAudioDeviceName(deviceID);

    if (std::string_view(deviceNameToTest) == name) {
      return deviceID;
    }
  }

  return std::nullopt;
}

int IPlugAPPHost::GetMIDIPortNumber(ERoute direction, const char* nameToTest) const {
  int start = 1;

  auto nameStrView = std::string_view(nameToTest);

  if (direction == ERoute::kInput) {
    if (nameStrView == OFF_TEXT) return 0;

#ifdef OS_MAC
    start = 2;
    if (nameStrView == "virtual input") return 1;
#endif

    for (int i = 0; i < mMidiIn->getPortCount(); i++) {
      if (nameStrView == mMidiIn->getPortName(i).c_str()) return (i + start);
    }
  } else {
    if (nameStrView == OFF_TEXT) return 0;

#ifdef OS_MAC
    start = 2;
    if (nameStrView == "virtual output") return 1;
#endif

    for (int i = 0; i < mMidiOut->getPortCount(); i++) {
      if (nameStrView == mMidiOut->getPortName(i).c_str()) return (i + start);
    }
  }

  return -1;
}

void IPlugAPPHost::ProbeAudioIO() {
  mAudioInputDevIDs.clear();
  mAudioOutputDevIDs.clear();
  mDefaultInputDev.reset();
  mDefaultOutputDev.reset();

  if (!mDAC) return;

  DBGMSG("\nRtAudio Version %s", RtAudio::getVersion().c_str());

  RtAudio::DeviceInfo info;

  auto deviceIDs = mDAC->getDeviceIds();

  for (auto deviceID : deviceIDs) {
    info = mDAC->getDeviceInfo(deviceID);

    if (info.inputChannels > 0) {
      mAudioInputDevIDs.push_back(deviceID);
    }

    if (info.outputChannels > 0) {
      mAudioOutputDevIDs.push_back(deviceID);
    }

    if (info.isDefaultInput) {
      mDefaultInputDev = deviceID;
    }

    if (info.isDefaultOutput) {
      mDefaultOutputDev = deviceID;
    }
  }
}

void IPlugAPPHost::ProbeMidiIO() {
  mMidiInputDevNames.clear();
  mMidiOutputDevNames.clear();

  if (!mMidiIn || !mMidiOut) return;
  else {
    int nInputPorts = mMidiIn->getPortCount();

    mMidiInputDevNames.push_back(OFF_TEXT);

#ifdef OS_MAC
    mMidiInputDevNames.push_back("virtual input");
#endif

    for (int i = 0; i < nInputPorts; i++) {
      mMidiInputDevNames.push_back(mMidiIn->getPortName(i));
    }

    int nOutputPorts = mMidiOut->getPortCount();

    mMidiOutputDevNames.push_back(OFF_TEXT);

#ifdef OS_MAC
    mMidiOutputDevNames.push_back("virtual output");
#endif

    for (int i = 0; i < nOutputPorts; i++) {
      mMidiOutputDevNames.push_back(mMidiOut->getPortName(i));
      // This means the virtual output port wont be added as an input
    }
  }
}

bool IPlugAPPHost::AudioSettingsInStateAreEqual(AppState& os, AppState& ns) {
  // Equal settings are not an equal runtime state if a failed or abandoned
  // driver change left audio stopped. This also makes Cancel restore audio
  // after selecting another driver and then returning to the original choice.
  if (!mDAC || !mDAC->isStreamRunning()) return false;

  if (os.mAudioDriverType != ns.mAudioDriverType) return false;
  if (std::string_view(os.mAudioInDev.Get()) != ns.mAudioInDev.Get()) return false;
  if (std::string_view(os.mAudioOutDev.Get()) != ns.mAudioOutDev.Get()) return false;
  if (os.mAudioSR != ns.mAudioSR) return false;
  if (os.mBufferSize != ns.mBufferSize) return false;
  if (os.mAudioInChanL != ns.mAudioInChanL) return false;
  if (os.mAudioInChanR != ns.mAudioInChanR) return false;
  if (os.mAudioOutChanL != ns.mAudioOutChanL) return false;
  if (os.mAudioOutChanR != ns.mAudioOutChanR) return false;
  //  if (os.mAudioInIsMono != ns.mAudioInIsMono) return false;

  return true;
}

bool IPlugAPPHost::MIDISettingsInStateAreEqual(AppState& os, AppState& ns) {
  if (std::string_view(os.mMidiInDev.Get()) != ns.mMidiInDev.Get()) return false;
  if (std::string_view(os.mMidiOutDev.Get()) != ns.mMidiOutDev.Get()) return false;
  if (os.mMidiInChan != ns.mMidiInChan) return false;
  if (os.mMidiOutChan != ns.mMidiOutChan) return false;

  return true;
}

bool IPlugAPPHost::TryToChangeAudioDriverType() {
  if (mDAC && mDAC->isStreamRunning()) gPreservedAudio = std::move(mDAC);
  else
    mDAC.reset();

  if (gPreservedAudio && gLastWorkingAudioState && mState.mAudioDriverType == gLastWorkingAudioState->mAudioDriverType) {
    mDAC = std::move(gPreservedAudio);
    return true;
  }

  // Skip RtAudio initialization in no-I/O mode or screenshot mode
  if (mNoIO || IsScreenshotMode()) return true;

#if defined OS_WIN
  if (mState.mAudioDriverType == kDeviceASIO) mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_ASIO);
  else if (mState.mAudioDriverType == addivox_standalone::kDeviceWASAPI)
    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_WASAPI);
  else if (mState.mAudioDriverType == kDeviceDS)
    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_DS);
#elif defined OS_MAC
  if (mState.mAudioDriverType == kDeviceCoreAudio) mDAC = std::make_unique<RtAudio>(RtAudio::MACOSX_CORE);
  // else
  // mDAC = std::make_unique<RtAudio>(RtAudio::UNIX_JACK);
#else
#error NOT IMPLEMENTED
#endif

  if (mDAC) {
    mDAC->setErrorCallback(ErrorCallback);
    return true;
  }

  return false;
}

bool IPlugAPPHost::TryToChangeAudio() {
  // Skip audio initialization in no-I/O mode or screenshot mode
  if (mNoIO || IsScreenshotMode()) return true;

#if defined OS_MAC
  // Another application changed the device's sample rate under our running
  // stream (see the rate listener above). Follow the hardware rather than
  // fight over the device: restart at the rate it is actually running.
  if (gDeviceRateToAdopt) {
    mState.mAudioSR = *gDeviceRateToAdopt;
    gDeviceRateToAdopt.reset();
  }
#endif

  // Re-scan the hardware first: devices may have been plugged in or unplugged
  // since the last probe, and the default-device fallback below must not
  // resolve stale device IDs.
  ProbeAudioIO();

  auto tryCurrentAudioSettings = [this]() {
    if (gLastWorkingAudioState && AudioSettingsInStateAreEqual(mState, *gLastWorkingAudioState)) return true;

#if defined OS_WIN
    // ASIO exposes one device for both directions, so use its output name for
    // the input ID as well.
    auto inputID = GetAudioDeviceID(mState.mAudioDriverType == kDeviceASIO ? mState.mAudioOutDev.Get() : mState.mAudioInDev.Get());
#elif defined OS_MAC
    auto inputID = GetAudioDeviceID(mState.mAudioInDev.Get());
#else
#error NOT IMPLEMENTED
#endif
    auto outputID = GetAudioDeviceID(mState.mAudioOutDev.Get());
    bool resetToDefault = false;

    if (!inputID && mDefaultInputDev) {
      resetToDefault = true;
      inputID = mDefaultInputDev;
      const std::string inputName = GetAudioDeviceName(*inputID);
      if (!inputName.empty()) mState.mAudioInDev.Set(inputName.c_str());
    }

    if (!outputID && mDefaultOutputDev) {
      resetToDefault = true;
      outputID = mDefaultOutputDev;
      const std::string outputName = GetAudioDeviceName(*outputID);
      if (!outputName.empty()) mState.mAudioOutDev.Set(outputName.c_str());
    }

    if (!inputID || !outputID) return false;

    if (resetToDefault) DBGMSG("Couldn't find the selected audio device; using the default device\n");
    return InitAudio(*inputID, *outputID, mState.mAudioSR, mState.mBufferSize);
  };

  const AppState requestedState = mState;
  if (tryCurrentAudioSettings()) return true;

  if (gLastWorkingAudioState) {
    CopyAudioSettings(mState, *gLastWorkingAudioState);

    bool restored = false;
    if (TryToChangeAudioDriverType()) {
      ProbeAudioIO();
      restored = tryCurrentAudioSettings();
    }

    if (restored) {
      if (HWND settingsDialog = GetActiveWindow(); settingsDialog && GetDlgItem(settingsDialog, IDC_COMBO_AUDIO_DRIVER)) {
        SendDlgItemMessage(settingsDialog, IDC_COMBO_AUDIO_DRIVER, CB_SETCURSEL, mState.mAudioDriverType, 0);
      }

      MessageBox(gHWND, "The requested audio configuration could not be started. Addivox restored the previous working configuration.", "Audio Settings",
                 MB_OK);
      return false;
    }
  }

  // Keep the last known-good configuration in the settings file even if the
  // device has disappeared and cannot currently be restarted.
  if (gLastWorkingAudioState) CopyAudioSettings(mState, *gLastWorkingAudioState);
  else
    mState = requestedState;

  MessageBox(gHWND, "The requested audio configuration could not be started, and the previous configuration could not be restored.", "Audio Settings",
             MB_OK);
  return false;
}

bool IPlugAPPHost::SelectMIDIDevice(ERoute direction, const char* pPortName) {
  int port = GetMIDIPortNumber(direction, pPortName);

  if (direction == ERoute::kInput) {
    if (port == -1) {
      mState.mMidiInDev.Set(OFF_TEXT);
      UpdateINI();
      port = 0;
    }

    // TODO: send all notes off?
    if (mMidiIn) {
      mMidiIn->closePort();

      if (port == 0) {
        return true;
      }
#if defined OS_WIN
      else {
        mMidiIn->openPort(port - 1);
        return true;
      }
#elif defined OS_MAC
      else if (port == 1) {
        std::string virtualMidiInputName = "To ";
        virtualMidiInputName += BUNDLE_NAME;
        mMidiIn->openVirtualPort(virtualMidiInputName);
        return true;
      } else {
        mMidiIn->openPort(port - 2);
        return true;
      }
#else
#error NOT IMPLEMENTED
#endif
    }
  } else {
    if (port == -1) {
      mState.mMidiOutDev.Set(OFF_TEXT);
      UpdateINI();
      port = 0;
    }

    if (mMidiOut) {
      // TODO: send all notes off?
      mMidiOut->closePort();

      if (port == 0) return true;
#if defined OS_WIN
      else {
        mMidiOut->openPort(port - 1);
        return true;
      }
#elif defined OS_MAC
      else if (port == 1) {
        std::string virtualMidiOutputName = "From ";
        virtualMidiOutputName += BUNDLE_NAME;
        mMidiOut->openVirtualPort(virtualMidiOutputName);
        return true;
      } else {
        mMidiOut->openPort(port - 2);
        return true;
      }
#else
#error NOT IMPLEMENTED
#endif
    }
  }

  return false;
}

void IPlugAPPHost::CloseAudio() {
#if defined OS_MAC
  RemoveDeviceRateListener();
#endif

  if (mDAC && mDAC->isStreamOpen()) {
    if (mDAC->isStreamRunning()) {
      mAudioEnding = true;

      // Wait for the audio callback to fade out and set mAudioDone, but only
      // for a bounded time: if the device has just been unplugged the callback
      // never fires again, and waiting on it forever would freeze the app.
      for (int waitedMs = 0; !mAudioDone && waitedMs < kCloseAudioTimeoutMilliseconds; waitedMs += 10) {
        if (!mDAC->isStreamRunning()) break; // RtAudio closed the stream (device disconnect)
        Sleep(10);
      }

      mDAC->abortStream();
    }

    mDAC->closeStream();
  }
}

bool IPlugAPPHost::InitAudio(uint32_t inID, uint32_t outID, uint32_t sr, uint32_t iovs) {
  if (gPreservedAudio) {
    auto pendingAudio = std::move(mDAC);
    mDAC = std::move(gPreservedAudio);
    CloseAudio();
    mDAC = std::move(pendingAudio);
  }

  CloseAudio();

  RtAudio::StreamParameters iParams, oParams;
  iParams.deviceId = inID;
  iParams.nChannels = GetPlug()->MaxNChannels(ERoute::kInput); // TODO: flexible channel count
  iParams.firstChannel = 0;                                    // TODO: flexible channel count

  oParams.deviceId = outID;
  oParams.nChannels = GetPlug()->MaxNChannels(ERoute::kOutput); // TODO: flexible channel count
  oParams.firstChannel = 0;                                     // TODO: flexible channel count

  mBufferSize = iovs; // mBufferSize may get changed by stream

  DBGMSG("trying to start audio stream @ %i sr, buffer size %i\nindev = "
         "%s\noutdev = %s\ninputs = %i\noutputs = %i\n",
         sr, mBufferSize, GetAudioDeviceName(inID).c_str(), GetAudioDeviceName(outID).c_str(), iParams.nChannels, oParams.nChannels);

  RtAudio::StreamOptions options;
  options.flags = RTAUDIO_NONINTERLEAVED;
  // options.streamName = BUNDLE_NAME; // JACK stream name, not used on other
  // streams

  mSamplesElapsed = 0;
  mSampleRate = static_cast<double>(sr);
  mVecWait = 0;
  mAudioEnding = false;
  mAudioDone = false;

  auto status = mDAC->openStream(&oParams, iParams.nChannels > 0 ? &iParams : nullptr, RTAUDIO_FLOAT64, sr, &mBufferSize, &AudioCallback, this, &options);

  if (status != RtAudioErrorType::RTAUDIO_NO_ERROR) {
    DBGMSG("%s", mDAC->getErrorText().c_str());
    return false;
  }

  // RtAudio/CoreAudio may honor buffer sizes that are smaller than, or not a
  // multiple of, APP_SIGNAL_VECTOR_SIZE. The standalone host must therefore run
  // the plugin at the actual stream buffer size instead of slicing callbacks
  // into fixed 64-sample sub-blocks.
  mIPlug->SetBlockSize(static_cast<int>(mBufferSize));
  mIPlug->SetSampleRate(mSampleRate);
  mIPlug->OnReset();

  mInputBufPtrs.Empty();
  mOutputBufPtrs.Empty();

  for (int i = 0; i < iParams.nChannels; i++) {
    mInputBufPtrs.Add(nullptr); // will be set in callback
  }

  for (int i = 0; i < oParams.nChannels; i++) {
    mOutputBufPtrs.Add(nullptr); // will be set in callback
  }

  IMidiMsg pendingMidiMessage;
  while (mIPlug->mMidiMsgsFromCallback.Pop(pendingMidiMessage)) {}

  if (mDAC->startStream() != RTAUDIO_NO_ERROR) {
    DBGMSG("Error starting stream: %s\n", mDAC->getErrorText().c_str());
    return false;
  }

  mState.mBufferSize = mBufferSize;

  if (HWND settingsDialog = GetActiveWindow(); settingsDialog && GetDlgItem(settingsDialog, IDC_COMBO_AUDIO_BUF_SIZE)) {
    WDL_String bufferSizeText;
    bufferSizeText.SetFormatted(32, "%u", mBufferSize);
    const LRESULT index =
        SendDlgItemMessage(settingsDialog, IDC_COMBO_AUDIO_BUF_SIZE, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(bufferSizeText.Get()));
    if (index != CB_ERR) SendDlgItemMessage(settingsDialog, IDC_COMBO_AUDIO_BUF_SIZE, CB_SETCURSEL, index, 0);
  }

#if defined OS_MAC
  InstallDeviceRateListener(mDAC->getDeviceInfo(outID).name, mSampleRate);
#endif

  mActiveState = mState;
  gLastWorkingAudioState = mState;

  return true;
}

bool IPlugAPPHost::InitMidi() {
  // Skip MIDI initialization in no-I/O mode or screenshot mode
  if (mNoIO || IsScreenshotMode()) return true;

  try {
    mMidiIn = std::make_unique<RtMidiIn>();
  } catch (RtMidiError& error) {
    mMidiIn = nullptr;
    error.printMessage();
    return false;
  }

  try {
    mMidiOut = std::make_unique<RtMidiOut>();
  } catch (RtMidiError& error) {
    mMidiOut = nullptr;
    error.printMessage();
    return false;
  }

  mMidiIn->setCallback(&MIDICallback, this);
  mMidiIn->ignoreTypes(false, true, false);

  return true;
}

void ApplyFades(double* pBuffer, int nChans, int nFrames, bool down) {
  for (int i = 0; i < nChans; i++) {
    double* pIO = pBuffer + (i * nFrames);

    if (down) {
      for (int j = 0; j < nFrames; j++) pIO[j] *= ((double)(nFrames - (j + 1)) / (double)nFrames);
    } else {
      for (int j = 0; j < nFrames; j++) pIO[j] *= ((double)j / (double)nFrames);
    }
  }
}

// static
int IPlugAPPHost::AudioCallback(void* pOutputBuffer, void* pInputBuffer, uint32_t nFrames, double streamTime, RtAudioStreamStatus status, void* pUserData) {
  IPlugAPPHost* _this = (IPlugAPPHost*)pUserData;

  int nins = _this->GetPlug()->MaxNChannels(ERoute::kInput);
  int nouts = _this->GetPlug()->MaxNChannels(ERoute::kOutput);

  double* pInputBufferD = static_cast<double*>(pInputBuffer);
  double* pOutputBufferD = static_cast<double*>(pOutputBuffer);

  bool startWait = _this->mVecWait >= APP_N_VECTOR_WAIT; // wait APP_N_VECTOR_WAIT * iovs before
                                                         // processing audio, to avoid clicks
  bool doFade = _this->mVecWait == APP_N_VECTOR_WAIT || _this->mAudioEnding;

  if (startWait && !_this->mAudioDone) {
    if (doFade && pInputBufferD) ApplyFades(pInputBufferD, nins, nFrames, _this->mAudioEnding);

    for (int c = 0; c < nins; c++) {
      _this->mInputBufPtrs.Set(c, pInputBufferD ? (pInputBufferD + (c * nFrames)) : nullptr);
    }

    for (int c = 0; c < nouts; c++) {
      _this->mOutputBufPtrs.Set(c, pOutputBufferD + (c * nFrames));
    }

    _this->mIPlug->AppProcess(_this->mInputBufPtrs.GetList(), _this->mOutputBufPtrs.GetList(), static_cast<int>(nFrames));
    _this->mSamplesElapsed += nFrames;

    for (int c = 0; c < nouts; c++) {
      double* pOutput = pOutputBufferD + (c * nFrames);
      for (uint32_t i = 0; i < nFrames; i++) pOutput[i] *= APP_MULT;
    }

    if (doFade) ApplyFades(pOutputBufferD, nouts, nFrames, _this->mAudioEnding);

    if (_this->mAudioEnding) _this->mAudioDone = true;
  } else {
    memset(pOutputBufferD, 0, nFrames * nouts * sizeof(double));
  }

  _this->mVecWait = std::min(_this->mVecWait + 1, uint32_t(APP_N_VECTOR_WAIT + 1));

  return 0;
}

// static
void IPlugAPPHost::MIDICallback(double deltatime, std::vector<uint8_t>* pMsg, void* pUserData) {
  IPlugAPPHost* _this = (IPlugAPPHost*)pUserData;

  if (pMsg->size() == 0 || _this->mExiting) return;

  if (pMsg->size() > 3) {
    if (pMsg->size() > MAX_SYSEX_SIZE) {
      DBGMSG("SysEx message exceeds MAX_SYSEX_SIZE\n");
      return;
    }

    SysExData data{0, static_cast<int>(pMsg->size()), pMsg->data()};

    _this->mIPlug->mSysExMsgsFromCallback.Push(data);
    return;
  } else if (pMsg->size()) {
    IMidiMsg msg;
    msg.mStatus = pMsg->at(0);
    pMsg->size() > 1 ? msg.mData1 = pMsg->at(1) : msg.mData1 = 0;
    pMsg->size() > 2 ? msg.mData2 = pMsg->at(2) : msg.mData2 = 0;

    _this->mIPlug->mMidiMsgsFromCallback.Push(msg);
  }
}

// static
void IPlugAPPHost::ErrorCallback(RtAudioErrorType type, const std::string& errorText) {
  if (type == RTAUDIO_DEVICE_DISCONNECT) {
    if (auto* appHost = sInstance.get(); appHost && !appHost->mExiting) {
      gAudioRecoveryPending = true;
      ScheduleAudioRecovery();
    }
  }

  std::cerr << "\nerrorCallback: " << errorText << "\n\n";
}

/*
 ==============================================================================

 Adapted from the iPlug 2 standalone dialog for Addivox.
 Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "../../../iPlug2/IPlug/APP/IPlugAPP_host.h"
#include "config.h"
#include "resource.h"

#include <ctime>

#ifdef OS_WIN
#include "asio.h"
#include "win32_utf8.h"
extern float GetScaleForHWND(HWND hWnd);
#define GET_MENU() GetMenu(gHWND)
extern bool SaveWindowScreenshot(HWND hwnd, const char* path);
#elif defined OS_MAC
#define GET_MENU() SWELL_GetCurrentMenu()
extern "C" bool SaveWindowScreenshot(void* hwnd, const char* path);
#endif

using namespace iplug;

#if !defined NO_IGRAPHICS
#include "IGraphics.h"
using namespace igraphics;
#endif

#define IDT_SCREENSHOT_TIMER 1001

// Keep the shared host signature; Addivox only uses the output device.
void IPlugAPPHost::PopulateSampleRateList(HWND hwndDlg, RtAudio::DeviceInfo*, RtAudio::DeviceInfo* outputDevInfo) {
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_SR, CB_RESETCONTENT, 0, 0);
  WDL_String text;
  for (auto rate : outputDevInfo->sampleRates) {
    text.SetFormatted(32, "%u", rate);
    const LRESULT index = SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_SR, CB_ADDSTRING, 0, (LPARAM)text.Get());
    SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_SR, CB_SETITEMDATA, index, rate);
  }
  text.SetFormatted(32, "%u", mState.mAudioSR);
  const LRESULT index = SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_SR, CB_FINDSTRINGEXACT, -1, (LPARAM)text.Get());
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_SR, CB_SETCURSEL, index, 0);
}

void IPlugAPPHost::PopulateAudioOutputList(HWND hwndDlg, RtAudio::DeviceInfo* info) {
  WDL_String buf;

  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_L, CB_RESETCONTENT, 0, 0);
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_R, CB_RESETCONTENT, 0, 0);

  for (unsigned int channel = 1; channel <= info->outputChannels; ++channel) {
    buf.SetFormatted(20, "%u", channel);
    if (channel < info->outputChannels) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_L, CB_ADDSTRING, 0, (LPARAM)buf.Get());
    SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_R, CB_ADDSTRING, 0, (LPARAM)buf.Get());
  }

  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_L, CB_SETCURSEL, mState.mAudioOutChanL - 1, 0);
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_R, CB_SETCURSEL, mState.mAudioOutChanR - 1, 0);
}

void IPlugAPPHost::PopulateDriverSpecificControls(HWND hwndDlg) {
#ifdef OS_WIN
  Button_Enable(GetDlgItem(hwndDlg, IDC_BUTTON_OS_DEV_SETTINGS), mState.mAudioDriverType == kDeviceASIO);
#endif
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_DEV, CB_RESETCONTENT, 0, 0);
  int selected = 0;
  for (int i = 0; i < mAudioOutputDevIDs.size(); ++i) {
    const std::string name = GetAudioDeviceName(mAudioOutputDevIDs[i]);
    SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_DEV, CB_ADDSTRING, 0, (LPARAM)name.c_str());
    if (name == mState.mAudioOutDev.Get()) selected = i;
  }
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_DEV, CB_SETCURSEL, selected, 0);
  RtAudio::DeviceInfo info;
  if (!mAudioOutputDevIDs.empty()) info = mDAC->getDeviceInfo(mAudioOutputDevIDs[selected]);
  PopulateAudioOutputList(hwndDlg, &info);
  PopulateSampleRateList(hwndDlg, nullptr, &info);
}

void IPlugAPPHost::PopulateAudioDialogs(HWND hwndDlg) {
  PopulateDriverSpecificControls(hwndDlg);

  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_RESETCONTENT, 0, 0);
  for (const auto& size : kBufferSizeOptions) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_ADDSTRING, 0, (LPARAM)size.c_str());

  WDL_String str;
  str.SetFormatted(32, "%i", mState.mBufferSize);

  const LRESULT selected = SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_FINDSTRINGEXACT, -1, (LPARAM)str.Get());
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_SETCURSEL, selected, 0);
}

bool IPlugAPPHost::PopulateMidiDialogs(HWND hwndDlg) {
  if (!mMidiIn || !mMidiOut) return false;

  auto populateDevices = [&](int control, const std::vector<std::string>& names, WDL_String& selectedDevice) {
    for (const auto& name : names) SendDlgItemMessage(hwndDlg, control, CB_ADDSTRING, 0, (LPARAM)name.c_str());

    LRESULT selected = SendDlgItemMessage(hwndDlg, control, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedDevice.Get());
    if (selected == CB_ERR) {
      selectedDevice.Set("off");
      UpdateINI();
      selected = 0;
    }
    SendDlgItemMessage(hwndDlg, control, CB_SETCURSEL, selected, 0);
  };

  populateDevices(IDC_COMBO_MIDI_IN_DEV, mMidiInputDevNames, mState.mMidiInDev);
  populateDevices(IDC_COMBO_MIDI_OUT_DEV, mMidiOutputDevNames, mState.mMidiOutDev);

  SendDlgItemMessage(hwndDlg, IDC_COMBO_MIDI_IN_CHAN, CB_ADDSTRING, 0, (LPARAM) "all");
  SendDlgItemMessage(hwndDlg, IDC_COMBO_MIDI_OUT_CHAN, CB_ADDSTRING, 0, (LPARAM) "all");
  WDL_String channel;
  for (int i = 1; i <= 16; ++i) {
    channel.SetFormatted(20, "%i", i);
    SendDlgItemMessage(hwndDlg, IDC_COMBO_MIDI_IN_CHAN, CB_ADDSTRING, 0, (LPARAM)channel.Get());
    SendDlgItemMessage(hwndDlg, IDC_COMBO_MIDI_OUT_CHAN, CB_ADDSTRING, 0, (LPARAM)channel.Get());
  }
  SendDlgItemMessage(hwndDlg, IDC_COMBO_MIDI_IN_CHAN, CB_SETCURSEL, mState.mMidiInChan, 0);
  SendDlgItemMessage(hwndDlg, IDC_COMBO_MIDI_OUT_CHAN, CB_SETCURSEL, mState.mMidiOutChan, 0);
  return true;
}

void IPlugAPPHost::PopulatePreferencesDialog(HWND hwndDlg) {
#ifdef OS_WIN
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_DRIVER, CB_ADDSTRING, 0, (LPARAM) "DirectSound");
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_DRIVER, CB_ADDSTRING, 0, (LPARAM) "ASIO");
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_DRIVER, CB_ADDSTRING, 0, (LPARAM) "WASAPI");
#elif defined OS_MAC
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_DRIVER, CB_ADDSTRING, 0, (LPARAM) "CoreAudio");
#else
#error NOT IMPLEMENTED
#endif
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_DRIVER, CB_SETCURSEL, mState.mAudioDriverType, 0);
  PopulateAudioDialogs(hwndDlg);
  PopulateMidiDialogs(hwndDlg);
}

WDL_DLGRET IPlugAPPHost::PreferencesDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  IPlugAPPHost* _this = sInstance.get();
  AppState& mState = _this->mState;
  AppState& mTempState = _this->mTempState;
  AppState& mActiveState = _this->mActiveState;

  auto selectedIndex = [&](int control) { return (int)SendDlgItemMessage(hwndDlg, control, CB_GETCURSEL, 0, 0); };
  auto readSelectedDevice = [&](WDL_String& str, int item) {
    const int idx = selectedIndex(item);
    const long len = (long)SendDlgItemMessage(hwndDlg, item, CB_GETLBTEXTLEN, idx, 0) + 1;
    if (len <= 0) return;
    std::string tempString(len, '\0');
    SendDlgItemMessage(hwndDlg, item, CB_GETLBTEXT, idx, (LPARAM)tempString.data());
    str.Set(tempString.c_str());
  };

  switch (uMsg) {
  case WM_INITDIALOG:
#ifdef OS_WIN
    WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO_AUDIO_OUT_DEV));
    WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO_MIDI_IN_DEV));
    WDL_UTF8_HookComboBox(GetDlgItem(hwndDlg, IDC_COMBO_MIDI_OUT_DEV));
#endif
    _this->ProbeAudioIO();
    _this->ProbeMidiIO();
    _this->PopulatePreferencesDialog(hwndDlg);
    mTempState = mState;

    return TRUE;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDOK:
      if (mActiveState != mState) _this->TryToChangeAudio();

      EndDialog(hwndDlg, IDOK); // MainDlgProc saves the settings.
      break;
    case IDAPPLY: _this->TryToChangeAudio(); break;
    case IDCANCEL:
      EndDialog(hwndDlg, IDCANCEL);

      // if state has been changed reset to previous state, INI file won't be changed
      if (!_this->AudioSettingsInStateAreEqual(mState, mTempState) || !_this->MIDISettingsInStateAreEqual(mState, mTempState)) {
        mState = mTempState;

        _this->TryToChangeAudioDriverType();
        _this->ProbeAudioIO();
        _this->TryToChangeAudio();
      }

      break;

    case IDC_COMBO_AUDIO_DRIVER:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        const int driver = selectedIndex(IDC_COMBO_AUDIO_DRIVER);
        if (driver != mState.mAudioDriverType) {
          mState.mAudioDriverType = driver;

          _this->TryToChangeAudioDriverType();
          _this->ProbeAudioIO();

          if (!_this->mAudioOutputDevIDs.empty()) mState.mAudioOutDev.Set(_this->GetAudioDeviceName(_this->mAudioOutputDevIDs[0]).c_str());

          mState.mAudioOutChanL = 1;
          mState.mAudioOutChanR = 2;

          _this->PopulateAudioDialogs(hwndDlg);
        }
      }
      break;

    case IDC_COMBO_AUDIO_OUT_DEV:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        readSelectedDevice(mState.mAudioOutDev, IDC_COMBO_AUDIO_OUT_DEV);

        mState.mAudioOutChanL = 1;
        mState.mAudioOutChanR = 2;

        _this->PopulateDriverSpecificControls(hwndDlg);
      }
      break;

    case IDC_COMBO_AUDIO_OUT_L:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        mState.mAudioOutChanL = selectedIndex(IDC_COMBO_AUDIO_OUT_L) + 1;

        mState.mAudioOutChanR = mState.mAudioOutChanL + 1;
        SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_R, CB_SETCURSEL, mState.mAudioOutChanR - 1, 0);
      }
      break;

    case IDC_COMBO_AUDIO_OUT_R:
      if (HIWORD(wParam) == CBN_SELCHANGE) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_R, CB_SETCURSEL, mState.mAudioOutChanR - 1, 0);
      mState.mAudioOutChanR = selectedIndex(IDC_COMBO_AUDIO_OUT_R);
      break;

    case IDC_COMBO_AUDIO_BUF_SIZE:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        mState.mBufferSize = atoi(kBufferSizeOptions[selectedIndex(IDC_COMBO_AUDIO_BUF_SIZE)].c_str());
      }
      break;
    case IDC_COMBO_AUDIO_SR:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        int idx = selectedIndex(IDC_COMBO_AUDIO_SR);
        mState.mAudioSR = (uint32_t)SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_SR, CB_GETITEMDATA, idx, 0);
      }
      break;

    case IDC_BUTTON_OS_DEV_SETTINGS:
      if (HIWORD(wParam) == BN_CLICKED) {
#ifdef OS_WIN
        if (mState.mAudioDriverType == kDeviceASIO && _this->mDAC->isStreamRunning())
          ASIOControlPanel();
#elif defined OS_MAC
        if (SWELL_GetOSXVersion() >= 0x1200) {
          system("open \"/System/Applications/Utilities/Audio MIDI Setup.app\"");
        } else {
          system("open \"/Applications/Utilities/Audio MIDI Setup.app\"");
        }
#else
#error NOT IMPLEMENTED
#endif
      }
      break;

    case IDC_COMBO_MIDI_IN_DEV:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        readSelectedDevice(mState.mMidiInDev, IDC_COMBO_MIDI_IN_DEV);
        _this->SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get());
      }
      break;

    case IDC_COMBO_MIDI_OUT_DEV:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        readSelectedDevice(mState.mMidiOutDev, IDC_COMBO_MIDI_OUT_DEV);
        _this->SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get());
      }
      break;

    case IDC_COMBO_MIDI_IN_CHAN:
      if (HIWORD(wParam) == CBN_SELCHANGE) mState.mMidiInChan = selectedIndex(IDC_COMBO_MIDI_IN_CHAN);
      break;

    case IDC_COMBO_MIDI_OUT_CHAN:
      if (HIWORD(wParam) == CBN_SELCHANGE) mState.mMidiOutChan = selectedIndex(IDC_COMBO_MIDI_OUT_CHAN);
      break;

    default: break;
    }
    break;
  default: return FALSE;
  }
  return TRUE;
}

static POINT WindowFrameSize(HWND hwnd) {
  RECT client, window;
  GetClientRect(hwnd, &client);
  GetWindowRect(hwnd, &window);
  return {(window.right - window.left) - client.right, (window.bottom - window.top) - client.bottom};
}

static void ClientResize(HWND hwnd, int width, int height) {
  const int x = GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2;
  const int y = GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2;
  const POINT frame = WindowFrameSize(hwnd);
  SetWindowPos(hwnd, 0, x, y, width + frame.x, height + frame.y, 0);
}

#ifdef ID_SCREENSHOT
static void SaveScreenshot(HWND hwndDlg) {
  WDL_String path;
  char timestamp[32];
  time_t now = time(nullptr);
  strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));

#ifdef OS_WIN
  char tempPath[MAX_PATH];
  GetTempPathA(MAX_PATH, tempPath);
  path.SetFormatted(512, "%s%s_screenshot_%s.png", tempPath, PLUG_NAME, timestamp);
#elif defined OS_MAC
  const char* tmpDir = getenv("TMPDIR");
  path.SetFormatted(512, "%s%s_screenshot_%s.png", tmpDir ? tmpDir : "/tmp/", PLUG_NAME, timestamp);
#endif

  if (SaveWindowScreenshot(gHWND, path.Get())) {
    WDL_String msg;
    msg.SetFormatted(512, "Screenshot saved to:\n%s\n\nOpen it?", path.Get());
    int result = MessageBox(hwndDlg, msg.Get(), "Screenshot Saved", MB_YESNO);

    if (result == IDYES) {
#ifdef OS_WIN
      ShellExecuteA(NULL, "open", path.Get(), NULL, NULL, SW_SHOWNORMAL);
#elif defined OS_MAC
      WDL_String cmd;
      cmd.SetFormatted(1024, "open \"%s\"", path.Get());
      system(cmd.Get());
#endif
    }
  } else {
    MessageBox(hwndDlg, "Failed to save screenshot", "Error", MB_OK);
  }
}
#endif

WDL_DLGRET IPlugAPPHost::MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  IPlugAPPHost* pAppHost = IPlugAPPHost::sInstance.get();

  switch (uMsg) {
  case WM_INITDIALOG: {
    gHWND = hwndDlg;
    IPlugAPP* pPlug = pAppHost->GetPlug();

    if (!pAppHost->OpenWindow(gHWND)) {
      DBGMSG("couldn't attach gui\n");
    }

    ClientResize(hwndDlg, pPlug->GetEditorWidth(), pPlug->GetEditorHeight());

    ShowWindow(hwndDlg, SW_SHOW);

    // Let the UI initialize before capturing it.
    if (pAppHost->IsScreenshotMode()) {
      SetTimer(hwndDlg, IDT_SCREENSHOT_TIMER, 500, nullptr);
    }

    return 1;
  }
  case WM_TIMER: {
    if (wParam == IDT_SCREENSHOT_TIMER) {
      KillTimer(hwndDlg, IDT_SCREENSHOT_TIMER);

      SaveWindowScreenshot(gHWND, pAppHost->GetScreenshotPath());

      DestroyWindow(hwndDlg);
      return 0;
    }
    break;
  }
  case WM_DESTROY:
    pAppHost->CloseWindow();
    gHWND = NULL;
    IPlugAPPHost::sInstance = nullptr;

#ifdef OS_WIN
    PostQuitMessage(0);
#else
    SWELL_PostQuitMessage(hwndDlg);
#endif

    return 0;
  case WM_CLOSE: DestroyWindow(hwndDlg); return 0;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case ID_QUIT: {
      DestroyWindow(hwndDlg);
      return 0;
    }
    case ID_ABOUT: {
      IPlugAPP* pPlug = pAppHost->GetPlug();

      if (!pPlug->OnHostRequestingAboutBox()) MessageBox(hwndDlg, PLUG_COPYRIGHT_STR "\nBuilt on " __DATE__, PLUG_NAME, MB_OK);

      return 0;
    }
    case ID_HELP: {
      IPlugAPP* pPlug = pAppHost->GetPlug();

      if (!pPlug->OnHostRequestingProductHelp()) MessageBox(hwndDlg, "See the manual", PLUG_NAME, MB_OK);
      return 0;
    }
    case ID_PREFERENCES: {
      INT_PTR ret = DialogBox(gHINSTANCE, MAKEINTRESOURCE(IDD_DIALOG_PREF), hwndDlg, IPlugAPPHost::PreferencesDlgProc);

      if (ret == IDOK) pAppHost->UpdateINI();

      return 0;
    }
#ifdef ID_SCREENSHOT
    case ID_SCREENSHOT: SaveScreenshot(hwndDlg); return 0;
#endif
#if defined _DEBUG && !defined NO_IGRAPHICS
    case ID_LIVE_EDIT:
    case ID_SHOW_DRAWN:
    case ID_SHOW_BOUNDS:
    case ID_SHOW_FPS: {
      auto* editor = dynamic_cast<IGEditorDelegate*>(pAppHost->GetPlug());
      IGraphics* graphics = editor ? editor->GetUI() : nullptr;
      if (!graphics) return 0;

      bool enabled = false;
      switch (LOWORD(wParam)) {
      case ID_LIVE_EDIT:
        enabled = graphics->LiveEditEnabled();
        graphics->EnableLiveEdit(!enabled);
        break;
      case ID_SHOW_DRAWN:
        enabled = graphics->ShowAreaDrawnEnabled();
        graphics->ShowAreaDrawn(!enabled);
        break;
      case ID_SHOW_BOUNDS:
        enabled = graphics->ShowControlBoundsEnabled();
        graphics->ShowControlBounds(!enabled);
        break;
      case ID_SHOW_FPS:
        enabled = graphics->ShowingFPSDisplay();
        graphics->ShowFPSDisplay(!enabled);
        break;
      }
      CheckMenuItem(GET_MENU(), LOWORD(wParam), (MF_BYCOMMAND | enabled) ? MF_UNCHECKED : MF_CHECKED);
      return 0;
    }
#endif
    }
    return 0;
  case WM_GETMINMAXINFO: {
    if (!pAppHost) return 1;

    IPlugAPP* pPlug = pAppHost->GetPlug();

    MINMAXINFO* mmi = (MINMAXINFO*)lParam;
    mmi->ptMinTrackSize.x = pPlug->GetMinWidth();
    mmi->ptMinTrackSize.y = pPlug->GetMinHeight();
    mmi->ptMaxTrackSize.x = pPlug->GetMaxWidth();
    mmi->ptMaxTrackSize.y = pPlug->GetMaxHeight();

#ifdef OS_WIN
    float scale = GetScaleForHWND(hwndDlg);
    mmi->ptMinTrackSize.x = static_cast<LONG>(static_cast<float>(mmi->ptMinTrackSize.x) * scale);
    mmi->ptMinTrackSize.y = static_cast<LONG>(static_cast<float>(mmi->ptMinTrackSize.y) * scale);
    mmi->ptMaxTrackSize.x = static_cast<LONG>(static_cast<float>(mmi->ptMaxTrackSize.x) * scale);
    mmi->ptMaxTrackSize.y = static_cast<LONG>(static_cast<float>(mmi->ptMaxTrackSize.y) * scale);
#endif

    return 0;
  }
#ifdef OS_WIN
  case WM_DPICHANGED: {
    RECT* rect = (RECT*)lParam;
    float scale = GetScaleForHWND(hwndDlg);

    const POINT frame = WindowFrameSize(hwndDlg);

#ifndef NO_IGRAPHICS
    IGEditorDelegate* pPlug = dynamic_cast<IGEditorDelegate*>(pAppHost->GetPlug());

    if (pPlug) {
      IGraphics* pGraphics = pPlug->GetUI();

      if (pGraphics) {
        pGraphics->SetScreenScale(scale);
      }
    }
#else
    IEditorDelegate* pPlug = dynamic_cast<IEditorDelegate*>(pAppHost->GetPlug());
#endif

    int w = pPlug->GetEditorWidth();
    int h = pPlug->GetEditorHeight();

    SetWindowPos(hwndDlg, 0, rect->left, rect->top, w + frame.x, h + frame.y, 0);

    return 0;
  }
#endif
  case WM_SIZE: {
    IPlugAPP* pPlug = pAppHost->GetPlug();

    switch (LOWORD(wParam)) {
    case SIZE_RESTORED:
    case SIZE_MAXIMIZED: {
      if (pPlug->GetHostResizeEnabled()) {
        RECT r;
        GetClientRect(hwndDlg, &r);
        pPlug->OnParentWindowResize(static_cast<int>(r.right), static_cast<int>(r.bottom));
      }
      return 1;
    }
    default: return 0;
    }
  }
  }
  return 0;
}

/*
 ==============================================================================

 This file adapts the iPlug2 standalone dialog for Addivox without modifying the
 iPlug2 submodule.

 ==============================================================================
*/

#include "../../../iPlug2/IPlug/APP/IPlugAPP_host.h"
#include "audio_driver_ids.h"
#include "resource.h"

#include <cstring>

namespace {
LRESULT SendAddivoxDlgItemMessage(HWND dialog, int controlID, UINT message, WPARAM wParam, LPARAM lParam) {
  if (controlID == IDC_COMBO_AUDIO_DRIVER && message == CB_ADDSTRING && std::strcmp(reinterpret_cast<const char*>(lParam), "ASIO") == 0) {
    const LRESULT result = SendDlgItemMessage(dialog, controlID, message, wParam, lParam);
    if (result == iplug::kDeviceASIO && SendDlgItemMessage(dialog, controlID, CB_GETCOUNT, 0, 0) == addivox_standalone::kDeviceWASAPI) {
      SendDlgItemMessage(dialog, controlID, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("WASAPI"));
    }
    return result;
  }

  if (controlID == IDC_COMBO_AUDIO_BUF_SIZE && message == CB_ADDSTRING &&
      SendDlgItemMessage(dialog, controlID, CB_GETCOUNT, 0, 0) >= iplug::kNumBufferSizeOptions &&
      std::strcmp(reinterpret_cast<const char*>(lParam), iplug::kBufferSizeOptions[0].c_str()) == 0) {
    SendDlgItemMessage(dialog, controlID, CB_RESETCONTENT, 0, 0);
  }

  return SendDlgItemMessage(dialog, controlID, message, wParam, lParam);
}
} // namespace

#ifdef SendDlgItemMessage
#undef SendDlgItemMessage
#endif
#define SendDlgItemMessage SendAddivoxDlgItemMessage
#include "../../../iPlug2/IPlug/APP/IPlugAPP_dialog.cpp"
#undef SendDlgItemMessage

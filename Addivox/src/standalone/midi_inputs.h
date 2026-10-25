#pragma once

#include "RtMidi.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if defined(OS_MAC)
#include <CoreMIDI/CoreMIDI.h>
#elif defined(OS_WIN)
#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#endif

namespace addivox_standalone {

class MidiInputs {
public:
  MidiInputs(RtMidiIn& probe, RtMidiIn::RtMidiCallback callback, void* context) : mProbe(probe), mCallback(callback), mContext(context) {}
  ~MidiInputs() { mConnections.clear(); }

  bool Refresh(const std::string& selection) {
    mSelection = selection;
    return Refresh();
  }

  bool Refresh() {
    if (mRefreshing) return false; // CoreMIDI enumeration can dispatch another timer event.
    mRefreshing = true;
    struct ResetFlag {
      bool& flag;
      ~ResetFlag() { flag = false; }
    } reset{mRefreshing};
    const std::string selection = mSelection;

    std::vector<Port> ports;
    try {
      const unsigned count = mProbe.getPortCount();
      for (unsigned i = 0; i < count; ++i) {
        const auto name = mProbe.getPortName(i);
        const auto identity = Identity(i, name);
        if (!name.empty() && identity != mIgnoredSource) ports.push_back({name, identity, i});
      }
    } catch (const RtMidiError&) {
      return false;
    }

    const auto previousNames = mNames;
    mNames = {"all", "off"};
#if defined(OS_MAC)
    mNames.push_back("virtual input");
    if (selection == "all" || selection == "virtual input") ports.push_back({"virtual input", L"virtual input", 0});
#endif
    for (const auto& port : ports) {
      if (port.name != "virtual input") mNames.push_back(port.name);
    }

    auto selected = std::find_if(ports.begin(), ports.end(), [&](const Port& port) { return port.name == selection; });
#if defined(OS_WIN) && !defined(RTMIDI_DO_NOT_ENSURE_UNIQUE_PORTNAMES)
    if (selected == ports.end()) {
      const auto name = WithoutPortNumber(selection);
      if (std::count_if(ports.begin(), ports.end(), [&](const Port& port) { return WithoutPortNumber(port.name) == name; }) == 1)
        selected = std::find_if(ports.begin(), ports.end(), [&](const Port& port) { return WithoutPortNumber(port.name) == name; });
    }
#endif
    mSelectedName = selected == ports.end() ? selection : selected->name;
    if (selection != "all") {
      if (selected == ports.end() || selection == "off") ports.clear();
      else
        ports = {*selected};
    }

    const auto previousCount = mConnections.size();
    mConnections.erase(std::remove_if(mConnections.begin(), mConnections.end(),
                                      [&](const Connection& connection) {
                                        return std::none_of(ports.begin(), ports.end(), [&](const Port& port) { return port.identity == connection.identity; });
                                      }),
                       mConnections.end());
    if (mConnections.size() != previousCount) {
      std::vector<unsigned char> notesOff{0xB0, 123, 0};
      Receive(0, &notesOff, this);
    }

    for (const auto& port : ports) {
      if (std::any_of(mConnections.begin(), mConnections.end(), [&](const Connection& connection) { return connection.identity == port.identity; })) continue;
      try {
        auto input = std::make_unique<RtMidiIn>();
        input->setCallback(Receive, this);
        input->ignoreTypes(false, true, false);
        if (port.name == "virtual input") input->openVirtualPort("To " BUNDLE_NAME);
        else
          input->openPort(port.index);
        if (port.name == "virtual input" || input->isPortOpen()) mConnections.push_back({port.identity, std::move(input)});
      } catch (const RtMidiError&) {
        // A busy or disappearing device must not prevent other inputs from working.
      }
    }

    const auto previousLabel = mSelectionLabel;
    mSelectionLabel = mSelectedName;
    if (selection != "all" && selection != "off") {
      if (ports.empty()) mSelectionLabel += " (disconnected)";
      else if (mConnections.empty())
        mSelectionLabel += " (unavailable)";
    }
    return mNames != previousNames || mSelectionLabel != previousLabel;
  }

  const std::vector<std::string>& Names() const { return mNames; }
  const std::string& SelectedName() const { return mSelectedName; }
  const std::string& SelectionLabel() const { return mSelectionLabel; }

#if defined(OS_MAC)
  void OpenVirtualOutput(RtMidiOut& output) {
    std::vector<MIDIEndpointRef> previous;
    for (unsigned i = 0; i < MIDIGetNumberOfSources(); ++i) previous.push_back(MIDIGetSource(i));
    output.openVirtualPort("From " BUNDLE_NAME);
    for (unsigned i = 0; i < MIDIGetNumberOfSources(); ++i) {
      const auto source = MIDIGetSource(i);
      if (std::find(previous.begin(), previous.end(), source) != previous.end()) continue;
      CFStringRef name = nullptr;
      if (MIDIObjectGetStringProperty(source, kMIDIPropertyName, &name) == noErr && name) {
        if (CFEqual(name, CFSTR("From " BUNDLE_NAME))) mIgnoredSource = std::to_wstring(source);
        CFRelease(name);
      }
    }
  }
#endif

private:
  struct Port {
    std::string name;
    std::wstring identity;
    unsigned index;
  };
  struct Connection {
    std::wstring identity;
    std::unique_ptr<RtMidiIn> input;
  };

  static std::string WithoutPortNumber(const std::string& name) {
    const auto space = name.rfind(' ');
    if (space != std::string::npos && space + 1 < name.size() && name.find_first_not_of("0123456789", space + 1) == std::string::npos)
      return name.substr(0, space);
    return name;
  }

  static std::wstring Identity(unsigned index, const std::string& name) {
#if defined(OS_MAC)
    return std::to_wstring(MIDIGetSource(index));
#elif defined(OS_WIN)
    ULONG bytes = 0;
    const auto device = reinterpret_cast<HMIDIIN>(static_cast<UINT_PTR>(index));
    if (midiInMessage(device, DRV_QUERYDEVICEINTERFACESIZE, reinterpret_cast<DWORD_PTR>(&bytes), 0) == MMSYSERR_NOERROR && bytes) {
      std::vector<wchar_t> path(bytes / sizeof(wchar_t) + 1, L'\0');
      if (midiInMessage(device, DRV_QUERYDEVICEINTERFACE, reinterpret_cast<DWORD_PTR>(path.data()), bytes) == MMSYSERR_NOERROR && path[0])
        return std::wstring(path.data());
    }
#endif
    return std::wstring(name.begin(), name.end());
  }

  static void Receive(double time, std::vector<unsigned char>* message, void* context) {
    auto& inputs = *static_cast<MidiInputs*>(context);
    // Serialize producers only; the audio thread consumes the queue without locking.
    std::lock_guard<std::mutex> lock(inputs.mCallbackMutex);
    inputs.mCallback(time, message, inputs.mContext);
  }

  RtMidiIn& mProbe;
  RtMidiIn::RtMidiCallback mCallback;
  void* mContext;
  std::mutex mCallbackMutex;
  std::vector<Connection> mConnections;
  std::vector<std::string> mNames;
  std::string mSelection = "off";
  std::string mSelectedName;
  std::string mSelectionLabel;
  std::wstring mIgnoredSource;
  bool mRefreshing = false;
};

} // namespace addivox_standalone

#pragma once

#include <atomic>
#include <string>

#if defined(OS_MAC)
#include <CoreAudio/CoreAudio.h>
#elif defined(OS_WIN)
#include <mmdeviceapi.h>
#endif

namespace addivox_standalone {

class SystemAudioOutput {
public:
  explicit SystemAudioOutput(void (*changed)()) : mChanged(changed) {
#if defined(OS_MAC)
    for (const auto& address : kAddresses) AudioObjectAddPropertyListener(kAudioObjectSystemObject, &address, Changed, this);
#elif defined(OS_WIN)
    mInitializedCOM = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&mEnumerator)))) {
      mListener = new Listener(changed);
      mEnumerator->RegisterEndpointNotificationCallback(mListener);
    }
#endif
  }

  ~SystemAudioOutput() {
#if defined(OS_MAC)
    for (const auto& address : kAddresses) AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &address, Changed, this);
#elif defined(OS_WIN)
    if (mEnumerator) {
      mEnumerator->UnregisterEndpointNotificationCallback(mListener);
      mListener->Release();
      mEnumerator->Release();
    }
    if (mInitializedCOM) CoUninitialize();
#endif
  }

  SystemAudioOutput(const SystemAudioOutput&) = delete;
  SystemAudioOutput& operator=(const SystemAudioOutput&) = delete;

  std::wstring DefaultEndpoint() const {
#if defined(OS_MAC)
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &kAddresses[0], 0, nullptr, &size, &device) == noErr && device) return std::to_wstring(device);
#elif defined(OS_WIN)
    IMMDevice* device = nullptr;
    if (mEnumerator && SUCCEEDED(mEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) {
      LPWSTR id = nullptr;
      const HRESULT result = device->GetId(&id);
      const std::wstring identity = SUCCEEDED(result) && id ? id : L"";
      CoTaskMemFree(id);
      device->Release();
      return identity;
    }
#endif
    return {};
  }

private:
  void (*mChanged)();
#if defined(OS_MAC)
  inline static constexpr AudioObjectPropertyAddress kAddresses[] = {{kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, 0},
                                                                     {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, 0}};

  static OSStatus Changed(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void* context) {
    static_cast<SystemAudioOutput*>(context)->mChanged();
    return noErr;
  }
#elif defined(OS_WIN)
  class Listener final : public IMMNotificationClient {
  public:
    explicit Listener(void (*changed)()) : mChanged(changed) {}
    ULONG STDMETHODCALLTYPE AddRef() override { return ++mReferences; }
    ULONG STDMETHODCALLTYPE Release() override {
      const ULONG references = --mReferences;
      if (!references) delete this;
      return references;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override {
      if (!object) return E_POINTER;
      *object = nullptr;
      if (id != __uuidof(IUnknown) && id != __uuidof(IMMNotificationClient)) return E_NOINTERFACE;
      *object = static_cast<IMMNotificationClient*>(this);
      AddRef();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
      if (flow == eRender && role == eConsole) mChanged();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
      mChanged();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
      mChanged();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
      mChanged();
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

  private:
    std::atomic<ULONG> mReferences{1};
    void (*mChanged)();
  };

  IMMDeviceEnumerator* mEnumerator = nullptr;
  Listener* mListener = nullptr;
  bool mInitializedCOM = false;
#endif
};

} // namespace addivox_standalone

#ifndef WX_AUDIO_INTERNAL_XAUDIO2_H_
#define WX_AUDIO_INTERNAL_XAUDIO2_H_

#include "wx/audio/audio.h"
#include <memory>
#include <vector>

// Windows headers for IMMNotificationClient
#include <windows.h>
#include <mmdeviceapi.h>

namespace audio {
namespace internal {

// XAudio2_Output interface for device change notifications
class XAudio2_Output {
public:
    virtual void device_change() = 0;
    virtual ~XAudio2_Output() = default;
};

// Device Notifier for hotplug support
class XAudio2_Device_Notifier : public IMMNotificationClient {
    volatile LONG registered;
    IMMDeviceEnumerator* pEnumerator;
    std::wstring last_device;
    CRITICAL_SECTION lock;
    std::vector<XAudio2_Output*> instances;

public:
    XAudio2_Device_Notifier();
    ~XAudio2_Device_Notifier();

    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface) override;

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

    void do_register(XAudio2_Output* p_instance);
    void do_unregister(XAudio2_Output* p_instance);
};

// Global notifier instance declaration
extern XAudio2_Device_Notifier g_notifier;

// Returns the set of XAudio2 devices.
std::vector<AudioDevice> GetXAudio2Devices();

// Creates an XAudio2 sound driver.
std::unique_ptr<SoundDriver> CreateXAudio2Driver();

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_XAUDIO2_H_
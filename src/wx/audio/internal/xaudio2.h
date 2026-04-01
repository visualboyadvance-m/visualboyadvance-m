#ifndef WX_AUDIO_INTERNAL_XAUDIO2_H_
#define WX_AUDIO_INTERNAL_XAUDIO2_H_

#if !defined(__WXMSW__)
#error "This file should only be included on Windows"
#endif

#if !defined(VBAM_ENABLE_XAUDIO2)
#error "This file should only be compiled if XAudio2 is enabled"
#endif

#include "wx/audio/audio.h"
#include <memory>
#include <vector>

// Windows headers for IMMNotificationClient
#include <windows.h>
#include <mmdeviceapi.h>

namespace audio {
namespace internal {

// XAudio2_Output interface for device change notifications.
// signal_device_change() is called from the MMDevice notification thread and
// must only set an atomic flag - no wx calls, no locking, no allocation.
class XAudio2_Output {
public:
    virtual void signal_device_change() = 0;
    virtual ~XAudio2_Output() = default;
};

// Device Notifier for hotplug support.
// Callbacks fire on the MMDevice COM notification thread, which is NOT the wx
// main thread. No wx API calls are permitted inside any callback.
class XAudio2_Device_Notifier : public IMMNotificationClient {
    LONG _cRef;
    IMMDeviceEnumerator* _pEnumerator;
    std::wstring last_device;

    static const int MAX_INSTANCES = 8;
    XAudio2_Output* instances[MAX_INSTANCES];
    volatile LONG instance_count;

    // Protects instances[] and instance_count for register/unregister.
    // The notification callbacks only read instance_count and instances[],
    // which is safe because do_unregister nulls the slot before decrementing
    // the count, so a concurrent callback iteration at worst skips a null.
    CRITICAL_SECTION instances_cs;

public:
    XAudio2_Device_Notifier();
    ~XAudio2_Device_Notifier();

    // IUnknown methods
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface) override;

    // IMMNotificationClient methods - all called on the COM notification thread
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

    void do_register(XAudio2_Output* p_instance);
    void do_unregister(XAudio2_Output* p_instance, bool is_destructing = false);
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

#if !defined(VBAM_ENABLE_XAUDIO2)
#error "This file should only be compiled if XAudio2 is enabled"
#endif

#include "wx/audio/internal/xaudio2.h"
#include "wx/audio/internal/xaudio2_7.h"
#include "wx/audio/internal/xaudio2_9.h"

#include <windows.h>
#include <wx/log.h>
#include <wx/translation.h>

#include <mmdeviceapi.h>
#include <objbase.h>

// Include for log() function
#include "core/base/system.h"

namespace audio {
namespace internal {

namespace {

enum class XAudio2Version {
    None,
    XAudio2_7,
    XAudio2_8,
    XAudio2_9
};

// Device Notifier for hotplug support
class XAudio2_Device_Notifier : public IMMNotificationClient {
    volatile LONG registered;
    IMMDeviceEnumerator* pEnumerator;
    std::wstring last_device;
    CRITICAL_SECTION lock;
    std::vector<class XAudio2_Output*> instances;

public:
    XAudio2_Device_Notifier() : registered(0), pEnumerator(nullptr) { 
        InitializeCriticalSection(&lock); 
    }
    
    ~XAudio2_Device_Notifier() { 
        DeleteCriticalSection(&lock); 
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface) override {
        if (IID_IUnknown == riid) {
            *ppvInterface = (IUnknown*)this;
        } else if (__uuidof(IMMNotificationClient) == riid) {
            *ppvInterface = (IMMNotificationClient*)this;
        } else {
            *ppvInterface = NULL;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole, LPCWSTR pwstrDeviceId) override {
        if (flow == eRender) {
            EnterCriticalSection(&lock);
            last_device = pwstrDeviceId ? pwstrDeviceId : L"";
            for (auto instance : instances) {
                if (instance) {
                    instance->device_change();
                }
            }
            LeaveCriticalSection(&lock);
            log("XAudio2: Default audio device changed\n");
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override {
        EnterCriticalSection(&lock);
        log("XAudio2: Audio device removed\n");
        // Trigger device change on removal as well
        for (auto instance : instances) {
            if (instance) {
                instance->device_change();
            }
        }
        LeaveCriticalSection(&lock);
        return S_OK;
    }
    
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

    void do_register(class XAudio2_Output* p_instance) {
        EnterCriticalSection(&lock);
        
        if (InterlockedIncrement(&registered) == 1) {
            HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
            if (SUCCEEDED(hr) && pEnumerator) {
                pEnumerator->RegisterEndpointNotificationCallback(this);
                log("XAudio2: Registered for device notifications\n");
            }
        }

        instances.push_back(p_instance);
        LeaveCriticalSection(&lock);
    }

    void do_unregister(class XAudio2_Output* p_instance) {
        EnterCriticalSection(&lock);
        
        // Remove instance from list
        for (auto it = instances.begin(); it != instances.end(); ) {
            if (*it == p_instance) {
                it = instances.erase(it);
            } else {
                ++it;
            }
        }

        if (InterlockedDecrement(&registered) == 0) {
            if (pEnumerator) {
                pEnumerator->UnregisterEndpointNotificationCallback(this);
                pEnumerator->Release();
                pEnumerator = nullptr;
                log("XAudio2: Unregistered from device notifications\n");
            }
        }
        
        LeaveCriticalSection(&lock);
    }
};

// Forward declaration
class XAudio2_Output {
public:
    virtual void device_change() = 0;
    virtual ~XAudio2_Output() = default;
};

XAudio2_Device_Notifier g_notifier;

struct XAudio2Context {
    XAudio2Version version = XAudio2Version::None;
    HMODULE hXAudio2 = nullptr;
    IXAudio2* xaudio2 = nullptr;
    
    // For XAudio 2.7 COM
    bool comInitialized = false;
    
    ~XAudio2Context() {
        if (xaudio2) {
            xaudio2->Release();
            xaudio2 = nullptr;
        }
        if (hXAudio2) {
            FreeLibrary(hXAudio2);
            hXAudio2 = nullptr;
        }
        if (comInitialized) {
            CoUninitialize();
            comInitialized = false;
        }
    }
    
    bool Initialize() {
        // Try XAudio 2.9 first (Windows 10+)
        hXAudio2 = LoadLibraryExW(L"XAudio2_9.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hXAudio2) {
            typedef HRESULT(__stdcall* XAudio2CreateFn)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);
            auto pXAudio2Create = (XAudio2CreateFn)GetProcAddress(hXAudio2, "XAudio2Create");
            if (pXAudio2Create) {
                HRESULT hr = pXAudio2Create(&xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
                if (SUCCEEDED(hr)) {
                    version = XAudio2Version::XAudio2_9;
                    log("XAudio2: Using XAudio 2.9 (Windows 10+)\n");
                    return true;
                }
            }
            FreeLibrary(hXAudio2);
            hXAudio2 = nullptr;
        }
        
        // Try XAudio 2.8 next (Windows 8)
        hXAudio2 = LoadLibraryExW(L"XAudio2_8.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hXAudio2) {
            typedef HRESULT(__stdcall* XAudio2CreateFn)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);
            auto pXAudio2Create = (XAudio2CreateFn)GetProcAddress(hXAudio2, "XAudio2Create");
            if (pXAudio2Create) {
                HRESULT hr = pXAudio2Create(&xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
                if (SUCCEEDED(hr)) {
                    version = XAudio2Version::XAudio2_8;
                    log("XAudio2: Using XAudio 2.8 (Windows 8)\n");
                    return true;
                }
            }
            FreeLibrary(hXAudio2);
            hXAudio2 = nullptr;
        }
        
        // Fall back to XAudio 2.7 (Windows 7)
        hXAudio2 = LoadLibraryExW(L"XAudio2_7.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hXAudio2) {
            HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
            if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
                FreeLibrary(hXAudio2);
                hXAudio2 = nullptr;
                return false;
            }
            comInitialized = (hr == S_OK);
            
            // Use COM to create XAudio2 2.7 instance
            static const CLSID CLSID_XAudio2_7 = {0x5a508685, 0xa254, 0x4fba, {0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf}};
            static const IID IID_IXAudio2_7 = {0x8bcf1f58, 0x9fe7, 0x4583, {0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb}};
            
            hr = CoCreateInstance(CLSID_XAudio2_7, NULL, CLSCTX_INPROC_SERVER, IID_IXAudio2_7, (void**)&xaudio2);
            if (SUCCEEDED(hr) && xaudio2) {
                // XAudio 2.7 REQUIRES Initialize() call!
                hr = xaudio2->Initialize(0, XAUDIO2_DEFAULT_PROCESSOR);
                if (SUCCEEDED(hr)) {
                    version = XAudio2Version::XAudio2_7;
                    log("XAudio2: Using XAudio 2.7 (Windows 7)\n");
                    return true;
                } else {
                    wxLogError(_("XAudio2: XAudio2_7 Initialize failed with HRESULT: 0x%08X"), hr);
                    xaudio2->Release();
                    xaudio2 = nullptr;
                }
            } else {
                wxLogError(_("XAudio2: Failed to create XAudio2 2.7"));
            }
            
            FreeLibrary(hXAudio2);
            hXAudio2 = nullptr;
        }
        
        wxLogError(_("XAudio2: Could not load XAudio2_9.dll, XAudio2_8.dll, or XAudio2_7.dll!"));
        return false;
    }
};

XAudio2Context g_xaudio2_context;

}  // namespace

std::vector<AudioDevice> GetXAudio2Devices() {
    if (!g_xaudio2_context.xaudio2 && !g_xaudio2_context.Initialize()) {
        return {};
    }

    // For XAudio2 2.7, use the 2.7-specific device enumeration
    if (g_xaudio2_context.version == XAudio2Version::XAudio2_7) {
        try {
            return GetXAudio2_7_Devices(g_xaudio2_context.xaudio2);
        } catch (...) {
            wxLogError(_("XAudio2: Failed to enumerate devices for XAudio2 2.7"));
            return {};
        }
    } else {
        // Both XAudio2 2.8 and 2.9 can use the same device enumeration logic
        return GetXAudio2_9_Devices(g_xaudio2_context.xaudio2);
    }
}

std::unique_ptr<SoundDriver> CreateXAudio2Driver() {
    if (!g_xaudio2_context.xaudio2 && !g_xaudio2_context.Initialize()) {
        return nullptr;
    }

    switch (g_xaudio2_context.version) {
    case XAudio2Version::XAudio2_9:
    case XAudio2Version::XAudio2_8:
        // XAudio2 2.8 and 2.9 are compatible - use the same driver
        return CreateXAudio2_9_Driver(g_xaudio2_context.xaudio2);
    case XAudio2Version::XAudio2_7:
        return CreateXAudio2_7_Driver(g_xaudio2_context.xaudio2);
    default:
        return nullptr;
    }
}

}  // namespace internal
}  // namespace audio
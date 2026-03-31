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

#if _MSC_VER
#include <xaudio2.legacy.h>
#else
// Suppress warnings about MSVC-specific #pragma warning in third-party headers
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif
#include <XAudio2.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif
// Include for log() function
#include "core/base/system.h"

namespace audio {
namespace internal {

// Implement the notifier methods
XAudio2_Device_Notifier g_notifier;

XAudio2_Device_Notifier::XAudio2_Device_Notifier() : _cRef(1), _pEnumerator(nullptr), instance_count(0) {
    for (int i = 0; i < MAX_INSTANCES; i++) {
        instances[i] = nullptr;
    }
    InitializeCriticalSection(&instances_cs);
}

XAudio2_Device_Notifier::~XAudio2_Device_Notifier() {
    if (_pEnumerator) {
        _pEnumerator->UnregisterEndpointNotificationCallback(this);
        _pEnumerator->Release();
        _pEnumerator = nullptr;
    }
    DeleteCriticalSection(&instances_cs);
}

// IUnknown methods - for global static object, we don't delete on Release
ULONG STDMETHODCALLTYPE XAudio2_Device_Notifier::AddRef() {
    return InterlockedIncrement(&_cRef);
}

ULONG STDMETHODCALLTYPE XAudio2_Device_Notifier::Release() {
    ULONG ulRef = InterlockedDecrement(&_cRef);
    // Do not delete - this is a global static object.
    return ulRef;
}

HRESULT STDMETHODCALLTYPE XAudio2_Device_Notifier::QueryInterface(REFIID riid, VOID** ppvInterface) {
    if (IID_IUnknown == riid) {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
        AddRef();
        *ppvInterface = (IMMNotificationClient*)this;
    } else {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE XAudio2_Device_Notifier::OnDefaultDeviceChanged(EDataFlow flow, ERole, LPCWSTR pwstrDeviceId) {
    // Called on the MMDevice notification thread - no wx calls permitted here.
    if (flow == eRender) {
        last_device = pwstrDeviceId ? pwstrDeviceId : L"";

        // Read count and iterate. Slots may be null if an instance is being
        // unregistered concurrently; null check handles that safely.
        LONG count = instance_count;
        for (int i = 0; i < MAX_INSTANCES && count > 0; i++) {
            XAudio2_Output* instance = instances[i];
            if (instance) {
                instance->signal_device_change();
                count--;
            }
        }
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE XAudio2_Device_Notifier::OnDeviceAdded(LPCWSTR) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE XAudio2_Device_Notifier::OnDeviceRemoved(LPCWSTR) {
    // Called on the MMDevice notification thread - no wx calls permitted here.
    LONG count = instance_count;
    for (int i = 0; i < MAX_INSTANCES && count > 0; i++) {
        XAudio2_Output* instance = instances[i];
        if (instance) {
            instance->signal_device_change();
            count--;
        }
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE XAudio2_Device_Notifier::OnDeviceStateChanged(LPCWSTR, DWORD) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE XAudio2_Device_Notifier::OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) {
    return S_OK;
}

void XAudio2_Device_Notifier::do_register(XAudio2_Output* p_instance) {
    EnterCriticalSection(&instances_cs);

    // Check if already registered.
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (instances[i] == p_instance) {
            LeaveCriticalSection(&instances_cs);
            log("XAudio2: Instance already registered, skipping\n");
            return;
        }
    }

    // Find first empty slot and assign before incrementing count, so a
    // concurrent callback iteration won't see a non-null slot with stale data.
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (instances[i] == nullptr) {
            instances[i] = p_instance;
            LONG new_count = InterlockedIncrement(&instance_count);
            LeaveCriticalSection(&instances_cs);

            log("XAudio2: Registered instance (total: %d)\n", new_count);

            // Register enumerator on first instance. Done outside the lock
            // since CoCreateInstance can be slow and we already hold the slot.
            if (new_count == 1 && !_pEnumerator) {
                HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                                              __uuidof(IMMDeviceEnumerator), (void**)&_pEnumerator);
                if (SUCCEEDED(hr) && _pEnumerator) {
                    _pEnumerator->RegisterEndpointNotificationCallback(this);
                    log("XAudio2: Registered for device notifications\n");
                }
            }
            return;
        }
    }

    LeaveCriticalSection(&instances_cs);
    wxLogError(_("XAudio2: Too many audio instances (max %d)"), MAX_INSTANCES);
}

void XAudio2_Device_Notifier::do_unregister(XAudio2_Output* p_instance) {
    EnterCriticalSection(&instances_cs);

    // Find and remove the instance.
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (instances[i] == p_instance) {
            // Null the slot before decrementing so concurrent callback
            // iterations see the slot as empty rather than dangling.
            instances[i] = nullptr;
            LONG new_count = InterlockedDecrement(&instance_count);
            LeaveCriticalSection(&instances_cs);
            // Use wxLogDebug rather than log() - do_unregister is called from
            // the destructor during teardown when the status bar may already be
            // partially destroyed, and log() routes through systemScreenMessage
            // which calls PopStatusText and asserts.
            wxLogDebug(_("XAudio2: Unregistered instance (remaining: %d)"), new_count);
            return;
        }
    }

    LeaveCriticalSection(&instances_cs);
    wxLogDebug(_("XAudio2: Warning - tried to unregister instance that wasn't registered"));
}

namespace {

enum class XAudio2Version {
    None,
    XAudio2_7,
    XAudio2_8,
    XAudio2_9
};

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

    // For XAudio2 2.7, use the 2.7-specific device enumeration.
    if (g_xaudio2_context.version == XAudio2Version::XAudio2_7) {
        try {
            return GetXAudio2_7_Devices(g_xaudio2_context.xaudio2);
        } catch (...) {
            wxLogError(_("XAudio2: Failed to enumerate devices for XAudio2 2.7"));
            return {};
        }
    } else {
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
        // XAudio2 2.8 and 2.9 are compatible - use the same driver.
        return CreateXAudio2_9_Driver(g_xaudio2_context.xaudio2);
    case XAudio2Version::XAudio2_7:
        return CreateXAudio2_7_Driver(g_xaudio2_context.xaudio2);
    default:
        return nullptr;
    }
}

}  // namespace internal
}  // namespace audio

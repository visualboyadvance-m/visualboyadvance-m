#if !defined(VBAM_ENABLE_FAUDIO)
#error "This file should only be compiled if FAudio is enabled"
#endif

#include <cstdio>

#include <string>
#include <vector>

// FAudio
#include <FAudio.h>

#include <mmdeviceapi.h>

#include <wx/arrstr.h>
#include <wx/log.h>
#include <wx/translation.h>

#include "core/base/sound_driver.h"
#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "wx/config/option-proxy.h"

namespace {

int GetFADevices(FAudio* fa, wxArrayString* names, wxArrayString* ids,
    const wxString* match)
{
    uint32_t hr;
    uint32_t dev_count = 0;
    hr = FAudio_GetDeviceCount(fa, &dev_count);

    if (hr != 0) {
        wxLogError(_("FAudio: Enumerating devices failed!"));
        return -1;
    } else {
        FAudioDeviceDetails dd;

        for (uint32_t i = 0; i < dev_count; i++) {
            hr = FAudio_GetDeviceDetails(fa, i, &dd);

            if (hr != 0) {
                continue;
            } else {
                if (ids) {
                    ids->push_back((wchar_t*) dd.DeviceID);
                    names->push_back((wchar_t*) dd.DisplayName);
                } else if (*match == wxString((wchar_t*) dd.DeviceID))
                    return i;
            }
        }
    }

    return -1;
}

int FAGetDev(FAudio* fa)
{
    const wxString& audio_device = OPTION(kSoundAudioDevice);
    if (audio_device.empty())
        return 0;
    else {
        int ret = GetFADevices(fa, NULL, NULL, &audio_device);
        return ret < 0 ? 0 : ret;
    }
}

class FAudio_BufferNotify : public FAudioVoiceCallback {
public:
    bool WaitForSignal() {
        return WaitForSingleObject(buffer_end_event_, 10000) != WAIT_TIMEOUT;
    }

    FAudio_BufferNotify()
    {
        buffer_end_event_ = nullptr;
        buffer_end_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        assert(buffer_end_event_ != nullptr);

        OnBufferEnd = &FAudio_BufferNotify::StaticOnBufferEnd;
        OnVoiceProcessingPassStart = &FAudio_BufferNotify::StaticOnVoiceProcessingPassStart;
        OnVoiceProcessingPassEnd = &FAudio_BufferNotify::StaticOnVoiceProcessingPassEnd;
        OnStreamEnd = &FAudio_BufferNotify::StaticOnStreamEnd;
        OnBufferStart = &FAudio_BufferNotify::StaticOnBufferStart;
        OnLoopEnd = &FAudio_BufferNotify::StaticOnLoopEnd;
        OnVoiceError = &FAudio_BufferNotify::StaticOnVoiceError;
    }
    ~FAudio_BufferNotify()
    {
        CloseHandle(buffer_end_event_);
        buffer_end_event_ = nullptr;
    }

private:
    void* buffer_end_event_;

    static void StaticOnBufferEnd(FAudioVoiceCallback* callback, void * pBufferContext) {
        FAudio_BufferNotify* self = static_cast<FAudio_BufferNotify*>(callback);
        if (self != nullptr && self->buffer_end_event_ != NULL)
        {
            SetEvent(self->buffer_end_event_);
        }
    }
    static void StaticOnVoiceProcessingPassStart(FAudioVoiceCallback* callback, uint32_t BytesRequired) {}
    static void StaticOnVoiceProcessingPassEnd(FAudioVoiceCallback* callback) {}
    static void StaticOnStreamEnd(FAudioVoiceCallback* callback) {}    
    static void StaticOnBufferStart(FAudioVoiceCallback* callback, void * pBufferContext) {}
    static void StaticOnLoopEnd(FAudioVoiceCallback* callback, void * pBufferContext) {}
    static void StaticOnVoiceError(FAudioVoiceCallback* callback, void * pBufferContext, uint32_t Error) {}
};

class FAudio_Output
    : public SoundDriver {
public:
    FAudio_Output();
    ~FAudio_Output();

    // Initialization
    bool init(long sampleRate);

    // Sound Data Feed
    void write(uint16_t* finalWave, int length);

    // Play Control
    void pause();
    void resume();
    void reset();
    void close();
    void device_change();

    // Configuration Changes
    void setThrottle(unsigned short throttle_);

private:
    bool failed;
    bool initialized;
    bool playing;
    uint32_t freq;
    uint32_t bufferCount;
    uint8_t* buffers;
    int currentBuffer;
    int soundBufferLen;

    volatile bool device_changed;

    FAudio* faud;
    FAudioMasteringVoice* mVoice; // listener
    FAudioSourceVoice* sVoice; // sound source
    FAudioBuffer buf;
    FAudioVoiceState vState;
    FAudio_BufferNotify notify; // buffer end notification
};

class FAudio_Device_Notifier : public IMMNotificationClient {
private:
    volatile LONG registered;
    IMMDeviceEnumerator* pEnumerator;

    std::wstring last_device;

    CRITICAL_SECTION lock;
    std::vector<FAudio_Output*> instances;

public:
    FAudio_Device_Notifier();
    ~FAudio_Device_Notifier();
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface);
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId);
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId);
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId);
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState);
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key);

    void do_register(FAudio_Output* p_instance);
    void do_unregister(FAudio_Output* p_instance);
};

FAudio_Device_Notifier f_notifier;

// Class Implementation
FAudio_Output::FAudio_Output()
{
    failed = false;
    initialized = false;
    playing = false;
    freq = 0;
    bufferCount = OPTION(kSoundBuffers);
    buffers = NULL;
    currentBuffer = 0;
    device_changed = false;
    faud = NULL;
    mVoice = NULL;
    sVoice = NULL;
    memset(&buf, NULL, sizeof(buf));
    memset(&vState, NULL, sizeof(vState));
    f_notifier.do_register(this);
}

FAudio_Output::~FAudio_Output()
{
    f_notifier.do_unregister(this);
    close();
}

void FAudio_Output::close()
{
    initialized = false;

    if (sVoice) {
        if (playing) {
            assert(FAudioSourceVoice_Stop(sVoice, 0, FAUDIO_COMMIT_NOW) == 0);
        }

        FAudioVoice_DestroyVoice(sVoice);
        sVoice = NULL;
    }

    if (buffers) {
        free(buffers);
        buffers = NULL;
    }

    if (mVoice) {
        FAudioVoice_DestroyVoice(mVoice);
        mVoice = NULL;
    }

    if (faud) {
        FAudio_Release(faud);
        faud = NULL;
    }
}

void FAudio_Output::device_change()
{
    device_changed = true;
}

bool FAudio_Output::init(long sampleRate)
{
    if (failed || initialized)
        return false;

    uint32_t hr;
    // Initialize FAudio
    uint32_t flags = 0;
    //#ifdef _DEBUG
    //	flags = FAUDIO_DEBUG_ENGINE;
    //#endif
    hr = FAudioCreate(&faud, flags, FAUDIO_DEFAULT_PROCESSOR);

    if (hr != 0) {
        wxLogError(_("The FAudio interface failed to initialize!"));
        failed = true;
        return false;
    }

    freq = sampleRate;
    // calculate the number of samples per frame first
    // then multiply it with the size of a sample frame (16 bit * stereo)
    soundBufferLen = (freq / 60) * 4;
    // create own buffers to store sound data because it must not be
    // manipulated while the voice plays from it
    buffers = (uint8_t*)malloc((bufferCount + 1) * soundBufferLen);
    // + 1 because we need one temporary buffer when all others are in use
    FAudioWaveFormatEx wfx;
    memset(&wfx, NULL, sizeof(wfx));
    wfx.wFormatTag = FAUDIO_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = freq;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    // create sound receiver
    hr = FAudio_CreateMasteringVoice(faud,
    &mVoice,
    FAUDIO_DEFAULT_CHANNELS,
    FAUDIO_DEFAULT_SAMPLERATE,
    0,
    FAGetDev(faud),
    NULL);

    if (hr != 0) {
        wxLogError(_("FAudio: Creating mastering voice failed!"));
        failed = true;
        return false;
    }

    // create sound emitter
    //This should be  FAudio_CreateSourceVoice()
    hr = FAudio_CreateSourceVoice(faud, &sVoice, &wfx, 0, 4.0f, &notify, NULL, NULL);

    if (hr != 0) {
        wxLogError(_("FAudio: Creating source voice failed!"));
        failed = true;
        return false;
    }

    if (OPTION(kSoundUpmix)) {
        // set up stereo upmixing
        FAudioDeviceDetails dd {};
        //memset(&dd, NULL, sizeof(dd));
        assert(FAudio_GetDeviceDetails(faud, 0, &dd) == 0);
        float* matrix = NULL;
        matrix = (float*)malloc(sizeof(float) * 2 * dd.OutputFormat.Format.nChannels);

        if (matrix == NULL)
            return false;

        bool matrixAvailable = true;

        switch (dd.OutputFormat.Format.nChannels) {
        case 4: // 4.0
            //Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f;
            matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f;
            matrix[3] = 1.0000f;
            /*Back  L*/ matrix[4] = 1.0000f;
            matrix[5] = 0.0000f;
            /*Back  R*/ matrix[6] = 0.0000f;
            matrix[7] = 1.0000f;
            break;

        case 5: // 5.0
            //Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f;
            matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f;
            matrix[3] = 1.0000f;
            /*Front C*/ matrix[4] = 0.7071f;
            matrix[5] = 0.7071f;
            /*Side  L*/ matrix[6] = 1.0000f;
            matrix[7] = 0.0000f;
            /*Side  R*/ matrix[8] = 0.0000f;
            matrix[9] = 1.0000f;
            break;

        case 6: // 5.1
            //Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f;
            matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f;
            matrix[3] = 1.0000f;
            /*Front C*/ matrix[4] = 0.7071f;
            matrix[5] = 0.7071f;
            /*LFE    */ matrix[6] = 0.0000f;
            matrix[7] = 0.0000f;
            /*Side  L*/ matrix[8] = 1.0000f;
            matrix[9] = 0.0000f;
            /*Side  R*/ matrix[10] = 0.0000f;
            matrix[11] = 1.0000f;
            break;

        case 7: // 6.1
            //Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f;
            matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f;
            matrix[3] = 1.0000f;
            /*Front C*/ matrix[4] = 0.7071f;
            matrix[5] = 0.7071f;
            /*LFE    */ matrix[6] = 0.0000f;
            matrix[7] = 0.0000f;
            /*Side  L*/ matrix[8] = 1.0000f;
            matrix[9] = 0.0000f;
            /*Side  R*/ matrix[10] = 0.0000f;
            matrix[11] = 1.0000f;
            /*Back  C*/ matrix[12] = 0.7071f;
            matrix[13] = 0.7071f;
            break;

        case 8: // 7.1
            //Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f;
            matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f;
            matrix[3] = 1.0000f;
            /*Front C*/ matrix[4] = 0.7071f;
            matrix[5] = 0.7071f;
            /*LFE    */ matrix[6] = 0.0000f;
            matrix[7] = 0.0000f;
            /*Back  L*/ matrix[8] = 1.0000f;
            matrix[9] = 0.0000f;
            /*Back  R*/ matrix[10] = 0.0000f;
            matrix[11] = 1.0000f;
            /*Side  L*/ matrix[12] = 1.0000f;
            matrix[13] = 0.0000f;
            /*Side  R*/ matrix[14] = 0.0000f;
            matrix[15] = 1.0000f;
            break;

        default:
            matrixAvailable = false;
            break;
        }

        if (matrixAvailable) {
            hr = FAudioVoice_SetOutputMatrix(sVoice, NULL, 2, dd.OutputFormat.Format.nChannels, matrix, FAUDIO_DEFAULT_CHANNELS);
            assert(hr == 0);
        }

        free(matrix);
        matrix = NULL;
    }

    hr = FAudioSourceVoice_Start(sVoice, 0, FAUDIO_COMMIT_NOW);
    assert(hr == 0);
    playing = true;
    currentBuffer = 0;
    device_changed = false;
    initialized = true;
    return true;
}

void FAudio_Output::write(uint16_t* finalWave, int length)
{
    uint32_t flags = 0;
    if (!initialized || failed)
        return;

    while (true) {
        if (device_changed) {
            close();

            if (!init(freq))
                return;
        }

        FAudioSourceVoice_GetState(sVoice, &vState, flags);
        assert(vState.BuffersQueued <= bufferCount);

        if (vState.BuffersQueued < bufferCount) {
            if (vState.BuffersQueued == 0) {
                // buffers ran dry
                if (systemVerbose & VERBOSE_SOUNDOUTPUT) {
                    static unsigned int i = 0;
                    log("FAudio: Buffers were not refilled fast enough (i=%i)\n", i++);
                }
            }

            // there is at least one free buffer
            break;
        } else {
            // the maximum number of buffers is currently queued
            if (!coreOptions.speedup && coreOptions.throttle && !gba_joybus_active) {
                // wait for one buffer to finish playing
                if (notify.WaitForSignal()) {
                    device_changed = true;
                }
            } else {
                // drop current audio frame
                return;
            }
        }
    }

    // copy & protect the audio data in own memory area while playing it
    memcpy(&buffers[currentBuffer * soundBufferLen], finalWave, soundBufferLen);
    buf.AudioBytes = soundBufferLen;
    buf.pAudioData = &buffers[currentBuffer * soundBufferLen];
    currentBuffer++;
    currentBuffer %= (bufferCount + 1); // + 1 because we need one temporary buffer
    uint32_t hr = FAudioSourceVoice_SubmitSourceBuffer(sVoice, &buf, NULL);
    assert(hr == 0);
}

void FAudio_Output::pause()
{
    if (!initialized || failed)
        return;

    if (playing) {
        uint32_t hr = FAudioSourceVoice_Stop(sVoice, 0, FAUDIO_COMMIT_NOW);
        assert(hr == 0);
        playing = false;
    }
}

void FAudio_Output::resume()
{
    if (!initialized || failed)
        return;

    if (!playing) {
        uint32_t hr = FAudioSourceVoice_Start(sVoice, 0, FAUDIO_COMMIT_NOW);
        assert(hr == 0);
        playing = true;
    }
}

void FAudio_Output::reset()
{
    if (!initialized || failed)
        return;

    if (playing) {
        uint32_t hr = FAudioSourceVoice_Stop(sVoice, 0, FAUDIO_COMMIT_NOW);
        assert(hr == 0);
    }

    FAudioSourceVoice_FlushSourceBuffers(sVoice);
    FAudioSourceVoice_Start(sVoice, 0, FAUDIO_COMMIT_NOW);
    playing = true;
}

void FAudio_Output::setThrottle(unsigned short throttle_)
{
    if (!initialized || failed)
        return;

    if (throttle_ == 0)
        throttle_ = 100;

    uint32_t hr = FAudioSourceVoice_SetFrequencyRatio(sVoice, (float)throttle_ / 100.0f, FAUDIO_COMMIT_NOW);
    assert(hr == 0);
}

FAudio_Device_Notifier::FAudio_Device_Notifier()
    : registered(0)
{
    InitializeCriticalSection(&lock);
}
FAudio_Device_Notifier::~FAudio_Device_Notifier()
{
    DeleteCriticalSection(&lock);
}

ULONG STDMETHODCALLTYPE FAudio_Device_Notifier::AddRef()
{
    return 1;
}

ULONG STDMETHODCALLTYPE FAudio_Device_Notifier::Release()
{
    return 1;
}

HRESULT STDMETHODCALLTYPE FAudio_Device_Notifier::QueryInterface(REFIID riid, VOID** ppvInterface)
{
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

HRESULT STDMETHODCALLTYPE FAudio_Device_Notifier::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
    if (flow == eRender && last_device.compare(pwstrDeviceId) != 0) {
        last_device = pwstrDeviceId;
        EnterCriticalSection(&lock);

        for (auto it = instances.begin(); it < instances.end(); ++it) {
            (*it)->device_change();
        }

        LeaveCriticalSection(&lock);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FAudio_Device_Notifier::OnDeviceAdded(LPCWSTR pwstrDeviceId) { return S_OK; }
HRESULT STDMETHODCALLTYPE FAudio_Device_Notifier::OnDeviceRemoved(LPCWSTR pwstrDeviceId) { return S_OK; }
HRESULT STDMETHODCALLTYPE FAudio_Device_Notifier::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) { return S_OK; }
HRESULT STDMETHODCALLTYPE FAudio_Device_Notifier::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) { return S_OK; }

void FAudio_Device_Notifier::do_register(FAudio_Output* p_instance)
{
    if (InterlockedIncrement(&registered) == 1) {
        pEnumerator = NULL;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

        if (SUCCEEDED(hr)) {
            pEnumerator->RegisterEndpointNotificationCallback(this);
        }
    }

    EnterCriticalSection(&lock);
    instances.push_back(p_instance);
    LeaveCriticalSection(&lock);
}

void FAudio_Device_Notifier::do_unregister(FAudio_Output* p_instance)
{
    if (InterlockedDecrement(&registered) == 0) {
        if (pEnumerator) {
            pEnumerator->UnregisterEndpointNotificationCallback(this);
            pEnumerator->Release();
            pEnumerator = NULL;
        }
    }

    EnterCriticalSection(&lock);

    for (auto it = instances.begin(); it < instances.end(); ++it) {
        if (*it == p_instance) {
            instances.erase(it);
            break;
        }
    }

    LeaveCriticalSection(&lock);
}


}  // namespace

bool GetFADevices(wxArrayString& names, wxArrayString& ids)
{
    uint32_t hr;
    FAudio* fa = NULL;
    uint32_t flags = 0;
#ifdef _DEBUG
    flags = FAUDIO_DEBUG_ENGINE;
#endif
    hr = FAudioCreate(&fa, flags, FAUDIO_DEFAULT_PROCESSOR);

    if (hr != 0) {
        wxLogError(_("The FAudio interface failed to initialize!"));
        return false;
    }

    GetFADevices(fa, &names, &ids, NULL);
    FAudio_Release(fa);
    return true;
}

// Class Declaration
SoundDriver* newFAudio_Output()
{
    return new FAudio_Output();
}

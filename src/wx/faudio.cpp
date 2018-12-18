#ifndef NO_FAUDIO

// Application
#include "wxvbam.h"
#include <stdio.h>

// Interface
#include "../common/ConfigManager.h"
#include "../common/SoundDriver.h"

// Faudio
#include <faudio.h>
#endif

// MMDevice API
#include <mmdeviceapi.h>
#include <string>
#include <vector>

// Internals
#include "../System.h" // for systemMessage()
#include "../gba/Globals.h"

int GetFA2Devices(Faudio* fa, wxArrayString* names, wxArrayString* ids,
    const wxString* match)
{
    HRESULT hr;
    UINT32 dev_count = 0;
    hr = fa->FAudio_GetDeviceCount(&dev_count);

    if (hr != S_OK) {
        wxLogError(_("FAudio: Enumerating devices failed!"));
        return true;
    } else {
        FaudioDeviceDetails dd;

        for (UINT32 i = 0; i < dev_count; i++) {
            hr = fa->GetDeviceDetails(i, &dd);

            if (hr != S_OK) {
                continue;
            } else {
                if (ids) {
                    ids->push_back(dd.DeviceID);
                    names->push_back(dd.DisplayName);
                } else if (*match == dd.DeviceID)
                    return i;
            }
        }
    }

    return -1;
}

bool GetFA2Devices(wxArrayString& names, wxArrayString& ids)
{
    HRESULT hr;
    FAudio* fa = NULL;
    UINT32 flags = 0;
#ifdef _DEBUG
    flags = FAUDIO_DEBUG_ENGINE;
#endif
    hr = FAudioreate(&xa, flags);

    if (hr != S_OK) {
        wxLogError(_("The FAudio interface failed to initialize!"));
        return false;
    }

    GetFA2Devices(fa, &names, &ids, NULL);
    fa->Release();
    return true;
}

static int FAGetDev(Faudio* fa)
{
    if (gopts.audio_dev.empty())
        return 0;
    else {
        int ret = GetFA2Devices(fa, NULL, NULL, &gopts.audio_dev);
        return ret < 0 ? 0 : ret;
    }
}

class FAudio_Output;

static void faudio_device_changed(FAudio_Output*);

class FAudio_Device_Notifier : public IMMNotificationClient {
    volatile LONG registered;
    IMMDeviceEnumerator* pEnumerator;

    std::wstring last_device;

    CRITICAL_SECTION lock;
    std::vector<FAudio_Output*> instances;

public:
    FAudio_Device_Notifier()
        : registered(0)
    {
        InitializeCriticalSection(&lock);
    }
    ~FAudio_Device_Notifier()
    {
        DeleteCriticalSection(&lock);
    }

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        return 1;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface)
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

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
    {
        if (flow == eRender && last_device.compare(pwstrDeviceId) != 0) {
            last_device = pwstrDeviceId;
            EnterCriticalSection(&lock);

            for (auto it = instances.begin(); it < instances.end(); ++it) {
                faudio_device_changed(*it);
            }

            LeaveCriticalSection(&lock);
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) { return S_OK; }

    void do_register(FAudio_Output* p_instance)
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

    void do_unregister(FAudio_Output* p_instance)
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
} g_notifier;

// Synchronization Event
class FAudio_BufferNotify : public FAudioVoiceCallback {
public:
    HANDLE hBufferEndEvent;

    FAudio_BufferNotify()
    {
        hBufferEndEvent = NULL;
        hBufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        assert(hBufferEndEvent != NULL);
    }

    ~FAudio_BufferNotify()
    {
        CloseHandle(hBufferEndEvent);
        hBufferEndEvent = NULL;
    }

    STDMETHOD_(void, OnBufferEnd)
    (void* pBufferContext)
    {
        assert(hBufferEndEvent != NULL);
        SetEvent(hBufferEndEvent);
    }

    // dummies:
    STDMETHOD_(void, OnVoiceProcessingPassStart)
    (UINT32 BytesRequired) {}
    STDMETHOD_(void, OnVoiceProcessingPassEnd)
    () {}
    STDMETHOD_(void, OnStreamEnd)
    () {}
    STDMETHOD_(void, OnBufferStart)
    (void* pBufferContext) {}
    STDMETHOD_(void, OnLoopEnd)
    (void* pBufferContext) {}
    STDMETHOD_(void, OnVoiceError)
    (void* pBufferContext, HRESULT Error){};
};

// Class Declaration
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
    void setThrottle(unsigned short throttle);

private:
    bool failed;
    bool initialized;
    bool playing;
    UINT32 freq;
    UINT32 bufferCount;
    BYTE* buffers;
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

// Class Implementation
FAudio_Output::FAudio_Output()
{
    failed = false;
    initialized = false;
    playing = false;
    freq = 0;
    bufferCount = gopts.audio_buffers;
    buffers = NULL;
    currentBuffer = 0;
    device_changed = false;
    faud = NULL;
    mVoice = NULL;
    sVoice = NULL;
    ZeroMemory(&buf, sizeof(buf));
    ZeroMemory(&vState, sizeof(vState));
    g_notifier.do_register(this);
}

FAudio_Output::~FAudio_Output()
{
    g_notifier.do_unregister(this);
    close();
}

void FAudio_Output::close()
{
    initialized = false;

    if (sVoice) {
        if (playing) {
            HRESULT hr = sVoice->Stop(0);
            assert(hr == S_OK);
        }

        sVoice->DestroyVoice();
        sVoice = NULL;
    }

    if (buffers) {
        free(buffers);
        buffers = NULL;
    }

    if (mVoice) {
        mVoice->DestroyVoice();
        mVoice = NULL;
    }

    if (faud) {
        faud->Release();
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

    HRESULT hr;
    // Initialize FAudio
    UINT32 flags = 0;
    //#ifdef _DEBUG
    //	flags = FAUDIO_DEBUG_ENGINE;
    //#endif
    hr = FAudioCreate(&faud, flags);

    if (hr != S_OK) {
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
    buffers = (BYTE*)malloc((bufferCount + 1) * soundBufferLen);
    // + 1 because we need one temporary buffer when all others are in use
    WAVEFORMATEX wfx;
    ZeroMemory(&wfx, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = freq;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    // create sound receiver
    hr = faud->CreateMasteringVoice(
        &mVoice,
        FAUDIO_DEFAULT_CHANNELS,
        FAUDIO_DEFAULT_SAMPLERATE,
        0,
        FAGetDev(faud),
        NULL);

    if (hr != S_OK) {
        wxLogError(_("FAudio: Creating mastering voice failed!"));
        failed = true;
        return false;
    }

    // create sound emitter
    hr = faud->CreateSourceVoice(&sVoice, &wfx, 0, 4.0f, &notify);

    if (hr != S_OK) {
        wxLogError(_("FAudio: Creating source voice failed!"));
        failed = true;
        return false;
    }

    if (gopts.upmix) {
        // set up stereo upmixing
        FAudioDeviceDetails dd;
        ZeroMemory(&dd, sizeof(dd));
        hr = faud->FAudio_GetDeviceDetails(0, &dd);
        assert(hr == S_OK);
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
            hr = sVoice->SetOutputMatrix(NULL, 2, dd.OutputFormat.Format.nChannels, matrix);
            assert(hr == S_OK);
        }

        free(matrix);
        matrix = NULL;
    }

    hr = sVoice->Start(0);
    assert(hr == S_OK);
    playing = true;
    currentBuffer = 0;
    device_changed = false;
    initialized = true;
    return true;
}

void FAudio_Output::write(uint16_t* finalWave, int length)
{
    if (!initialized || failed)
        return;

    while (true) {
        if (device_changed) {
            close();

            if (!init(freq))
                return;
        }

        sVoice->GetState(&vState);
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
            if (!speedup && throttle && !gba_joybus_active) {
                // wait for one buffer to finish playing
                if (WaitForSingleObject(notify.hBufferEndEvent, 10000) == WAIT_TIMEOUT) {
                    device_changed = true;
                }
            } else {
                // drop current audio frame
                return;
            }
        }
    }

    // copy & protect the audio data in own memory area while playing it
    CopyMemory(&buffers[currentBuffer * soundBufferLen], finalWave, soundBufferLen);
    buf.AudioBytes = soundBufferLen;
    buf.pAudioData = &buffers[currentBuffer * soundBufferLen];
    currentBuffer++;
    currentBuffer %= (bufferCount + 1); // + 1 because we need one temporary buffer
    HRESULT hr = sVoice->SubmitSourceBuffer(&buf); // send buffer to queue
    assert(hr == S_OK);
}

void FAudio_Output::pause()
{
    if (!initialized || failed)
        return;

    if (playing) {
        HRESULT hr = sVoice->Stop(0);
        assert(hr == S_OK);
        playing = false;
    }
}

void FAudio_Output::resume()
{
    if (!initialized || failed)
        return;

    if (!playing) {
        HRESULT hr = sVoice->Start(0);
        assert(hr == S_OK);
        playing = true;
    }
}

void FAudio_Output::reset()
{
    if (!initialized || failed)
        return;

    if (playing) {
        HRESULT hr = sVoice->Stop(0);
        assert(hr == S_OK);
    }

    sVoice->FlushSourceBuffers();
    sVoice->Start(0);
    playing = true;
}

void FAudio_Output::setThrottle(unsigned short throttle_)
{
    if (!initialized || failed)
        return;

    if (throttle_ == 0)
        throttle_ = 100;

    HRESULT hr = sVoice->SetFrequencyRatio((float)throttle_ / 100.0f);
    assert(hr == S_OK);
}

void faudio_device_changed(FAudio_Output* instance)
{
    instance->device_change();
}

SoundDriver* newFAudio_Output()
{
    return new FAudio_Output();
}

#endif // #ifndef NO_FAUDIO

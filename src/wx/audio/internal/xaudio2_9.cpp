#if !defined(VBAM_ENABLE_XAUDIO2)
#error "This file should only be compiled if XAudio2 is enabled"
#endif

#include "xaudio2_9.h"

#include <cstdio>
#include <string>
#include <vector>

// MMDevice API
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <wx/arrstr.h>
#include <wx/log.h>
#include <wx/translation.h>

#include "core/base/sound_driver.h"
#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "wx/config/option-proxy.h"

// Windows 10 SDK headers
#include <xaudio2.h>

namespace audio {
namespace internal {

namespace {

class XAudio2_9_Output : public SoundDriver {
public:
    XAudio2_9_Output(IXAudio2* xaudio2);
    ~XAudio2_9_Output() override;

    // SoundDriver implementation
    bool init(long sampleRate) override;
    void pause() override;
    void reset() override;
    void resume() override;
    void write(uint16_t* finalWave, int length) override;
    void setThrottle(unsigned short throttle_) override;

private:
    void close();
    void device_change();
    bool SetupStereoUpmix();

    bool failed = false;
    bool initialized = false;
    bool playing = false;
    UINT32 freq = 0;
    UINT32 bufferCount = 0;
    BYTE* buffers = nullptr;
    int currentBuffer = 0;
    int soundBufferLen = 0;
    volatile bool device_changed = false;

    IXAudio2* xaud = nullptr;
    IXAudio2MasteringVoice* mVoice = nullptr;
    IXAudio2SourceVoice* sVoice = nullptr;
    XAUDIO2_VOICE_STATE vState = {};
    
    class XAudio2_BufferNotify* notify = nullptr;
    std::wstring m_device_id;
};

// Buffer notification class
class XAudio2_BufferNotify : public IXAudio2VoiceCallback {
public:
    HANDLE hBufferEndEvent;

    XAudio2_BufferNotify() {
        hBufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        assert(hBufferEndEvent != NULL);
    }

    ~XAudio2_BufferNotify() {
        CloseHandle(hBufferEndEvent);
        hBufferEndEvent = NULL;
    }

    STDMETHOD_(void, OnBufferEnd)(void*) override {
        assert(hBufferEndEvent != NULL);
        SetEvent(hBufferEndEvent);
    }

    // Dummy implementations
    STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) override {}
    STDMETHOD_(void, OnVoiceProcessingPassEnd)() override {}
    STDMETHOD_(void, OnStreamEnd)() override {}
    STDMETHOD_(void, OnBufferStart)(void*) override {}
    STDMETHOD_(void, OnLoopEnd)(void*) override {}
    STDMETHOD_(void, OnVoiceError)(void*, HRESULT) override {}
};

bool XAudio2_9_Output::SetupStereoUpmix() {
    if (!OPTION(kSoundUpmix)) {
        return true; // Upmix not enabled
    }

    // For XAudio2 2.9, we need to get the channel mask from the mastering voice
    XAUDIO2_VOICE_DETAILS voiceDetails;
    mVoice->GetVoiceDetails(&voiceDetails);
    
    UINT32 outputChannels = voiceDetails.InputChannels;
    
    // Only setup upmix if we have more than 2 channels
    if (outputChannels <= 2) {
        return true;
    }

    // Allocate matrix for stereo -> multi-channel mapping
    float* matrix = (float*)malloc(sizeof(float) * 2 * outputChannels);
    if (!matrix) {
        wxLogWarning(_("XAudio2: Failed to allocate upmix matrix"));
        return false;
    }

    bool matrixAvailable = true;

    // Set up stereo upmixing matrix with full volume normalization
    // This reduces volume clipping at 100% on multichannel systems.
    switch (outputChannels) {
        case 4:  // 4.0 (Quad) - Normalize by sqrt(2)
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 0.7071f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 0.7071f;
            /*Back  L*/ matrix[4] = 0.7071f; matrix[5] = 0.0000f;
            /*Back  R*/ matrix[6] = 0.0000f; matrix[7] = 0.7071f;
            break;

        case 5:  // 5.0 - Normalize by sqrt(2.5)
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 0.6325f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 0.6325f;
            /*Front C*/ matrix[4] = 0.4472f; matrix[5] = 0.4472f;
            /*Side  L*/ matrix[6] = 0.6325f; matrix[7] = 0.0000f;
            /*Side  R*/ matrix[8] = 0.0000f; matrix[9] = 0.6325f;
            break;

        case 6:  // 5.1 - Normalize by sqrt(3.0)
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 0.5774f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 0.5774f;
            /*Front C*/ matrix[4] = 0.4082f; matrix[5] = 0.4082f;
            /*LFE    */ matrix[6] = 0.0000f; matrix[7] = 0.0000f;
            /*Side  L*/ matrix[8] = 0.5774f; matrix[9] = 0.0000f;
            /*Side  R*/ matrix[10] = 0.0000f; matrix[11] = 0.5774f;
            break;

        case 7:  // 6.1 - Normalize by sqrt(3.5)
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 0.5345f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 0.5345f;
            /*Front C*/ matrix[4] = 0.3780f; matrix[5] = 0.3780f;
            /*LFE    */ matrix[6] = 0.0000f; matrix[7] = 0.0000f;
            /*Side  L*/ matrix[8] = 0.5345f; matrix[9] = 0.0000f;
            /*Side  R*/ matrix[10] = 0.0000f; matrix[11] = 0.5345f;
            /*Back  C*/ matrix[12] = 0.3780f; matrix[13] = 0.3780f;
            break;

        case 8:  // 7.1 - Normalize by sqrt(4.0)
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 0.5000f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 0.5000f;
            /*Front C*/ matrix[4] = 0.3536f; matrix[5] = 0.3536f;
            /*LFE    */ matrix[6] = 0.0000f; matrix[7] = 0.0000f;
            /*Back  L*/ matrix[8] = 0.5000f; matrix[9] = 0.0000f;
            /*Back  R*/ matrix[10] = 0.0000f; matrix[11] = 0.5000f;
            /*Side  L*/ matrix[12] = 0.5000f; matrix[13] = 0.0000f;
            /*Side  R*/ matrix[14] = 0.0000f; matrix[15] = 0.5000f;
            break;

        default:
            matrixAvailable = false;
            wxLogWarning(_("XAudio2: Unsupported channel count for upmix: %d"), outputChannels);
            break;
    }

    if (matrixAvailable) {
        HRESULT hr = sVoice->SetOutputMatrix(NULL, 2, outputChannels, matrix);
        if (FAILED(hr)) {
            wxLogWarning(_("XAudio2: Failed to set output matrix for upmix"));
            matrixAvailable = false;
        } else {
            log("XAudio2: Stereo upmix enabled for %d channels\n", outputChannels);
        }
    }

    free(matrix);
    return matrixAvailable;
}

XAudio2_9_Output::XAudio2_9_Output(IXAudio2* xaudio2) : xaud(xaudio2) {
    bufferCount = OPTION(kSoundBuffers);
}

XAudio2_9_Output::~XAudio2_9_Output() {
    close();
}

void XAudio2_9_Output::close() {
    initialized = false;

    if (sVoice) {
        if (playing) {
            sVoice->Stop(0);
        }
        sVoice->DestroyVoice();
        sVoice = nullptr;
    }

    if (buffers) {
        free(buffers);
        buffers = nullptr;
    }

    if (mVoice) {
        mVoice->DestroyVoice();
        mVoice = nullptr;
    }

    delete notify;
    notify = nullptr;
}

void XAudio2_9_Output::device_change() {
    device_changed = true;
}

bool XAudio2_9_Output::init(long sampleRate) {
    if (failed || initialized)
        return false;

    if (!xaud) {
        wxLogError(_("XAudio2 instance not available!"));
        failed = true;
        return false;
    }

    freq = sampleRate;
    soundBufferLen = (freq / 60) * 4;
    buffers = (BYTE*)malloc((bufferCount + 1) * soundBufferLen);
    if (!buffers) {
        wxLogError(_("XAudio2: Failed to allocate audio buffers!"));
        failed = true;
        return false;
    }
    
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = freq;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    
    // Get device ID from config
    const wxString& audio_device = OPTION(kSoundAudioDevice);
    const wchar_t* deviceId = nullptr;
    if (!audio_device.empty()) {
        m_device_id = audio_device.wc_str();
        deviceId = m_device_id.c_str();
    }
    
    // Create mastering voice - XAudio2 2.9 uses device ID string
    HRESULT hr = xaud->CreateMasteringVoice(&mVoice, XAUDIO2_DEFAULT_CHANNELS, 
                                   XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceId, nullptr, AudioCategory_GameEffects);
    if (FAILED(hr)) {
        wxLogError(_("XAudio2: Creating mastering voice failed! HRESULT: 0x%08X"), hr);
        failed = true;
        return false;
    }

    // Create source voice
    notify = new XAudio2_BufferNotify();
    hr = xaud->CreateSourceVoice(&sVoice, &wfx, 0, 4.0f, notify);
    if (FAILED(hr)) {
        wxLogError(_("XAudio2: Creating source voice failed! HRESULT: 0x%08X"), hr);
        failed = true;
        delete notify;
        notify = nullptr;
        return false;
    }

    // Setup stereo upmix if enabled
    if (OPTION(kSoundUpmix)) {
        SetupStereoUpmix();
    }

    hr = sVoice->Start(0);
    if (FAILED(hr)) {
        wxLogError(_("XAudio2: Starting source voice failed! HRESULT: 0x%08X"), hr);
        failed = true;
        return false;
    }
    
    playing = true;
    currentBuffer = 0;
    device_changed = false;
    initialized = true;
    return true;
}

void XAudio2_9_Output::write(uint16_t* finalWave, int) {
    if (!initialized || failed)
        return;

    while (true) {
        if (device_changed) {
            close();
            if (!init(freq))
                return;
        }

        sVoice->GetState(&vState, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (vState.BuffersQueued > bufferCount) {
            wxLogWarning(_("XAudio2: Too many buffers queued, resetting"));
            reset();
            continue;
        }

        if (vState.BuffersQueued < bufferCount) {
            if (vState.BuffersQueued == 0) {
                if (systemVerbose & VERBOSE_SOUNDOUTPUT) {
                    static unsigned int i = 0;
                    log("XAudio2: Buffers were not refilled fast enough (i=%i)\n", i++);
                }
            }
            break;
        } else {
            if (!coreOptions.speedup && coreOptions.throttle && !gba_joybus_active) {
                if (WaitForSingleObject(notify->hBufferEndEvent, 10000) == WAIT_TIMEOUT) {
                    device_changed = true;
                }
            } else {
                return;
            }
        }
    }

    CopyMemory(&buffers[currentBuffer * soundBufferLen], finalWave, soundBufferLen);
    
    XAUDIO2_BUFFER newBuf = {};
    newBuf.AudioBytes = soundBufferLen;
    newBuf.pAudioData = &buffers[currentBuffer * soundBufferLen];
    
    HRESULT submit_hr = sVoice->SubmitSourceBuffer(&newBuf);
    if (FAILED(submit_hr)) {
        wxLogError(_("XAudio2: SubmitSourceBuffer failed! HRESULT: 0x%08X"), submit_hr);
        device_changed = true;
        return;
    }

    currentBuffer = (currentBuffer + 1) % (bufferCount + 1);
}

void XAudio2_9_Output::pause() {
    if (!initialized || failed) return;
    if (playing) {
        sVoice->Stop(0);
        playing = false;
    }
}

void XAudio2_9_Output::resume() {
    if (!initialized || failed) return;
    if (!playing) {
        HRESULT hr = sVoice->Start(0);
        if (SUCCEEDED(hr)) {
            playing = true;
        } else {
            wxLogError(_("XAudio2: Failed to resume playback!"));
        }
    }
}

void XAudio2_9_Output::reset() {
    if (!initialized || failed) return;
    if (playing) {
        sVoice->Stop(0);
    }
    sVoice->FlushSourceBuffers();
    HRESULT hr = sVoice->Start(0);
    if (SUCCEEDED(hr)) {
        playing = true;
    }
    currentBuffer = 0;
}

void XAudio2_9_Output::setThrottle(unsigned short throttle_) {
    if (!initialized || failed) return;
    if (throttle_ == 0) throttle_ = 100;
    sVoice->SetFrequencyRatio((float)throttle_ / 100.0f);
}

}  // namespace

std::unique_ptr<SoundDriver> CreateXAudio2_9_Driver(IXAudio2* xaudio2) {
    return std::make_unique<XAudio2_9_Output>(xaudio2);
}

std::vector<audio::AudioDevice> GetXAudio2_9_Devices(IXAudio2* xaudio2) {
    std::vector<audio::AudioDevice> devices;
    devices.push_back({_("Default device"), wxEmptyString});

    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                                  __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* pCollection = nullptr;
        hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
        
        if (SUCCEEDED(hr)) {
            UINT count = 0;
            pCollection->GetCount(&count);
            devices.reserve(count + 1);
            
            for (UINT i = 0; i < count; i++) {
                IMMDevice* pDevice = nullptr;
                hr = pCollection->Item(i, &pDevice);
                
                if (SUCCEEDED(hr)) {
                    LPWSTR pwszID = nullptr;
                    pDevice->GetId(&pwszID);
                    
                    IPropertyStore* pProps = nullptr;
                    pDevice->OpenPropertyStore(STGM_READ, &pProps);
                    
                    if (pProps) {
                        PROPVARIANT varName;
                        PropVariantInit(&varName);
                        
                        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                        if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
                            devices.push_back({wxString(varName.pwszVal), wxString(pwszID)});
                        }
                        
                        PropVariantClear(&varName);
                        pProps->Release();
                    }
                    
                    if (pwszID) {
                        CoTaskMemFree(pwszID);
                    }
                    pDevice->Release();
                }
            }
            
            pCollection->Release();
        }
        
        pEnumerator->Release();
    }

    return devices;
}

}  // namespace internal
}  // namespace audio
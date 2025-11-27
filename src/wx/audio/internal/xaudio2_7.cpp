#if !defined(VBAM_ENABLE_XAUDIO2)
#error "This file should only be compiled if XAudio2 is enabled"
#endif

#include "xaudio2_7.h"

#include <cstdio>
#include <string>
#include <vector>

#include <wx/arrstr.h>
#include <wx/log.h>
#include <wx/translation.h>

#include "core/base/sound_driver.h"
#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "wx/config/option-proxy.h"

namespace audio {
namespace internal {

namespace {

class XAudio2_7_Output : public SoundDriver {
public:
    XAudio2_7_Output(IXAudio2* xaudio2);
    ~XAudio2_7_Output() override;

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
    int XA2GetDev();
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

int XAudio2_7_Output::XA2GetDev() {
    const wxString& audio_device = OPTION(kSoundAudioDevice);
    if (audio_device.empty()) {
        return 0;
    }

    UINT32 dev_count = 0;
    HRESULT hr = xaud->GetDeviceCount(&dev_count);
    if (FAILED(hr)) {
        wxLogError(_("XAudio2: Enumerating devices failed!"));
        return 0;
    }

    for (UINT32 i = 0; i < dev_count; i++) {
        XAUDIO2_DEVICE_DETAILS dd;
        hr = xaud->GetDeviceDetails(i, &dd);
        if (FAILED(hr)) {
            continue;
        }
        const wxString device_id = wxString(dd.DeviceID);
        if (audio_device == device_id) {
            return i;
        }
    }

    wxLogWarning(_("XAudio2: Device not found, using default"));
    return 0;
}

bool XAudio2_7_Output::SetupStereoUpmix() {
    if (!OPTION(kSoundUpmix)) {
        return true; // Upmix not enabled
    }

    // Get device details to determine channel count
    XAUDIO2_DEVICE_DETAILS dd;
    HRESULT hr = xaud->GetDeviceDetails(XA2GetDev(), &dd);
    if (FAILED(hr)) {
        wxLogWarning(_("XAudio2: Could not get device details for upmix"));
        return false;
    }

    UINT32 outputChannels = dd.OutputFormat.Format.nChannels;
    
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

    // Set up stereo upmixing matrix based on channel count
    switch (outputChannels) {
        case 4:  // 4.0 (Quad)
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 1.0000f;
            /*Back  L*/ matrix[4] = 1.0000f; matrix[5] = 0.0000f;
            /*Back  R*/ matrix[6] = 0.0000f; matrix[7] = 1.0000f;
            break;

        case 5:  // 5.0
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 1.0000f;
            /*Front C*/ matrix[4] = 0.7071f; matrix[5] = 0.7071f;
            /*Side  L*/ matrix[6] = 1.0000f; matrix[7] = 0.0000f;
            /*Side  R*/ matrix[8] = 0.0000f; matrix[9] = 1.0000f;
            break;

        case 6:  // 5.1
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 1.0000f;
            /*Front C*/ matrix[4] = 0.7071f; matrix[5] = 0.7071f;
            /*LFE    */ matrix[6] = 0.0000f; matrix[7] = 0.0000f;
            /*Side  L*/ matrix[8] = 1.0000f; matrix[9] = 0.0000f;
            /*Side  R*/ matrix[10] = 0.0000f; matrix[11] = 1.0000f;
            break;

        case 7:  // 6.1
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 1.0000f;
            /*Front C*/ matrix[4] = 0.7071f; matrix[5] = 0.7071f;
            /*LFE    */ matrix[6] = 0.0000f; matrix[7] = 0.0000f;
            /*Side  L*/ matrix[8] = 1.0000f; matrix[9] = 0.0000f;
            /*Side  R*/ matrix[10] = 0.0000f; matrix[11] = 1.0000f;
            /*Back  C*/ matrix[12] = 0.7071f; matrix[13] = 0.7071f;
            break;

        case 8:  // 7.1
            // Speaker \ Left Source           Right Source
            /*Front L*/ matrix[0] = 1.0000f; matrix[1] = 0.0000f;
            /*Front R*/ matrix[2] = 0.0000f; matrix[3] = 1.0000f;
            /*Front C*/ matrix[4] = 0.7071f; matrix[5] = 0.7071f;
            /*LFE    */ matrix[6] = 0.0000f; matrix[7] = 0.0000f;
            /*Back  L*/ matrix[8] = 1.0000f; matrix[9] = 0.0000f;
            /*Back  R*/ matrix[10] = 0.0000f; matrix[11] = 1.0000f;
            /*Side  L*/ matrix[12] = 1.0000f; matrix[13] = 0.0000f;
            /*Side  R*/ matrix[14] = 0.0000f; matrix[15] = 1.0000f;
            break;

        default:
            matrixAvailable = false;
            wxLogWarning(_("XAudio2: Unsupported channel count for upmix: %d"), outputChannels);
            break;
    }

    if (matrixAvailable) {
        hr = sVoice->SetOutputMatrix(NULL, 2, outputChannels, matrix);
        if (FAILED(hr)) {
            wxLogWarning(_("XAudio2: Failed to set output matrix for upmix"));
            matrixAvailable = false;
        } else {
            wxLogInfo(_("XAudio2: Stereo upmix enabled for %d channels"), outputChannels);
        }
    }

    free(matrix);
    return matrixAvailable;
}

XAudio2_7_Output::XAudio2_7_Output(IXAudio2* xaudio2) : xaud(xaudio2) {
    if (!xaudio2) {
        wxLogError(_("XAudio2: Null XAudio2 instance passed to driver!"));
        failed = true;
        return;
    }
    
    bufferCount = OPTION(kSoundBuffers);
    if (bufferCount < 2 || bufferCount > 16) {
        bufferCount = 3;
    }
}

XAudio2_7_Output::~XAudio2_7_Output() {
    close();
}

void XAudio2_7_Output::close() {
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

void XAudio2_7_Output::device_change() {
    device_changed = true;
}

bool XAudio2_7_Output::init(long sampleRate) {
    if (failed || initialized)
        return false;

    if (!xaud) {
        wxLogError(_("XAudio2 instance not available!"));
        failed = true;
        return false;
    }

    // Verify the XAudio2 instance is still valid
    UINT32 dev_count = 0;
    HRESULT hr_check = xaud->GetDeviceCount(&dev_count);
    if (FAILED(hr_check)) {
        wxLogError(_("XAudio2 instance appears to be invalid"));
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
    
    // Create mastering voice - XAudio2 2.7 uses device index
    int deviceIndex = XA2GetDev();
    
    HRESULT hr = xaud->CreateMasteringVoice(&mVoice, 
                                           XAUDIO2_DEFAULT_CHANNELS, 
                                           freq,  // Use actual sample rate
                                           0, 
                                           deviceIndex, 
                                           NULL);
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

void XAudio2_7_Output::write(uint16_t* finalWave, int) {
    if (!initialized || failed)
        return;

    while (true) {
        if (device_changed) {
            close();
            if (!init(freq))
                return;
        }

        sVoice->GetState(&vState);
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

void XAudio2_7_Output::pause() {
    if (!initialized || failed) return;
    if (playing) {
        sVoice->Stop(0);
        playing = false;
    }
}

void XAudio2_7_Output::resume() {
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

void XAudio2_7_Output::reset() {
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

void XAudio2_7_Output::setThrottle(unsigned short throttle_) {
    if (!initialized || failed) return;
    if (throttle_ == 0) throttle_ = 100;
    sVoice->SetFrequencyRatio((float)throttle_ / 100.0f);
}

}  // namespace

std::unique_ptr<SoundDriver> CreateXAudio2_7_Driver(IXAudio2* xaudio2) {
    if (!xaudio2) {
        wxLogError(_("XAudio2: Cannot create driver with null XAudio2 instance"));
        return nullptr;
    }
    
    // Verify the instance is working by checking device count
    UINT32 dev_count = 0;
    HRESULT hr = xaudio2->GetDeviceCount(&dev_count);
    if (FAILED(hr)) {
        wxLogError(_("XAudio2: XAudio2 instance appears to be invalid"));
        return nullptr;
    }
    
    return std::make_unique<XAudio2_7_Output>(xaudio2);
}

std::vector<audio::AudioDevice> GetXAudio2_7_Devices(IXAudio2* xaudio2) {
    if (!xaudio2) {
        return {};
    }

    std::vector<audio::AudioDevice> devices;
    
    try {
        UINT32 dev_count = 0;
        HRESULT hr = xaudio2->GetDeviceCount(&dev_count);
        if (FAILED(hr)) {
            wxLogError(_("XAudio2: Enumerating devices failed!"));
            return {};
        }

        devices.reserve(dev_count + 1);
        devices.push_back({_("Default device"), wxEmptyString});

        for (UINT32 i = 0; i < dev_count; i++) {
            XAUDIO2_DEVICE_DETAILS dd;
            hr = xaudio2->GetDeviceDetails(i, &dd);
            if (SUCCEEDED(hr)) {
                devices.push_back({wxString(dd.DisplayName), wxString(dd.DeviceID)});
            }
        }
    } catch (...) {
        wxLogError(_("XAudio2: Exception during device enumeration"));
        return {};
    }

    return devices;
}

}  // namespace internal
}  // namespace audio
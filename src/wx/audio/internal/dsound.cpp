#if !defined(__WXMSW__)
#error "This file should only be compiled on Windows"
#endif

#include "wx/audio/internal/dsound.h"

// DirectSound8
#define DIRECTSOUND_VERSION 0x0800
#include <Windows.h>
#include <mmeapi.h>

#include <dsound.h>
#include <uuids.h>

#include <wx/arrstr.h>
#include <wx/log.h>
#include <wx/translation.h>

// Internals
#include "core/base/sound_driver.h"
#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "wx/config/option-proxy.h"
#include "wx/wxvbam.h"

extern bool soundBufferLow;

namespace audio {
namespace internal {

namespace {

class DirectSound : public SoundDriver {
private:
    LPDIRECTSOUND8 pDirectSound;       // DirectSound interface
    LPDIRECTSOUNDBUFFER dsbPrimary;    // Primary DirectSound buffer
    LPDIRECTSOUNDBUFFER dsbSecondary;  // Secondary DirectSound buffer
    LPDIRECTSOUNDNOTIFY dsbNotify;
    HANDLE dsbEvent;
    WAVEFORMATEX wfx;  // Primary buffer wave format
    int soundBufferLen;
    int soundBufferTotalLen;
    unsigned int soundNextPosition;

public:
    DirectSound();
    ~DirectSound() override;

    // SoundDriver implementation.
    bool init(long sampleRate) override;
    void pause() override;
    void reset() override;
    void resume() override;
    void write(uint16_t* finalWave, int length) override;
    void setThrottle(unsigned short throttle_) override;
};

DirectSound::DirectSound() {
    pDirectSound = NULL;
    dsbPrimary = NULL;
    dsbSecondary = NULL;
    dsbNotify = NULL;
    dsbEvent = NULL;
    soundBufferTotalLen = 14700;
    soundNextPosition = 0;
}

DirectSound::~DirectSound() {
    if (dsbNotify) {
        dsbNotify->Release();
        dsbNotify = NULL;
    }

    if (dsbEvent) {
        CloseHandle(dsbEvent);
        dsbEvent = NULL;
    }

    if (pDirectSound) {
        if (dsbPrimary) {
            dsbPrimary->Release();
            dsbPrimary = NULL;
        }

        if (dsbSecondary) {
            dsbSecondary->Release();
            dsbSecondary = NULL;
        }

        pDirectSound->Release();
        pDirectSound = NULL;
    }
}

bool DirectSound::init(long sampleRate) {
    HRESULT hr;
    DWORD freq;
    DSBUFFERDESC dsbdesc;
    int i;
    hr = CoCreateInstance(CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, IID_IDirectSound8,
                          (LPVOID*)&pDirectSound);

    if (hr != S_OK) {
        wxLogError(_("Cannot create Direct Sound %08x"), hr);
        return false;
    }

    GUID dev;

    const wxString& audio_device = OPTION(kSoundAudioDevice);
    if (audio_device.empty())
        dev = DSDEVID_DefaultPlayback;
    else
        CLSIDFromString(audio_device.wc_str(), &dev);

    pDirectSound->Initialize(&dev);

    if (hr != DS_OK) {
        wxLogError(_("Cannot create Direct Sound %08x"), hr);
        return false;
    }

    if (FAILED(hr = pDirectSound->SetCooperativeLevel((HWND)wxGetApp().frame->GetHandle(),
                                                      DSSCL_PRIORITY))) {
        wxLogError(_("Cannot SetCooperativeLevel %08x"), hr);
        return false;
    }

    // Create primary sound buffer
    ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    const bool hw_accel = OPTION(kSoundDSoundHWAccel);
    if (!hw_accel) {
        dsbdesc.dwFlags |= DSBCAPS_LOCSOFTWARE;
    }

    if (FAILED(hr = pDirectSound->CreateSoundBuffer(&dsbdesc, &dsbPrimary, NULL))) {
        wxLogError(_("Cannot CreateSoundBuffer %08x"), hr);
        return false;
    }

    freq = sampleRate;
    // calculate the number of samples per frame first
    // then multiply it with the size of a sample frame (16 bit * stereo)
    soundBufferLen = (freq / 60) * 4;
    soundBufferTotalLen = soundBufferLen * 10;
    soundNextPosition = 0;
    ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = freq;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (FAILED(hr = dsbPrimary->SetFormat(&wfx))) {
        wxLogError(_("CreateSoundBuffer(primary) failed %08x"), hr);
        return false;
    }

    // Create secondary sound buffer
    ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY |
                      DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLFREQUENCY;

    if (!hw_accel) {
        dsbdesc.dwFlags |= DSBCAPS_LOCSOFTWARE;
    }

    dsbdesc.dwBufferBytes = soundBufferTotalLen;
    dsbdesc.lpwfxFormat = &wfx;

    if (FAILED(hr = pDirectSound->CreateSoundBuffer(&dsbdesc, &dsbSecondary, NULL))) {
        wxLogError(_("CreateSoundBuffer(secondary) failed %08x"), hr);
        return false;
    }

    if (FAILED(hr = dsbSecondary->SetCurrentPosition(0))) {
        wxLogError(_("dsbSecondary->SetCurrentPosition failed %08x"), hr);
        return false;
    }

    if (SUCCEEDED(hr =
                      dsbSecondary->QueryInterface(IID_IDirectSoundNotify8, (LPVOID*)&dsbNotify))) {
        dsbEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        DSBPOSITIONNOTIFY notify[10];

        for (i = 0; i < 10; i++) {
            notify[i].dwOffset = i * soundBufferLen;
            notify[i].hEventNotify = dsbEvent;
        }

        if (FAILED(dsbNotify->SetNotificationPositions(10, notify))) {
            dsbNotify->Release();
            dsbNotify = NULL;
            CloseHandle(dsbEvent);
            dsbEvent = NULL;
        }
    }

    // Play primary buffer
    if (FAILED(hr = dsbPrimary->Play(0, 0, DSBPLAY_LOOPING))) {
        wxLogError(_("Cannot Play primary %08x"), hr);
        return false;
    }

    return true;
}

void DirectSound::setThrottle(unsigned short throttle_) {
    HRESULT hr;

    if (throttle_ == 0)
        throttle_ = 450;  // Close to upper bound on frequency.

    long freq = soundGetSampleRate();

    if (FAILED(hr = dsbSecondary->SetFrequency(freq * (throttle_ / 100.0)))) {
        wxLogDebug(wxT("Cannot SetFrequency %ld: %08x"), (long)(freq * (throttle_ / 100.0)), hr);
    }
}

void DirectSound::pause() {
    LPDIRECTSOUNDBUFFER bufs[] = {dsbPrimary, dsbSecondary};
    for (auto buf : bufs) {
        if (buf == NULL)
            continue;

        DWORD status;
        buf->GetStatus(&status);

        if (status & DSBSTATUS_PLAYING)
            buf->Stop();
    }
}

void DirectSound::reset() {
    if (dsbSecondary == NULL)
        return;

    dsbSecondary->Stop();
    dsbSecondary->SetCurrentPosition(0);
    soundNextPosition = 0;
}

void DirectSound::resume() {
    LPDIRECTSOUNDBUFFER bufs[] = {dsbPrimary, dsbSecondary};
    for (auto buf : bufs) {
        if (buf == NULL)
            return;

        buf->Play(0, 0, DSBPLAY_LOOPING);
    }
}

void DirectSound::write(uint16_t* finalWave, int) {
    if (!pDirectSound)
        return;

    HRESULT hr;
    DWORD status = 0;
    DWORD play = 0;
    LPVOID lpvPtr1;
    DWORD dwBytes1 = 0;
    LPVOID lpvPtr2;
    DWORD dwBytes2 = 0;

    if (!coreOptions.speedup && coreOptions.throttle && !gba_joybus_active) {
        hr = dsbSecondary->GetStatus(&status);

        if (status & DSBSTATUS_PLAYING) {
            if (!soundPaused) {
                while (true) {
                    dsbSecondary->GetCurrentPosition(&play, NULL);
                    int BufferLeft = ((soundNextPosition <= play)
                                          ? play - soundNextPosition
                                          : soundBufferTotalLen - soundNextPosition + play);

                    if (BufferLeft > soundBufferLen) {
                        if (BufferLeft > soundBufferTotalLen - (soundBufferLen * 3))
                            soundBufferLow = true;

                        break;
                    }

                    soundBufferLow = false;

                    if (dsbEvent) {
                        WaitForSingleObject(dsbEvent, 50);
                    }
                }
            }
        } /* else {

         // TODO: remove?
             setsoundPaused(true);
        }*/
    }

    // Obtain memory address of write block.
    // This will be in two parts if the block wraps around.
    if (DSERR_BUFFERLOST == (hr = dsbSecondary->Lock(soundNextPosition, soundBufferLen, &lpvPtr1,
                                                     &dwBytes1, &lpvPtr2, &dwBytes2, 0))) {
        // If DSERR_BUFFERLOST is returned, restore and retry lock.
        dsbSecondary->Restore();
        hr = dsbSecondary->Lock(soundNextPosition, soundBufferLen, &lpvPtr1, &dwBytes1, &lpvPtr2,
                                &dwBytes2, 0);
    }

    soundNextPosition += soundBufferLen;
    soundNextPosition = soundNextPosition % soundBufferTotalLen;

    if (SUCCEEDED(hr)) {
        // Write to pointers.
        CopyMemory(lpvPtr1, finalWave, dwBytes1);

        if (lpvPtr2) {
            CopyMemory(lpvPtr2, finalWave + dwBytes1, dwBytes2);
        }

        // Release the data back to DirectSound.
        hr = dsbSecondary->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);
    } else {
        wxLogError(_("dsbSecondary->Lock() failed: %08x"), hr);
        return;
    }
}

static BOOL CALLBACK DSEnumCB(LPGUID guid, LPCTSTR desc, LPCTSTR /*module*/, LPVOID user) {
    std::vector<AudioDevice>* devices = static_cast<std::vector<AudioDevice>*>(user);

    if (guid == NULL) {
        devices->push_back({desc, {}});
        return TRUE;
    }

    static constexpr size_t kGuidLength = 32 + 4 + 2 + 1;  // hex digits + "-" + "{}" + \0
    std::array<WCHAR, kGuidLength> device_id;
    StringFromGUID2(*guid, device_id.data(), device_id.size());

    devices->push_back({desc, device_id.data()});
    return TRUE;
}

}  // namespace

std::vector<AudioDevice> GetDirectSoundDevices() {
    std::vector<AudioDevice> devices;
    DirectSoundEnumerateW(DSEnumCB, &devices);
    return devices;
}

std::unique_ptr<SoundDriver> CreateDirectSoundDriver() {
    return std::make_unique<DirectSound>();
}

}  // namespace internal
}  // namespace audio

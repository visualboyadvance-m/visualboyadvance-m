// Application
#include "wxvbam.h"

// Internals
#include "../System.h"
#include "../common/SoundDriver.h"
#include "../gba/GBA.h"
#include "../gba/Globals.h"
#include "../gba/Sound.h"

// DirectSound8
#define DIRECTSOUND_VERSION 0x0800
#include <dsound.h>

extern bool soundBufferLow;

class DirectSound : public SoundDriver {
private:
    LPDIRECTSOUND8 pDirectSound; // DirectSound interface
    LPDIRECTSOUNDBUFFER dsbPrimary; // Primary DirectSound buffer
    LPDIRECTSOUNDBUFFER dsbSecondary; // Secondary DirectSound buffer
    LPDIRECTSOUNDNOTIFY dsbNotify;
    HANDLE dsbEvent;
    WAVEFORMATEX wfx; // Primary buffer wave format
    int soundBufferLen;
    int soundBufferTotalLen;
    unsigned int soundNextPosition;

public:
    DirectSound();
    virtual ~DirectSound();

    bool init(long sampleRate); // initialize the primary and secondary sound buffer
    void setThrottle(unsigned short throttle_); // set game speed
    void pause(); // pause the secondary sound buffer
    void reset(); // stop and reset the secondary sound buffer
    void resume(); // resume the secondary sound buffer
    void write(uint16_t* finalWave, int length); // write the emulated sound to the secondary sound buffer
};

DirectSound::DirectSound()
{
    pDirectSound = NULL;
    dsbPrimary = NULL;
    dsbSecondary = NULL;
    dsbNotify = NULL;
    dsbEvent = NULL;
    soundBufferTotalLen = 14700;
    soundNextPosition = 0;
}

DirectSound::~DirectSound()
{
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

bool DirectSound::init(long sampleRate)
{
    HRESULT hr;
    DWORD freq;
    DSBUFFERDESC dsbdesc;
    int i;
    hr = CoCreateInstance(CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, IID_IDirectSound8, (LPVOID*)&pDirectSound);

    if (hr != S_OK) {
        wxLogError(_("Cannot create DirectSound %08x"), hr);
        return false;
    }

    GUID dev;

    if (gopts.audio_dev.empty())
        dev = DSDEVID_DefaultPlayback;
    else
        CLSIDFromString(gopts.audio_dev.wc_str(), &dev);

    pDirectSound->Initialize(&dev);

    if (hr != DS_OK) {
        wxLogError(_("Cannot create DirectSound %08x"), hr);
        return false;
    }

    if (FAILED(hr = pDirectSound->SetCooperativeLevel((HWND)wxGetApp().frame->GetHandle(), DSSCL_EXCLUSIVE))) {
        wxLogError(_("Cannot SetCooperativeLevel %08x"), hr);
        return false;
    }

    // Create primary sound buffer
    ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    if (!gopts.dsound_hw_accel) {
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
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLFREQUENCY;

    if (!gopts.dsound_hw_accel) {
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

    if (SUCCEEDED(hr = dsbSecondary->QueryInterface(IID_IDirectSoundNotify8, (LPVOID*)&dsbNotify))) {
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
        throttle_ = 450; // Close to upper bound on frequency.

    long freq = soundGetSampleRate();

    if (FAILED(hr = dsbSecondary->SetFrequency(freq * (throttle_ / 100.0)))) {
        wxLogDebug(wxT("Cannot SetFrequency %ld: %08x"), (long)(freq * (throttle_ / 100.0)), hr);
    }
}

void DirectSound::pause()
{
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

void DirectSound::reset()
{
    if (dsbSecondary == NULL)
        return;

    dsbSecondary->Stop();
    dsbSecondary->SetCurrentPosition(0);
    soundNextPosition = 0;
}

void DirectSound::resume()
{
    LPDIRECTSOUNDBUFFER bufs[] = {dsbPrimary, dsbSecondary};
    for (auto buf : bufs) {
        if (buf == NULL)
            return;

        buf->Play(0, 0, DSBPLAY_LOOPING);
    }
}

void DirectSound::write(uint16_t* finalWave, int length)
{
    if (!pDirectSound)
        return;

    HRESULT hr;
    DWORD status = 0;
    DWORD play = 0;
    LPVOID lpvPtr1;
    DWORD dwBytes1 = 0;
    LPVOID lpvPtr2;
    DWORD dwBytes2 = 0;

    if (!speedup && throttle && !gba_joybus_active) {
        hr = dsbSecondary->GetStatus(&status);

        if (status & DSBSTATUS_PLAYING) {
            if (!soundPaused) {
                while (true) {
                    dsbSecondary->GetCurrentPosition(&play, NULL);
                    int BufferLeft = ((soundNextPosition <= play) ? play - soundNextPosition : soundBufferTotalLen - soundNextPosition + play);

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
    if (DSERR_BUFFERLOST == (hr = dsbSecondary->Lock(
                                 soundNextPosition,
                                 soundBufferLen,
                                 &lpvPtr1,
                                 &dwBytes1,
                                 &lpvPtr2,
                                 &dwBytes2,
                                 0))) {
        // If DSERR_BUFFERLOST is returned, restore and retry lock.
        dsbSecondary->Restore();
        hr = dsbSecondary->Lock(
            soundNextPosition,
            soundBufferLen,
            &lpvPtr1,
            &dwBytes1,
            &lpvPtr2,
            &dwBytes2,
            0);
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

SoundDriver* newDirectSound()
{
    return new DirectSound();
}

struct devnames {
    wxArrayString *names, *ids;
};

static BOOL CALLBACK DSEnumCB(LPGUID guid, LPCTSTR desc, LPCTSTR drvnam, LPVOID user)
{
    devnames* dn = (devnames*)user;
    dn->names->push_back(desc);
    WCHAR buf[32 + 4 + 2 + 1]; // hex digits + "-" + "{}" + \0
    StringFromGUID2(*guid, buf, sizeof(buf));
    dn->ids->push_back(buf);
    return TRUE;
}

bool GetDSDevices(wxArrayString& names, wxArrayString& ids)
{
    devnames dn = { &names, &ids };
    return DirectSoundEnumerate(DSEnumCB, (LPVOID)&dn) == DS_OK;
}

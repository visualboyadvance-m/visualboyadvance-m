#if !defined(_WIN32)
#error "This file should only be compiled on Windows"
#endif

#include "qt/audio/internal/dsound.h"

// DirectSound8
#define DIRECTSOUND_VERSION 0x0800
#include <windows.h>
#include <mmeapi.h>

#include <dsound.h>
#include <uuids.h>

#include <array>

#include <QCoreApplication>
#include <QString>
#include <QWidget>

// Internals
#include "core/base/sound_driver.h"
#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/log.h"
#include "qt/main-window.h"

extern bool soundBufferLow;

namespace audio {
namespace internal {

namespace {

QString Tr(const char* text) {
    return QCoreApplication::translate("vbam", text);
}

QString HrString(HRESULT hr) {
    return QStringLiteral("%1").arg(static_cast<unsigned long>(hr), 8, 16, QLatin1Char('0'));
}

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
    pDirectSound = nullptr;
    dsbPrimary = nullptr;
    dsbSecondary = nullptr;
    dsbNotify = nullptr;
    dsbEvent = nullptr;
    soundBufferTotalLen = 14700;
    soundNextPosition = 0;
}

DirectSound::~DirectSound() {
    if (dsbNotify) {
        dsbNotify->Release();
        dsbNotify = nullptr;
    }

    if (dsbEvent) {
        CloseHandle(dsbEvent);
        dsbEvent = nullptr;
    }

    if (pDirectSound) {
        if (dsbPrimary) {
            dsbPrimary->Release();
            dsbPrimary = nullptr;
        }

        if (dsbSecondary) {
            dsbSecondary->Release();
            dsbSecondary = nullptr;
        }

        pDirectSound->Release();
        pDirectSound = nullptr;
    }
}

bool DirectSound::init(long sampleRate) {
    HRESULT hr;
    DWORD freq;
    DSBUFFERDESC dsbdesc;
    int i;
    hr = CoCreateInstance(CLSID_DirectSound8, nullptr, CLSCTX_INPROC_SERVER, IID_IDirectSound8,
                          (LPVOID*)&pDirectSound);

    if (hr != S_OK) {
        vbam::LogError(Tr("Cannot create Direct Sound %1").arg(HrString(hr)));
        return false;
    }

    GUID dev;

    const QString audio_device = OPTION(kSoundAudioDevice);
    if (audio_device.isEmpty() || audio_device == audio::DefaultDeviceName())
        dev = DSDEVID_DefaultPlayback;
    else
        CLSIDFromString(reinterpret_cast<LPCOLESTR>(audio_device.utf16()), &dev);

    hr = pDirectSound->Initialize(&dev);

    if (hr != DS_OK) {
        vbam::LogError(Tr("Cannot create Direct Sound %1").arg(HrString(hr)));
        return false;
    }

    HWND hwnd = nullptr;
    if (vbamApp().frame) {
        hwnd = reinterpret_cast<HWND>(vbamApp().frame->winId());
    }
    if (!hwnd) {
        hwnd = GetDesktopWindow();
    }

    if (FAILED(hr = pDirectSound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY))) {
        vbam::LogError(Tr("Cannot SetCooperativeLevel %1").arg(HrString(hr)));
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

    if (FAILED(hr = pDirectSound->CreateSoundBuffer(&dsbdesc, &dsbPrimary, nullptr))) {
        vbam::LogError(Tr("Cannot CreateSoundBuffer %1").arg(HrString(hr)));
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
        vbam::LogError(Tr("CreateSoundBuffer(primary) failed %1").arg(HrString(hr)));
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

    if (FAILED(hr = pDirectSound->CreateSoundBuffer(&dsbdesc, &dsbSecondary, nullptr))) {
        vbam::LogError(Tr("CreateSoundBuffer(secondary) failed %1").arg(HrString(hr)));
        return false;
    }

    if (FAILED(hr = dsbSecondary->SetCurrentPosition(0))) {
        vbam::LogError(Tr("dsbSecondary->SetCurrentPosition failed %1").arg(HrString(hr)));
        return false;
    }

    if (SUCCEEDED(hr = dsbSecondary->QueryInterface(IID_IDirectSoundNotify8,
                                                    (LPVOID*)&dsbNotify))) {
        dsbEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        DSBPOSITIONNOTIFY notify[10];

        for (i = 0; i < 10; i++) {
            notify[i].dwOffset = i * soundBufferLen;
            notify[i].hEventNotify = dsbEvent;
        }

        if (FAILED(dsbNotify->SetNotificationPositions(10, notify))) {
            dsbNotify->Release();
            dsbNotify = nullptr;
            CloseHandle(dsbEvent);
            dsbEvent = nullptr;
        }
    }

    // Play primary buffer
    if (FAILED(hr = dsbPrimary->Play(0, 0, DSBPLAY_LOOPING))) {
        vbam::LogError(Tr("Cannot Play primary %1").arg(HrString(hr)));
        return false;
    }

    // Verify the secondary buffer's play cursor actually advances. Some
    // endpoints (seen with certain virtual / basic-render audio devices) report
    // DSBSTATUS_PLAYING but never move the cursor; write() would then block
    // forever draining a buffer that never empties, hanging emulation. Detect
    // that here so soundInit() can fall back to the null driver instead. The
    // buffer is silent and reset afterwards, so normal playback is unaffected.
    if (FAILED(hr = dsbSecondary->Play(0, 0, DSBPLAY_LOOPING))) {
        vbam::LogError(Tr("Cannot Play secondary %1").arg(HrString(hr)));
        return false;
    }
    {
        DWORD probe_start = 0, probe_now = 0;
        dsbSecondary->GetCurrentPosition(&probe_start, nullptr);
        bool advanced = false;
        for (int probe = 0; probe < 10 && !advanced; ++probe) {
            Sleep(20);  // up to ~200ms total before declaring the device stalled
            if (SUCCEEDED(dsbSecondary->GetCurrentPosition(&probe_now, nullptr)) &&
                probe_now != probe_start)
                advanced = true;
        }
        dsbSecondary->Stop();
        dsbSecondary->SetCurrentPosition(0);
        soundNextPosition = 0;
        if (!advanced) {
            // Silent: soundInit() falls back to the null driver. Log only to
            // the debug log so no error/warning dialog is shown to the user.
            vbam::LogDebug(QStringLiteral(
                "DirectSound playback cursor not advancing; falling back to the null sound driver."));
            return false;
        }
    }

    return true;
}

void DirectSound::setThrottle(unsigned short throttle_) {
    HRESULT hr;

    if (throttle_ == 0)
        throttle_ = 450;  // Close to upper bound on frequency.

    long freq = soundGetSampleRate();

    if (FAILED(hr = dsbSecondary->SetFrequency(freq * (throttle_ / 100.0)))) {
        vbam::LogDebug(QStringLiteral("Cannot SetFrequency %1: %2")
                           .arg((long)(freq * (throttle_ / 100.0)))
                           .arg(HrString(hr)));
    }
}

void DirectSound::pause() {
    LPDIRECTSOUNDBUFFER bufs[] = {dsbPrimary, dsbSecondary};
    for (auto buf : bufs) {
        if (buf == nullptr)
            continue;

        DWORD status;
        buf->GetStatus(&status);

        if (status & DSBSTATUS_PLAYING)
            buf->Stop();
    }
}

void DirectSound::reset() {
    if (dsbSecondary == nullptr)
        return;

    dsbSecondary->Stop();
    dsbSecondary->SetCurrentPosition(0);
    soundNextPosition = 0;
}

void DirectSound::resume() {
    LPDIRECTSOUNDBUFFER bufs[] = {dsbPrimary, dsbSecondary};
    for (auto buf : bufs) {
        if (buf == nullptr)
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
                    dsbSecondary->GetCurrentPosition(&play, nullptr);
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
        }
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
        vbam::LogError(Tr("dsbSecondary->Lock() failed: %1").arg(HrString(hr)));
        return;
    }
}

static BOOL CALLBACK DSEnumCB(LPGUID guid, LPCWSTR desc, LPCWSTR /*module*/, LPVOID user) {
    std::vector<AudioDevice>* devices = static_cast<std::vector<AudioDevice>*>(user);

    if (guid == nullptr) {
        devices->push_back({QString::fromWCharArray(desc), {}});
        return TRUE;
    }

    static constexpr size_t kGuidLength = 32 + 4 + 2 + 1;  // hex digits + "-" + "{}" + \0
    std::array<WCHAR, kGuidLength> device_id;
    StringFromGUID2(*guid, device_id.data(), static_cast<int>(device_id.size()));

    devices->push_back({QString::fromWCharArray(desc), QString::fromWCharArray(device_id.data())});
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

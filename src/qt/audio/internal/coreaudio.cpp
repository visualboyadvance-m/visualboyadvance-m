#ifdef __APPLE__
#include "qt/audio/internal/coreaudio.h"

// === LOGALL writes very detailed informations to stderr ===
// #define LOGALL

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <memory.h>
#include <stdlib.h>

#include <chrono>
#include <mutex>
#include <thread>

#include <QCoreApplication>
#include <QString>

#include "core/base/check.h"
#include "core/base/ringbuffer.h"
#include "core/base/sound_driver.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "qt/config/option-proxy.h"
#include "qt/log.h"

// kAudioObjectPropertyElementMain is an enumerator (not a macro) in the macOS
// 12+ SDKs; older SDKs only have the deprecated ...ElementMaster spelling.
#include <AvailabilityMacros.h>
#if !defined(MAC_OS_VERSION_12_0) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_12_0
#define kAudioObjectPropertyElementMain kAudioObjectPropertyElementMaster
#endif

#ifndef LOGALL
// replace logging functions with comments
#ifdef winlog
#undef winlog
#endif
// https://stackoverflow.com/a/1306690/262458
#define winlog(x, ...) \
    do {               \
    } while (0)
#define debugState()  //
#else
#include <cstdio>
#define winlog(...) fprintf(stderr, __VA_ARGS__)
#endif

extern int emulating;

namespace audio {
namespace internal {

namespace {

static const AudioObjectPropertyAddress devlist_address = {
    kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress addr = {kAudioDevicePropertyStreamConfiguration,
                                         kAudioDevicePropertyScopeOutput,
                                         kAudioObjectPropertyElementMain};

const AudioObjectPropertyAddress nameaddr = {kAudioObjectPropertyName,
                                             kAudioDevicePropertyScopeOutput,
                                             kAudioObjectPropertyElementMain};

static const AudioObjectPropertyAddress alive_address = {
    kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain};

// Converts a CFStringRef to a QString.
QString CFStringToQString(CFStringRef cfstr) {
    if (!cfstr)
        return QString();
    const CFIndex len =
        CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8);
    std::string buf(static_cast<size_t>(len) + 1, '\0');
    if (!CFStringGetCString(cfstr, buf.data(), len + 1, kCFStringEncodingUTF8))
        return QString();
    return QString::fromUtf8(buf.c_str());
}

class CoreAudioAudio : public SoundDriver {
public:
    CoreAudioAudio();
    ~CoreAudioAudio() override;

    bool init(long sampleRate) override;                  // initialize the sound buffer queue
    void deinit();
    void setThrottle(unsigned short throttle_) override;  // set game speed
    void pause() override;                                // pause the secondary sound buffer
    void reset() override;   // stop and reset the secondary sound buffer
    void resume() override;  // play/resume the secondary sound buffer
    void write(uint16_t* finalWave, int length) override;  // write the emulated sound to a sound buffer

    AudioStreamBasicDescription description;
    AudioQueueBufferRef* buffers = nullptr;
    AudioQueueRef audioQueue = nullptr;
    AudioDeviceID device = 0;
    uint16_t current_rate = 0;
    int current_buffer = 0;
    int filled_buffers = 0;
    int soundBufferLen = 0;
    AudioTimeStamp starttime;
    AudioTimeStamp timestamp;
    AudioQueueTimelineRef timeline = nullptr;
    std::mutex buffer_mutex;

private:
    AudioDeviceID GetCoreAudioDevice(const QString& name);
    void setBuffer(uint16_t* finalWave, int length);

    bool initialized = false;
};

static void PlaybackBufferReadyCallback(void* inUserData,
                                        AudioQueueRef inAQ,
                                        AudioQueueBufferRef inBuffer) {
    int curbuf = 0;
    CoreAudioAudio* cadevice = (CoreAudioAudio*)inUserData;
    (void)inAQ;

    // Safety check: if buffers is NULL, we're shutting down - just return
    if (cadevice->buffers == nullptr) {
        return;
    }

    // Find which buffer this is
    for (curbuf = 0; curbuf < OPTION(kSoundBuffers); curbuf++) {
        if (cadevice->buffers[curbuf] == inBuffer) {
            break;
        }
    }

    if (curbuf >= OPTION(kSoundBuffers)) {
        // Not on the GUI thread; no dialog here.
        fprintf(stderr, "PlaybackBufferReadyCallback - buffer not found in array\n");
        return;
    }

    // Idiomatic CoreAudio: just reset the buffer size to mark it as empty.
    // The buffer is reused, not freed and reallocated.
    cadevice->buffers[curbuf]->mAudioDataByteSize = 0;

    // Decrement filled buffers count so write() knows this buffer is available
    std::lock_guard<std::mutex> lock(cadevice->buffer_mutex);
    if (cadevice->filled_buffers > 0) {
        cadevice->filled_buffers--;
    }
}

static OSStatus DeviceAliveNotification(AudioObjectID devid,
                                        UInt32 num_addr,
                                        const AudioObjectPropertyAddress* addrs,
                                        void* data) {
    CoreAudioAudio* cadevice = (CoreAudioAudio*)data;
    UInt32 alive = 1;
    UInt32 size = sizeof(alive);
    const OSStatus error = AudioObjectGetPropertyData(devid, addrs, 0, nullptr, &size, &alive);
    (void)num_addr;

    bool dead = false;
    if (error == kAudioHardwareBadDeviceError) {
        dead = true;  // device was unplugged.
    } else if ((error == kAudioHardwareNoError) && (!alive)) {
        dead = true;  // device died in some other way.
    }

    if (dead) {
        cadevice->reset();
    }

    return noErr;
}

AudioDeviceID CoreAudioAudio::GetCoreAudioDevice(const QString& name) {
    uint32_t size = 0;
    AudioDeviceID* devs = nullptr;
    AudioBufferList* buflist = nullptr;
    OSStatus result = 0;
    CFStringRef cfstr = nullptr;

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devlist_address, 0, nullptr,
                                       &size) != kAudioHardwareNoError) {
        return 0;
    } else if ((devs = (AudioDeviceID*)malloc(size)) == nullptr) {
        return 0;
    } else if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &devlist_address, 0, nullptr,
                                          &size, devs) != kAudioHardwareNoError) {
        free(devs);
        return 0;
    }

    const UInt32 total_devices = (UInt32)(size / sizeof(AudioDeviceID));
    for (UInt32 i = 0; i < total_devices; i++) {
        if (AudioObjectGetPropertyDataSize(devs[i], &addr, 0, nullptr, &size) != noErr) {
            continue;
        } else if ((buflist = (AudioBufferList*)malloc(size)) == nullptr) {
            continue;
        }

        result = AudioObjectGetPropertyData(devs[i], &addr, 0, nullptr, &size, buflist);

        if (result != noErr || buflist->mNumberBuffers == 0) {
            free(buflist);
            continue;
        }

        size = sizeof(CFStringRef);

        if (AudioObjectGetPropertyData(devs[i], &nameaddr, 0, nullptr, &size, &cfstr) !=
            kAudioHardwareNoError) {
            free(buflist);
            continue;
        }

        const QString device_name = CFStringToQString(cfstr);
        CFRelease(cfstr);
        free(buflist);

        if (device_name == name) {
            const AudioDeviceID dev = devs[i];
            free(devs);
            return dev;
        }
    }

    free(devs);

    return 0;
}

CoreAudioAudio::CoreAudioAudio()
    : current_rate(static_cast<unsigned short>(coreOptions.throttle)), initialized(false) {}

void CoreAudioAudio::deinit() {
    if (!initialized)
        return;

    initialized = false;

    if (device != 0) {
        AudioObjectRemovePropertyListener(device, &alive_address, DeviceAliveNotification, this);
        device = 0;
    }

    // Idiomatic CoreAudio cleanup order:
    // 1. Stop the queue (blocks until callbacks complete)
    // 2. Free buffers explicitly
    // 3. Dispose timeline
    // 4. Dispose queue
    if (audioQueue != nullptr) {
        AudioQueueStop(audioQueue, TRUE);  // TRUE = immediate/synchronous stop
    }

    // Set buffers to NULL so any straggling callbacks will see it and return early
    AudioQueueBufferRef* buffers_to_free = buffers;
    buffers = nullptr;

    if (buffers_to_free != nullptr) {
        for (int i = 0; i < OPTION(kSoundBuffers); i++) {
            if (buffers_to_free[i] != nullptr) {
                AudioQueueFreeBuffer(audioQueue, buffers_to_free[i]);
                buffers_to_free[i] = nullptr;
            }
        }
        free(buffers_to_free);
    }

    if (timeline != nullptr) {
        AudioQueueDisposeTimeline(audioQueue, timeline);
        timeline = nullptr;
    }

    if (audioQueue != nullptr) {
        AudioQueueDispose(audioQueue, TRUE);  // TRUE = synchronous dispose
        audioQueue = nullptr;
    }

    current_buffer = 0;
    filled_buffers = 0;
}

CoreAudioAudio::~CoreAudioAudio() {
    deinit();
}

static bool AssignDeviceToAudioQueue(CoreAudioAudio* cadevice) {
    const AudioObjectPropertyAddress prop = {kAudioDevicePropertyDeviceUID,
                                             kAudioDevicePropertyScopeOutput,
                                             kAudioObjectPropertyElementMain};

    OSStatus result;
    CFStringRef devuid;
    UInt32 devuidsize = sizeof(devuid);
    result = AudioObjectGetPropertyData(cadevice->device, &prop, 0, nullptr, &devuidsize, &devuid);

    if (result != noErr) {
        return false;
    }

    result = AudioQueueSetProperty(cadevice->audioQueue, kAudioQueueProperty_CurrentDevice, &devuid,
                                   devuidsize);

    CFRelease(devuid);

    return (bool)(result == noErr);
}

static bool PrepareDevice(CoreAudioAudio* cadevice) {
    const AudioDeviceID devid = cadevice->device;
    OSStatus result = noErr;
    UInt32 size = 0;

    AudioObjectPropertyAddress paddr = {0, kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMain};

    UInt32 alive = 0;
    size = sizeof(alive);
    paddr.mSelector = kAudioDevicePropertyDeviceIsAlive;
    paddr.mScope = kAudioDevicePropertyScopeOutput;
    result = AudioObjectGetPropertyData(devid, &paddr, 0, nullptr, &size, &alive);

    if (result != noErr) {
        return false;
    }

    if (!alive) {
        return false;
    }

    // some devices don't support this property, so errors are fine here.
    pid_t pid = 0;
    size = sizeof(pid);
    paddr.mSelector = kAudioDevicePropertyHogMode;
    paddr.mScope = kAudioDevicePropertyScopeOutput;
    result = AudioObjectGetPropertyData(devid, &paddr, 0, nullptr, &size, &pid);
    if ((result == noErr) && (pid != -1)) {
        return false;
    }

    return true;
}

bool CoreAudioAudio::init(long sampleRate) {
    OSStatus result = 0;

    if (initialized) {
        deinit();
    }

    AudioChannelLayout layout;
    memset(&layout, 0, sizeof(layout));

    const QString device_name = OPTION(kSoundAudioDevice);
    const bool use_default_device =
        (device_name.isEmpty() || device_name == audio::DefaultDeviceName());

    if (!use_default_device) {
        device = GetCoreAudioDevice(device_name);

        if (device == 0) {
            vbam::LogError(
                QCoreApplication::translate("vbam", "Could not get CoreAudio device"));
            return false;
        }
    } else {
        device = 0;
    }

    description.mFormatID = kAudioFormatLinearPCM;
    description.mFormatFlags = kLinearPCMFormatFlagIsPacked;
    description.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
    description.mChannelsPerFrame = 2;
    description.mBitsPerChannel = 16;
    description.mSampleRate = current_rate
                                  ? static_cast<UInt32>(sampleRate * (current_rate / 100.0))
                                  : static_cast<UInt32>(sampleRate);
    description.mFramesPerPacket = 1;
    description.mBytesPerFrame =
        description.mChannelsPerFrame * (description.mBitsPerChannel / 8);
    description.mBytesPerPacket = description.mBytesPerFrame * description.mFramesPerPacket;

    soundBufferLen = (sampleRate / 60) * description.mBytesPerPacket;

    if (!use_default_device) {
        PrepareDevice(this);
        AudioObjectAddPropertyListener(device, &alive_address, DeviceAliveNotification, this);
    }

    result = AudioQueueNewOutput(&description, PlaybackBufferReadyCallback, this, nullptr, nullptr,
                                 0, &audioQueue);

    if (result != noErr) {
        return false;
    }

    if (!use_default_device) {
        AssignDeviceToAudioQueue(this);
    }

    layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
    result = AudioQueueSetProperty(audioQueue, kAudioQueueProperty_ChannelLayout, &layout,
                                   sizeof(layout));

    if (result != noErr) {
        return false;
    }

    buffers = (AudioQueueBufferRef*)calloc(OPTION(kSoundBuffers), sizeof(AudioQueueBufferRef));

    for (int i = 0; i < OPTION(kSoundBuffers); i++) {
        result = AudioQueueAllocateBuffer(audioQueue, soundBufferLen, &buffers[i]);

        if (result != noErr) {
            vbam::LogError(
                QCoreApplication::translate("vbam", "Failed to allocate CoreAudio buffer %1: error %2")
                    .arg(i)
                    .arg((int)result));
            return false;
        }

        // Initialize buffer with silence and set size to 0 (empty, ready for write())
        memset(buffers[i]->mAudioData, 0x00, buffers[i]->mAudioDataBytesCapacity);
        buffers[i]->mAudioDataByteSize = 0;
    }

    result = AudioQueueStart(audioQueue, nullptr);

    if (result != noErr) {
        vbam::LogError(
            QCoreApplication::translate("vbam", "Failed to start CoreAudio queue: error %1")
                .arg((int)result));
        return false;
    }

    return initialized = true;
}

void CoreAudioAudio::setThrottle(unsigned short throttle_) {
    if (!initialized)
        return;

    if (throttle_ == 0)
        throttle_ = 200;

    current_rate = throttle_;
    reset();
}

void CoreAudioAudio::resume() {
    if (!initialized)
        return;

    AudioQueueStart(audioQueue, nullptr);

    winlog("CoreAudioAudio::resume\n");
}

void CoreAudioAudio::pause() {
    if (!initialized)
        return;

    AudioQueuePause(audioQueue);

    winlog("CoreAudioAudio::pause\n");
}

void CoreAudioAudio::reset() {
    if (!initialized)
        return;

    winlog("CoreAudioAudio::reset\n");

    init(soundGetSampleRate());
}

void CoreAudioAudio::setBuffer(uint16_t* finalWave, int length) {
    AudioQueueBufferRef this_buf = nullptr;
    OSStatus status = 0;

    this_buf = buffers[current_buffer];

    if (this_buf == nullptr) {
        return;
    }

    // Bounds checking: ensure we don't write past buffer capacity
    if (this_buf->mAudioDataByteSize + length > this_buf->mAudioDataBytesCapacity) {
        length = this_buf->mAudioDataBytesCapacity - this_buf->mAudioDataByteSize;
        if (length <= 0) {
            return;
        }
    }

    memcpy((uint8_t*)this_buf->mAudioData + this_buf->mAudioDataByteSize, finalWave, length);
    this_buf->mAudioDataByteSize += (UInt32)length;

    if (this_buf->mAudioDataByteSize == this_buf->mAudioDataBytesCapacity) {
        status = AudioQueueCreateTimeline(audioQueue, &timeline);
        if (status == noErr) {
            AudioQueueGetCurrentTime(audioQueue, timeline, &starttime, nullptr);
            AudioQueueEnqueueBufferWithParameters(audioQueue, this_buf, 0, nullptr, 0, 0, 0,
                                                  nullptr, &starttime, &timestamp);
        } else {
            AudioQueueEnqueueBufferWithParameters(audioQueue, this_buf, 0, nullptr, 0, 0, 0,
                                                  nullptr, nullptr, &timestamp);
        }
    }
}

void CoreAudioAudio::write(uint16_t* finalWave, int length) {
    std::size_t samples = length / (description.mBitsPerChannel / 8);
    std::size_t avail = 0;

    if (!initialized)
        return;

    const auto wait_for_buffer = [this] {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                if (filled_buffers < OPTION(kSoundBuffers)) {
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(
                ((soundGetSampleRate() / 60) * 4) / (soundGetSampleRate() >> 7)));
        }
    };

    while ((avail = ((buffers[current_buffer]->mAudioDataBytesCapacity -
                      buffers[current_buffer]->mAudioDataByteSize) /
                     (description.mBitsPerChannel / 8))) < samples) {
        setBuffer(finalWave, (avail * (description.mBitsPerChannel / 8)));

        finalWave += avail;
        samples -= avail;

        if (buffers[current_buffer]->mAudioDataByteSize >=
            buffers[current_buffer]->mAudioDataBytesCapacity) {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            current_buffer++;
            filled_buffers++;
        }

        if (current_buffer >= OPTION(kSoundBuffers)) {
            current_buffer = 0;
        }

        wait_for_buffer();
    }

    setBuffer(finalWave, samples * (description.mBitsPerChannel / 8));

    if (buffers[current_buffer]->mAudioDataByteSize >=
        buffers[current_buffer]->mAudioDataBytesCapacity) {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        current_buffer++;
        filled_buffers++;
    }

    if (current_buffer >= OPTION(kSoundBuffers)) {
        current_buffer = 0;
    }

    wait_for_buffer();
}

}  // namespace

std::vector<AudioDevice> GetCoreAudioDevices() {
    std::vector<AudioDevice> devices;
    uint32_t size = 0;
    AudioDeviceID* devs = nullptr;
    AudioBufferList* buflist = nullptr;
    OSStatus result = 0;
    CFStringRef cfstr = nullptr;

    devices.push_back({audio::DefaultDeviceName(), QString()});

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devlist_address, 0, nullptr,
                                       &size) != kAudioHardwareNoError) {
        return devices;
    } else if ((devs = (AudioDeviceID*)malloc(size)) == nullptr) {
        return devices;
    } else if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &devlist_address, 0, nullptr,
                                          &size, devs) != kAudioHardwareNoError) {
        free(devs);
        return devices;
    }

    const UInt32 total_devices = (UInt32)(size / sizeof(AudioDeviceID));
    for (UInt32 i = 0; i < total_devices; i++) {
        if (AudioObjectGetPropertyDataSize(devs[i], &addr, 0, nullptr, &size) != noErr) {
            continue;
        } else if ((buflist = (AudioBufferList*)malloc(size)) == nullptr) {
            continue;
        }

        result = AudioObjectGetPropertyData(devs[i], &addr, 0, nullptr, &size, buflist);

        if (result != noErr || buflist->mNumberBuffers == 0) {
            free(buflist);
            continue;
        }

        size = sizeof(CFStringRef);

        if (AudioObjectGetPropertyData(devs[i], &nameaddr, 0, nullptr, &size, &cfstr) !=
            kAudioHardwareNoError) {
            free(buflist);
            continue;
        }

        const QString device_name = CFStringToQString(cfstr);
        if (!device_name.isEmpty()) {
            devices.push_back({device_name, device_name});
        }

        free(buflist);
        CFRelease(cfstr);
    }

    free(devs);

    return devices;
}

std::unique_ptr<SoundDriver> CreateCoreAudioDriver() {
    winlog("newCoreAudio\n");
    return std::make_unique<CoreAudioAudio>();
}

}  // namespace internal
}  // namespace audio

#endif

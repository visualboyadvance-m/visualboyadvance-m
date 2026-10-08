#ifdef __APPLE__
#include "wx/audio/internal/coreaudio.h"
#include "wx/audio/internal/stall-detector.h"

// === LOGALL writes very detailed informations to vba-trace.log ===
// #define LOGALL

// on win32 and mac, pointer typedefs only happen with AL_NO_PROTOTYPES
// on mac, ALC_NO_PROTOTYPES as well

// #define AL_NO_PROTOTYPES 1

// on mac, alc pointer typedefs ony happen for ALC if ALC_NO_PROTOTYPES
// unfortunately, there is a bug in the system headers (use of ALCvoid when
// void should be used; shame on Apple for introducing this error, and shame
// on Creative for making a typedef to void in the first place)
// #define ALC_NO_PROTOTYPES 1

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioToolbox.h>
#include <stdlib.h>
#include <memory.h>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <vector>

#include <wx/arrstr.h>
#include <wx/log.h>
#include <wx/translation.h>
#include <wx/utils.h>

#include "core/base/sound_driver.h"
#include "core/base/check.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "core/base/ringbuffer.h"
#include "wx/config/option-proxy.h"

#ifndef kAudioObjectPropertyElementMain
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
#endif

extern int emulating;

namespace audio {
namespace internal {

namespace {

static const AudioObjectPropertyAddress devlist_address = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain
};

const AudioObjectPropertyAddress addr = {
    kAudioDevicePropertyStreamConfiguration,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMain
};

const AudioObjectPropertyAddress nameaddr = {
    kAudioObjectPropertyName,
    kAudioDevicePropertyScopeOutput,
    kAudioObjectPropertyElementMain
};

static const AudioObjectPropertyAddress alive_address = {
    kAudioDevicePropertyDeviceIsAlive,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain
};

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
    AudioQueueBufferRef *buffers = NULL;
    AudioQueueRef audioQueue = NULL;
    AudioDeviceID device = 0;
    uint16_t current_rate = 0;
    int current_buffer = 0;
    int filled_buffers = 0;
    int soundBufferLen = 0;
    std::mutex buffer_mutex;
    std::condition_variable buffer_ready;
    std::vector<bool> buffer_pending;

private:
    AudioDeviceID GetCoreAudioDevice(wxString name);
    void setBuffer(uint16_t* finalWave, int length);
    void primeSilence();
    std::vector<uint16_t> silence_;
    StallDetector stall_;

    bool initialized = false;
};

static void PlaybackBufferReadyCallback(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer)
{
    auto* cadevice = static_cast<CoreAudioAudio*>(inUserData);
    (void)inAQ;
    std::lock_guard<std::mutex> lock(cadevice->buffer_mutex);
    if (!cadevice->buffers) return;
    for (size_t i = 0; i < cadevice->buffer_pending.size(); ++i) {
        if (cadevice->buffers[i] != inBuffer) continue;
        if (cadevice->buffer_pending[i]) {
            inBuffer->mAudioDataByteSize = 0;
            cadevice->buffer_pending[i] = false;
            --cadevice->filled_buffers;
            cadevice->buffer_ready.notify_one();
        }
        return;
    }
}

static OSStatus DeviceAliveNotification(AudioObjectID devid, UInt32 num_addr, const AudioObjectPropertyAddress *addrs, void *data)
{
    CoreAudioAudio *cadevice = (CoreAudioAudio *)data;
    UInt32 alive = 1;
    UInt32 size = sizeof(alive);
    const OSStatus error = AudioObjectGetPropertyData(devid, addrs, 0, NULL, &size, &alive);
    (void)num_addr;
    
    bool dead = false;
    if (error == kAudioHardwareBadDeviceError) {
        dead = true; // device was unplugged.
    } else if ((error == kAudioHardwareNoError) && (!alive)) {
        dead = true; // device died in some other way.
    }
    
    if (dead) {
        cadevice->reset();
    }
    
    return noErr;
}

AudioDeviceID CoreAudioAudio::GetCoreAudioDevice(wxString name)
{
    uint32_t size = 0;
    AudioDeviceID dev = 0;
    AudioDeviceID *devs = NULL;
    AudioBufferList *buflist = NULL;
    OSStatus result = 0;
    CFStringRef cfstr = NULL;

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devlist_address, 0, NULL, &size) != kAudioHardwareNoError) {
        return 0;
    } else if ((devs = (AudioDeviceID *)malloc(size)) == NULL) {
        return 0;
    } else if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &devlist_address, 0, NULL, &size, devs) != kAudioHardwareNoError) {
        free(devs);
        return 0;
    }

    const UInt32 total_devices = (UInt32) (size / sizeof(AudioDeviceID));
    for (UInt32 i = 0; i < total_devices; i++)
    {
        if (AudioObjectGetPropertyDataSize(devs[i], &addr, 0, NULL, &size) != noErr) {
            continue;
        } else if ((buflist = (AudioBufferList *)malloc(size)) == NULL) {
            continue;
        }

        result = AudioObjectGetPropertyData(devs[i], &addr, 0, NULL, &size, buflist);

        if (result != noErr) {
            free(buflist);

            continue;
        }

        if (buflist->mNumberBuffers == 0) {
            free(buflist);

            continue;
        }

        size = sizeof(CFStringRef);

        if (AudioObjectGetPropertyData(devs[i], &nameaddr, 0, NULL, &size, &cfstr) != kAudioHardwareNoError) {
            free(buflist);

            continue;
        }

        CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8);
        const char *device_name_cstr = (const char *)malloc(len + 1);
        CFStringGetCString(cfstr, (char *)device_name_cstr, len + 1, kCFStringEncodingUTF8);

        if (device_name_cstr != NULL)
        {
            const wxString device_name(device_name_cstr, wxConvLibc);
            if (device_name == name) {
                dev = devs[i];
                free(devs);
                free(buflist);
                free((void *)device_name_cstr);
                CFRelease(cfstr);

                return dev;
            }
        }

        free(buflist);
        free((void *)device_name_cstr);
        CFRelease(cfstr);
    }

    free(devs);

    return 0;
}

CoreAudioAudio::CoreAudioAudio():
current_rate(static_cast<unsigned short>(coreOptions.throttle)),
initialized(false)
{}

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
    // 3. Dispose queue

    // Stop the audio queue first and wait for all callbacks to complete
    if (audioQueue != NULL) {
        AudioQueueStop(audioQueue, TRUE);  // TRUE = immediate/synchronous stop
    }

    // Set buffers to NULL so any straggling callbacks will see it and return early
    AudioQueueBufferRef *buffers_to_free = buffers;
    buffers = NULL;

    // Free all allocated buffers
    if (buffers_to_free != NULL) {
        for (size_t i = 0; i < buffer_pending.size(); i++) {
            if (buffers_to_free[i] != NULL) {
                AudioQueueFreeBuffer(audioQueue, buffers_to_free[i]);
                buffers_to_free[i] = NULL;
            }
        }
        free(buffers_to_free);
    }

    // Finally dispose the queue itself
    if (audioQueue != NULL) {
        AudioQueueDispose(audioQueue, TRUE);  // TRUE = synchronous dispose
        audioQueue = NULL;
    }

    current_buffer = 0;
    filled_buffers = 0;
    buffer_pending.clear();
    buffer_ready.notify_all();
}

CoreAudioAudio::~CoreAudioAudio() {
    deinit();
}

static bool AssignDeviceToAudioQueue(CoreAudioAudio *cadevice)
{
    const AudioObjectPropertyAddress prop = {
        kAudioDevicePropertyDeviceUID,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    OSStatus result;
    CFStringRef devuid;
    UInt32 devuidsize = sizeof(devuid);
    result = AudioObjectGetPropertyData(cadevice->device, &prop, 0, NULL, &devuidsize, &devuid);

    if (result != noErr) {
        return false;
    }

    result = AudioQueueSetProperty(cadevice->audioQueue, kAudioQueueProperty_CurrentDevice, &devuid, devuidsize);

    CFRelease(devuid);  // Release devuid; we're done with it and AudioQueueSetProperty should have retained if it wants to keep it.

    return (bool)(result == noErr);
}

// Sets this process's I/O cycle on `devid` to a whole fraction of a 60 Hz
// frame's worth of samples.
//
// write() waits for the queue to hand a buffer back, and that wait paces the
// emulator. The queue hands buffers back only at the end of a device I/O cycle,
// though, and at the usual 512 frames -- 10.7 ms at 48 kHz -- a buffer of 1/60 s
// (800 frames) ends on alternate cycles, one and then two apart: frames came out
// 10.7 and 21.3 ms apart instead of 16.7, a steady judder at a full 60 fps.
// With a cycle that divides the frame, every buffer ends exactly a frame after
// the one before. The size is per process; other clients of the device keep theirs.
static void SetFrameDividingIoCycle(AudioObjectID devid)
{
    AudioObjectPropertyAddress addr = {kAudioDevicePropertyNominalSampleRate,
                                       kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain};
    Float64 rate = 0;
    UInt32 size = sizeof(rate);
    if (AudioObjectGetPropertyData(devid, &addr, 0, nullptr, &size, &rate) != noErr || rate <= 0)
        return;
    const UInt32 frame = static_cast<UInt32>(rate / 60.0 + 0.5);
    if (std::fabs(frame * 60.0 - rate) > 0.5)
        return;  // no whole number of samples in a frame: nothing divides it

    AudioValueRange range = {0, 0};
    addr.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
    size = sizeof(range);
    if (AudioObjectGetPropertyData(devid, &addr, 0, nullptr, &size, &range) != noErr)
        return;

    // The largest divisor of the frame up to about 5 ms: short enough to keep
    // the wake-ups cheap and long enough to stay well clear of underruns.
    const UInt32 limit = static_cast<UInt32>(rate * 0.005);
    UInt32 cycle = 0;
    for (UInt32 d = 1; d <= frame; ++d) {
        const UInt32 c = frame / d;
        if (frame % d == 0 && c <= limit && c >= range.mMinimum && c <= range.mMaximum) {
            cycle = c;
            break;
        }
    }
    if (!cycle)
        return;
    addr.mSelector = kAudioDevicePropertyBufferFrameSize;
    AudioObjectSetPropertyData(devid, &addr, 0, nullptr, sizeof(cycle), &cycle);
}

static bool PrepareDevice(CoreAudioAudio *cadevice)
{
    const AudioDeviceID devid = cadevice->device;
    OSStatus result = noErr;
    UInt32 size = 0;
    
    AudioObjectPropertyAddress addr = {
        0,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    
    UInt32 alive = 0;
    size = sizeof(alive);
    addr.mSelector = kAudioDevicePropertyDeviceIsAlive;
    addr.mScope = kAudioDevicePropertyScopeOutput;
    result = AudioObjectGetPropertyData(devid, &addr, 0, NULL, &size, &alive);
    
    if (result != noErr) {
        return false;
    }
    
    if (!alive) {
        return false;
    }
    
    // some devices don't support this property, so errors are fine here.
    pid_t pid = 0;
    size = sizeof(pid);
    addr.mSelector = kAudioDevicePropertyHogMode;
    addr.mScope = kAudioDevicePropertyScopeOutput;
    result = AudioObjectGetPropertyData(devid, &addr, 0, NULL, &size, &pid);
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

    const wxString device_name = OPTION(kSoundAudioDevice);
    const bool use_default_device = (device_name.IsEmpty() || device_name == _("Default device"));

    if (!use_default_device) {
        device = GetCoreAudioDevice(device_name);

        if (device == 0) {
            wxLogError(_("Could not get CoreAudio device"));
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
    description.mSampleRate = current_rate ? static_cast<UInt32>(sampleRate * (current_rate / 100.0)) : static_cast<UInt32>(sampleRate);
    description.mFramesPerPacket = 1;
    description.mBytesPerFrame = description.mChannelsPerFrame * (description.mBitsPerChannel / 8);
    description.mBytesPerPacket = description.mBytesPerFrame * description.mFramesPerPacket;

    soundBufferLen = (sampleRate / 60) * description.mBytesPerPacket;

    if (!use_default_device) {
        PrepareDevice(this);
        AudioObjectAddPropertyListener(device, &alive_address, DeviceAliveNotification, this);
    }

    result = AudioQueueNewOutput(&description, PlaybackBufferReadyCallback, this, NULL, NULL, 0, &audioQueue);

    if (result != noErr) {
        return false;
    }

    if (!use_default_device) {
        AssignDeviceToAudioQueue(this);
    }

    layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
    result = AudioQueueSetProperty(audioQueue, kAudioQueueProperty_ChannelLayout, &layout, sizeof(layout));

    if (result != noErr) {
        return false;
    }

    buffer_pending.assign(OPTION(kSoundBuffers), false);
    buffers = (AudioQueueBufferRef *)calloc(buffer_pending.size(), sizeof(AudioQueueBufferRef));

    for (size_t i = 0; i < buffer_pending.size(); i++) {
        result = AudioQueueAllocateBuffer(audioQueue, soundBufferLen, &buffers[i]);

        if (result != noErr) {
            wxLogError(_("Failed to allocate CoreAudio buffer %d: error %d"), i, (int)result);
            return false;
        }

        // Initialize buffer with silence and set size to 0 (empty, ready for write())
        memset(buffers[i]->mAudioData, 0x00, buffers[i]->mAudioDataBytesCapacity);
        buffers[i]->mAudioDataByteSize = 0;
    }

    {
        AudioObjectID out = device;
        if (use_default_device) {
            UInt32 sz = sizeof(out);
            const AudioObjectPropertyAddress a = {kAudioHardwarePropertyDefaultOutputDevice,
                                                  kAudioObjectPropertyScopeGlobal,
                                                  kAudioObjectPropertyElementMain};
            if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &sz, &out) != noErr)
                out = 0;
        }
        if (out)
            SetFrameDividingIoCycle(out);
    }

    result = AudioQueueStart(audioQueue, NULL);

    if (result != noErr) {
        wxLogError(_("Failed to start CoreAudio queue: error %d"), (int)result);
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
    
    AudioQueueStart(audioQueue, NULL);

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
    size_t index = 0;
    bool enqueue = false;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        index = current_buffer;
        this_buf = buffers[index];
        const int available = this_buf->mAudioDataBytesCapacity - this_buf->mAudioDataByteSize;
        length = std::min(length, available);
        if (length <= 0) return;
        memcpy(static_cast<uint8_t*>(this_buf->mAudioData) + this_buf->mAudioDataByteSize,
               finalWave, length);
        this_buf->mAudioDataByteSize += length;
        if (this_buf->mAudioDataByteSize == this_buf->mAudioDataBytesCapacity) {
            // Register ownership BEFORE enqueue: a fast callback can otherwise
            // complete before the producer increments its pending-buffer count.
            buffer_pending[index] = true;
            ++filled_buffers;
            current_buffer = (current_buffer + 1) % buffer_pending.size();
            enqueue = true;
        }
    }
    if (enqueue) {
        // Plain FIFO playback is sufficient. Scheduling at a cached AudioTimeStamp
        // from the previous-rate queue can put new buffers seconds into the future.
        const OSStatus status = AudioQueueEnqueueBuffer(audioQueue, this_buf, 0, nullptr);
        if (status != noErr) {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            if (buffer_pending[index]) {
                buffer_pending[index] = false;
                --filled_buffers;
            }
            this_buf->mAudioDataByteSize = 0;
            buffer_ready.notify_one();
        }
    }
}

// Tops the queue up with silence to one buffer short of full: where write() keeps it.
void CoreAudioAudio::primeSilence() {
    for (;;) {
        int room = 0;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            if (!buffers || filled_buffers + 1 >= static_cast<int>(buffer_pending.size()) ||
                buffer_pending[current_buffer])
                return;
            const AudioQueueBufferRef buf = buffers[current_buffer];
            room = static_cast<int>(buf->mAudioDataBytesCapacity - buf->mAudioDataByteSize);
        }
        if (room <= 0)
            return;
        silence_.assign(room / sizeof(uint16_t) + 1, 0);
        setBuffer(silence_.data(), room);  // fills the buffer, which enqueues it
    }
}

void CoreAudioAudio::write(uint16_t* finalWave, int length) {
    if (!initialized) return;

    // The queue paces the emulator: write() waits while every buffer is pending. When the
    // emulator has not run for a while and the queue played on -- a live window resize,
    // the fullscreen animation, anything that holds the event loop -- it is empty when the
    // emulator comes back, and refilling it from the game runs the game at full speed
    // until it is full again: a burst of fast-forward. The gap is in the sound already,
    // so fill it with silence instead, and the game goes on at its own rate. Only after a
    // stall, though (StallDetector): a frame or two that ran late is made up by catching
    // up, as it always was.
    //
    // Fast-forward, an unthrottled speed and a GBA joybus link are not paced by the
    // queue, as in the other drivers: write() takes what fits and drops the rest, and
    // the emulator runs on. Waiting here held a fast-forward to the queue's rate, the
    // nine frames in ten it skips included: 5 fps on the screen and no faster.
    const bool paced = !coreOptions.speedup && coreOptions.throttle && !gba_joybus_active;
    const bool stalled = stall_.Stalled();
    bool drained;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        drained = filled_buffers == 0;
    }
    if (drained && stalled && paced)
        primeSilence();
    auto* source = reinterpret_cast<uint8_t*>(finalWave);
    while (length > 0) {
        int chunk = 0;
        {
            std::unique_lock<std::mutex> lock(buffer_mutex);
            if (!paced && buffer_pending[current_buffer])
                return;
            buffer_ready.wait(lock, [this] {
                return !initialized || !buffer_pending[current_buffer];
            });
            if (!initialized) return;
            const auto buffer = buffers[current_buffer];
            chunk = std::min(length, static_cast<int>(
                buffer->mAudioDataBytesCapacity - buffer->mAudioDataByteSize));
        }
        if (chunk <= 0) return;
        setBuffer(reinterpret_cast<uint16_t*>(source), chunk);
        source += chunk;
        length -= chunk;
    }
}

}  // namespace

std::vector<AudioDevice> GetCoreAudioDevices() {
    std::vector<AudioDevice> devices;
    uint32_t size = 0;
    AudioDeviceID *devs = NULL;
    AudioBufferList *buflist = NULL;
    OSStatus result = 0;
    CFStringRef cfstr = NULL;

    devices.push_back({_("Default device"), wxEmptyString});

    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devlist_address, 0, NULL, &size) != kAudioHardwareNoError) {
        return devices;
    } else if ((devs = (AudioDeviceID *)malloc(size)) == NULL) {
        return devices;
    } else if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &devlist_address, 0, NULL, &size, devs) != kAudioHardwareNoError) {
        free(devs);
        return devices;
    }

    const UInt32 total_devices = (UInt32) (size / sizeof(AudioDeviceID));
    for (UInt32 i = 0; i < total_devices; i++)
    {
        if (AudioObjectGetPropertyDataSize(devs[i], &addr, 0, NULL, &size) != noErr) {
            continue;
        } else if ((buflist = (AudioBufferList *)malloc(size)) == NULL) {
            continue;
        }

        result = AudioObjectGetPropertyData(devs[i], &addr, 0, NULL, &size, buflist);

        if (result != noErr) {
            free(buflist);

            continue;
        }

        if (buflist->mNumberBuffers == 0) {
            free(buflist);

            continue;
        }

        size = sizeof(CFStringRef);

        if (AudioObjectGetPropertyData(devs[i], &nameaddr, 0, NULL, &size, &cfstr) != kAudioHardwareNoError) {
            free(buflist);
            
            continue;
        }

        CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8);
        const char *name = (const char *)malloc(len + 1);
        CFStringGetCString(cfstr, (char *)name, len + 1, kCFStringEncodingUTF8);

        if (name != NULL)
        {
            const wxString device_name(name, wxConvLibc);
            devices.push_back({device_name, device_name});
        }

        free(buflist);
        free((void *)name);
        CFRelease(cfstr);
    }

    return devices;
}

std::unique_ptr<SoundDriver> CreateCoreAudioDriver() {
    winlog("newCoreAudio\n");
    return std::make_unique<CoreAudioAudio>();
}

}  // namespace internal
}  // namespace audio

#endif

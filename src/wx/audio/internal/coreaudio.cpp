#ifdef __APPLE__
#include "wx/audio/internal/coreaudio.h"

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
#include <mutex>

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
    AudioTimeStamp starttime;
    AudioTimeStamp timestamp;
    AudioQueueTimelineRef timeline = NULL;
    std::mutex buffer_mutex;

private:
    AudioDeviceID GetCoreAudioDevice(wxString name);
    void setBuffer(uint16_t* finalWave, int length);

    bool initialized = false;
};

static void PlaybackBufferReadyCallback(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer)
{
    int curbuf = 0;
    CoreAudioAudio *cadevice = (CoreAudioAudio *)inUserData;
    (void)inAQ;

    for (curbuf = 0; curbuf < OPTION(kSoundBuffers); curbuf++) {
        if (cadevice->buffers[curbuf] == inBuffer) {
            break;
        }
    }

    if (curbuf >= OPTION(kSoundBuffers))
        return;

    // buffer is unexpectedly here? We're probably dying, but try to requeue this buffer with silence.
    if (cadevice->buffers[curbuf] != NULL) {
        AudioQueueFreeBuffer(inAQ, cadevice->buffers[curbuf]);
        cadevice->soundBufferLen = (soundGetSampleRate() / 60) * cadevice->description.mBytesPerPacket;
        AudioQueueAllocateBuffer(inAQ, cadevice->soundBufferLen, &cadevice->buffers[curbuf]);
        cadevice->buffers[curbuf]->mAudioDataByteSize = 0;
    } else {
        cadevice->soundBufferLen = (soundGetSampleRate() / 60) * cadevice->description.mBytesPerPacket;
        AudioQueueAllocateBuffer(inAQ, cadevice->soundBufferLen, &cadevice->buffers[curbuf]);
        cadevice->buffers[curbuf]->mAudioDataByteSize = 0;
    }

    std::lock_guard<std::mutex> lock(cadevice->buffer_mutex);
    if (cadevice->filled_buffers > 0) {
        cadevice->filled_buffers--;
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

    if (device != 0) {
        AudioObjectRemovePropertyListener(device, &alive_address, DeviceAliveNotification, this);
    }

    if (buffers != NULL) {
        for (int i = 0; i < OPTION(kSoundBuffers); i++) {
            if (buffers[i] != NULL) {
                AudioQueueFreeBuffer(audioQueue, buffers[i]);
            }
        }
        free(buffers);
        buffers = NULL;
    }

    AudioQueueStop(audioQueue, TRUE);

    if (timeline != NULL) {
        AudioQueueDisposeTimeline(audioQueue, timeline);
        timeline = NULL;
    }

    if (audioQueue != NULL) {
        AudioQueueDispose(audioQueue, TRUE);
        audioQueue = NULL;
    }

    device = 0;
    current_buffer = 0;
    filled_buffers = 0;

    initialized = false;
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

    buffers = (AudioQueueBufferRef *)calloc(OPTION(kSoundBuffers), sizeof(AudioQueueBufferRef));

    for (int i = 0; i < OPTION(kSoundBuffers); i++) {
        result = AudioQueueAllocateBuffer(audioQueue, soundBufferLen, &buffers[i]);

        if (result != noErr) {
            return false;
        }

        memset(buffers[i]->mAudioData, 0x00, buffers[i]->mAudioDataBytesCapacity);
        buffers[i]->mAudioDataByteSize = buffers[i]->mAudioDataBytesCapacity;

        PlaybackBufferReadyCallback(this, audioQueue, buffers[i]);
        buffers[i]->mAudioDataByteSize = 0;
    }

    result = AudioQueueStart(audioQueue, NULL);

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
    AudioQueueBufferRef this_buf = NULL;
    OSStatus status = 0;

    this_buf = buffers[current_buffer];

    if (this_buf == NULL) {
        return;
    }

    // Bounds checking: ensure we don't write past buffer capacity
    if (this_buf->mAudioDataByteSize + length > this_buf->mAudioDataBytesCapacity) {
        length = this_buf->mAudioDataBytesCapacity - this_buf->mAudioDataByteSize;
        if (length <= 0) {
            return;
        }
    }

    memcpy((uint8_t *)this_buf->mAudioData + this_buf->mAudioDataByteSize, finalWave, length);
    this_buf->mAudioDataByteSize += (UInt32)length;

    if (this_buf->mAudioDataByteSize == this_buf->mAudioDataBytesCapacity) {
        status = AudioQueueCreateTimeline(audioQueue, &timeline);
        if(status == noErr) {
            AudioQueueGetCurrentTime(audioQueue, timeline, &starttime, NULL);
            AudioQueueEnqueueBufferWithParameters(audioQueue, this_buf, 0, NULL, 0, 0, 0, NULL, &starttime, &timestamp);
        } else {
            AudioQueueEnqueueBufferWithParameters(audioQueue, this_buf, 0, NULL, 0, 0, 0, NULL, NULL, &timestamp);
        }
    }
}

void CoreAudioAudio::write(uint16_t* finalWave, int length) {
    std::size_t samples = length / (description.mBitsPerChannel / 8);
    std::size_t avail = 0;

    if (!initialized)
        return;

    while ((avail = ((buffers[current_buffer]->mAudioDataBytesCapacity - buffers[current_buffer]->mAudioDataByteSize) / (description.mBitsPerChannel / 8))) < samples)
    {
        setBuffer(finalWave, (avail * (description.mBitsPerChannel / 8)));

        finalWave += avail;
        samples -= avail;

        if (buffers[current_buffer]->mAudioDataByteSize >= buffers[current_buffer]->mAudioDataBytesCapacity) {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            current_buffer++;
            filled_buffers++;
        }

        if (current_buffer >= OPTION(kSoundBuffers)) {
            current_buffer = 0;
        }

        while (true) {
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                if (filled_buffers < OPTION(kSoundBuffers)) {
                    break;
                }
            }
            wxMilliSleep(((soundGetSampleRate() / 60) * 4) / (soundGetSampleRate() >> 7));
        }
    }

    setBuffer(finalWave, samples * (description.mBitsPerChannel / 8));

    if (buffers[current_buffer]->mAudioDataByteSize >= buffers[current_buffer]->mAudioDataBytesCapacity) {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        current_buffer++;
        filled_buffers++;
    }

    if (current_buffer >= OPTION(kSoundBuffers)) {
        current_buffer = 0;
    }

    while (true) {
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            if (filled_buffers < OPTION(kSoundBuffers)) {
                break;
            }
        }
        wxMilliSleep(((soundGetSampleRate() / 60) * 4) / (soundGetSampleRate() >> 7));
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

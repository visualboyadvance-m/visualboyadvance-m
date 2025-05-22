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

static const AudioObjectPropertyAddress default_playback_device_address = {
    kAudioHardwarePropertyDefaultOutputDevice,
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
    bool must_wait = false;
    uint16_t current_rate = 0;
    int current_buffer = 0;
    AudioTimeStamp timestamp;

private:
    int soundBufferLen = 0;
    AudioDeviceID GetCoreAudioDevice(wxString name);
    void setBuffer(uint16_t* finalWave, int length);

    bool initialized = false;
};

static void PlaybackBufferReadyCallback(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer)
{
    int bufIndex = 0;
    CoreAudioAudio *cadevice = (CoreAudioAudio *)inUserData;
    (void)inAQ;

    for (int i = 0; i < OPTION(kSoundBuffers); i++)
    {
        if (inBuffer == cadevice->buffers[i])
        {
            bufIndex = i;
            break;
        }
    }

    // buffer is unexpectedly here? We're probably dying, but try to requeue this buffer with silence.
    if (inBuffer) {
        memset(inBuffer->mAudioData, 0, inBuffer->mAudioDataBytesCapacity);
        inBuffer->mAudioDataByteSize = 0;
    }

    if ((OPTION(kSoundBuffers) - 1) >= bufIndex) {
        cadevice->must_wait = false;
        cadevice->current_buffer = 0;
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
    
    if (name == _("Default device")) {
        if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &default_playback_device_address, 0, NULL, &size) != kAudioHardwareNoError) {
            return 0;
        } else if ((devs = (AudioDeviceID *)malloc(size)) == NULL) {
            return 0;
        } else if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_playback_device_address, 0, NULL, &size, devs) != kAudioHardwareNoError) {
            free(devs);
            return 0;
        }
        dev = devs[0];
        
        free(devs);
        
        return dev;
    } else {
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
            const char *name = (const char *)malloc(len + 1);
            CFStringGetCString(cfstr, (char *)name, len + 1, kCFStringEncodingUTF8);
            
            if (name != NULL)
            {
                const wxString device_name(name, wxConvLibc);
                if (device_name == name) {
                    dev = devs[i];
                    free(devs);
                    free(buflist);
                    free((void *)name);

                    return dev;
                }
            }
            
            free(buflist);
            free((void *)name);
        }
    }
    
    return 0;
}

CoreAudioAudio::CoreAudioAudio():
current_rate(static_cast<unsigned short>(coreOptions.throttle)),
initialized(false)
{}

void CoreAudioAudio::deinit() {
    if (!initialized)
        return;
    
    AudioObjectRemovePropertyListener(device, &alive_address, DeviceAliveNotification, this);
    
    for (int i = 0; i < OPTION(kSoundBuffers); i++) {
        AudioQueueFreeBuffer(audioQueue, buffers[i]);
    }

    AudioQueueStop(audioQueue, TRUE);
    device = 0;
    
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
    
    if (result != noErr) {
        return false;
    }

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
    
    device = GetCoreAudioDevice(OPTION(kSoundAudioDevice));
    
    if (device == 0) {
        fprintf(stderr, "Couldn't get Core Audio device\n");
        return false;
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

    PrepareDevice(this);

    AudioObjectAddPropertyListener(device, &alive_address, DeviceAliveNotification, this);
    result = AudioQueueNewOutput(&description, PlaybackBufferReadyCallback, this, NULL, NULL, 0, &audioQueue);

    if (result != noErr) {
        return false;
    }

    AssignDeviceToAudioQueue(this);

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
    
    AudioQueueDeviceGetNearestStartTime(audioQueue, &timestamp, 0);
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

    this_buf = buffers[current_buffer];

    if (this_buf == NULL) {
        fprintf(stderr, "Current buffer is NULL\n");
        return;
    }

    memcpy((uint8_t *)this_buf->mAudioData + this_buf->mAudioDataByteSize, finalWave, length);
    this_buf->mAudioDataByteSize += (UInt32)length;

    if (this_buf->mAudioDataByteSize == this_buf->mAudioDataBytesCapacity) {
        AudioQueueEnqueueBufferWithParameters(audioQueue, this_buf, 0, NULL, 0, 0, 0, NULL, NULL, &timestamp);
    }

    must_wait = true;
}

void CoreAudioAudio::write(uint16_t* finalWave, int length) {
    std::size_t samples = length / (description.mBitsPerChannel / 8);
    std::size_t avail = 0;

    if (!initialized)
        return;

    while ((avail = ((buffers[current_buffer]->mAudioDataBytesCapacity - buffers[current_buffer]->mAudioDataByteSize) / (description.mBitsPerChannel / 8))) < samples)
    {
        setBuffer(finalWave, (avail * (description.mBitsPerChannel / 8)));

        finalWave += (avail * (description.mBitsPerChannel / 8));
        samples -= avail;

        if (buffers[current_buffer]->mAudioDataByteSize >= buffers[current_buffer]->mAudioDataBytesCapacity) {
            current_buffer++;
        }

        while ((current_buffer >= OPTION(kSoundBuffers)) && must_wait) {
            wxMilliSleep(1);
        }
    }

    setBuffer(finalWave, samples * (description.mBitsPerChannel / 8));

    if (buffers[current_buffer]->mAudioDataByteSize >= buffers[current_buffer]->mAudioDataBytesCapacity) {
        current_buffer++;
    }

    while ((current_buffer >= OPTION(kSoundBuffers)) && must_wait) {
        wxMilliSleep(1);
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

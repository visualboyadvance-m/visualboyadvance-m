#if defined(__APPLE__)

#include "wx/audio/internal/macos-io-cycle.h"

#include <cmath>
#include <vector>

#include <CoreAudio/CoreAudio.h>
#include <wx/translation.h>

#include "wx/audio/internal/dividing-period.h"

namespace audio {
namespace internal {

namespace {

AudioObjectID DefaultOutputDevice() {
    AudioObjectID device = 0;
    UInt32 size = sizeof(device);
    const AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDefaultOutputDevice,
                                             kAudioObjectPropertyScopeGlobal,
                                             kAudioObjectPropertyElementMain};
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &device) !=
        noErr)
        return 0;
    return device;
}

bool HasOutput(AudioObjectID device) {
    const AudioObjectPropertyAddress addr = {kAudioDevicePropertyStreams,
                                             kAudioObjectPropertyScopeOutput,
                                             kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    return AudioObjectGetPropertyDataSize(device, &addr, 0, nullptr, &size) == noErr && size > 0;
}

wxString DeviceName(AudioObjectID device) {
    const AudioObjectPropertyAddress addr = {kAudioObjectPropertyName,
                                             kAudioObjectPropertyScopeGlobal,
                                             kAudioObjectPropertyElementMain};
    CFStringRef name = nullptr;
    UInt32 size = sizeof(name);
    if (AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, &name) != noErr || !name)
        return wxString();
    char buf[512];
    const bool ok = CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8);
    CFRelease(name);
    return ok ? wxString::FromUTF8(buf) : wxString();
}

}  // namespace

uint32_t MacosOutputDevice(const wxString& name) {
    if (!name.empty() && name != _("Default device")) {
        const AudioObjectPropertyAddress addr = {kAudioHardwarePropertyDevices,
                                                 kAudioObjectPropertyScopeGlobal,
                                                 kAudioObjectPropertyElementMain};
        UInt32 size = 0;
        if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size) ==
            noErr) {
            std::vector<AudioObjectID> devices(size / sizeof(AudioObjectID));
            if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size,
                                           devices.data()) == noErr) {
                for (AudioObjectID device : devices)
                    if (HasOutput(device) && DeviceName(device) == name)
                        return device;
            }
        }
    }
    return DefaultOutputDevice();
}

void SetMacosIoCycle(uint32_t device, uint32_t buffer_frames, uint32_t buffer_rate) {
    if (!device)
        return;
    AudioObjectPropertyAddress addr = {kAudioDevicePropertyNominalSampleRate,
                                       kAudioObjectPropertyScopeGlobal,
                                       kAudioObjectPropertyElementMain};
    Float64 rate = 0;
    UInt32 size = sizeof(rate);
    if (AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, &rate) != noErr || rate <= 0 ||
        rate != std::floor(rate))
        return;

    AudioValueRange range = {0, 0};
    addr.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
    size = sizeof(range);
    if (AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, &range) != noErr)
        return;

    UInt32 cycle = DividingPeriod(buffer_frames, buffer_rate, static_cast<uint32_t>(rate),
                                  static_cast<uint32_t>(std::ceil(range.mMinimum)),
                                  static_cast<uint32_t>(range.mMaximum));
    if (!cycle)
        return;
    addr.mSelector = kAudioDevicePropertyBufferFrameSize;
    AudioObjectSetPropertyData(device, &addr, 0, nullptr, sizeof(cycle), &cycle);
}

}  // namespace internal
}  // namespace audio

#endif  // __APPLE__

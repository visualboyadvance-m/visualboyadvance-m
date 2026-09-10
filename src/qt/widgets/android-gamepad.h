#ifndef VBAM_QT_WIDGETS_ANDROID_GAMEPAD_H_
#define VBAM_QT_WIDGETS_ANDROID_GAMEPAD_H_

#if defined(__ANDROID__)

// Physical game controllers on Android.
//
// SDL's joystick subsystem cannot be used under the QtActivity host (it needs
// the SDLActivity Java lifecycle), so controller input comes from the activity
// instead: org.visualboyadvance_m.VbamGamepad intercepts controller key and
// joystick-motion events on the Android UI thread and pushes them here over
// JNI. SdlPoller drains the queue from its timer on the GUI thread and turns
// the raw button/axis changes into the same joystick UserInputBatch entries
// the SDL path produces on desktop, using SDL gamepad numbering so the default
// Joystick 1 bindings apply as they are.

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "qt/widgets/input-dispatcher.h"

namespace widgets {

class AndroidGamepad final {
public:
    // Process-wide instance; the JNI entry points need a fixed address.
    static AndroidGamepad& Instance();

    AndroidGamepad(const AndroidGamepad&) = delete;
    AndroidGamepad& operator=(const AndroidGamepad&) = delete;

    // Registers the native methods of the VbamGamepad Java class and starts
    // controller discovery there. Idempotent. Must run once Qt's JNI bridge is
    // up, i.e. from the running event loop, not from static initialization.
    void Initialize();

    // Takes every change queued by the Java side since the last call and
    // converts it into user input entries. Call from the GUI thread only.
    std::vector<UserInputBatch::Data> Drain();

    // Starts or stops rumble on the first connected controller.
    void SetRumble(bool rumble);

private:
    AndroidGamepad() = default;
    ~AndroidGamepad() = default;

    // What the Java side reports. Axis values follow SDL: -32768..32767 for
    // sticks, 0..32767 for triggers.
    struct RawEvent {
        enum class Type { DeviceAdded, DeviceRemoved, Button, Axis };
        Type type;
        int device_id;
        uint8_t index;
        int16_t value;  // axis value, or 1/0 for a button press/release
    };

    // Per-controller state, keyed by the joystick index handed out below.
    struct DeviceState {
        int device_id;
        std::map<uint8_t, bool> buttons;
        std::map<uint8_t, int8_t> axes;  // +1 / -1 / 0 = past +/- threshold
    };

    // JNI entry points (Android UI thread). They only append to `pending_`.
    static void JniDeviceAdded(void* env, void* clazz, int device_id, void* name);
    static void JniDeviceRemoved(void* env, void* clazz, int device_id);
    static void JniButton(void* env, void* clazz, int device_id, int button, uint8_t pressed);
    static void JniAxis(void* env, void* clazz, int device_id, int axis, float value);

    void Push(RawEvent event);

    // Finds the slot for an Android device id, assigning the lowest free one
    // to a device seen for the first time. GUI thread only.
    int SlotFor(int device_id);

    // Releases everything the device in `slot` holds down and forgets it.
    void RemoveSlot(int slot, std::vector<UserInputBatch::Data>* event_data);

    std::mutex mutex_;
    std::vector<RawEvent> pending_;
    bool initialized_ = false;

    // Slot (config::JoyId index) -> state. GUI thread only.
    std::map<int, DeviceState> devices_;
};

}  // namespace widgets

#endif  // __ANDROID__

#endif  // VBAM_QT_WIDGETS_ANDROID_GAMEPAD_H_

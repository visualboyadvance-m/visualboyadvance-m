#include "qt/widgets/android-gamepad.h"

#if defined(__ANDROID__)

#include <algorithm>
#include <cmath>

#include <android/log.h>
#include <jni.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QJniEnvironment>
#include <QtCore/QJniObject>

#include "qt/config/user-input.h"

namespace widgets {

namespace {

constexpr const char kJavaClass[] = "org/visualboyadvance_m/VbamGamepad";

// Same deflection SdlPoller uses before an axis counts as pressed (~25%).
constexpr int16_t kAxisThreshold = 0x1fff;

int8_t AxisStatus(int16_t value) {
    if (value > kAxisThreshold) {
        return 1;
    }
    if (value < -kAxisThreshold) {
        return -1;
    }
    return 0;
}

config::JoyControl AxisControl(int8_t status) {
    return status > 0 ? config::JoyControl::AxisPlus : config::JoyControl::AxisMinus;
}

int16_t ToSdlAxisValue(float value) {
    const float clamped = std::max(-1.0f, std::min(1.0f, value));
    return static_cast<int16_t>(std::lround(clamped * 32767.0f));
}

}  // namespace

// static
AndroidGamepad& AndroidGamepad::Instance() {
    static AndroidGamepad instance;
    return instance;
}

void AndroidGamepad::Initialize() {
    if (initialized_) {
        return;
    }
    initialized_ = true;

    static const JNINativeMethod kMethods[] = {
        {"nativeDeviceAdded", "(ILjava/lang/String;)V",
         reinterpret_cast<void*>(&AndroidGamepad::JniDeviceAdded)},
        {"nativeDeviceRemoved", "(I)V", reinterpret_cast<void*>(&AndroidGamepad::JniDeviceRemoved)},
        {"nativeButton", "(IIZ)V", reinterpret_cast<void*>(&AndroidGamepad::JniButton)},
        {"nativeAxis", "(IIF)V", reinterpret_cast<void*>(&AndroidGamepad::JniAxis)},
    };

    QJniEnvironment env;
    if (!env.registerNativeMethods(kJavaClass, kMethods,
                                   sizeof(kMethods) / sizeof(kMethods[0]))) {
        __android_log_print(ANDROID_LOG_ERROR, "VBAM",
                            "gamepad: registering %s natives failed; controllers disabled",
                            kJavaClass);
        return;
    }

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    QJniObject::callStaticMethod<void>(kJavaClass, "install", "(Ljava/lang/Object;)V",
                                       activity.isValid() ? activity.object() : nullptr);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    __android_log_print(ANDROID_LOG_INFO, "VBAM", "gamepad: Android controller input enabled");
}

// --- JNI entry points (Android UI thread) -----------------------------------

// static
void AndroidGamepad::JniDeviceAdded(void* env, void*, int device_id, void* name) {
    auto* jenv = static_cast<JNIEnv*>(env);
    auto jname = static_cast<jstring>(name);
    if (jname) {
        const char* utf = jenv->GetStringUTFChars(jname, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "VBAM", "gamepad: device %d added: %s", device_id,
                            utf ? utf : "?");
        if (utf) {
            jenv->ReleaseStringUTFChars(jname, utf);
        }
    }
    Instance().Push({RawEvent::Type::DeviceAdded, device_id, 0, 0});
}

// static
void AndroidGamepad::JniDeviceRemoved(void*, void*, int device_id) {
    __android_log_print(ANDROID_LOG_INFO, "VBAM", "gamepad: device %d removed", device_id);
    Instance().Push({RawEvent::Type::DeviceRemoved, device_id, 0, 0});
}

// static
void AndroidGamepad::JniButton(void*, void*, int device_id, int button, uint8_t pressed) {
    if (button < 0 || button > 0xff) {
        return;
    }
    Instance().Push({RawEvent::Type::Button, device_id, static_cast<uint8_t>(button),
                     static_cast<int16_t>(pressed ? 1 : 0)});
}

// static
void AndroidGamepad::JniAxis(void*, void*, int device_id, int axis, float value) {
    if (axis < 0 || axis > 0xff) {
        return;
    }
    Instance().Push(
        {RawEvent::Type::Axis, device_id, static_cast<uint8_t>(axis), ToSdlAxisValue(value)});
}

void AndroidGamepad::Push(RawEvent event) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(event);
}

// --- GUI thread --------------------------------------------------------------

int AndroidGamepad::SlotFor(int device_id) {
    for (const auto& entry : devices_) {
        if (entry.second.device_id == device_id) {
            return entry.first;
        }
    }
    // Lowest index not in use, so the first controller is Joystick 1 and a
    // re-plugged controller takes the slot it left.
    int slot = 0;
    while (devices_.count(slot)) {
        ++slot;
    }
    devices_[slot] = DeviceState{device_id, {}, {}};
    __android_log_print(ANDROID_LOG_INFO, "VBAM", "gamepad: device %d is joystick %d", device_id,
                        slot + 1);
    return slot;
}

void AndroidGamepad::RemoveSlot(int slot, std::vector<UserInputBatch::Data>* event_data) {
    auto it = devices_.find(slot);
    if (it == devices_.end()) {
        return;
    }
    const config::JoyId joy(slot);
    for (const auto& button : it->second.buttons) {
        if (button.second) {
            event_data->emplace_back(config::JoyInput(joy, config::JoyControl::Button, button.first),
                                     false);
        }
    }
    for (const auto& axis : it->second.axes) {
        if (axis.second != 0) {
            event_data->emplace_back(config::JoyInput(joy, AxisControl(axis.second), axis.first),
                                     false);
        }
    }
    devices_.erase(it);
}

std::vector<UserInputBatch::Data> AndroidGamepad::Drain() {
    std::vector<RawEvent> events;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events.swap(pending_);
    }

    std::vector<UserInputBatch::Data> event_data;
    for (const RawEvent& event : events) {
        switch (event.type) {
            case RawEvent::Type::DeviceAdded:
                SlotFor(event.device_id);
                break;

            case RawEvent::Type::DeviceRemoved: {
                for (const auto& entry : devices_) {
                    if (entry.second.device_id == event.device_id) {
                        RemoveSlot(entry.first, &event_data);
                        break;
                    }
                }
                break;
            }

            case RawEvent::Type::Button: {
                const int slot = SlotFor(event.device_id);
                DeviceState& state = devices_[slot];
                const bool pressed = event.value != 0;
                auto it = state.buttons.find(event.index);
                const bool was_pressed = it != state.buttons.end() && it->second;
                if (pressed == was_pressed) {
                    break;
                }
                state.buttons[event.index] = pressed;
                event_data.emplace_back(
                    config::JoyInput(config::JoyId(slot), config::JoyControl::Button, event.index),
                    pressed);
                break;
            }

            case RawEvent::Type::Axis: {
                const int slot = SlotFor(event.device_id);
                DeviceState& state = devices_[slot];
                const int8_t status = AxisStatus(event.value);
                auto it = state.axes.find(event.index);
                const int8_t previous = it != state.axes.end() ? it->second : 0;
                if (status == previous) {
                    break;
                }
                state.axes[event.index] = status;
                const config::JoyId joy(slot);
                if (previous != 0) {
                    event_data.emplace_back(
                        config::JoyInput(joy, AxisControl(previous), event.index), false);
                }
                if (status != 0) {
                    event_data.emplace_back(
                        config::JoyInput(joy, AxisControl(status), event.index), true);
                }
                break;
            }
        }
    }
    return event_data;
}

void AndroidGamepad::SetRumble(bool rumble) {
    if (!initialized_ || devices_.empty()) {
        return;
    }
    QJniObject::callStaticMethod<void>(kJavaClass, "setRumble", "(IZ)V",
                                       static_cast<jint>(devices_.begin()->second.device_id),
                                       static_cast<jboolean>(rumble));
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

}  // namespace widgets

#endif  // __ANDROID__

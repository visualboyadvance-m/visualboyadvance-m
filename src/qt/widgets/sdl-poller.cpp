#include "qt/widgets/sdl-poller.h"

#if defined(__ANDROID__)
#include "qt/widgets/android-gamepad.h"
#endif

#include <unordered_map>
#include <vector>

#include <QTimer>

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include "core/base/check.h"
#include "core/base/sdl_motion.h"
#include "qt/config/option-id.h"
#include "qt/config/option-observer.h"
#include "qt/config/option-proxy.h"
#include "qt/config/option.h"
#include "qt/config/user-input.h"
#include "qt/widgets/input-dispatcher.h"

namespace widgets {

namespace {

enum class JoyAxisStatus { Neutral = 0, Plus, Minus };

JoyAxisStatus AxisValueToStatus(const int16_t& x) {
    if (x > 0x1fff)
        return JoyAxisStatus::Plus;
    if (x < -0x1fff)
        return JoyAxisStatus::Minus;
    return JoyAxisStatus::Neutral;
}

config::JoyControl AxisStatusToJoyControl(const JoyAxisStatus& status) {
    switch (status) {
        case JoyAxisStatus::Plus:
            return config::JoyControl::AxisPlus;
        case JoyAxisStatus::Minus:
            return config::JoyControl::AxisMinus;
        case JoyAxisStatus::Neutral:
        default:
            // This should never happen.
            VBAM_NOTREACHED_RETURN(config::JoyControl::AxisPlus);
    }
}

config::JoyControl HatStatusToJoyControl(const uint8_t status) {
    switch (status) {
        case SDL_HAT_UP:
            return config::JoyControl::HatNorth;
        case SDL_HAT_DOWN:
            return config::JoyControl::HatSouth;
        case SDL_HAT_LEFT:
            return config::JoyControl::HatWest;
        case SDL_HAT_RIGHT:
            return config::JoyControl::HatEast;
        default:
            // This should never happen.
            VBAM_NOTREACHED_RETURN(config::JoyControl::HatNorth);
    }
}

}  // namespace

// Represents the current state of a joystick. This class takes care of
// initializing and destroying SDL resources on construction and destruction so
// every associated SDL state for a joystick dies with this object.
class JoyState final {
public:
    JoyState(bool enable_game_controller, int sdl_index);
    ~JoyState();

    // Disable copy constructor and assignment.
    JoyState(const JoyState&) = delete;
    JoyState& operator=(const JoyState&) = delete;

    // Returns true if this object was properly initialized.
    bool IsValid() const;

    // Returns true if this object is a game controller.
    bool is_game_controller() const { return !!game_controller_; }

    // Processes the corresponding events.
    std::vector<UserInputBatch::Data> ProcessAxisEvent(const uint8_t index,
                                                       const JoyAxisStatus status);
    std::vector<UserInputBatch::Data> ProcessButtonEvent(const uint8_t index, const bool pressed);
    std::vector<UserInputBatch::Data> ProcessHatEvent(const uint8_t index, const uint8_t status);

    // Activates or deactivates rumble.
    void SetRumble(bool activate_rumble);

    SDL_JoystickID joystick_id() const { return joystick_id_; }

private:
    // The Joystick abstraction for UI events.
    config::JoyId joy_id_;

    // SDL Joystick ID used for events.
    SDL_JoystickID joystick_id_ = 0;

    // The SDL GameController instance.
#ifndef ENABLE_SDL3
    SDL_GameController* game_controller_ = nullptr;
#else
    SDL_Gamepad* game_controller_ = nullptr;
#endif

    // The SDL Joystick instance.
    SDL_Joystick* sdl_joystick_ = nullptr;

    // Current state of Joystick axis.
    std::unordered_map<uint8_t, JoyAxisStatus> axis_{};

    // Current state of Joystick buttons.
    std::unordered_map<uint8_t, bool> buttons_{};

    // Current state of Joystick HAT. Unused for GameControllers.
    std::unordered_map<uint8_t, uint8_t> hats_{};

    // Set to true to activate joystick rumble.
    bool rumbling_ = false;

    // Re-triggers the rumble effect periodically while rumbling_ is set.
    QTimer rumble_timer_;
};

JoyState::JoyState(bool enable_game_controller, int sdl_index) : joy_id_(sdl_index) {
#ifndef ENABLE_SDL3
    if (enable_game_controller && SDL_IsGameController(sdl_index)) {
        game_controller_ = SDL_GameControllerOpen(sdl_index);
        if (game_controller_)
            sdl_joystick_ = SDL_GameControllerGetJoystick(game_controller_);
    } else {
        sdl_joystick_ = SDL_JoystickOpen(sdl_index);
    }
#else
    int nrjoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&nrjoysticks);
    if (!joysticks || sdl_index >= nrjoysticks) {
        SDL_free(joysticks);
        return;
    }
    const SDL_JoystickID instance = joysticks[sdl_index];
    SDL_free(joysticks);

    if (enable_game_controller && SDL_IsGamepad(instance)) {
        game_controller_ = SDL_OpenGamepad(instance);
        if (game_controller_) {
            sdl_joystick_ = SDL_GetGamepadJoystick(game_controller_);
        }
    } else {
        sdl_joystick_ = SDL_OpenJoystick(instance);
    }
#endif

    if (!sdl_joystick_)
        return;

#ifndef ENABLE_SDL3
    joystick_id_ = SDL_JoystickInstanceID(sdl_joystick_);
#else
    joystick_id_ = SDL_GetJoystickID(sdl_joystick_);
#endif

    // If this is a controller (not a raw joystick) and it has a motion sensor,
    // hand it to the global SdlMotion so the GBA tilt-sensor reads from it.
    // Only one pad supplies motion data at a time -- re-attaching with each new
    // pad replaces any prior attachment.
    if (game_controller_) {
        vbam::core::SdlMotion::Instance().Attach(static_cast<void*>(game_controller_));
    }

    rumble_timer_.setInterval(150);
    QObject::connect(&rumble_timer_, &QTimer::timeout, [this] { SetRumble(rumbling_); });
}

JoyState::~JoyState() {
    rumble_timer_.stop();

    // Nothing to do if this object is not initialized.
    if (!sdl_joystick_)
        return;

    // If this pad was the active motion-sensor source, detach it so closing the
    // SDL handle below doesn't leave SdlMotion holding a dangling pointer.
    if (game_controller_) {
        vbam::core::SdlMotion::Instance().Detach();
    }

#ifndef ENABLE_SDL3
    if (game_controller_) {
        SDL_GameControllerClose(game_controller_);
    } else {
        SDL_JoystickClose(sdl_joystick_);
    }
#else
    if (game_controller_) {
        SDL_CloseGamepad(game_controller_);
    } else {
        SDL_CloseJoystick(sdl_joystick_);
    }
#endif
}

bool JoyState::IsValid() const {
    return sdl_joystick_ != nullptr;
}

std::vector<UserInputBatch::Data> JoyState::ProcessAxisEvent(const uint8_t index,
                                                             const JoyAxisStatus status) {
    const JoyAxisStatus previous_status = axis_[index];
    std::vector<UserInputBatch::Data> event_data;

    // Nothing to do if no-op.
    if (status == previous_status) {
        return event_data;
    }

    // Update the value.
    axis_[index] = status;

    if (previous_status != JoyAxisStatus::Neutral) {
        // Send the "unpressed" event.
        event_data.emplace_back(
            config::JoyInput(joy_id_, AxisStatusToJoyControl(previous_status), index), false);
    }

    // We already sent the "unpressed" event so nothing more to do.
    if (status == JoyAxisStatus::Neutral) {
        return event_data;
    }

    // Send the "pressed" event.
    event_data.emplace_back(config::JoyInput(joy_id_, AxisStatusToJoyControl(status), index),
                            true);

    return event_data;
}

std::vector<UserInputBatch::Data> JoyState::ProcessButtonEvent(const uint8_t index,
                                                               const bool status) {
    const bool previous_status = buttons_[index];
    std::vector<UserInputBatch::Data> event_data;

    // Nothing to do if no-op.
    if (status == previous_status) {
        return event_data;
    }

    // Update the value.
    buttons_[index] = status;

    // Send the event.
    event_data.emplace_back(config::JoyInput(joy_id_, config::JoyControl::Button, index), status);

    return event_data;
}

std::vector<UserInputBatch::Data> JoyState::ProcessHatEvent(const uint8_t index,
                                                            const uint8_t status) {
    const uint16_t previous_status = hats_[index];
    std::vector<UserInputBatch::Data> event_data;

    // Nothing to do if no-op.
    if (status == previous_status) {
        return event_data;
    }

    // Update the value.
    hats_[index] = status;

    // For HATs, the status value is a bit field, where each bit corresponds to
    // a direction. These are parsed here to send the corresponding "pressed"
    // and "unpressed" events.
    for (uint8_t bit = 0x01; bit != 0x10; bit <<= 1) {
        const bool old_control_pressed = (previous_status & bit) != 0;
        const bool new_control_pressed = (status & bit) != 0;
        if (old_control_pressed && !new_control_pressed) {
            // Send the "unpressed" event.
            event_data.emplace_back(config::JoyInput(joy_id_, HatStatusToJoyControl(bit), index),
                                    false);
        }
        if (!old_control_pressed && new_control_pressed) {
            // Send the "pressed" event.
            event_data.emplace_back(config::JoyInput(joy_id_, HatStatusToJoyControl(bit), index),
                                    true);
        }
    }

    return event_data;
}

void JoyState::SetRumble(bool activate_rumble) {
    rumbling_ = activate_rumble;

#ifdef ENABLE_SDL3
    if (game_controller_ == nullptr)
        return;

    if (rumbling_) {
        SDL_RumbleGamepad(game_controller_, 0xFFFF, 0xFFFF, 300);
        if (!rumble_timer_.isActive()) {
            rumble_timer_.start();
        }
    } else {
        SDL_RumbleGamepad(game_controller_, 0, 0, 0);
        rumble_timer_.stop();
    }
#elif SDL_VERSION_ATLEAST(2, 0, 9)
    if (!game_controller_)
        return;

    if (rumbling_) {
        SDL_GameControllerRumble(game_controller_, 0xFFFF, 0xFFFF, 300);
        if (!rumble_timer_.isActive()) {
            rumble_timer_.start();
        }
    } else {
        SDL_GameControllerRumble(game_controller_, 0, 0, 0);
        rumble_timer_.stop();
    }
#endif
}

SdlPoller::SdlPoller(InputDispatcher* dispatcher)
    : enable_game_controller_(OPTION(kSDLGameControllerMode)),
      dispatcher_(dispatcher),
      game_controller_enabled_observer_(
          config::OptionID::kSDLGameControllerMode,
          [this](config::Option* option) { ReconnectControllers(option->GetBool()); }) {
    VBAM_CHECK(dispatcher_);

#ifndef ENABLE_SDL3
    SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#else
    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
    SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
    SDL_SetGamepadEventsEnabled(true);
    SDL_SetJoystickEventsEnabled(true);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#endif

    // Poll SDL controller events at ~5ms intervals (200Hz), like the wx port:
    // the average latency from a controller button press to a dispatched input
    // is then ~2.5ms, well below the per-frame budget.
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(5);
    connect(&timer_, &QTimer::timeout, this, &SdlPoller::Poll);
    timer_.start();
}

SdlPoller::~SdlPoller() {
    timer_.stop();
    // Close every joystick before shutting the subsystems down.
    joystick_states_.clear();
#ifndef ENABLE_SDL3
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
#else
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
#endif

    SDL_Quit();
}

void SdlPoller::SetRumble(bool rumble) {
#if defined(__ANDROID__)
    AndroidGamepad::Instance().SetRumble(rumble);
    return;
#endif
    if (joystick_states_.empty())
        return;

    auto it = joystick_states_.begin();
    it->second->SetRumble(rumble);
}

void SdlPoller::ReconnectControllers(bool enable_game_controller) {
    enable_game_controller_ = enable_game_controller;
    RemapControllers();
}

void SdlPoller::Poll() {
#if defined(__ANDROID__)
    // SDL's joystick backend is tied to the SDLActivity lifecycle, which the
    // QtActivity host does not provide; the activity hands controller changes
    // to widgets::AndroidGamepad over JNI instead. Drain those here so they
    // take the same route as SDL joystick events elsewhere. Initialize() is
    // idempotent and needs Qt's JNI bridge, which is up by the first tick.
    {
        AndroidGamepad& gamepad = AndroidGamepad::Instance();
        gamepad.Initialize();
        std::vector<UserInputBatch::Data> android_data = gamepad.Drain();
        if (!android_data.empty()) {
            UserInputBatch batch;
            batch.data = std::move(android_data);
            dispatcher_->Dispatch(batch);
        }
    }
#endif
    SDL_Event sdl_event;

    while (SDL_PollEvent(&sdl_event)) {
        std::vector<UserInputBatch::Data> event_data;
        JoyState* joy_state = nullptr;
        switch (sdl_event.type) {
#ifndef ENABLE_SDL3
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                joy_state = FindJoyState(sdl_event.cbutton.which);
                if (joy_state) {
                    event_data = joy_state->ProcessButtonEvent(sdl_event.cbutton.button,
                                                               sdl_event.cbutton.state);
                }
#else
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                joy_state = FindJoyState(sdl_event.gbutton.which);
                if (joy_state) {
                    event_data = joy_state->ProcessButtonEvent(sdl_event.gbutton.button,
                                                               sdl_event.gbutton.down);
                }
#endif
                break;

#ifndef ENABLE_SDL3
            case SDL_CONTROLLERAXISMOTION:
                joy_state = FindJoyState(sdl_event.caxis.which);
                if (joy_state) {
                    event_data = joy_state->ProcessAxisEvent(
                        sdl_event.caxis.axis, AxisValueToStatus(sdl_event.caxis.value));
                }
#else
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                joy_state = FindJoyState(sdl_event.gaxis.which);
                if (joy_state) {
                    event_data = joy_state->ProcessAxisEvent(
                        sdl_event.gaxis.axis, AxisValueToStatus(sdl_event.gaxis.value));
                }
#endif
                break;

#ifndef ENABLE_SDL3
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
#else
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
#endif
                // Do nothing. This will be handled with JOYDEVICEADDED and
                // JOYDEVICEREMOVED events.
                break;

            // Joystick events for non-GameControllers.
#ifndef ENABLE_SDL3
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
#else
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
#endif
                joy_state = FindJoyState(sdl_event.jbutton.which);
                if (joy_state && !joy_state->is_game_controller()) {
#ifndef ENABLE_SDL3
                    event_data = joy_state->ProcessButtonEvent(sdl_event.jbutton.button,
                                                               sdl_event.jbutton.state);
#else
                    event_data = joy_state->ProcessButtonEvent(sdl_event.jbutton.button,
                                                               sdl_event.jbutton.down);
#endif
                }
                break;

#ifndef ENABLE_SDL3
            case SDL_JOYAXISMOTION:
#else
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
#endif
                joy_state = FindJoyState(sdl_event.jaxis.which);
                if (joy_state && !joy_state->is_game_controller()) {
                    event_data = joy_state->ProcessAxisEvent(
                        sdl_event.jaxis.axis, AxisValueToStatus(sdl_event.jaxis.value));
                }
                break;

#ifndef ENABLE_SDL3
            case SDL_JOYHATMOTION:
#else
            case SDL_EVENT_JOYSTICK_HAT_MOTION:
#endif
                joy_state = FindJoyState(sdl_event.jhat.which);
                if (joy_state && !joy_state->is_game_controller()) {
                    event_data =
                        joy_state->ProcessHatEvent(sdl_event.jhat.hat, sdl_event.jhat.value);
                }
                break;

#ifndef ENABLE_SDL3
            case SDL_JOYDEVICEADDED:
#else
            case SDL_EVENT_JOYSTICK_ADDED:
#endif
                // Always remap all controllers.
                RemapControllers();
                break;

#ifndef ENABLE_SDL3
            case SDL_JOYDEVICEREMOVED:
#else
            case SDL_EVENT_JOYSTICK_REMOVED:
#endif
                joystick_states_.erase(sdl_event.jdevice.which);
                break;

            default:
                break;
        }

        if (!event_data.empty()) {
            UserInputBatch batch;
            batch.data = std::move(event_data);
            dispatcher_->Dispatch(batch);
        }
    }
}

JoyState* SdlPoller::FindJoyState(const SDL_JoystickID& joy_id) {
    auto it = joystick_states_.find(joy_id);
    if (it == joystick_states_.end()) {
        return nullptr;
    }

    return it->second.get();
}

void SdlPoller::RemapControllers() {
    // Clear the current joystick states.
    joystick_states_.clear();

#ifdef ENABLE_SDL3
    int total_joysticks = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&total_joysticks);
    SDL_free(ids);
#else
    const int total_joysticks = SDL_NumJoysticks();
#endif

    // Reconnect all controllers.
    for (int i = 0; i < total_joysticks; ++i) {
        auto joy_state = std::make_unique<JoyState>(enable_game_controller_, i);
        if (joy_state->IsValid()) {
            const SDL_JoystickID id = joy_state->joystick_id();
            joystick_states_[id] = std::move(joy_state);
        }
    }
}

uint32_t SdlPoller::GetCurrentSdlMods() {
    const SDL_Keymod sdl_mod = SDL_GetModState();

    uint32_t mod = config::kKeyModNone;

    // Check for specific left/right modifiers
#ifndef ENABLE_SDL3
    if (sdl_mod & KMOD_LSHIFT)
        mod |= config::kKeyModLeftShift;
    if (sdl_mod & KMOD_RSHIFT)
        mod |= config::kKeyModRightShift;
    if (sdl_mod & KMOD_LCTRL)
        mod |= config::kKeyModLeftControl;
    if (sdl_mod & KMOD_RCTRL)
        mod |= config::kKeyModRightControl;
    if (sdl_mod & KMOD_LALT)
        mod |= config::kKeyModLeftAlt;
    if (sdl_mod & KMOD_RALT)
        mod |= config::kKeyModRightAlt;
    if (sdl_mod & KMOD_LGUI)
        mod |= config::kKeyModLeftMeta;
    if (sdl_mod & KMOD_RGUI)
        mod |= config::kKeyModRightMeta;
#else
    if (sdl_mod & SDL_KMOD_LSHIFT)
        mod |= config::kKeyModLeftShift;
    if (sdl_mod & SDL_KMOD_RSHIFT)
        mod |= config::kKeyModRightShift;
    if (sdl_mod & SDL_KMOD_LCTRL)
        mod |= config::kKeyModLeftControl;
    if (sdl_mod & SDL_KMOD_RCTRL)
        mod |= config::kKeyModRightControl;
    if (sdl_mod & SDL_KMOD_LALT)
        mod |= config::kKeyModLeftAlt;
    if (sdl_mod & SDL_KMOD_RALT)
        mod |= config::kKeyModRightAlt;
    if (sdl_mod & SDL_KMOD_LGUI)
        mod |= config::kKeyModLeftMeta;
    if (sdl_mod & SDL_KMOD_RGUI)
        mod |= config::kKeyModRightMeta;
#endif

    return mod;
}

}  // namespace widgets

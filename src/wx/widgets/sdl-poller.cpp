#include "wx/widgets/sdl-poller.h"

#include <map>

#include <wx/timer.h>
#include <wx/toplevel.h>

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include "core/base/check.h"
#include "wx/config/option-id.h"
#include "wx/config/option-observer.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/config/user-input.h"
#include "wx/widgets/user-input-event.h"

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
            VBAM_NOTREACHED();
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
            VBAM_NOTREACHED();
    }
}

}  // namespace

// Represents the current state of a joystick. This class takes care of
// initializing and destroying SDL resources on construction and destruction so
// every associated SDL state for a joystick dies with this object.
class JoyState : public wxTimer {
public:
    JoyState(bool enable_game_controller, int sdl_index);
    ~JoyState() override;

    // Move constructor and assignment.
    JoyState(JoyState&& other);
    JoyState& operator=(JoyState&& other);

    // Disable copy constructor and assignment.
    JoyState(const JoyState&) = delete;
    JoyState& operator=(const JoyState&) = delete;

    // Returns true if this object was properly initialized.
    bool IsValid() const;

    // Returns true if this object is a game controller.
    bool is_game_controller() const { return !!game_controller_; }

    // Processes the corresponding events.
    std::vector<UserInputEvent::Data> ProcessAxisEvent(const uint8_t index,
                                                       const JoyAxisStatus status);
    std::vector<UserInputEvent::Data> ProcessButtonEvent(const uint8_t index, const bool pressed);
    std::vector<UserInputEvent::Data> ProcessHatEvent(const uint8_t index, const uint8_t status);

    // Activates or deactivates rumble.
    void SetRumble(bool activate_rumble);

    SDL_JoystickID joystick_id() const { return joystick_id_; }

private:
    // wxTimer implementation.
    // Used to rumble on a timer.
    void Notify() override;

    // The Joystick abstraction for UI events.
    config::JoyId wx_joystick_;

    // SDL Joystick ID used for events.
    SDL_JoystickID joystick_id_;

    // The SDL GameController instance.
#ifndef ENABLE_SDL3
    SDL_GameController* game_controller_ = NULL;
#else
    SDL_Gamepad* game_controller_ = NULL;
#endif

    // The SDL Joystick instance.
    SDL_Joystick* sdl_joystick_ = NULL;

    // Current state of Joystick axis.
    std::unordered_map<uint8_t, JoyAxisStatus> axis_{};

    // Current state of Joystick buttons.
    std::unordered_map<uint8_t, bool> buttons_{};

    // Current state of Joystick HAT. Unused for GameControllers.
    std::unordered_map<uint8_t, uint8_t> hats_{};

    // Set to true to activate joystick rumble.
    bool rumbling_ = false;
};

JoyState::JoyState(bool enable_game_controller, int sdl_index) : wx_joystick_(sdl_index) {
#ifndef ENABLE_SDL3
    if (enable_game_controller && SDL_IsGameController(sdl_index)) {
        game_controller_ = SDL_GameControllerOpen(sdl_index);
        if (game_controller_)
            sdl_joystick_ = SDL_GameControllerGetJoystick(game_controller_);
    } else {
        sdl_joystick_ = SDL_JoystickOpen(sdl_index);
    }
#else
    int nrgamepads = 0;
    int nrjoysticks = 0;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&nrgamepads);
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&nrjoysticks);
    if (enable_game_controller && SDL_IsGamepad(gamepads[sdl_index])) {
        game_controller_ = SDL_OpenGamepad(gamepads[sdl_index]);
        if (game_controller_)
        {
            sdl_joystick_ = SDL_GetGamepadJoystick(game_controller_);
        }
    } else {
        sdl_joystick_ = SDL_OpenJoystick(joysticks[sdl_index]);
    }
#endif

    if (!sdl_joystick_)
        return;

#ifndef ENABLE_SDL3
    joystick_id_ = SDL_JoystickInstanceID(sdl_joystick_);
#else
    joystick_id_ = SDL_GetJoystickID(sdl_joystick_);
#endif
}

JoyState::~JoyState() {
    // Nothing to do if this object is not initialized.
    if (!sdl_joystick_)
        return;

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

JoyState::JoyState(JoyState&& other) : wx_joystick_(other.wx_joystick_) {
    if (this == &other) {
        return;
    }

    sdl_joystick_ = other.sdl_joystick_;
    game_controller_ = other.game_controller_;
    joystick_id_ = other.joystick_id_;
    axis_ = std::move(other.axis_);
    buttons_ = std::move(other.buttons_);
    hats_ = std::move(other.hats_);
    rumbling_ = other.rumbling_;

    other.sdl_joystick_ = NULL;
    other.game_controller_ = NULL;
}

JoyState& JoyState::operator=(JoyState&& other) {
    if (this == &other) {
        return *this;
    }

    sdl_joystick_ = other.sdl_joystick_;
    game_controller_ = other.game_controller_;
    joystick_id_ = other.joystick_id_;
    axis_ = std::move(other.axis_);
    buttons_ = std::move(other.buttons_);
    hats_ = std::move(other.hats_);
    rumbling_ = other.rumbling_;

    other.sdl_joystick_ = NULL;
    other.game_controller_ = NULL;

    return *this;
}

bool JoyState::IsValid() const {
    return sdl_joystick_;
}

std::vector<UserInputEvent::Data> JoyState::ProcessAxisEvent(const uint8_t index,
                                                             const JoyAxisStatus status) {
    const JoyAxisStatus previous_status = axis_[index];
    std::vector<UserInputEvent::Data> event_data;

    // Nothing to do if no-op.
    if (status == previous_status) {
        return event_data;
    }

    // Update the value.
    axis_[index] = status;

    if (previous_status != JoyAxisStatus::Neutral) {
        // Send the "unpressed" event.
        event_data.emplace_back(
            config::JoyInput(wx_joystick_, AxisStatusToJoyControl(previous_status), index), false);
    }

    // We already sent the "unpressed" event so nothing more to do.
    if (status == JoyAxisStatus::Neutral) {
        return event_data;
    }

    // Send the "pressed" event.
    event_data.emplace_back(config::JoyInput(wx_joystick_, AxisStatusToJoyControl(status), index),
                            true);

    return event_data;
}

std::vector<UserInputEvent::Data> JoyState::ProcessButtonEvent(const uint8_t index,
                                                               const bool status) {
    const bool previous_status = buttons_[index];
    std::vector<UserInputEvent::Data> event_data;

    // Nothing to do if no-op.
    if (status == previous_status) {
        return event_data;
    }

    // Update the value.
    buttons_[index] = status;

    // Send the event.
    event_data.emplace_back(config::JoyInput(wx_joystick_, config::JoyControl::Button, index),
                            status);

    return event_data;
}

std::vector<UserInputEvent::Data> JoyState::ProcessHatEvent(const uint8_t index,
                                                            const uint8_t status) {
    const uint16_t previous_status = hats_[index];
    std::vector<UserInputEvent::Data> event_data;

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
            event_data.emplace_back(
                config::JoyInput(wx_joystick_, HatStatusToJoyControl(bit), index), false);
        }
        if (!old_control_pressed && new_control_pressed) {
            // Send the "pressed" event.
            event_data.emplace_back(
                config::JoyInput(wx_joystick_, HatStatusToJoyControl(bit), index), true);
        }
    }

    return event_data;
}

void JoyState::SetRumble(bool activate_rumble) {
    rumbling_ = activate_rumble;

#ifdef ENABLE_SDL3
        if (game_controller_ == NULL)
            return;

        if (rumbling_) {
            SDL_RumbleGamepad(game_controller_, 0xFFFF, 0xFFFF, 300);
            if (!IsRunning()) {
                Start(150);
            }
        } else {
            SDL_RumbleGamepad(game_controller_, 0, 0, 0);
            Stop();
        }
#elif SDL_VERSION_ATLEAST(2, 0, 9)
    if (!game_controller_)
        return;

    if (rumbling_) {
        SDL_GameControllerRumble(game_controller_, 0xFFFF, 0xFFFF, 300);
        if (!IsRunning()) {
            Start(150);
        }
    } else {
        SDL_GameControllerRumble(game_controller_, 0, 0, 0);
        Stop();
    }
#endif
}

void JoyState::Notify() {
    SetRumble(rumbling_);
}

SdlPoller::SdlPoller(EventHandlerProvider* const handler_provider)
    : enable_game_controller_(OPTION(kSDLGameControllerMode)),
      handler_provider_(handler_provider),
      game_controller_enabled_observer_(
          config::OptionID::kSDLGameControllerMode,
          [this](config::Option* option) { ReconnectControllers(option->GetBool()); }) {
    VBAM_CHECK(handler_provider);

    wxTimer::Start(50);
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
#endif
}

SdlPoller::~SdlPoller() {
    wxTimer::Stop();
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
    if (rumble) {
        if (!joystick_states_.empty()) {
            auto it = joystick_states_.begin();
            it->second.SetRumble(true);
        }
    } else {
        if (!joystick_states_.empty()) {
            auto it = joystick_states_.begin();
            it->second.SetRumble(false);
        }
    }
}

void SdlPoller::ReconnectControllers(bool enable_game_controller) {
    enable_game_controller_ = enable_game_controller;
    RemapControllers();
}

void SdlPoller::Notify() {
    SDL_Event sdl_event;

    while (SDL_PollEvent(&sdl_event)) {
        std::vector<UserInputEvent::Data> event_data;
        JoyState* joy_state = NULL;
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
                    event_data = joy_state->ProcessButtonEvent(sdl_event.cbutton.button,
                                                               sdl_event.cbutton.state);
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
#ifndef ENABLE_SDL3
                        sdl_event.caxis.axis, AxisValueToStatus(sdl_event.caxis.value));
#else
                        sdl_event.jaxis.axis, AxisValueToStatus(sdl_event.jaxis.value));
#endif
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
        }

        if (!event_data.empty()) {
            wxEvtHandler* handler = handler_provider_->event_handler();
            if (handler) {
                handler->QueueEvent(new UserInputEvent(std::move(event_data)));
            }
        }
    }
}

JoyState* SdlPoller::FindJoyState(const SDL_JoystickID& joy_id) {
    auto it = joystick_states_.find(joy_id);
    if (it == joystick_states_.end()) {
        return NULL;
    }

    return &it->second;
}

void SdlPoller::RemapControllers() {
#ifdef ENABLE_SDL3
    int total_joysticks = 0;

    SDL_GetJoysticks(&total_joysticks);
#endif

    // Clear the current joystick states.
    joystick_states_.clear();

    // Reconnect all controllers.
#ifndef ENABLE_SDL3
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
#else
    for (int i = 0; i < total_joysticks; ++i) {
#endif
        JoyState joy_state(enable_game_controller_, i);
        if (joy_state.IsValid()) {
            joystick_states_.insert({joy_state.joystick_id(), std::move(joy_state)});
        }
    }
}

}  // namespace widgets

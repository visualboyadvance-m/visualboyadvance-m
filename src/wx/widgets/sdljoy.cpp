#include "wx/sdljoy.h"

#include <algorithm>
#include <wx/timer.h>
#include <SDL.h>

#include "../common/contains.h"
#include "../wxvbam.h"

namespace {

const std::string SDLEventTypeToDebugString(int32_t event_type) {
    switch (event_type) {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        return "SDL_CONTROLLERBUTTON";
    case SDL_CONTROLLERAXISMOTION:
        return "SDL_CONTROLLERAXISMOTION";
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        return "SDL_JOYBUTTON";
    case SDL_JOYAXISMOTION:
        return "SDL_JOYAXISMOTION";
    case SDL_JOYHATMOTION:
        return "SDL_JOYHATMOTION";
    default:
        assert(false);
        return "UNKNOWN SDL EVENT";
    }
}

// Converts an axis value to a direction since we do not need analog controls.
static int16_t AxisValueToDirection(int16_t x) {
    if (x > 0x1fff)
        return 1;
    else if (x < -0x1fff)
        return -1;

    return 0;
}

// The interval between 2 polls in ms.
const wxLongLong kPollTimeInterval(25);

} // namespace

// For testing a GameController as a Joystick:
//#define SDL_IsGameController(x) false

DEFINE_EVENT_TYPE(wxEVT_SDLJOY)

wxSDLJoyEvent::wxSDLJoyEvent(
    unsigned player_index,
    wxSDLControl control,
    uint8_t control_index,
    int16_t control_value) :
        wxCommandEvent(wxEVT_SDLJOY),
        player_index_(player_index),
        control_(control),
        control_index_(control_index),
        control_value_(control_value) {}

// Represents the current state of a joystick. This class takes care of
// initializing and destroying SDL resources on construction and destruction so
// every associated SDL state for a joystick dies with this object.
class wxSDLJoyState : public wxTimer {
public:
    explicit wxSDLJoyState(int sdl_index);
    ~wxSDLJoyState() override;

    // Disable copy constructor and assignment. This is to prevent double
    // closure of the SDL objects.
    wxSDLJoyState(const wxSDLJoyState&) = delete;
    wxSDLJoyState& operator=(const wxSDLJoyState&) = delete;

    // Returns true if this object was properly initialized.
    bool IsValid() const;

    // Processes an SDL event.
    void ProcessEvent(int32_t event_type,
                      uint8_t control_index,
                      int16_t control_value);

    // Polls the current state of the joystick and sends events as needed.
    void Poll();

    // Activates or deactivates rumble.
    void SetRumble(bool activate_rumble);

    SDL_JoystickID joystick_id() const { return joystick_id_; }

private:
    // wxTimer implementation.
    // Used to rumble on a timer.
    void Notify() override;

    // The Joystick player index.
    unsigned player_index_;

    // SDL Joystick ID used for events.
    SDL_JoystickID joystick_id_;

    // The SDL GameController instance.
    SDL_GameController* game_controller_ = nullptr;

    // The SDL Joystick instance.
    SDL_Joystick* joystick_ = nullptr;

    // Current state of Joystick axis.
    std::unordered_map<uint8_t, int16_t> axis_{};

    // Current state of Joystick buttons.
    std::unordered_map<uint8_t, uint8_t> buttons_{};

    // Current state of Joystick HAT. Unused for GameControllers.
    std::unordered_map<uint8_t, uint8_t> hats_{};

    // Set to true to activate joystick rumble.
    bool rumbling_ = false;
};

wxSDLJoyState::wxSDLJoyState(int sdl_index)
    : player_index_(sdl_index + 1) {
    if (SDL_IsGameController(sdl_index)) {
        game_controller_ = SDL_GameControllerOpen(sdl_index);
        if (game_controller_)
            joystick_ = SDL_GameControllerGetJoystick(game_controller_);
    } else {
        joystick_ = SDL_JoystickOpen(sdl_index);
    }

    if (!joystick_)
        return;

    joystick_id_ = SDL_JoystickInstanceID(joystick_);
    systemScreenMessage(
        wxString::Format(_("Connected joystick %d: %s"),
                            player_index_, SDL_JoystickNameForIndex(sdl_index)));
}

wxSDLJoyState::~wxSDLJoyState() {
    // Nothing to do if this object is not initialized.
    if (!joystick_)
        return;

    if (game_controller_)
        SDL_GameControllerClose(game_controller_);
    else
        SDL_JoystickClose(joystick_);

    systemScreenMessage(
        wxString::Format(_("Disconnected joystick %d"), player_index_));
}

bool wxSDLJoyState::IsValid() const {
    return joystick_;
}

void wxSDLJoyState::ProcessEvent(int32_t event_type,
                                 uint8_t control_index,
                                 int16_t control_value) {
    int16_t previous_value = 0;
    wxSDLControl control;
    bool value_changed = false;

    switch (event_type) {
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        // Do not process joystick events for game controllers.
        if (game_controller_) {
            return;
        }
    // Fallhrough.
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        control = wxSDLControl::WXSDLJOY_BUTTON;
        previous_value = buttons_[control_index];
        if (previous_value != control_value) {
            buttons_[control_index] = control_value;
            value_changed = true;
        }
        break;

    case SDL_JOYHATMOTION:
        // Do not process joystick events for game controllers.
        if (game_controller_) {
            return;
        }
        control = wxSDLControl::WXSDLJOY_HAT;
        previous_value = hats_[control_index];
        if (previous_value != control_value) {
            hats_[control_index] = control_value;
            value_changed = true;
        }
        break;

    case SDL_JOYAXISMOTION:
        // Do not process joystick events for game controllers.
        if (game_controller_) {
            return;
        }
    // Fallhrough.
    case SDL_CONTROLLERAXISMOTION:
        control = wxSDLControl::WXSDLJOY_AXIS;
        previous_value = axis_[control_index];
        if (previous_value != control_value) {
            axis_[control_index] = control_value;
            value_changed = true;
        }
        break;

    default:
        // This should never happen.
        assert(false);
        return;
    }

    if (value_changed) {
        wxLogDebug("GOT %s: joy:%d ctrl_idx:%d val:%d prev_val:%d",
                   SDLEventTypeToDebugString(event_type), player_index_,
                   control_index, control_value, previous_value);

        auto handler = wxGetApp().frame->GetJoyEventHandler();
        if (!handler)
            return;

        wxQueueEvent(handler,
                     new wxSDLJoyEvent(
                         player_index_, control, control_index, control_value));
    }
}

void wxSDLJoyState::Poll() {
    if (game_controller_) {
        for (uint8_t but = 0; but < SDL_CONTROLLER_BUTTON_MAX; but++) {
            uint16_t previous_value = buttons_[but];
            uint16_t current_value =
                SDL_GameControllerGetButton(
                    game_controller_,
                    static_cast<SDL_GameControllerButton>(but));

            if (previous_value != current_value)
                ProcessEvent(SDL_CONTROLLERBUTTONUP, but, current_value);
        }

        for (uint8_t axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
            uint16_t previous_value = axis_[axis];
            uint16_t current_value =
                AxisValueToDirection(
                    SDL_GameControllerGetAxis(
                        game_controller_,
                        static_cast<SDL_GameControllerAxis>(axis)));

            if (previous_value != current_value)
                ProcessEvent(SDL_CONTROLLERAXISMOTION, axis, current_value);
        }
    } else {
        for (uint8_t but = 0; but < SDL_JoystickNumButtons(joystick_); but++) {
            uint16_t previous_value = buttons_[but];
            uint16_t current_value = SDL_JoystickGetButton(joystick_, but);

            if (previous_value != current_value)
                ProcessEvent(SDL_JOYBUTTONUP, but, current_value);
        }

        for (uint8_t axis = 0; axis < SDL_JoystickNumAxes(joystick_); axis++) {
            uint16_t previous_value = axis_[axis];
            uint16_t current_value =
                AxisValueToDirection(SDL_JoystickGetButton(joystick_, axis));

            if (previous_value != current_value)
                ProcessEvent(SDL_JOYAXISMOTION, axis, current_value);
        }

        for (uint8_t hat = 0; hat < SDL_JoystickNumHats(joystick_); hat++) {
            uint16_t previous_value = hats_[hat];
            uint16_t current_value = SDL_JoystickGetHat(joystick_, hat);

            if (previous_value != current_value)
                ProcessEvent(SDL_JOYHATMOTION, hat, current_value);
        }
    }
}

void wxSDLJoyState::SetRumble(bool activate_rumble) {
    rumbling_ = activate_rumble;

#if SDL_VERSION_ATLEAST(2, 0, 9)
    if (!game_controller_)
        return;

    if (rumbling_) {
        SDL_GameControllerRumble(game_controller_, 0xFFFF, 0xFFFF, 300);
        if (!IsRunning())
            Start(150);
    } else {
        SDL_GameControllerRumble(game_controller_, 0, 0, 0);
        Stop();
    }
#endif
}

void wxSDLJoyState::Notify() {
    SetRumble(rumbling_);
}

wxSDLJoy::wxSDLJoy() {
    // Start up joystick if not already started
    // FIXME: check for errors
    SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
}

wxSDLJoy::~wxSDLJoy() {
    // It is necessary to free all SDL resources before quitting SDL.
    joystick_states_.clear();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void wxSDLJoy::Poll() {
    SDL_Event e;
    bool got_event = false;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.cbutton.which);
            if (joy_state) {
                joy_state->ProcessEvent(
                    e.type, e.cbutton.button, e.cbutton.state);
            }
            got_event = true;
            break;
        }

        case SDL_CONTROLLERAXISMOTION:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.caxis.which);
            if (joy_state) {
                joy_state->ProcessEvent(
                    e.type, e.caxis.axis, AxisValueToDirection(e.caxis.value));
            }
            got_event = true;
            break;
        }

        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            // Do nothing. This will be handled with JOYDEVICEADDED and
            // JOYDEVICEREMOVED events.
            break;

        // Joystick events for non-GameControllers.
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.jbutton.which);
            if (joy_state) {
                joy_state->ProcessEvent(
                    e.type, e.jbutton.button, e.jbutton.state);
            }
            got_event = true;
            break;
        }

        case SDL_JOYAXISMOTION:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.jaxis.which);
            if (joy_state) {
                joy_state->ProcessEvent(
                    e.type, e.jaxis.axis, AxisValueToDirection(e.jaxis.value));
            }
            got_event = true;
            break;
        }

        case SDL_JOYHATMOTION:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.jhat.which);
            if (joy_state) {
                joy_state->ProcessEvent(
                    e.type, e.jhat.hat, AxisValueToDirection(e.jhat.value));
            }
            got_event = true;
            break;
        }

        case SDL_JOYDEVICEADDED:
        {
            // Always remap all controllers.
            RemapControllers();
            got_event = true;
            break;
        }

        case SDL_JOYDEVICEREMOVED:
        {
            joystick_states_.erase(e.jdevice.which);
            got_event = true;
            break;
        }

        default:
            // Ignore all other events.
            break;
        }
    }

    wxLongLong now = wxGetUTCTimeMillis();
    if (got_event) {
        last_poll_ = now;
    } else if (now - last_poll_ > kPollTimeInterval) {
        for (auto&& joy_state : joystick_states_) {
            joy_state.second->Poll();
        }
        last_poll_ = now;
    }
}

void wxSDLJoy::RemapControllers() {
    if (!is_polling_active_) {
        // Nothing to do when we're not actively polling.
        return;
    }

    joystick_states_.clear();

    if (requested_sdl_indexes_.empty()) {
        // Connect all joysticks.
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            std::unique_ptr<wxSDLJoyState> joy_state(new wxSDLJoyState(i));
            if (joy_state->IsValid()) {
                joystick_states_.emplace(
                    joy_state->joystick_id(), std::move(joy_state));
            }
        }
    } else {
        // Only attempt to add the joysticks we care about.
        for (const int& sdl_index : requested_sdl_indexes_) {
            std::unique_ptr<wxSDLJoyState> joy_state(
                new wxSDLJoyState(sdl_index));
            if (joy_state->IsValid()) {
                joystick_states_.emplace(
                    joy_state->joystick_id(), std::move(joy_state));
            }
        }
    }
}

wxSDLJoyState* wxSDLJoy::FindJoyState(const SDL_JoystickID& joy_id) {
    const auto iter = joystick_states_.find(joy_id);
    if (iter == joystick_states_.end())
        return nullptr;
    return iter->second.get();
}

void wxSDLJoy::PollJoysticks(std::unordered_set<unsigned> indexes) {
    // Reset the polling state.
    StopPolling();

    if (indexes.empty()) {
        // Nothing to poll. Return early.
        return;
    }

    is_polling_active_ = true;
    std::for_each(
        indexes.begin(), indexes.end(),
        [&](const unsigned& player_index) {
            requested_sdl_indexes_.insert(player_index - 1);
        });
    RemapControllers();
}

void wxSDLJoy::PollAllJoysticks() {
    // Reset the polling state.
    StopPolling();
    is_polling_active_ = true;
    RemapControllers();
}

void wxSDLJoy::StopPolling() {
    joystick_states_.clear();
    requested_sdl_indexes_.clear();
    is_polling_active_ = false;
}

void wxSDLJoy::SetRumble(bool activate_rumble) {
    if (joystick_states_.empty())
        return;

    // Do rumble only on the first device.
    joystick_states_.begin()->second->SetRumble(activate_rumble);
}

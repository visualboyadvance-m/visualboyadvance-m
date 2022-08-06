#include "wx/sdljoy.h"

#include <algorithm>
#include <wx/timer.h>
#include <SDL.h>

#include "../wxvbam.h"

namespace {

enum class wxAxisStatus {
    Neutral = 0,
    Plus,
    Minus
};

enum class wxHatStatus {
    Neutral = 0,
    North,
    South,
    West,
    East,
    NorthWest,
    NorthEast,
    SouthWest,
    SouthEast
};

wxAxisStatus AxisValueToStatus(const int16_t& x) {
    if (x > 0x1fff)
        return wxAxisStatus::Plus;
    if (x < -0x1fff)
        return wxAxisStatus::Minus;
    return wxAxisStatus::Neutral;
}

wxHatStatus HatValueToStatus(const int16_t& x) {
    switch (x) {
    case 1:
        return wxHatStatus::North;
    case 2:
        return wxHatStatus::East;
    case 3:
        return wxHatStatus::NorthEast;
    case 4:
        return wxHatStatus::South;
    case 6:
        return wxHatStatus::SouthEast;
    case 8:
        return wxHatStatus::West;
    case 9:
        return wxHatStatus::NorthWest;
    case 12:
        return wxHatStatus::SouthWest;
    default:
        return wxHatStatus::Neutral;
    }
}

wxJoyControl AxisStatusToJoyControl(const wxAxisStatus& status) {
    switch (status) {
    case wxAxisStatus::Plus:
        return wxJoyControl::AxisPlus;
    case wxAxisStatus::Minus:
        return wxJoyControl::AxisMinus;
    case wxAxisStatus::Neutral:
    default:
        // This should never happen.
        assert(false);
        return wxJoyControl::AxisPlus;
    }
}

std::set<wxJoyControl> HatStatusToJoyControls(const wxHatStatus& status) {
    switch (status) {
    case wxHatStatus::Neutral:
        return {};
    case wxHatStatus::North:
        return {wxJoyControl::HatNorth};
    case wxHatStatus::South:
        return {wxJoyControl::HatSouth};
    case wxHatStatus::West:
        return {wxJoyControl::HatWest};
    case wxHatStatus::East:
        return {wxJoyControl::HatEast};
    case wxHatStatus::NorthWest:
        return {wxJoyControl::HatNorth, wxJoyControl::HatWest};
    case wxHatStatus::NorthEast:
        return {wxJoyControl::HatNorth, wxJoyControl::HatEast};
    case wxHatStatus::SouthWest:
        return {wxJoyControl::HatSouth, wxJoyControl::HatWest};
    case wxHatStatus::SouthEast:
        return {wxJoyControl::HatSouth, wxJoyControl::HatEast};
    default:
        // This should never happen.
        assert(false);
        return {};
    }
}

} // namespace

// For testing a GameController as a Joystick:
//#define SDL_IsGameController(x) false

DEFINE_EVENT_TYPE(wxEVT_JOY)

// static
wxJoystick wxJoystick::Invalid() {
    return wxJoystick(kInvalidSdlIndex);
}

// static
wxJoystick wxJoystick::FromLegacyPlayerIndex(unsigned player_index) {
    assert(player_index != 0);
    return wxJoystick(player_index - 1);
}

wxString wxJoystick::ToString() {
    return wxString::Format("Joy%d", sdl_index_ + 1);
}

bool wxJoystick::operator==(const wxJoystick& other) const {
    return sdl_index_ == other.sdl_index_;
}
bool wxJoystick::operator!=(const wxJoystick& other) const {
    return !(*this == other);
}
bool wxJoystick::operator<(const wxJoystick& other) const {
    return sdl_index_ < other.sdl_index_;
}
bool wxJoystick::operator<=(const wxJoystick& other) const {
    return !(*this > other);
}
bool wxJoystick::operator>(const wxJoystick& other) const {
    return other < *this;
}
bool wxJoystick::operator>=(const wxJoystick& other) const {
    return !(*this < other);
}

wxJoystick::wxJoystick(int sdl_index) : sdl_index_(sdl_index) {}

wxJoyEvent::wxJoyEvent(
    wxJoystick joystick,
    wxJoyControl control,
    uint8_t control_index,
    bool pressed) :
        wxCommandEvent(wxEVT_JOY),
        joystick_(joystick),
        control_(control),
        control_index_(control_index),
        pressed_(pressed) {}

// Represents the current state of a joystick. This class takes care of
// initializing and destroying SDL resources on construction and destruction so
// every associated SDL state for a joystick dies with this object.
class wxSDLJoyState : public wxTimer {
public:
    explicit wxSDLJoyState(int sdl_index);
    explicit wxSDLJoyState(wxJoystick joystick);
    ~wxSDLJoyState() override;

    // Disable copy constructor and assignment. This is to prevent double
    // closure of the SDL objects.
    wxSDLJoyState(const wxSDLJoyState&) = delete;
    wxSDLJoyState& operator=(const wxSDLJoyState&) = delete;

    // Returns true if this object was properly initialized.
    bool IsValid() const;

    // Returns true if `sdl_event` should be processed by this object.
    bool ShouldProcessEvent(uint32_t sdl_event) const;

    // Processes the corresponding events.
    void ProcessAxisEvent(uint8_t index, wxAxisStatus status);
    void ProcessButtonEvent(uint8_t index, bool pressed);
    void ProcessHatEvent(uint8_t index, wxHatStatus status);

    // Activates or deactivates rumble.
    void SetRumble(bool activate_rumble);

    SDL_JoystickID joystick_id() const { return joystick_id_; }

private:
    // wxTimer implementation.
    // Used to rumble on a timer.
    void Notify() override;

    // The Joystick abstraction for UI events.
    wxJoystick wx_joystick_;

    // SDL Joystick ID used for events.
    SDL_JoystickID joystick_id_;

    // The SDL GameController instance.
    SDL_GameController* game_controller_ = nullptr;

    // The SDL Joystick instance.
    SDL_Joystick* sdl_joystick_ = nullptr;

    // Current state of Joystick axis.
    std::unordered_map<uint8_t, wxAxisStatus> axis_{};

    // Current state of Joystick buttons.
    std::unordered_map<uint8_t, bool> buttons_{};

    // Current state of Joystick HAT. Unused for GameControllers.
    std::unordered_map<uint8_t, wxHatStatus> hats_{};

    // Set to true to activate joystick rumble.
    bool rumbling_ = false;
};

wxSDLJoyState::wxSDLJoyState(int sdl_index)
    : wxSDLJoyState(wxJoystick(sdl_index)) {}

wxSDLJoyState::wxSDLJoyState(wxJoystick joystick) : wx_joystick_(joystick) {
    int sdl_index = wx_joystick_.sdl_index_;
    if (SDL_IsGameController(sdl_index)) {
        game_controller_ = SDL_GameControllerOpen(sdl_index);
        if (game_controller_)
            sdl_joystick_ = SDL_GameControllerGetJoystick(game_controller_);
    } else {
        sdl_joystick_ = SDL_JoystickOpen(sdl_index);
    }

    if (!sdl_joystick_)
        return;

    joystick_id_ = SDL_JoystickInstanceID(sdl_joystick_);
    systemScreenMessage(
        wxString::Format(_("Connected %s: %s"),
            wx_joystick_.ToString(), SDL_JoystickNameForIndex(sdl_index)));
}

wxSDLJoyState::~wxSDLJoyState() {
    // Nothing to do if this object is not initialized.
    if (!sdl_joystick_)
        return;

    if (game_controller_)
        SDL_GameControllerClose(game_controller_);
    else
        SDL_JoystickClose(sdl_joystick_);

    systemScreenMessage(
        wxString::Format(_("Disconnected %s"), wx_joystick_.ToString()));
}

bool wxSDLJoyState::IsValid() const {
    return sdl_joystick_;
}

bool wxSDLJoyState::ShouldProcessEvent(uint32_t sdl_event) const {
    switch(sdl_event) {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERAXISMOTION:
    case SDL_CONTROLLERDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED:
        // Always process game controller events.
        return true;

    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
    case SDL_JOYAXISMOTION:
    case SDL_JOYHATMOTION:
        // Only process joystick events if this is not a game controller.
        return !game_controller_;

    default:
        // Ignore everything else.
        return false;
    }
}

void wxSDLJoyState::ProcessAxisEvent(uint8_t index, wxAxisStatus status) {
    auto handler = wxGetApp().frame->GetJoyEventHandler();
    const wxAxisStatus previous_status = axis_[index];

    // Nothing to do if no-op.
    if (status == previous_status) {
        return;
    }

    // Update the value.
    axis_[index] = status;

    // Nothing more to do if we can't send events.
    if (!handler) {
        return;
    }

    wxLogDebug("Got Axis motion: %s ctrl_idx:%d val:%d prev_val:%d",
                wx_joystick_.ToString(), index, status, previous_status);

    if (previous_status != wxAxisStatus::Neutral) {
        // Send the "unpressed" event.
        wxQueueEvent(handler, new wxJoyEvent(
            wx_joystick_, AxisStatusToJoyControl(previous_status), index, false));
    }

    // We already sent the "unpressed" event so nothing more to do.
    if (status == wxAxisStatus::Neutral) {
        return;
    }

    // Send the "pressed" event.
    wxQueueEvent(handler, new wxJoyEvent(
        wx_joystick_, AxisStatusToJoyControl(status), index, true));

}

void wxSDLJoyState::ProcessButtonEvent(uint8_t index, bool status) {
    auto handler = wxGetApp().frame->GetJoyEventHandler();
    const bool previous_status = buttons_[index];

    // Nothing to do if no-op.
    if (status == previous_status) {
        return;
    }

    // Update the value.
    buttons_[index] = status;

    // Nothing more to do if we can't send events.
    if (!handler) {
        return;
    }

    wxLogDebug("Got Button event: %s ctrl_idx:%d val:%d prev_val:%d",
                wx_joystick_.ToString(), index, status, previous_status);

    // Send the event.
    wxQueueEvent(handler, new wxJoyEvent(
        wx_joystick_, wxJoyControl::Button, index, status));
}

void wxSDLJoyState::ProcessHatEvent(uint8_t index, wxHatStatus status) {
    auto handler = wxGetApp().frame->GetJoyEventHandler();
    const wxHatStatus previous_status = hats_[index];

    // Nothing to do if no-op.
    if (status == previous_status) {
        return;
    }

    // Update the value.
    hats_[index] = status;

    // Nothing more to do if we can't send events.
    if (!handler) {
        return;
    }

    wxLogDebug("Got Hat event: %s ctrl_idx:%d val:%d prev_val:%d",
                wx_joystick_.ToString(), index, status, previous_status);

    const std::set<wxJoyControl> old_controls = HatStatusToJoyControls(previous_status);
    const std::set<wxJoyControl> new_controls = HatStatusToJoyControls(status);

    // Send the "unpressed" events.
    for (const wxJoyControl& control : old_controls) {
        if (new_controls.find(control) != new_controls.end()) {
            // No need to unpress the old direction.
            continue;
        }
        wxQueueEvent(handler, new wxJoyEvent(
            wx_joystick_, control, index, false));
    }

    // Send the "pressed" events.
    for (const wxJoyControl& control : new_controls) {
        if (old_controls.find(control) != old_controls.end()) {
            // No need to press the new direction twice.
            continue;
        }
        wxQueueEvent(handler, new wxJoyEvent(
            wx_joystick_, control, index, true));
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

wxJoyPoller::wxJoyPoller() {
    // Start up joystick if not already started
    // FIXME: check for errors
    SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
}

wxJoyPoller::~wxJoyPoller() {
    // It is necessary to free all SDL resources before quitting SDL.
    joystick_states_.clear();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void wxJoyPoller::Poll() {
    SDL_Event e;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.cbutton.which);
            if (joy_state && joy_state->ShouldProcessEvent(e.type)) {
                joy_state->ProcessButtonEvent(
                    e.cbutton.button, e.cbutton.state);
            }
            break;
        }

        case SDL_CONTROLLERAXISMOTION:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.caxis.which);
            if (joy_state && joy_state->ShouldProcessEvent(e.type)) {
                joy_state->ProcessAxisEvent(
                    e.caxis.axis, AxisValueToStatus(e.caxis.value));
            }
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
            if (joy_state && joy_state->ShouldProcessEvent(e.type)) {
                joy_state->ProcessButtonEvent(
                    e.jbutton.button, e.jbutton.state);
            }
            break;
        }

        case SDL_JOYAXISMOTION:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.jaxis.which);
            if (joy_state && joy_state->ShouldProcessEvent(e.type)) {
                joy_state->ProcessAxisEvent(
                    e.jaxis.axis, AxisValueToStatus(e.jaxis.value));
            }
            break;
        }

        case SDL_JOYHATMOTION:
        {
            wxSDLJoyState* joy_state = FindJoyState(e.jhat.which);
            if (joy_state && joy_state->ShouldProcessEvent(e.type)) {
                joy_state->ProcessHatEvent(
                    e.jhat.hat, HatValueToStatus(e.jhat.value));
            }
            break;
        }

        case SDL_JOYDEVICEADDED:
        {
            // Always remap all controllers.
            RemapControllers();
            break;
        }

        case SDL_JOYDEVICEREMOVED:
        {
            joystick_states_.erase(e.jdevice.which);
            break;
        }

        default:
            // Ignore all other events.
            break;
        }
    }
}

void wxJoyPoller::RemapControllers() {
    if (!is_polling_active_) {
        // Nothing to do when we're not actively polling.
        return;
    }

    joystick_states_.clear();

    if (requested_joysticks_.empty()) {
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
        for (const wxJoystick& joystick : requested_joysticks_) {
            std::unique_ptr<wxSDLJoyState> joy_state(
                new wxSDLJoyState(joystick));
            if (joy_state->IsValid()) {
                joystick_states_.emplace(
                    joy_state->joystick_id(), std::move(joy_state));
            }
        }
    }
}

wxSDLJoyState* wxJoyPoller::FindJoyState(const SDL_JoystickID& joy_id) {
    const auto iter = joystick_states_.find(joy_id);
    if (iter == joystick_states_.end())
        return nullptr;
    return iter->second.get();
}

void wxJoyPoller::PollJoysticks(std::set<wxJoystick> joysticks) {
    // Reset the polling state.
    StopPolling();

    if (joysticks.empty()) {
        // Nothing to poll. Return early.
        return;
    }

    is_polling_active_ = true;
    requested_joysticks_ = joysticks;
    RemapControllers();
}

void wxJoyPoller::PollAllJoysticks() {
    // Reset the polling state.
    StopPolling();
    is_polling_active_ = true;
    RemapControllers();
}

void wxJoyPoller::StopPolling() {
    joystick_states_.clear();
    requested_joysticks_.clear();
    is_polling_active_ = false;
}

void wxJoyPoller::SetRumble(bool activate_rumble) {
    if (joystick_states_.empty())
        return;

    // Do rumble only on the first device.
    joystick_states_.begin()->second->SetRumble(activate_rumble);
}

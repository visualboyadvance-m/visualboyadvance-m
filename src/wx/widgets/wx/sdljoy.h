#ifndef JOYEVT_H
#define JOYEVT_H

#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <wx/time.h>
#include <wx/event.h>
#include <SDL_joystick.h>
#include <SDL_gamecontroller.h>
#include <SDL_events.h>

// The different types of supported controls.
enum wxSDLControl {
    WXSDLJOY_AXIS,  // Control value is signed 16
    WXSDLJOY_HAT,   // Control value is bitmask NESW/URDL
    WXSDLJOY_BUTTON // Control value is 0 or 1
};

// Represents a Joystick event.
class wxSDLJoyEvent : public wxCommandEvent {
public:
    wxSDLJoyEvent(
        unsigned player_index,
        wxSDLControl control,
        uint8_t control_index,
        int16_t control_value);
    virtual ~wxSDLJoyEvent() = default;

    unsigned player_index() const { return player_index_; }
    wxSDLControl control() const { return control_; }
    uint8_t control_index() const { return control_index_; }
    int16_t control_value() const { return control_value_; }

private:
    unsigned player_index_;
    wxSDLControl control_;
    uint8_t control_index_;
    int16_t control_value_;
};

class wxSDLJoyState;

// This is my own SDL-based joystick handler, since wxJoystick is brain-dead.
// It's geared towards keyboard emulation.
//
// After initilization, use PollJoystick() or PollAllJoysticks() for the
// joysticks you wish to monitor. The target window will then receive
// EVT_SDLJOY events of type wxSDLJoyEvent.
// Handling of the player_index() value is different depending on the polling
// mode. After calls to PollJoysticks(), that value will remain constant for a
// given device, even if other joysticks disconnect. This ensures the joystick
// remains active during gameplay even if other joysticks disconnect.
// However, after calls to PollAllJoysticks(), all joysticks are re-connected
// on joystick connect/disconnect. This ensures the right player_index() value
// is sent to the UI during input event configuration.
class wxSDLJoy {
public:
    wxSDLJoy();
    ~wxSDLJoy();

    // Adds a set of joysticks to the list of polled joysticks.
    // This will disconnect every active joysticks, and reactivates the ones
    // matching an index in |indexes|. Missing joysticks will be connected if
    // they connect later on.
    void PollJoysticks(std::unordered_set<unsigned> indexes);

    // Adds all joysticks to the list of polled joysticks. This will
    // disconnect every active joysticks, reconnect them and start polling.
    void PollAllJoysticks();

    // Removes all joysticks from the list of polled joysticks.
    // This will stop polling.
    void StopPolling();

    // Activates or deactivates rumble on active joysticks.
    void SetRumble(bool activate_rumble);

    // Polls active joysticks and empties the SDL event buffer.
    void Poll();

private:
    // Reconnects all controllers.
    void RemapControllers();

    // Helper method to find a joystick state from a joystick ID.
    // Returns nullptr if not present.
    wxSDLJoyState* FindJoyState(const SDL_JoystickID& joy_id);

    // Map of SDL joystick ID to joystick state. Only contains active joysticks.
    std::unordered_map<SDL_JoystickID, std::unique_ptr<wxSDLJoyState>> joystick_states_;

    // Set of requested SDL joystick indexes.
    std::unordered_set<int> requested_sdl_indexes_;

    // Set to true when we are actively polling controllers.
    bool is_polling_active_ = false;

    // Timestamp when the latest poll was done.
    wxLongLong last_poll_ = wxGetUTCTimeMillis();
};

// Note: this means sdljoy can't be part of a library w/o extra work
DECLARE_LOCAL_EVENT_TYPE(wxEVT_SDLJOY, -1)
typedef void (wxEvtHandler::*wxSDLJoyEventFunction)(wxSDLJoyEvent&);
#define EVT_SDLJOY(fn)                                                   \
    DECLARE_EVENT_TABLE_ENTRY(wxEVT_SDLJOY,                              \
        wxID_ANY,                                                        \
        wxID_ANY,                                                        \
        (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction) \
            wxStaticCastEvent(wxSDLJoyEventFunction, &fn),               \
        (wxObject*)NULL)                                                 \
    ,

#endif /* JOYEVT_H */

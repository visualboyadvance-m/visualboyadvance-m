#ifndef _WX_SDLJOY_H
#define _WX_SDLJOY_H

#include <memory>
#include <set>
#include <map>
#include <unordered_map>
#include <wx/time.h>
#include <wx/event.h>
#include <SDL_joystick.h>
#include <SDL_gamecontroller.h>
#include <SDL_events.h>

// Forward declarations.
class wxSDLJoyState;
class wxJoyPoller;

// The different types of supported controls.
enum wxJoyControl {
    AxisPlus = 0,
    AxisMinus,
    Button,
    HatNorth,
    HatSouth,
    HatWest,
    HatEast,
    Last = HatEast
};

// Abstraction for a single joystick. In the current implementation, this
// encapsulates an |sdl_index_|. Creation of these objects should only be
// handled by wxSDLJoyState, with the exception of the legacy player_index
// constructor, which should eventually go away.
class wxJoystick {
public:
    static wxJoystick Invalid();

    // TODO: Remove this constructor once the transition to the new UserInput
    // type is complete.
    static wxJoystick FromLegacyPlayerIndex(unsigned player_index);

    virtual ~wxJoystick() = default;

    wxString ToString();

    // TODO: Remove this API once the transition to the new UserInput type is
    // complete.
    unsigned player_index() { return sdl_index_ + 1; }

    bool operator==(const wxJoystick& other) const;
    bool operator!=(const wxJoystick& other) const;
    bool operator<(const wxJoystick& other) const;
    bool operator<=(const wxJoystick& other) const;
    bool operator>(const wxJoystick& other) const;
    bool operator>=(const wxJoystick& other) const;

private:
    static const int kInvalidSdlIndex = -1;
    friend class wxSDLJoyState;

    wxJoystick() = delete;
    wxJoystick(int sdl_index);

    int sdl_index_;
};

// Represents a Joystick event.
class wxJoyEvent : public wxCommandEvent {
public:
    wxJoyEvent(
        wxJoystick joystick,
        wxJoyControl control,
        uint8_t control_index,
        bool pressed);
    virtual ~wxJoyEvent() = default;

    wxJoystick joystick() const { return joystick_; }
    wxJoyControl control() const { return control_; }
    uint8_t control_index() const { return control_index_; }
    bool pressed() const { return pressed_; }

private:
    wxJoystick joystick_;
    wxJoyControl control_;
    uint8_t control_index_;
    bool pressed_;
};

// This is my own SDL-based joystick handler, since wxJoystick is brain-dead.
// It's geared towards keyboard emulation.
//
// After initilization, use PollJoystick() or PollAllJoysticks() for the
// joysticks you wish to monitor. The target window will then receive EVT_SDLJOY
// events of type wxJoyEvent.
// Handling of the wxJoystick value is different depending on the polling mode.
// After calls to PollJoysticks(), that value will remain constant for a given
// device, even if other joysticks disconnect. This ensures the joystick remains
// active during gameplay even if other joysticks disconnect.
// However, after calls to PollAllJoysticks(), all joysticks are re-connected
// on joystick connect/disconnect. This ensures the right wxJoystick value is
// sent to the UI during input event configuration.
class wxJoyPoller {
public:
    wxJoyPoller();
    ~wxJoyPoller();

    // Adds a set of joysticks to the list of polled joysticks.
    // This will disconnect every active joysticks, and reactivates the ones
    // matching an index in |joysticks|. Missing joysticks will be connected if
    // they connect later on.
    void PollJoysticks(std::set<wxJoystick> joysticks);

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
    std::set<wxJoystick> requested_joysticks_;

    // Set to true when we are actively polling controllers.
    bool is_polling_active_ = false;

    // Timestamp when the latest poll was done.
    wxLongLong last_poll_ = wxGetUTCTimeMillis();
};

// Note: this means sdljoy can't be part of a library w/o extra work
DECLARE_LOCAL_EVENT_TYPE(wxEVT_JOY, -1)
typedef void (wxEvtHandler::*wxJoyEventFunction)(wxJoyEvent&);
#define EVT_SDLJOY(fn)                                                   \
    DECLARE_EVENT_TABLE_ENTRY(wxEVT_JOY,                              \
        wxID_ANY,                                                        \
        wxID_ANY,                                                        \
        (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction) \
            wxStaticCastEvent(wxJoyEventFunction, &fn),               \
        (wxObject*)NULL)                                                 \
    ,

#endif /* _WX_SDLJOY_H */

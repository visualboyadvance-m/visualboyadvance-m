#ifndef JOYEVT_H
#define JOYEVT_H

// This is my own SDL-based joystick handler, since wxJoystick is brain-dead.
// It's geared towards keyboard emulation

// To use, create a wxSDLJoy object Add() the joysticks you want to monitor.
//
//   The target window will receive EVT_SDLJOY events of type wxSDLJoyEvent.

#include <cstddef>
#include <array>
#include <vector>
#include <unordered_map>
#include <wx/time.h>
#include <wx/event.h>
#include <wx/timer.h>
#include <SDL_joystick.h>
#include <SDL_gamecontroller.h>
#include "../common/contains.h"

struct wxSDLJoyDev {
    private:
        union {
            SDL_GameController* dev_gc = nullptr;
            SDL_Joystick*       dev_js;
        };
    public:
        operator SDL_GameController*&();
        SDL_GameController*& operator=(SDL_GameController* ptr);

        operator SDL_Joystick*&();
        SDL_Joystick*& operator=(SDL_Joystick* ptr);

        operator bool();

        std::nullptr_t& operator=(std::nullptr_t&& null_ptr);
};

struct wxSDLJoyState {
    wxSDLJoyDev dev;
    std::unordered_map<uint8_t, int16_t> axis{};
    std::unordered_map<uint8_t, uint8_t> button{};
};

class wxSDLJoy : public wxTimer {
public:
    wxSDLJoy();
    // add another joystick to the list of polled sticks
    // -1 == add all
    // If joy > # of joysticks, it is ignored
    // This will start polling if a valid joystick is selected
    void Add(int8_t joy = -1);
    // remove a joystick from the polled sticks
    // -1 == remove all
    // If joy > # of joysticks, it is ignored
    // This will stop polling if all joysticks are disabled
    void Remove(int8_t joy = -1);
    // query if a stick is being polled
    bool IsPolling(uint8_t joy) { return contains(joystate, joy); }

    // true = currently rumbling, false = turn off rumbling
    void SetRumble(bool do_rumble);

    void Poll();

    virtual ~wxSDLJoy();

protected:
    // used to continue rumbling on a timer
    void Notify();
    void ConnectController(uint8_t joy);
    void DisconnectController(uint8_t joy);
    void CreateAndSendEvent(unsigned short joy, unsigned short ctrl_type, unsigned short ctrl_idx, short ctrl_val, short prev_val);

    const uint8_t POLL_TIME_MS = 10;

private:
    std::unordered_map<uint8_t, wxSDLJoyState> joystate;
    bool add_all = false, rumbling = false;

    wxLongLong last_poll = wxGetUTCTimeMillis();
};

enum {
    // The types of supported controls
    // values are signed-16 for axis, 0/1 for button
    // hat is bitmask NESW/URDL
    WXSDLJOY_AXIS,
    WXSDLJOY_HAT,
    WXSDLJOY_BUTTON
};

class wxSDLJoyEvent : public wxCommandEvent {
    friend class wxSDLJoy;

public:
    // Default constructor
    wxSDLJoyEvent(wxEventType commandType = wxEVT_NULL)
        : wxCommandEvent(commandType)
    {
    }
    // accessors
    unsigned short GetJoy()
    {
        return joy;
    }
    unsigned short GetControlType()
    {
        return ctrl_type;
    }
    unsigned short GetControlIndex()
    {
        return ctrl_idx;
    }
    short GetControlValue()
    {
        return ctrl_val;
    }
    short GetControlPrevValue()
    {
        return prev_val;
    }

protected:
    unsigned short joy;
    unsigned short ctrl_type;
    unsigned short ctrl_idx;
    short ctrl_val;
    short prev_val;
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

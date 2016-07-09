#ifndef JOYEVT_H
#define JOYEVT_H

// This is my own SDL-based joystick handler, since wxJoystick is brain-dead.
// It's geared towards keyboard emulation

// To use, create a wxSDLJoy object Add() the joysticks you want to monitor.
// wxSDLJoy derives from wxTimer, so you can pause it with Stop() and
// resume with Start().
//
//   The target window will receive EVT_SDLJOY events of type wxSDLJoyEvent.

#include <wx/event.h>
#include <wx/timer.h>

typedef struct wxSDLJoyState wxSDLJoyState_t;

class wxSDLJoy : public wxTimer {
public:
    // if analog, send events for all axis movement
    // otherwise, only send events when values cross the 50% mark
    // and max out values
    wxSDLJoy(bool analog = false);
    // but flag can be set later
    void SetAnalog(bool analog = true)
    {
        digital = !analog;
    };
    // send events to this handler
    // If NULL (default), send to window with keyboard focus
    wxEvtHandler* Attach(wxEvtHandler*);
    // add another joystick to the list of polled sticks
    // -1 == add all
    // If joy > # of joysticks, it is ignored
    // This will start polling if a valid joystick is selected
    void Add(int joy = -1);
    // remove a joystick from the polled sticks
    // -1 == remove all
    // If joy > # of joysticks, it is ignored
    // This will stop polling if all joysticks are disabled
    void Remove(int joy = -1);
    // query if a stick is being polled
    bool IsPolling(int joy);
    // query # of joysticks
    int GetNumJoysticks()
    {
        return njoy;
    }
    // query # of axes on given joystick
    // 0 is returned if joy is invalid
    int GetNumAxes(int joy);
    // query # of hats on given joystick
    // 0 is returned if joy is invalid
    int GetNumHats(int joy);
    // query # of buttons on given joystick
    // 0 is returned if joy is invalid
    int GetNumButtons(int joy);

    virtual ~wxSDLJoy();

protected:
    bool digital;
    int njoy;
    wxSDLJoyState_t* joystate;
    wxEvtHandler* evthandler;
    bool nosticks;
    void Notify();
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
    wxSDLJoyEvent(wxEventType commandType = wxEVT_NULL, int id = 0)
        : wxCommandEvent(commandType, id)
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
    // required for PostEvent, apparently
    wxEvent* Clone();

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

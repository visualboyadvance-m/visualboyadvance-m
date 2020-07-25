#include <cstddef>
#include "wxvbam.h"
#include "wx/sdljoy.h"
#include "SDL.h"
#include <SDL_events.h>
#include <wx/window.h>
#include "../common/range.hpp"
#include "../common/contains.h"

using namespace Range;

// For testing a GameController as a Joystick:
//#define SDL_IsGameController(x) false

DEFINE_EVENT_TYPE(wxEVT_SDLJOY)

wxSDLJoy::wxSDLJoy()
    : wxTimer()
    , evthandler(nullptr)
{
    // Start up joystick if not already started
    // FIXME: check for errors
    SDL_Init(SDL_INIT_JOYSTICK);
    SDL_Init(SDL_INIT_GAMECONTROLLER);
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
}

wxSDLJoy::~wxSDLJoy()
{
    // SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static int16_t axisval(int16_t x)
{
    if (x > 0x1fff)
        return 1;
    else if (x < -0x1fff)
        return -1;

    return 0;
}

void wxSDLJoy::CreateAndSendEvent(wxEvtHandler* handler, unsigned short joy, unsigned short ctrl_type, unsigned short ctrl_idx, short ctrl_val, short prev_val)
{
    if (!handler) {
        GameArea *panel = wxGetApp().frame->GetPanel();
        if (panel) handler = panel->GetEventHandler();
        else return;
    }

    wxSDLJoyEvent *ev = new wxSDLJoyEvent(wxEVT_SDLJOY);
    ev->joy           = joy;
    ev->ctrl_type     = ctrl_type;
    ev->ctrl_idx      = ctrl_idx;
    ev->ctrl_val      = ctrl_val;
    ev->prev_val      = prev_val;

    wxQueueEvent(handler, ev);
}

void wxSDLJoy::Poll()
{
    wxEvtHandler* handler = evthandler ? evthandler : wxWindow::FindFocus();
    SDL_Event e;

    bool got_event = false;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            {
                auto joy = e.cbutton.which;

                if (!SDL_IsGameController(joy))
                    break;

                if (contains(joystate, joy)) {
                    auto but      = e.cbutton.button;
                    auto val      = e.cbutton.state;
                    auto prev_val = joystate[joy].button[but];

                    if (val != prev_val) {
                        CreateAndSendEvent(handler, joy, WXSDLJOY_BUTTON, but, val, prev_val);

                        joystate[joy].button[but] = val;

                        wxLogDebug("GOT SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", joy, but, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERAXISMOTION:
            {
                auto joy = e.caxis.which;

                if (!SDL_IsGameController(joy))
                    break;

                if (contains(joystate, joy)) {
                    auto axis     = e.caxis.axis;
                    auto val      = axisval(e.caxis.value);
                    auto prev_val = joystate[joy].axis[axis];

                    if (val != prev_val) {
                        CreateAndSendEvent(handler, joy, WXSDLJOY_AXIS, axis, val, prev_val);

                        joystate[joy].axis[axis] = val;

                        wxLogDebug("GOT SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy, axis, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMAPPED:
            {
                auto joy = e.cdevice.which;

                if (!SDL_IsGameController(joy))
                    break;

                if (add_all || contains(joystate, joy)) {
                    DisconnectController(joy);
                    ConnectController(joy);

                    systemScreenMessage(wxString::Format(_("Connected game controller %d"), joy + 1));
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED:
            {
                auto joy = e.cdevice.which;

                if (contains(joystate, joy)) {
                    DisconnectController(joy);

                    systemScreenMessage(wxString::Format(_("Disconnected game controller %d"), joy + 1));
                }

                got_event = true;

                break;
            }

            // Joystck events for non-GameControllers.

            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            {
                auto joy = e.jbutton.which;

                if (SDL_IsGameController(joy))
                    break;

                if (contains(joystate, joy)) {
                    auto but      = e.jbutton.button;
                    auto val      = e.jbutton.state;
                    auto prev_val = joystate[joy].button[but];

                    if (val != prev_val) {
                        CreateAndSendEvent(handler, joy, WXSDLJOY_BUTTON, but, val, prev_val);

                        joystate[joy].button[but] = val;

                        wxLogDebug("GOT SDL_JOYBUTTON: joy:%d but:%d val:%d prev_val:%d", joy, but, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_JOYAXISMOTION:
            {
                auto joy = e.jaxis.which;

                if (SDL_IsGameController(joy))
                    break;

                if (contains(joystate, joy)) {
                    auto axis     = e.jaxis.axis;
                    auto val      = axisval(e.jaxis.value);
                    auto prev_val = joystate[joy].axis[axis];

                    if (val != prev_val) {
                        CreateAndSendEvent(handler, joy, WXSDLJOY_AXIS, axis, val, prev_val);

                        joystate[joy].axis[axis] = val;

                        wxLogDebug("GOT SDL_JOYAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy, axis, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_JOYDEVICEADDED:
            {
                auto joy = e.cdevice.which;

                if (SDL_IsGameController(joy))
                    break;

                if (add_all || contains(joystate, joy)) {
                    DisconnectController(joy);
                    ConnectController(joy);

                    systemScreenMessage(wxString::Format(_("Connected joystick %d"), joy + 1));
                }

                got_event = true;

                break;
            }
            case SDL_JOYDEVICEREMOVED:
            {
                auto joy = e.cdevice.which;

                if (SDL_IsGameController(joy))
                    break;

                if (contains(joystate, joy)) {
                    DisconnectController(joy);

                    systemScreenMessage(wxString::Format(_("Disconnected joystick %d"), joy + 1));
                }

                got_event = true;

                break;
            }
        }
    }

    bool do_poll  = false;
    wxLongLong tm = wxGetUTCTimeMillis();

    if (got_event)
        last_poll = tm;
    else if (tm - last_poll > POLL_TIME_MS) {
        do_poll   = true;
        last_poll = tm;
    }

    if (do_poll) {
        for (auto&& joy : joystate) {
            if (!joy.second.dev) continue;

            if (SDL_IsGameController(joy.first)) {
                for (uint8_t but = 0; but < SDL_CONTROLLER_BUTTON_MAX; but++) {
                    auto last_state = joy.second.button[but];
                    auto state      = SDL_GameControllerGetButton(joy.second.dev, static_cast<SDL_GameControllerButton>(but));

                    if (last_state != state) {
                        CreateAndSendEvent(handler, joy.first, WXSDLJOY_BUTTON, but, state, last_state);

                        joy.second.button[but] = state;

                        wxLogDebug("POLLED SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", joy.first, but, state, last_state);
                    }
                }

                for (uint8_t axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
                    auto val      = axisval(SDL_GameControllerGetAxis(joy.second.dev, static_cast<SDL_GameControllerAxis>(axis)));
                    auto prev_val = joy.second.axis[axis];

                    if (val != prev_val) {
                        CreateAndSendEvent(handler, joy.first, WXSDLJOY_AXIS, axis, val, prev_val);

                        joy.second.axis[axis] = val;

                        wxLogDebug("POLLED SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy.first, axis, val, prev_val);
                    }
                }
            }
            else {
                for (uint8_t but = 0; but < SDL_JoystickNumButtons(joy.second.dev); but++) {
                    auto last_state = joy.second.button[but];
                    auto state      = SDL_JoystickGetButton(joy.second.dev, but);

                    if (last_state != state) {
                        CreateAndSendEvent(handler, joy.first, WXSDLJOY_BUTTON, but, state, last_state);

                        joy.second.button[but] = state;

                        wxLogDebug("POLLED SDL_JOYBUTTON: joy:%d but:%d val:%d prev_val:%d", joy.first, but, state, last_state);
                    }
                }

                for (uint8_t axis = 0; axis < SDL_JoystickNumAxes(joy.second.dev); axis++) {
                    auto val      = axisval(SDL_JoystickGetAxis(joy.second.dev, axis));
                    auto prev_val = joy.second.axis[axis];

                    if (val != prev_val) {
                        CreateAndSendEvent(handler, joy.first, WXSDLJOY_AXIS, axis, val, prev_val);

                        joy.second.axis[axis] = val;

                        wxLogDebug("POLLED SDL_JOYAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy.first, axis, val, prev_val);
                    }
                }
            }
        }
    }
}

void wxSDLJoy::ConnectController(uint8_t joy)
{
    if (SDL_IsGameController(joy)) {
        if (!(joystate[joy].dev = SDL_GameControllerOpen(joy))) {
            wxLogDebug("SDL_GameControllerOpen(%d) failed: %s", joy, SDL_GetError());
            return;
        }
    }
    else {
        if (!(joystate[joy].dev = SDL_JoystickOpen(joy))) {
            wxLogDebug("SDL_JoystickOpen(%d) failed: %s", joy, SDL_GetError());
            return;
        }
    }
}

void wxSDLJoy::DisconnectController(uint8_t joy)
{
    if (auto& dev = joystate[joy].dev) {
        if (SDL_IsGameController(joy)) {
            if (SDL_GameControllerGetAttached(dev))
                SDL_GameControllerClose(dev);
        }
        else {
            if (SDL_JoystickGetAttached(dev))
                SDL_JoystickClose(dev);
        }

        dev = nullptr;
    }
}

wxEvtHandler* wxSDLJoy::Attach(wxEvtHandler* handler)
{
    wxEvtHandler* prev = evthandler;
    evthandler = handler;
    return prev;
}

void wxSDLJoy::Add(int8_t joy_n)
{
    if (joy_n < 0) {
        for (uint8_t joy : range(0, SDL_NumJoysticks()))
            ConnectController(joy);

        add_all = true;

        return;
    }

    ConnectController(joy_n);
}

void wxSDLJoy::Remove(int8_t joy_n)
{
    add_all = false;

    if (joy_n < 0) {
        for (auto&& joy : joystate)
            DisconnectController(joy.first);

        joystate.clear();

        return;
    }

    DisconnectController(joy_n);
    joystate.erase(joy_n);
}

void wxSDLJoy::SetRumble(bool do_rumble)
{
    rumbling = do_rumble;

#if SDL_VERSION_ATLEAST(2, 0, 9)
    // Do rumble only on device 0, and only if it's a GameController.
    auto dev = joystate[0].dev;
    if (dev && SDL_IsGameController(0)) {
        if (rumbling) {
            SDL_GameControllerRumble(dev, 0xFFFF, 0xFFFF, 300);
            if (!IsRunning())
                Start(150);
        }
        else {
            SDL_GameControllerRumble(dev, 0, 0, 0);
            Stop();
        }
    }
#endif
}

void wxSDLJoy::Notify()
{
    SetRumble(rumbling);
}

wxSDLJoyDev::operator SDL_GameController*&()
{
    return dev_gc;
}

SDL_GameController*& wxSDLJoyDev::operator=(SDL_GameController* ptr)
{
    dev_gc = ptr;
    return dev_gc;
}


wxSDLJoyDev::operator SDL_Joystick*&()
{
    return dev_js;
}

SDL_Joystick*& wxSDLJoyDev::operator=(SDL_Joystick* ptr)
{
    dev_js = ptr;
    return dev_js;
}

wxSDLJoyDev::operator bool()
{
    return dev_gc != nullptr;
}

std::nullptr_t& wxSDLJoyDev::operator=(std::nullptr_t&& null_ptr)
{
    dev_gc = null_ptr;
    return null_ptr;
}

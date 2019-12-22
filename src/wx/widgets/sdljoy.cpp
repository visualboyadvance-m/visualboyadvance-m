#include "wxvbam.h"
#include "wx/sdljoy.h"
#include "SDL.h"
#include <SDL_events.h>
#include <wx/window.h>
#include "../common/range.hpp"
#include "../common/contains.h"

using namespace Range;

DEFINE_EVENT_TYPE(wxEVT_SDLJOY)

wxSDLJoy::wxSDLJoy()
    : wxTimer()
    , evthandler(nullptr)
{
    // Start up joystick if not already started
    // FIXME: check for errors
    SDL_Init(SDL_INIT_GAMECONTROLLER);
    SDL_GameControllerEventState(SDL_ENABLE);
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

                if (contains(joystate, joy)) {
                    auto but      = e.cbutton.button;
                    auto val      = e.cbutton.state;
                    auto prev_val = joystate[joy].button[but];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.joy           = joy;
                        ev.ctrl_type     = WXSDLJOY_BUTTON;
                        ev.ctrl_idx      = but;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);
                    }

                    joystate[joy].button[but] = val;

                    wxLogDebug("GOT SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", joy, but, val, prev_val);
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERAXISMOTION:
            {
                auto joy = e.caxis.which;

                if (contains(joystate, joy)) {
                    auto axis     = e.caxis.axis;
                    auto val      = axisval(e.caxis.value);
                    auto prev_val = joystate[joy].axis[axis];

                    if (handler && val != prev_val) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.joy           = joy;
                        ev.ctrl_type     = WXSDLJOY_AXIS;
                        ev.ctrl_idx      = axis;
                        ev.ctrl_val      = val;
                        ev.prev_val      = prev_val;

                        handler->ProcessEvent(ev);

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
        }
    }

    // I am only doing this shit because hotplug support is fucking broken in SDL right now.

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
            for (uint8_t but = 0; but < SDL_CONTROLLER_BUTTON_MAX; but++) {
                auto last_state = joy.second.button[but];
                auto state      = SDL_GameControllerGetButton(joy.second.dev, static_cast<SDL_GameControllerButton>(but));

                if (last_state != state) {
                    if (handler) {
                        wxSDLJoyEvent ev(wxEVT_SDLJOY);
                        ev.joy           = joy.first;
                        ev.ctrl_type     = WXSDLJOY_BUTTON;
                        ev.ctrl_idx      = but;
                        ev.ctrl_val      = state;
                        ev.prev_val      = last_state;

                        handler->ProcessEvent(ev);
                    }

                    joy.second.button[but] = state;

                    wxLogDebug("POLLED SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", joy.first, but, state, last_state);
                }
            }

            for (uint8_t axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
                auto val      = axisval(SDL_GameControllerGetAxis(joy.second.dev, static_cast<SDL_GameControllerAxis>(axis)));
                auto prev_val = joy.second.axis[axis];

                if (handler && val != prev_val) {
                    wxSDLJoyEvent ev(wxEVT_SDLJOY);
                    ev.joy           = joy.first;
                    ev.ctrl_type     = WXSDLJOY_AXIS;
                    ev.ctrl_idx      = axis;
                    ev.ctrl_val      = val;
                    ev.prev_val      = prev_val;

                    handler->ProcessEvent(ev);

                    joy.second.axis[axis] = val;

                    wxLogDebug("POLLED SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy.first, axis, val, prev_val);
                }
            }
        }
    }
}

void wxSDLJoy::ConnectController(uint8_t joy)
{
    if (!(joystate[joy].dev = SDL_GameControllerOpen(joy)))
        wxLogDebug("SDL_GameControllerOpen(%d) failed: %s", joy, SDL_GetError());
}

void wxSDLJoy::DisconnectController(uint8_t joy)
{
    if (auto& dev = joystate[joy].dev) {
        if (SDL_GameControllerGetAttached(dev))
            SDL_GameControllerClose(dev);

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
        for (auto joy : joystate)
            DisconnectController(std::get<0>(joy));

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
    // do rumble only on device 0
    auto dev = joystate[0].dev;
    if (dev) {
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
    else
        Stop();
#endif
}

void wxSDLJoy::Notify()
{
    SetRumble(rumbling);
}

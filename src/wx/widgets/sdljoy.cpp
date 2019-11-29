#include "wxvbam.h"
#include "wx/sdljoy.h"
#include "SDL.h"
#include <SDL_events.h>
#include <SDL_gamecontroller.h>
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

static int axisval(int x)
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

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            {
                auto joy = e.cbutton.which;

                if (contains(joystate, joy)) {
                    auto but      = e.cbutton.button;
                    auto val      = e.cbutton.state;
                    auto prev_val = !val;

                    if (handler) {
                        wxSDLJoyEvent* ev = new wxSDLJoyEvent(wxEVT_SDLJOY);
                        ev->joy           = joy;
                        ev->ctrl_type     = WXSDLJOY_BUTTON;
                        ev->ctrl_idx      = but;
                        ev->ctrl_val      = val;
                        ev->prev_val      = prev_val;

                        handler->QueueEvent(ev);
                    }

                    wxLogDebug("GOT SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", joy, but, val, prev_val);
                }
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
                        wxSDLJoyEvent* ev = new wxSDLJoyEvent(wxEVT_SDLJOY);
                        ev->joy           = joy;
                        ev->ctrl_type     = WXSDLJOY_AXIS;
                        ev->ctrl_idx      = axis;
                        ev->ctrl_val      = val;
                        ev->prev_val      = prev_val;

                        handler->QueueEvent(ev);

                        joystate[joy].axis[axis] = val;

                        wxLogDebug("GOT SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", joy, axis, val, prev_val);
                    }
                }
                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMAPPED:
            {
                auto joy = e.cdevice.which;

                if (add_all || contains(joystate, joy)) {
                    joystate[joy].dev = SDL_GameControllerOpen(joy);

                    systemScreenMessage(wxString::Format(_("Connected game controller %d"), joy + 1));
                }
                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED:
            {
                auto joy = e.cdevice.which;

                if (contains(joystate, joy)) {
                    joystate[joy].dev = nullptr;

                    systemScreenMessage(wxString::Format(_("Disconnected game controller %d"), joy + 1));
                }
                break;
            }
        }
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
            joystate[joy].dev = SDL_GameControllerOpen(joy);

        add_all = true;

        return;
    }

    joystate[joy_n].dev = SDL_GameControllerOpen(joy_n);
}

void wxSDLJoy::Remove(int8_t joy_n)
{
    add_all = false;

    if (joy_n < 0) {
        for (auto joy : joystate) {
            if (auto dev = std::get<1>(joy).dev)
                SDL_GameControllerClose(dev);
        }

        joystate.clear();

        return;
    }

    if (auto dev = joystate[joy_n].dev)
        SDL_GameControllerClose(dev);

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

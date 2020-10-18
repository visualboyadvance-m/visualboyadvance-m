#include <cstddef>
#include <unordered_set>
#include "wxvbam.h"
#include "wx/sdljoy.h"
#include "SDL.h"
#include <SDL_events.h>
#include "../common/range.hpp"
#include "../common/contains.h"

// Use these as a joystick and not as a GameController.
static const std::unordered_set<std::string> JOYSTICK_OVERRIDES{
    "Nintendo Switch Pro Controller",
    "Mayflash GameCube Controller Adapter"
};

using namespace Range;

// For testing a GameController as a Joystick:
//#define SDL_IsGameController(x) false

DEFINE_EVENT_TYPE(wxEVT_SDLJOY)

wxSDLJoy::wxSDLJoy()
    : wxTimer()
{
    // Start up joystick if not already started
    // FIXME: check for errors
    SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER);
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

void wxSDLJoy::CreateAndSendEvent(unsigned short joy, unsigned short ctrl_type, unsigned short ctrl_idx, short ctrl_val, short prev_val)
{
    auto handler = wxGetApp().frame->GetJoyEventHandler();

    if (!handler)
        return;

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
    SDL_Event e;

    bool got_event = false;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
            {
                auto joy = e.cbutton.which;

                if (contains(instance_map, joy)) {
                    auto& state   = *(instance_map[joy]);

                    if (!state.is_gc)
                        break;

                    auto but      = e.cbutton.button;
                    auto val      = e.cbutton.state;
                    auto prev_val = state.button[but];

                    if (val != prev_val) {
                        CreateAndSendEvent(state.index, WXSDLJOY_BUTTON, but, val, prev_val);

                        state.button[but] = val;

                        wxLogDebug("GOT SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", state.index, but, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERAXISMOTION:
            {
                auto joy = e.caxis.which;

                if (contains(instance_map, joy)) {
                    auto& state   = *(instance_map[joy]);

                    if (!state.is_gc)
                        break;

                    auto axis     = e.caxis.axis;
                    auto val      = axisval(e.caxis.value);
                    auto prev_val = state.axis[axis];

                    if (val != prev_val) {
                        CreateAndSendEvent(state.index, WXSDLJOY_AXIS, axis, val, prev_val);

                        state.axis[axis] = val;

                        wxLogDebug("GOT SDL_CONTROLLERAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", state.index, axis, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
            {
                auto joy = e.cdevice.which;

                if (add_all || contains(joystate, joy))
                    RemapControllers();

                got_event = true;

                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED:
            {
                auto joy = e.cdevice.which;

                if (contains(instance_map, joy))
                    RemapControllers();

                got_event = true;

                break;
            }

            // Joystck events for non-GameControllers.

            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
            {
                auto joy = e.jbutton.which;

                if (contains(instance_map, joy)) {
                    auto& state   = *(instance_map[joy]);

                    if (state.is_gc)
                        break;

                    auto but      = e.jbutton.button;
                    auto val      = e.jbutton.state;
                    auto prev_val = state.button[but];

                    if (val != prev_val) {
                        CreateAndSendEvent(state.index, WXSDLJOY_BUTTON, but, val, prev_val);

                        state.button[but] = val;

                        wxLogDebug("GOT SDL_JOYBUTTON: joy:%d but:%d val:%d prev_val:%d", state.index, but, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_JOYAXISMOTION:
            {
                auto joy = e.jaxis.which;

                if (contains(instance_map, joy)) {
                    auto& state   = *(instance_map[joy]);

                    if (state.is_gc)
                        break;

                    auto axis     = e.jaxis.axis;
                    auto val      = axisval(e.jaxis.value);
                    auto prev_val = state.axis[axis];

                    if (val != prev_val) {
                        CreateAndSendEvent(state.index, WXSDLJOY_AXIS, axis, val, prev_val);

                        state.axis[axis] = val;

                        wxLogDebug("GOT SDL_JOYAXISMOTION: joy:%d axis:%d val:%d prev_val:%d", state.index, axis, val, prev_val);
                    }
                }

                got_event = true;

                break;
            }
            case SDL_JOYDEVICEADDED:
            {
                auto joy = e.jdevice.which;

                if (SDL_IsGameController(joy))
                    break;

                if (add_all || contains(joystate, joy))
                    RemapControllers();

                got_event = true;

                break;
            }
            case SDL_JOYDEVICEREMOVED:
            {
                auto joy = e.jdevice.which;

                if (contains(instance_map, joy))
                    RemapControllers();

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

            if (joy.second.is_gc) {
                for (uint8_t but = 0; but < SDL_CONTROLLER_BUTTON_MAX; but++) {
                    auto last_state = joy.second.button[but];
                    auto state      = SDL_GameControllerGetButton(joy.second.dev, static_cast<SDL_GameControllerButton>(but));

                    if (last_state != state) {
                        CreateAndSendEvent(joy.first, WXSDLJOY_BUTTON, but, state, last_state);

                        joy.second.button[but] = state;

                        wxLogDebug("POLLED SDL_CONTROLLERBUTTON: joy:%d but:%d val:%d prev_val:%d", joy.first, but, state, last_state);
                    }
                }

                for (uint8_t axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
                    auto val      = axisval(SDL_GameControllerGetAxis(joy.second.dev, static_cast<SDL_GameControllerAxis>(axis)));
                    auto prev_val = joy.second.axis[axis];

                    if (val != prev_val) {
                        CreateAndSendEvent(joy.first, WXSDLJOY_AXIS, axis, val, prev_val);

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
                        CreateAndSendEvent(joy.first, WXSDLJOY_BUTTON, but, state, last_state);

                        joy.second.button[but] = state;

                        wxLogDebug("POLLED SDL_JOYBUTTON: joy:%d but:%d val:%d prev_val:%d", joy.first, but, state, last_state);
                    }
                }

                for (uint8_t axis = 0; axis < SDL_JoystickNumAxes(joy.second.dev); axis++) {
                    auto val      = axisval(SDL_JoystickGetAxis(joy.second.dev, axis));
                    auto prev_val = joy.second.axis[axis];

                    if (val != prev_val) {
                        CreateAndSendEvent(joy.first, WXSDLJOY_AXIS, axis, val, prev_val);

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
    SDL_Joystick* js_dev = nullptr;
    bool is_gc;
    std::string name;

    if ((is_gc = SDL_IsGameController(joy))) {
        auto dev = SDL_GameControllerOpen(joy);

        if (dev) {
            name   = SDL_GameControllerName(dev);
            js_dev = SDL_GameControllerGetJoystick(dev);

            std::string extra_msg;

            if (contains(JOYSTICK_OVERRIDES, name)) {
                is_gc             = false;
                joystate[joy].dev = js_dev;
                extra_msg         = "as joystick";
            }
            else {
                joystate[joy].dev = dev;
            }

            systemScreenMessage(wxString::Format(_("Connected game controller") + " %d: '%s' %s", joy, name, extra_msg));
        }
    }
    else {
        if ((js_dev = SDL_JoystickOpen(joy))) {
            joystate[joy].dev = js_dev;

            name = SDL_JoystickName(js_dev);

            systemScreenMessage(wxString::Format(_("Connected joystick") + " %d: '%s'", joy, name));
        }
    }

    if (js_dev) {
        auto instance = SDL_JoystickInstanceID(js_dev);

        instance_map[instance] = &(joystate[joy]);

        joystate[joy].instance = instance;
    }

    joystate[joy].index = joy;
    joystate[joy].is_gc = is_gc;
    joystate[joy].name  = name;
}

void wxSDLJoy::RemapControllers()
{
    for (auto&& joy : joystate) {
        auto& state = joy.second;

        DisconnectController(state);
        ConnectController(joy.first);
    }
}

void wxSDLJoy::DisconnectController(wxSDLJoyState& state)
{
    if (auto& dev = state.dev) {
        if (state.is_gc) {
            SDL_GameControllerClose(dev);
            systemScreenMessage(wxString::Format(_("Disconnected game controller") + " %d", state.index));
        }
        else {
            SDL_JoystickClose(dev);
            systemScreenMessage(wxString::Format(_("Disconnected joystick") + " %d", state.index));
        }

        dev = nullptr;
    }

    instance_map.erase(state.instance);
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
            DisconnectController(joy.second);

        joystate.clear();

        return;
    }

    DisconnectController(joystate[joy_n]);
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

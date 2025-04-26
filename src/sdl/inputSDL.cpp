// VBA-M, A Nintendo Handheld Console Emulator
// Copyright (C) 2015 VBA-M development team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "sdl/inputSDL.h"

#include "sdl/ConfigManager.h"

#include <cstdlib>

#define SDLBUTTONS_NUM 14

static void sdlUpdateKey(uint32_t key, bool down);
static void sdlUpdateJoyButton(int which, int button, bool pressed);
static void sdlUpdateJoyHat(int which, int hat, int value);
static void sdlUpdateJoyAxis(int which, int axis, int value);
static bool sdlCheckJoyKey(int key);

static bool sdlButtons[4][SDLBUTTONS_NUM] = {
    { false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false },
    { false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false },
    { false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false },
    { false, false, false, false, false, false,
        false, false, false, false, false, false,
        false, false }
};

static bool sdlMotionButtons[4] = { false, false, false, false };

static int sdlNumDevices = 0;
static SDL_Joystick** sdlDevices = NULL;

static EPad sdlDefaultJoypad = PAD_MAIN;

static int autoFire = 0;
static int autoFireToggle = 0;
static int autoFireCountdown = 0;
static int sensorX = 0;
static int sensorY = 0;

static uint32_t joypad[5][SDLBUTTONS_NUM] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    {
        SDLK_LEFT, SDLK_RIGHT,
        SDLK_UP, SDLK_DOWN,
#ifdef ENABLE_SDL3
        SDLK_Z, SDLK_X,
#else
        SDLK_z, SDLK_x,
#endif
        SDLK_RETURN, SDLK_BACKSPACE,
#ifdef ENABLE_SDL3
        SDLK_A, SDLK_S,
#else
        SDLK_a, SDLK_s,
#endif
        SDLK_SPACE, SDLK_F12,
#ifdef ENABLE_SDL3
        SDLK_Q, SDLK_W,
#else
        SDLK_q, SDLK_w,
#endif
    }
};

static uint32_t motion[4] = {
    SDLK_KP_4, SDLK_KP_6, SDLK_KP_8, SDLK_KP_2
};

static uint32_t defaultMotion[4] = {
    SDLK_KP_4, SDLK_KP_6, SDLK_KP_8, SDLK_KP_2
};

static uint32_t sdlGetHatCode(const SDL_Event& event)
{
    if (!event.jhat.value)
        return 0;

    return (
        ((event.jhat.which + 1) << 16) | 32 | (event.jhat.hat << 2) | (event.jhat.value & SDL_HAT_UP ? 0 : event.jhat.value & SDL_HAT_DOWN ? 1 : event.jhat.value & SDL_HAT_RIGHT ? 2 : event.jhat.value & SDL_HAT_LEFT ? 3 : 0));
}

static uint32_t sdlGetButtonCode(const SDL_Event& event)
{
    return (
        ((event.jbutton.which + 1) << 16) | (event.jbutton.button + 0x80));
}

static uint32_t sdlGetAxisCode(const SDL_Event& event)
{
    if (event.jaxis.value >= -16384 && event.jaxis.value <= 16384)
        return 0;

    return (
        ((event.jaxis.which + 1) << 16) | (event.jaxis.axis << 1) | (event.jaxis.value > 16384 ? 1 : event.jaxis.value < -16384 ? 0 : 0));
}

uint32_t inputGetEventCode(const SDL_Event& event)
{
    switch (event.type) {
#ifdef ENABLE_SDL3
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        return event.key.key;
#else
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        return event.key.keysym.sym;
#endif
        break;
#ifdef ENABLE_SDL3
    case SDL_EVENT_JOYSTICK_HAT_MOTION:
#else
    case SDL_JOYHATMOTION:
#endif
        return sdlGetHatCode(event);
        break;
#ifdef ENABLE_SDL3
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
#else
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
#endif
        return sdlGetButtonCode(event);
        break;
#ifdef ENABLE_SDL3
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
#else
    case SDL_JOYAXISMOTION:
#endif
        return sdlGetAxisCode(event);
        break;
    default:
        return 0;
        break;
    }
}

uint32_t inputGetKeymap(EPad pad, EKey key)
{
    return joypad[pad][key];
}

void inputSetKeymap(EPad pad, EKey key, uint32_t code)
{
    joypad[pad][key] = code;
}

void inputSetMotionKeymap(EKey key, uint32_t code)
{
    motion[key] = code;
}

bool inputGetAutoFire(EKey key)
{
    int mask = 0;

    switch (key) {
    case KEY_BUTTON_A:
        mask = 1 << 0;
        break;
    case KEY_BUTTON_B:
        mask = 1 << 1;
        break;
    case KEY_BUTTON_R:
        mask = 1 << 8;
        break;
    case KEY_BUTTON_L:
        mask = 1 << 9;
        break;
    default:
        break;
    }

    return !(autoFire & mask);
}

bool inputToggleAutoFire(EKey key)
{
    int mask = 0;

    switch (key) {
    case KEY_BUTTON_A:
        mask = 1 << 0;
        break;
    case KEY_BUTTON_B:
        mask = 1 << 1;
        break;
    case KEY_BUTTON_R:
        mask = 1 << 8;
        break;
    case KEY_BUTTON_L:
        mask = 1 << 9;
        break;
    default:
        break;
    }

    if (autoFire & mask) {
        autoFire &= ~mask;
        return false;
    } else {
        autoFire |= mask;
        return true;
    }
}

static void sdlUpdateKey(uint32_t key, bool down)
{
    int i;
    for (int j = 0; j < 4; j++) {
        for (i = 0; i < SDLBUTTONS_NUM; i++) {
            //if((joypad[j][i] & 0xffff0000) == 0) {
            if (key == joypad[j][i])
                sdlButtons[j][i] = down;
            //}
        }
    }
    for (i = 0; i < 4; i++) {
        //if((motion[i] & 0xffff0000) == 0) {
        if (key == motion[i])
            sdlMotionButtons[i] = down;
        //}
    }
}

static void sdlUpdateJoyButton(int which,
    int button,
    bool pressed)
{
    int i;
    for (int j = 0; j < 4; j++) {
        for (i = 0; i < SDLBUTTONS_NUM; i++) {
            int dev = (joypad[j][i] >> 16);
            int b = joypad[j][i] & 0xffff;
            if (dev) {
                dev--;

                if ((dev == which) && (b >= 128) && (b == (button + 128))) {
                    sdlButtons[j][i] = pressed;
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        int dev = (motion[i] >> 16);
        int b = motion[i] & 0xffff;
        if (dev) {
            dev--;

            if ((dev == which) && (b >= 128) && (b == (button + 128))) {
                sdlMotionButtons[i] = pressed;
            }
        }
    }
}

static void sdlUpdateJoyHat(int which,
    int hat,
    int value)
{
    int i;
    for (int j = 0; j < 4; j++) {
        for (i = 0; i < SDLBUTTONS_NUM; i++) {
            int dev = (joypad[j][i] >> 16);
            int a = joypad[j][i] & 0xffff;
            if (dev) {
                dev--;

                if ((dev == which) && (a >= 32) && (a < 48) && (((a & 15) >> 2) == hat)) {
                    int dir = a & 3;
                    int v = 0;
                    switch (dir) {
                    case 0:
                        v = value & SDL_HAT_UP;
                        break;
                    case 1:
                        v = value & SDL_HAT_DOWN;
                        break;
                    case 2:
                        v = value & SDL_HAT_RIGHT;
                        break;
                    case 3:
                        v = value & SDL_HAT_LEFT;
                        break;
                    }
                    sdlButtons[j][i] = (v ? true : false);
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        int dev = (motion[i] >> 16);
        int a = motion[i] & 0xffff;
        if (dev) {
            dev--;

            if ((dev == which) && (a >= 32) && (a < 48) && (((a & 15) >> 2) == hat)) {
                int dir = a & 3;
                int v = 0;
                switch (dir) {
                case 0:
                    v = value & SDL_HAT_UP;
                    break;
                case 1:
                    v = value & SDL_HAT_DOWN;
                    break;
                case 2:
                    v = value & SDL_HAT_RIGHT;
                    break;
                case 3:
                    v = value & SDL_HAT_LEFT;
                    break;
                }
                sdlMotionButtons[i] = (v ? true : false);
            }
        }
    }
}

static void sdlUpdateJoyAxis(int which,
    int axis,
    int value)
{
    int i;
    for (int j = 0; j < 4; j++) {
        for (i = 0; i < SDLBUTTONS_NUM; i++) {
            int dev = (joypad[j][i] >> 16);
            int a = joypad[j][i] & 0xffff;
            if (dev) {
                dev--;

                if ((dev == which) && (a < 32) && ((a >> 1) == axis)) {
                    sdlButtons[j][i] = (a & 1) ? (value > 16384) : (value < -16384);
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        int dev = (motion[i] >> 16);
        int a = motion[i] & 0xffff;
        if (dev) {
            dev--;

            if ((dev == which) && (a < 32) && ((a >> 1) == axis)) {
                sdlMotionButtons[i] = (a & 1) ? (value > 16384) : (value < -16384);
            }
        }
    }
}

static bool sdlCheckJoyKey(int key)
{
    int dev = (key >> 16) - 1;
    int what = key & 0xffff;

    if (what >= 128) {
        // joystick button
        int button = what - 128;

#ifdef ENABLE_SDL3
        if (button >= SDL_GetNumJoystickButtons(sdlDevices[dev]))
#else
        if (button >= SDL_JoystickNumButtons(sdlDevices[dev]))
#endif
            return false;
    } else if (what < 0x20) {
        // joystick axis
        what >>= 1;
#ifdef ENABLE_SDL3
        if (what >= SDL_GetNumJoystickAxes(sdlDevices[dev]))
#else
        if (what >= SDL_JoystickNumAxes(sdlDevices[dev]))
#endif
            return false;
    } else if (what < 0x30) {
        // joystick hat
        what = (what & 15);
        what >>= 2;
#ifdef ENABLE_SDL3
        if (what >= SDL_GetNumJoystickHats(sdlDevices[dev]))
#else
        if (what >= SDL_JoystickNumHats(sdlDevices[dev]))
#endif
            return false;
    }

    // no problem found
    return true;
}

void inputInitJoysticks()
{
#ifdef ENABLE_SDL3
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&sdlNumDevices);
#else
    sdlNumDevices = SDL_NumJoysticks();
#endif
    bool usesJoy = false;

    // The main joypad has to be entirely defined
    for (int i = 0; i < SDLBUTTONS_NUM; i++) {
        if (!joypad[PAD_MAIN][i])
            joypad[PAD_MAIN][i] = joypad[PAD_DEFAULT][i];
    }

    if (sdlNumDevices)
    {
        sdlDevices = (SDL_Joystick**)calloc(1, sdlNumDevices * sizeof(SDL_Joystick**));
    }

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < SDLBUTTONS_NUM; i++) {
            int dev = joypad[j][i] >> 16;
            if (dev) {
                dev--;
                bool ok = false;

                if (sdlDevices) {
                    if (dev < sdlNumDevices) {
                        if (sdlDevices[dev] == NULL) {
#ifndef ENABLE_SDL3
                            sdlDevices[dev] = SDL_JoystickOpen(dev);
#else
                            sdlDevices[dev] = SDL_OpenJoystick(joysticks[dev]);
#endif
                        }

                        ok = sdlCheckJoyKey(joypad[j][i]);
                    } else {
                        ok = false;
                    }
                }

                if (!ok)
                    joypad[j][i] = joypad[PAD_DEFAULT][i];
                else
                    usesJoy = true;
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        int dev = motion[i] >> 16;
        if (dev) {
            dev--;
            bool ok = false;

            if (sdlDevices) {
                if (dev < sdlNumDevices) {
                    if (sdlDevices[dev] == NULL) {
#ifndef ENABLE_SDL3
                        sdlDevices[dev] = SDL_JoystickOpen(dev);
#else
                        sdlDevices[dev] = SDL_OpenJoystick(joysticks[dev]);
#endif
                    }

                    ok = sdlCheckJoyKey(motion[i]);
                } else
                    ok = false;
            }

            if (!ok)
                motion[i] = defaultMotion[i];
            else
                usesJoy = true;
        }
    }

    if (usesJoy)
    {
#ifdef ENABLE_SDL3
        SDL_SetJoystickEventsEnabled(true);
#else
        SDL_JoystickEventState(SDL_ENABLE);
#endif
    }
}

void inputProcessSDLEvent(const SDL_Event& event)
{
    //    fprintf(stdout, "%x\n", inputGetEventCode(event));

    switch (event.type) {
#ifdef ENABLE_SDL3
    case SDL_EVENT_KEY_DOWN:
        if (!event.key.mod)
            sdlUpdateKey(event.key.key, true);
#else
    case SDL_KEYDOWN:
        if (!event.key.keysym.mod)
            sdlUpdateKey(event.key.keysym.sym, true);
#endif
        break;
#ifdef ENABLE_SDL3
    case SDL_EVENT_KEY_UP:
        if (!event.key.mod)
            sdlUpdateKey(event.key.key, false);
#else
    case SDL_KEYUP:
        if (!event.key.keysym.mod)
            sdlUpdateKey(event.key.keysym.sym, false);
#endif
        break;
#ifdef ENABLE_SDL3
    case SDL_EVENT_JOYSTICK_HAT_MOTION:
#else
    case SDL_JOYHATMOTION:
#endif
        sdlUpdateJoyHat(event.jhat.which,
            event.jhat.hat,
            event.jhat.value);
        break;
#ifdef ENABLE_SDL3
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
        sdlUpdateJoyButton(event.jbutton.which,
            event.jbutton.button,
            event.jbutton.down == true);
#else
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
            sdlUpdateJoyButton(event.jbutton.which,
                event.jbutton.button,
                event.jbutton.state == SDL_PRESSED);
#endif
        break;
#ifdef ENABLE_SDL3
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
#else
    case SDL_JOYAXISMOTION:
#endif
        sdlUpdateJoyAxis(event.jaxis.which,
            event.jaxis.axis,
            event.jaxis.value);
        break;
    }
}

uint32_t inputReadJoypad(int which)
{
    int realAutoFire = autoFire;

    if (which < 0 || which > 3)
        which = sdlDefaultJoypad;

    uint32_t res = 0;

    if (sdlButtons[which][KEY_BUTTON_A])
        res |= 1;
    if (sdlButtons[which][KEY_BUTTON_B])
        res |= 2;
    if (sdlButtons[which][KEY_BUTTON_SELECT])
        res |= 4;
    if (sdlButtons[which][KEY_BUTTON_START])
        res |= 8;
    if (sdlButtons[which][KEY_RIGHT])
        res |= 16;
    if (sdlButtons[which][KEY_LEFT])
        res |= 32;
    if (sdlButtons[which][KEY_UP])
        res |= 64;
    if (sdlButtons[which][KEY_DOWN])
        res |= 128;
    if (sdlButtons[which][KEY_BUTTON_R])
        res |= 256;
    if (sdlButtons[which][KEY_BUTTON_L])
        res |= 512;
    if (sdlButtons[which][KEY_BUTTON_AUTO_A])
        realAutoFire ^= 1;
    if (sdlButtons[which][KEY_BUTTON_AUTO_B])
        realAutoFire ^= 2;

    // disallow L+R or U+D of being pressed at the same time
    if ((res & 48) == 48)
        res &= ~16;
    if ((res & 192) == 192)
        res &= ~128;

    if (sdlButtons[which][KEY_BUTTON_SPEED])
        res |= 1024;
    if (sdlButtons[which][KEY_BUTTON_CAPTURE])
        res |= 2048;

    if (realAutoFire) {
        res &= (~realAutoFire);
        if (autoFireToggle)
            res |= realAutoFire;
        autoFireCountdown--; // this needs decrementing even when autoFireToggle is toggled,
        // so that autoFireMaxCount==1 (the default) will alternate at the maximum possible
        // frequency (every time this code is reached). Which is what it did before
        // introducing autoFireCountdown.
        if (autoFireCountdown <= 0) {
            autoFireToggle = !autoFireToggle;
            autoFireCountdown = autoFireMaxCount;
        }
    }

    return res;
}

void inputUpdateMotionSensor()
{
    if (sdlMotionButtons[KEY_LEFT]) {
        sensorX += 3;
        if (sensorX > 2197)
            sensorX = 2197;
        if (sensorX < 2047)
            sensorX = 2057;
    } else if (sdlMotionButtons[KEY_RIGHT]) {
        sensorX -= 3;
        if (sensorX < 1897)
            sensorX = 1897;
        if (sensorX > 2047)
            sensorX = 2037;
    } else if (sensorX > 2047) {
        sensorX -= 2;
        if (sensorX < 2047)
            sensorX = 2047;
    } else {
        sensorX += 2;
        if (sensorX > 2047)
            sensorX = 2047;
    }

    if (sdlMotionButtons[KEY_UP]) {
        sensorY += 3;
        if (sensorY > 2197)
            sensorY = 2197;
        if (sensorY < 2047)
            sensorY = 2057;
    } else if (sdlMotionButtons[KEY_DOWN]) {
        sensorY -= 3;
        if (sensorY < 1897)
            sensorY = 1897;
        if (sensorY > 2047)
            sensorY = 2037;
    } else if (sensorY > 2047) {
        sensorY -= 2;
        if (sensorY < 2047)
            sensorY = 2047;
    } else {
        sensorY += 2;
        if (sensorY > 2047)
            sensorY = 2047;
    }
}

int inputGetSensorX()
{
    return sensorX;
}

int inputGetSensorY()
{
    return sensorY;
}

void inputSetDefaultJoypad(EPad pad)
{
    sdlDefaultJoypad = pad;
}

EPad inputGetDefaultJoypad()
{
    return sdlDefaultJoypad;
}

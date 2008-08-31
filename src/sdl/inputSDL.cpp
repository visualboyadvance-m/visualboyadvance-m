// VBA-M, A Nintendo Handheld Console Emulator
// Copyright (C) 2008 VBA-M development team
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
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "inputSDL.h"

#define SDLBUTTONS_NUM 14

static void sdlUpdateKey(int key, bool down);
static void sdlUpdateJoyButton(int which, int button, bool pressed);
static void sdlUpdateJoyHat(int which, int hat, int value);
static void sdlUpdateJoyAxis(int which, int axis, int value);
static bool sdlCheckJoyKey(int key);

bool sdlButtons[4][SDLBUTTONS_NUM] = {
  { false, false, false, false, false, false,
    false, false, false, false, false, false,
    false, false
  },
  { false, false, false, false, false, false,
    false, false, false, false, false, false,
    false, false
  },
  { false, false, false, false, false, false,
    false, false, false, false, false, false,
    false, false
  },
  { false, false, false, false, false, false,
    false, false, false, false, false, false,
    false, false
  }
};

bool sdlMotionButtons[4] = { false, false, false, false };

int sdlNumDevices = 0;
SDL_Joystick **sdlDevices = NULL;

int sdlDefaultJoypad = 0;

int autoFire = 0;
bool autoFireToggle = false;

uint16_t joypad[4][SDLBUTTONS_NUM] = {
  { SDLK_LEFT,  SDLK_RIGHT,
    SDLK_UP,    SDLK_DOWN,
    SDLK_z,     SDLK_x,
    SDLK_RETURN,SDLK_BACKSPACE,
    SDLK_a,     SDLK_s,
    SDLK_SPACE, SDLK_F12,
    SDLK_q,     SDLK_w,
  },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

uint16_t defaultJoypad[SDLBUTTONS_NUM] = {
  SDLK_LEFT,  SDLK_RIGHT,
  SDLK_UP,    SDLK_DOWN,
  SDLK_z,     SDLK_x,
  SDLK_RETURN,SDLK_BACKSPACE,
  SDLK_a,     SDLK_s,
  SDLK_SPACE, SDLK_F12,
  SDLK_q,     SDLK_w
};

uint16_t motion[4] = {
  SDLK_KP4, SDLK_KP6, SDLK_KP8, SDLK_KP2
};

uint16_t defaultMotion[4] = {
  SDLK_KP4, SDLK_KP6, SDLK_KP8, SDLK_KP2
};

int sensorX = 2047;
int sensorY = 2047;

void inputSetKeymap(int joy, EKey key, uint16_t code)
{
    joypad[joy][key] = code;
}

void inputSetMotionKeymap(EKey key, uint16_t code)
{
    motion[key] = code;
}

bool inputToggleAutoFire(EKey key)
{
    int mask = 0;
    
    switch (key)
    {
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

    if(autoFire & mask)
    {
        autoFire &= ~mask;
        return false;
    }
    else
    {
        autoFire |= mask;
        return true;
    }
}

static void sdlUpdateKey(int key, bool down)
{
  int i;
  for(int j = 0; j < 4; j++) {
    for(i = 0 ; i < SDLBUTTONS_NUM; i++) {
      if((joypad[j][i] & 0xf000) == 0) {
        if(key == joypad[j][i])
          sdlButtons[j][i] = down;
      }
    }
  }
  for(i = 0 ; i < 4; i++) {
    if((motion[i] & 0xf000) == 0) {
      if(key == motion[i])
        sdlMotionButtons[i] = down;
    }
  }
}

static void sdlUpdateJoyButton(int which,
                               int button,
                               bool pressed)
{
  int i;
  for(int j = 0; j < 4; j++) {
    for(i = 0; i < SDLBUTTONS_NUM; i++) {
      int dev = (joypad[j][i] >> 12);
      int b = joypad[j][i] & 0xfff;
      if(dev) {
        dev--;

        if((dev == which) && (b >= 128) && (b == (button+128))) {
          sdlButtons[j][i] = pressed;
        }
      }
    }
  }
  for(i = 0; i < 4; i++) {
    int dev = (motion[i] >> 12);
    int b = motion[i] & 0xfff;
    if(dev) {
      dev--;

      if((dev == which) && (b >= 128) && (b == (button+128))) {
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
  for(int j = 0; j < 4; j++) {
    for(i = 0; i < SDLBUTTONS_NUM; i++) {
      int dev = (joypad[j][i] >> 12);
      int a = joypad[j][i] & 0xfff;
      if(dev) {
        dev--;

        if((dev == which) && (a>=32) && (a < 48) && (((a&15)>>2) == hat)) {
          int dir = a & 3;
          int v = 0;
          switch(dir) {
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
  for(i = 0; i < 4; i++) {
    int dev = (motion[i] >> 12);
    int a = motion[i] & 0xfff;
    if(dev) {
      dev--;

      if((dev == which) && (a>=32) && (a < 48) && (((a&15)>>2) == hat)) {
        int dir = a & 3;
        int v = 0;
        switch(dir) {
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
  for(int j = 0; j < 4; j++) {
    for(i = 0; i < SDLBUTTONS_NUM; i++) {
      int dev = (joypad[j][i] >> 12);
      int a = joypad[j][i] & 0xfff;
      if(dev) {
        dev--;

        if((dev == which) && (a < 32) && ((a>>1) == axis)) {
          sdlButtons[j][i] = (a & 1) ? (value > 16384) : (value < -16384);
        }
      }
    }
  }
  for(i = 0; i < 4; i++) {
    int dev = (motion[i] >> 12);
    int a = motion[i] & 0xfff;
    if(dev) {
      dev--;

      if((dev == which) && (a < 32) && ((a>>1) == axis)) {
        sdlMotionButtons[i] = (a & 1) ? (value > 16384) : (value < -16384);
      }
    }
  }
}

static bool sdlCheckJoyKey(int key)
{
  int dev = (key >> 12) - 1;
  int what = key & 0xfff;

  if(what >= 128) {
    // joystick button
    int button = what - 128;

    if(button >= SDL_JoystickNumButtons(sdlDevices[dev]))
      return false;
  } else if (what < 0x20) {
    // joystick axis
    what >>= 1;
    if(what >= SDL_JoystickNumAxes(sdlDevices[dev]))
      return false;
  } else if (what < 0x30) {
    // joystick hat
    what = (what & 15);
    what >>= 2;
    if(what >= SDL_JoystickNumHats(sdlDevices[dev]))
      return false;
  }

  // no problem found
  return true;
}

void inputInitJoysticks()
{
  sdlNumDevices = SDL_NumJoysticks();

  if(sdlNumDevices)
    sdlDevices = (SDL_Joystick **)calloc(1,sdlNumDevices *
                                         sizeof(SDL_Joystick **));
  int i;

  bool usesJoy = false;

  for(int j = 0; j < 4; j++) {
    for(i = 0; i < SDLBUTTONS_NUM; i++) {
      int dev = joypad[j][i] >> 12;
      if(dev) {
        dev--;
        bool ok = false;

        if(sdlDevices) {
          if(dev < sdlNumDevices) {
            if(sdlDevices[dev] == NULL) {
              sdlDevices[dev] = SDL_JoystickOpen(dev);
            }

            ok = sdlCheckJoyKey(joypad[j][i]);
          } else
            ok = false;
        }

        if(!ok)
          joypad[j][i] = defaultJoypad[i];
        else
          usesJoy = true;
      }
    }
  }

  for(i = 0; i < 4; i++) {
    int dev = motion[i] >> 12;
    if(dev) {
      dev--;
      bool ok = false;

      if(sdlDevices) {
        if(dev < sdlNumDevices) {
          if(sdlDevices[dev] == NULL) {
            sdlDevices[dev] = SDL_JoystickOpen(dev);
          }

          ok = sdlCheckJoyKey(motion[i]);
        } else
          ok = false;
      }

      if(!ok)
        motion[i] = defaultMotion[i];
      else
        usesJoy = true;
    }
  }

  if(usesJoy)
    SDL_JoystickEventState(SDL_ENABLE);
}

void inputProcessSDLEvent(const SDL_Event &event)
{
    switch(event.type)
    {
        case SDL_KEYDOWN:
            sdlUpdateKey(event.key.keysym.sym, true);
            break;
        case SDL_KEYUP:
            sdlUpdateKey(event.key.keysym.sym, false);
            break;
        case SDL_JOYHATMOTION:
            sdlUpdateJoyHat(event.jhat.which,
                            event.jhat.hat,
                            event.jhat.value);
            break;
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            sdlUpdateJoyButton(event.jbutton.which,
                               event.jbutton.button,
                               event.jbutton.state == SDL_PRESSED);
            break;
        case SDL_JOYAXISMOTION:
            sdlUpdateJoyAxis(event.jaxis.which,
                             event.jaxis.axis,
                             event.jaxis.value);
            break;
    }
}

uint32_t inputReadJoypad(int which)
{
  int realAutoFire  = autoFire;

  if(which < 0 || which > 3)
    which = sdlDefaultJoypad;

  uint32_t res = 0;

  if(sdlButtons[which][KEY_BUTTON_A])
    res |= 1;
  if(sdlButtons[which][KEY_BUTTON_B])
    res |= 2;
  if(sdlButtons[which][KEY_BUTTON_SELECT])
    res |= 4;
  if(sdlButtons[which][KEY_BUTTON_START])
    res |= 8;
  if(sdlButtons[which][KEY_RIGHT])
    res |= 16;
  if(sdlButtons[which][KEY_LEFT])
    res |= 32;
  if(sdlButtons[which][KEY_UP])
    res |= 64;
  if(sdlButtons[which][KEY_DOWN])
    res |= 128;
  if(sdlButtons[which][KEY_BUTTON_R])
    res |= 256;
  if(sdlButtons[which][KEY_BUTTON_L])
    res |= 512;
  if(sdlButtons[which][KEY_BUTTON_AUTO_A])
    realAutoFire ^= 1;
  if(sdlButtons[which][KEY_BUTTON_AUTO_B])
    realAutoFire ^= 2;

  // disallow L+R or U+D of being pressed at the same time
  if((res & 48) == 48)
    res &= ~16;
  if((res & 192) == 192)
    res &= ~128;

  if(sdlButtons[which][KEY_BUTTON_SPEED])
    res |= 1024;
  if(sdlButtons[which][KEY_BUTTON_CAPTURE])
    res |= 2048;

  if(realAutoFire) {
    res &= (~realAutoFire);
    if(autoFireToggle)
      res |= realAutoFire;
    autoFireToggle = !autoFireToggle;
  }

  return res;
}

void inputUpdateMotionSensor()
{
  if(sdlMotionButtons[KEY_LEFT]) {
    sensorX += 3;
    if(sensorX > 2197)
      sensorX = 2197;
    if(sensorX < 2047)
      sensorX = 2057;
  } else if(sdlMotionButtons[KEY_RIGHT]) {
    sensorX -= 3;
    if(sensorX < 1897)
      sensorX = 1897;
    if(sensorX > 2047)
      sensorX = 2037;
  } else if(sensorX > 2047) {
    sensorX -= 2;
    if(sensorX < 2047)
      sensorX = 2047;
  } else {
    sensorX += 2;
    if(sensorX > 2047)
      sensorX = 2047;
  }

  if(sdlMotionButtons[KEY_UP]) {
    sensorY += 3;
    if(sensorY > 2197)
      sensorY = 2197;
    if(sensorY < 2047)
      sensorY = 2057;
  } else if(sdlMotionButtons[KEY_DOWN]) {
    sensorY -= 3;
    if(sensorY < 1897)
      sensorY = 1897;
    if(sensorY > 2047)
      sensorY = 2037;
  } else if(sensorY > 2047) {
    sensorY -= 2;
    if(sensorY < 2047)
      sensorY = 2047;
  } else {
    sensorY += 2;
    if(sensorY > 2047)
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

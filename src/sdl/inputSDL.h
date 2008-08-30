#ifndef VBAM_INPUTSDL_FILTERS_H
#define VBAM_INPUTSDL_FILTERS_H

#include "../System.h"

enum EKey {
  KEY_LEFT, KEY_RIGHT,
  KEY_UP, KEY_DOWN,
  KEY_BUTTON_A, KEY_BUTTON_B,
  KEY_BUTTON_START, KEY_BUTTON_SELECT,
  KEY_BUTTON_L, KEY_BUTTON_R,
  KEY_BUTTON_SPEED, KEY_BUTTON_CAPTURE,
  KEY_BUTTON_AUTO_A, KEY_BUTTON_AUTO_B
};

void inputSetKeymap(int joy, EKey key, u16 code);
void inputSetMotionKeymap(EKey key, u16 code);
bool inputToggleAutoFire(EKey key);

void sdlUpdateKey(int key, bool down);
void sdlUpdateJoyButton(int which, int button, bool pressed);
void sdlUpdateJoyHat(int which, int hat, int value);
void sdlUpdateJoyAxis(int which, int axis, int value);
void sdlCheckKeys();

#endif // VBAM_INPUTSDL_FILTERS_H
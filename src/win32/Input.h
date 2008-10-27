#pragma once

#include "../System.h"

#define JOYCONFIG_MESSAGE (WM_USER + 1000)
#define JOYPADS 4
#define MOTION_KEYS 4
#define KEYS_PER_PAD 13
#define MOTION(i) ((JOYPADS*KEYS_PER_PAD)+i)
#define JOYPAD(i,j) ((i*KEYS_PER_PAD)+j)

#define DEVICEOF(key) (key >> 8)
#define KEYOF(key) (key & 255)

typedef CList<LONG_PTR,LONG_PTR> KeyList;

enum {
  KEY_LEFT, KEY_RIGHT,
  KEY_UP, KEY_DOWN,
  KEY_BUTTON_A, KEY_BUTTON_B,
  KEY_BUTTON_START, KEY_BUTTON_SELECT,
  KEY_BUTTON_L, KEY_BUTTON_R,
  KEY_BUTTON_SPEED, KEY_BUTTON_CAPTURE,
  KEY_BUTTON_GS
};

class Input {

 public:
  KeyList joypaddata[JOYPADS * KEYS_PER_PAD + MOTION_KEYS];

  Input() {};
  virtual ~Input() {};

  virtual bool initialize() = 0;

  virtual bool readDevices() = 0;
  virtual u32 readDevice(int which) = 0;
  virtual CString getKeyName(LONG_PTR key) = 0;
  virtual void checkKeys() = 0;
  virtual void checkMotionKeys() = 0;
  virtual void checkDevices() = 0;
  virtual void activate() = 0;
  virtual void loadSettings() = 0;
  virtual void saveSettings() = 0;
};

// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

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

#include "stdafx.h"
#include "Reg.h"
#include "WinResUtil.h"
#include "Input.h"

#define DIRECTINPUT_VERSION 0x0500
#include <dinput.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern void directXMessage(const char *);
extern void winlog(const char *msg,...);

#define POV_UP    1
#define POV_DOWN  2
#define POV_RIGHT 4
#define POV_LEFT  8

class DirectInput : public Input {
private:
  HINSTANCE dinputDLL;

public:
  virtual void checkDevices();
  DirectInput();
  virtual ~DirectInput();

  virtual bool initialize();
  virtual bool readDevices();
  virtual u32 readDevice(int which);
  virtual CString getKeyName(int key);
  virtual void checkKeys();
  virtual void checkMotionKeys();
  virtual void activate();
  virtual void loadSettings();
  virtual void saveSettings();
};

struct deviceInfo {
  LPDIRECTINPUTDEVICE device;
  BOOL isPolled;
  int nButtons;
  int nAxes;
  int nPovs;
  BOOL first;
  struct {
    DWORD offset;
    LONG center;
    LONG negative;
    LONG positive;
  } axis[8];
  int needed;
  union {
    UCHAR data[256];
    DIJOYSTATE state;
  };
};

static deviceInfo *currentDevice = NULL;
static int numDevices = 1;
static deviceInfo *pDevices = NULL;
static LPDIRECTINPUT pDirectInput = NULL;
static int joyDebug = 0;
static int axisNumber = 0;


KeyList joypad[JOYPADS * KEYS_PER_PAD + MOTION_KEYS]; 


USHORT defvalues[JOYPADS * KEYS_PER_PAD + MOTION_KEYS] =
  {
    DIK_LEFT,  DIK_RIGHT,
    DIK_UP,    DIK_DOWN,
    DIK_Z,     DIK_X,
    DIK_RETURN,DIK_BACK,
    DIK_A,     DIK_S,
    DIK_SPACE, DIK_F12,
    DIK_C,
	0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,
    DIK_NUMPAD4, DIK_NUMPAD6, DIK_NUMPAD8, DIK_NUMPAD2
};


void winReadKey(const char *name, KeyList& Keys)
{
  CString TxtKeyList = regQueryStringValue(name, "");
  int curPos= 0;

  CString resToken=TxtKeyList.Tokenize(",",curPos);
  while (resToken != "")
  {
	  Keys.AddTail(atoi(resToken));
	  resToken= TxtKeyList.Tokenize(",",curPos);
  };
}

void winReadKey(const char *name, int num, KeyList& Keys)
{
  char buffer[80];

  sprintf(buffer, "Joy%d_%s", num, name);
  winReadKey(name, Keys);
}


void winReadKeys()
{

  for(int i = 0; i < JOYPADS; i++) {
    winReadKey("Left", i, joypad[JOYPAD(i,KEY_LEFT)]);
    winReadKey("Right", i, joypad[JOYPAD(i, KEY_RIGHT)]);
    winReadKey("Up", i, joypad[JOYPAD(i,KEY_UP)]);
    winReadKey("Down", i, joypad[JOYPAD(i,KEY_DOWN)]);
    winReadKey("A", i, joypad[JOYPAD(i,KEY_BUTTON_A)]);
    winReadKey("B", i, joypad[JOYPAD(i,KEY_BUTTON_B)]);
    winReadKey("L", i, joypad[JOYPAD(i,KEY_BUTTON_L)]);
    winReadKey("R", i, joypad[JOYPAD(i,KEY_BUTTON_R)]);  
    winReadKey("Start", i, joypad[JOYPAD(i,KEY_BUTTON_START)]);
    winReadKey("Select", i, joypad[JOYPAD(i,KEY_BUTTON_SELECT)]);
    winReadKey("Speed", i, joypad[JOYPAD(i,KEY_BUTTON_SPEED)]);
    winReadKey("Capture", i, joypad[JOYPAD(i,KEY_BUTTON_CAPTURE)]);
    winReadKey("GS", i, joypad[JOYPAD(i,KEY_BUTTON_GS)]);
  }
  winReadKey("Motion_Left", joypad[MOTION(KEY_LEFT)]);
  winReadKey("Motion_Right", joypad[MOTION(KEY_RIGHT)]);
  winReadKey("Motion_Up", joypad[MOTION(KEY_UP)]);
  winReadKey("Motion_Down", joypad[MOTION(KEY_DOWN)]);
}

void winSaveKey(char *name, KeyList &value)
{
	CString txtKeys;
   
	POSITION p = value.GetHeadPosition();
	while(p!=NULL)
	{
		CString tmp;
		tmp.Format("%d", value.GetNext(p));
		txtKeys+=tmp;
		if (p!=NULL)
			txtKeys+=",";
	}
	regSetStringValue(name, txtKeys);
}

static void winSaveKey(char *name, int num, KeyList& value)
{
  char buffer[80];

  sprintf(buffer, "Joy%d_%s", num, name);
  winSaveKey(buffer, value);
}

void winSaveKeys()
{
  for(int i = 0; i < JOYPADS; i++) {
    winSaveKey("Left", i, joypad[JOYPAD(i,KEY_LEFT)]);
    winSaveKey("Right", i, joypad[JOYPAD(i,KEY_RIGHT)]);
    winSaveKey("Up", i, joypad[JOYPAD(i,KEY_UP)]);
    winSaveKey("Speed", i, joypad[JOYPAD(i,KEY_BUTTON_SPEED)]);
    winSaveKey("Capture", i, joypad[JOYPAD(i,KEY_BUTTON_CAPTURE)]);
    winSaveKey("GS", i, joypad[JOYPAD(i,KEY_BUTTON_GS)]);  
    winSaveKey("Down", i, joypad[JOYPAD(i,KEY_DOWN)]);
    winSaveKey("A", i, joypad[JOYPAD(i,KEY_BUTTON_A)]);
    winSaveKey("B", i, joypad[JOYPAD(i,KEY_BUTTON_B)]);
    winSaveKey("L", i, joypad[JOYPAD(i,KEY_BUTTON_L)]);
    winSaveKey("R", i, joypad[JOYPAD(i,KEY_BUTTON_R)]);  
    winSaveKey("Start", i, joypad[JOYPAD(i,KEY_BUTTON_START)]);
    winSaveKey("Select", i, joypad[JOYPAD(i,KEY_BUTTON_SELECT)]);
  }
  regSetDwordValue("joyVersion", 1);  

  winSaveKey("Motion_Left",
                   joypad[MOTION(KEY_LEFT)]);
  winSaveKey("Motion_Right",
                   joypad[MOTION(KEY_RIGHT)]);
  winSaveKey("Motion_Up",
                   joypad[MOTION(KEY_UP)]);
  winSaveKey("Motion_Down",
                   joypad[MOTION(KEY_DOWN)]);
}

static BOOL CALLBACK EnumAxesCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,
                                       VOID* pContext )
{
  DIPROPRANGE diprg; 
  diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
  diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
  diprg.diph.dwHow        = DIPH_BYOFFSET; 
  diprg.diph.dwObj        = pdidoi->dwOfs; // Specify the enumerated axis

  diprg.lMin = -32768;
  diprg.lMax = 32767;
  // try to set the range
  if(FAILED(currentDevice->device->SetProperty(DIPROP_RANGE, &diprg.diph))) {
    // Get the range for the axis
    if( FAILED(currentDevice->device->
               GetProperty( DIPROP_RANGE, &diprg.diph ) ) ) {
      return DIENUM_STOP;
    }
  }

  DIPROPDWORD didz;

  didz.diph.dwSize = sizeof(didz);
  didz.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  didz.diph.dwHow = DIPH_BYOFFSET;
  didz.diph.dwObj = pdidoi->dwOfs;

  didz.dwData = 5000;

  currentDevice->device->SetProperty(DIPROP_DEADZONE, &didz.diph);
  
  LONG center = (diprg.lMin + diprg.lMax)/2;
  LONG threshold = (diprg.lMax - center)/2;

  // only 8 axis supported
  if(axisNumber < 8) {
    currentDevice->axis[axisNumber].center = center;
    currentDevice->axis[axisNumber].negative = center - threshold;
    currentDevice->axis[axisNumber].positive = center + threshold;
    currentDevice->axis[axisNumber].offset = pdidoi->dwOfs;
  }
  axisNumber++;
  return DIENUM_CONTINUE;
}

static BOOL CALLBACK EnumPovsCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,
                                       VOID* pContext )
{
  return DIENUM_CONTINUE;  
}

static BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE pInst,
                                           LPVOID lpvContext)
{
  ZeroMemory(&pDevices[numDevices],sizeof(deviceInfo));
  
  HRESULT hRet = pDirectInput->CreateDevice(pInst->guidInstance,
                                            &pDevices[numDevices].device,
                                            NULL);
  
  if(hRet != DI_OK)
    return DIENUM_STOP;
  
  DIDEVCAPS caps;
  caps.dwSize=sizeof(DIDEVCAPS);
  
  hRet = pDevices[numDevices].device->GetCapabilities(&caps);
  
  if(hRet == DI_OK) {
    if(caps.dwFlags & DIDC_POLLEDDATAFORMAT ||
       caps.dwFlags & DIDC_POLLEDDEVICE)
      pDevices[numDevices].isPolled = TRUE;
    
    pDevices[numDevices].nButtons = caps.dwButtons;
    pDevices[numDevices].nAxes = caps.dwAxes;
    pDevices[numDevices].nPovs = caps.dwPOVs;

    for(int i = 0; i < 6; i++) {
      pDevices[numDevices].axis[i].center = 0x8000;
      pDevices[numDevices].axis[i].negative = 0x4000;
      pDevices[numDevices].axis[i].positive = 0xc000;
    }
  } else if(joyDebug)
    winlog("Failed to get device capabilities %08x\n", hRet);

  if(joyDebug) {
    // don't translate. debug only
    winlog("******************************\n");
    winlog("Joystick %2d name    : %s\n", numDevices, pInst->tszProductName);
  }
  
  numDevices++;

  
  return DIENUM_CONTINUE;
}

BOOL CALLBACK DIEnumDevicesCallback2(LPCDIDEVICEINSTANCE pInst,
                                     LPVOID lpvContext)
{
  numDevices++;
  
  return DIENUM_CONTINUE;
}

static int getPovState(DWORD value)
{
  int state = 0;
  if(LOWORD(value) != 0xFFFF) {
    if(value < 9000 || value > 27000)
      state |= POV_UP;
    if(value > 0 && value < 18000)
      state |= POV_RIGHT;
    if(value > 9000 && value < 27000)
      state |= POV_DOWN;
    if(value > 18000)
      state |= POV_LEFT;
  }
  return state;
}

static void checkKeys()
{
  int dev = 0;
  int i;

  for(i = 0; i < (sizeof(joypad) / sizeof(joypad[0])); i++) 
  {
	  if (joypad[i].IsEmpty() && defvalues[i]) 
		  joypad[i].AddTail(defvalues[i]);
	  POSITION p = joypad[i].GetHeadPosition();
	  while(p!=NULL)
	  {
		  USHORT k = joypad[i].GetNext(p);
		  if (k > 0 && DEVICEOF(k) < numDevices)
			  pDevices[DEVICEOF(k)].needed = true;
	  }
  }
}

#define KEYDOWN(buffer,key) (buffer[key] & 0x80)

static bool readKeyboard()
{
  if(pDevices[0].needed) {
    if (!theApp.dinputKeyFocus) {
      memset(pDevices[0].data, 0, 256);
      return true;
    }
    HRESULT hret = pDevices[0].device->
      GetDeviceState(256,
                     (LPVOID)pDevices[0].data);
    
    if(hret == DIERR_INPUTLOST || hret == DIERR_NOTACQUIRED) {
      hret = pDevices[0].device->Acquire();
      if(hret != DI_OK)
        return false;
      hret = pDevices[0].device->GetDeviceState(256,(LPVOID)pDevices[0].data);
    }
 
    return hret == DI_OK;
  }
  return true;
}

static bool readJoystick(int joy)
{
  if(pDevices[joy].needed) {
    if(pDevices[joy].isPolled)
      ((LPDIRECTINPUTDEVICE2)pDevices[joy].device)->Poll();
    
    HRESULT hret = pDevices[joy].device->
      GetDeviceState(sizeof(DIJOYSTATE),
                     (LPVOID)&pDevices[joy].state);
    
    if(hret == DIERR_INPUTLOST || hret == DIERR_NOTACQUIRED) {
      hret = pDevices[joy].device->Acquire();
      
      if(hret == DI_OK) {
        
        if(pDevices[joy].isPolled)
          ((LPDIRECTINPUTDEVICE2)pDevices[joy].device)->Poll();
        
        hret = pDevices[joy].device->
          GetDeviceState(sizeof(DIJOYSTATE),
                         (LPVOID)&pDevices[joy].state);
      }
    }

    return hret == DI_OK;
  }

  return true;
}

static void checkKeyboard()
{
  HRESULT hret = pDevices[0].device->Acquire();  
  hret = pDevices[0].device->
    GetDeviceState(256,
                   (LPVOID)pDevices[0].data);
  
  if(hret == DIERR_INPUTLOST || hret == DIERR_NOTACQUIRED) {
    return;
  }
 
  if(hret == DI_OK) {
    for(int i = 0; i < 256; i++) {
      if(KEYDOWN(pDevices[0].data, i)) {
        SendMessage(GetFocus(), JOYCONFIG_MESSAGE,0,i);
        break;
      }
    }
  }
}

static void checkJoypads()
{
  DIDEVICEOBJECTINSTANCE di;

  ZeroMemory(&di,sizeof(DIDEVICEOBJECTINSTANCE));

  di.dwSize = sizeof(DIDEVICEOBJECTINSTANCE);

  int i =0;

  DIJOYSTATE joystick;
  
  for(i = 1; i < numDevices; i++) {
    HRESULT hret = pDevices[i].device->Acquire();
    

    if(pDevices[i].isPolled)
      ((LPDIRECTINPUTDEVICE2)pDevices[i].device)->Poll();
    
    hret = pDevices[i].device->GetDeviceState(sizeof(joystick), &joystick);

    int j;

    if(pDevices[i].first) {
      memcpy(&pDevices[i].state, &joystick, sizeof(joystick));
      pDevices[i].first = FALSE;
      continue;
    }
    
    for(j = 0; j < pDevices[i].nButtons; j++) {
      if(((pDevices[i].state.rgbButtons[j] ^ joystick.rgbButtons[j]) 
          & joystick.rgbButtons[j]) & 0x80) {
        HWND focus = GetFocus();

        SendMessage(focus, JOYCONFIG_MESSAGE, i,j+128);
      }      
    }

    for(j = 0; j < pDevices[i].nAxes && j < 8; j++) {
      LONG value = pDevices[i].axis[j].center;
      LONG old = 0;
      switch(pDevices[i].axis[j].offset) {
      case DIJOFS_X:
        value = joystick.lX;
        old = pDevices[i].state.lX;
        break;
      case DIJOFS_Y:
        value = joystick.lY;
        old = pDevices[i].state.lY;     
        break;
      case DIJOFS_Z:
        value = joystick.lZ;
        old = pDevices[i].state.lZ;     
        break;
      case DIJOFS_RX:
        value = joystick.lRx;
        old = pDevices[i].state.lRx;    
        break;
      case DIJOFS_RY:
        value = joystick.lRy;
        old = pDevices[i].state.lRy;    
        break;
      case DIJOFS_RZ:
        value = joystick.lRz;
        old = pDevices[i].state.lRz;    
        break;
      case DIJOFS_SLIDER(0):
        value = joystick.rglSlider[0];
        old = pDevices[i].state.rglSlider[0];   
        break;
      case DIJOFS_SLIDER(1):
        value = joystick.rglSlider[1];
        old = pDevices[i].state.rglSlider[1];   
        break;
      }
      if(value != old) {
        if(value < pDevices[i].axis[j].negative)
          SendMessage(GetFocus(), JOYCONFIG_MESSAGE, i, (j<<1));
        else if (value > pDevices[i].axis[j].positive)
          SendMessage(GetFocus(), JOYCONFIG_MESSAGE, i, (j<<1)+1);
      }
    }

    for(j = 0;j < 4 && j < pDevices[i].nPovs; j++) {
      if(LOWORD(pDevices[i].state.rgdwPOV[j]) != LOWORD(joystick.rgdwPOV[j])) {
        int state = getPovState(joystick.rgdwPOV[j]);
        
        if(state & POV_UP)
          SendMessage(GetFocus(), JOYCONFIG_MESSAGE, i, (j<<2)+0x20);
        else if(state & POV_DOWN)
          SendMessage(GetFocus(), JOYCONFIG_MESSAGE, i, (j<<2)+0x21);
        else if(state & POV_RIGHT)
          SendMessage(GetFocus(), JOYCONFIG_MESSAGE, i, (j<<2)+0x22);
        else if(state & POV_LEFT)
          SendMessage(GetFocus(), JOYCONFIG_MESSAGE, i, (j<<2)+0x23);
      }
    }

    memcpy(&pDevices[i].state, &joystick, sizeof(joystick));
  }
}

BOOL checkKey(int key)
{
  int dev = DEVICEOF(key);
  int k =  KEYOF(key);
  
  if(dev == 0) {
    return KEYDOWN(pDevices[0].data,k);
  } else {
    if(k < 16) {
      int axis = k >> 1;
      LONG value = pDevices[dev].axis[axis].center;
      switch(pDevices[dev].axis[axis].offset) {
      case DIJOFS_X:
        value = pDevices[dev].state.lX;
        break;
      case DIJOFS_Y:
        value = pDevices[dev].state.lY;
        break;
      case DIJOFS_Z:
        value = pDevices[dev].state.lZ;
        break;
      case DIJOFS_RX:
        value = pDevices[dev].state.lRx;
        break;
      case DIJOFS_RY:
        value = pDevices[dev].state.lRy;
        break;
      case DIJOFS_RZ:
        value = pDevices[dev].state.lRz;
        break;
      case DIJOFS_SLIDER(0):
        value = pDevices[dev].state.rglSlider[0];
        break;
      case DIJOFS_SLIDER(1):
        value = pDevices[dev].state.rglSlider[1];
        break;
      }

      if(k & 1)
        return value > pDevices[dev].axis[axis].positive;
      return value < pDevices[dev].axis[axis].negative;
    } else if(k < 48) {
      int hat = (k >> 2) & 3;
      int state = getPovState(pDevices[dev].state.rgdwPOV[hat]);
      BOOL res = FALSE;
      switch(k & 3) {
      case 0:
        res = state & POV_UP;
        break;
      case 1:
        res = state & POV_DOWN;
        break;
      case 2:
        res = state & POV_RIGHT;
        break;
      case 3:
        res = state & POV_LEFT;
        break;
      }
      return res;
    } else if(k  >= 128) {
      return pDevices[dev].state.rgbButtons[k-128] & 0x80;
    }
  }

  return FALSE;
}

BOOL checkKey(KeyList &k)
{
	POSITION p = k.GetHeadPosition();
	while(p!=NULL)
	{
		if (checkKey(k.GetNext(p)))
			return TRUE;
	}
	return FALSE;
}

DirectInput::DirectInput()
{
  dinputDLL = NULL;
}

DirectInput::~DirectInput()
{
  saveSettings();
  if(pDirectInput != NULL) {
    if(pDevices) {
      for(int i = 0; i < numDevices ; i++) {
        if(pDevices[i].device) {
          pDevices[i].device->Unacquire();
          pDevices[i].device->Release();
          pDevices[i].device = NULL;
        }
      }
      free(pDevices);
      pDevices = NULL;
    }
    
    pDirectInput->Release();
    pDirectInput = NULL;
  }

  if(dinputDLL) {
    FreeLibrary(dinputDLL);
    dinputDLL = NULL;
  }
}

bool DirectInput::initialize()
{
  joyDebug = GetPrivateProfileInt("config",
                                  "joyDebug",
                                  0,
                                  "VBA.ini");
  dinputDLL = LoadLibrary("DINPUT.DLL");
  HRESULT (WINAPI *DInputCreate)(HINSTANCE,DWORD,LPDIRECTINPUT *,IUnknown *);  
  if(dinputDLL != NULL) {    
    DInputCreate = (HRESULT (WINAPI *)(HINSTANCE,DWORD,LPDIRECTINPUT *,IUnknown *))
      GetProcAddress(dinputDLL, "DirectInputCreateA");
    
    if(DInputCreate == NULL) {
      directXMessage("DirectInputCreateA");
      return false;
    }
  } else {
    directXMessage("DINPUT.DLL");
    return false;
  }
  
  HRESULT hret = DInputCreate(AfxGetInstanceHandle(),
                              DIRECTINPUT_VERSION,
                              &pDirectInput,
                              NULL);
  if(hret != DI_OK) {
    //    errorMessage(myLoadString(IDS_ERROR_DISP_CREATE), hret);
    return false;
  }

  hret = pDirectInput->EnumDevices(DIDEVTYPE_JOYSTICK,
                                   DIEnumDevicesCallback2,
                                   NULL,
                                   DIEDFL_ATTACHEDONLY);

  
  
  pDevices = (deviceInfo *)calloc(numDevices, sizeof(deviceInfo));

  hret = pDirectInput->CreateDevice(GUID_SysKeyboard,&pDevices[0].device,NULL);
  pDevices[0].isPolled = false;
  pDevices[0].needed  = true;

  if(hret != DI_OK) {
    //    errorMessage(myLoadString(IDS_ERROR_DISP_CREATEDEVICE), hret);
    return false;
  }

  
  numDevices = 1;

  hret = pDirectInput->EnumDevices(DIDEVTYPE_JOYSTICK,
                                   DIEnumDevicesCallback,
                                   NULL,
                                   DIEDFL_ATTACHEDONLY);  

  //  hret = pDevices[0].device->SetCooperativeLevel(hWindow,
  //                                             DISCL_FOREGROUND|
  //                                             DISCL_NONEXCLUSIVE);
  
  if(hret != DI_OK) {
    //    errorMessage(myLoadString(IDS_ERROR_DISP_LEVEL), hret);
    return false;
  }
  
  hret = pDevices[0].device->SetDataFormat(&c_dfDIKeyboard);

  if(hret != DI_OK) {
    //    errorMessage(myLoadString(IDS_ERROR_DISP_DATAFORMAT), hret);
    return false;
  }

  for(int i = 1; i < numDevices; i++) {
    pDevices[i].device->SetDataFormat(&c_dfDIJoystick);
    pDevices[i].needed = false;
    currentDevice = &pDevices[i];
    axisNumber = 0;
    currentDevice->device->EnumObjects(EnumAxesCallback, NULL, DIDFT_AXIS);
    currentDevice->device->EnumObjects(EnumPovsCallback, NULL, DIDFT_POV);
    if(joyDebug) {
      // don't translate. debug only
      winlog("Joystick %2d polled  : %d\n",    i, currentDevice->isPolled);
      winlog("Joystick %2d buttons : %d\n",    i, currentDevice->nButtons);
      winlog("Joystick %2d povs    : %d\n",    i, currentDevice->nPovs);
      winlog("Joystick %2d axes    : %d\n",    i, currentDevice->nAxes);
      for(int j = 0; j < currentDevice->nAxes; j++) {
        winlog("Axis %2d offset      : %08lx\n", j, currentDevice->axis[j].
               offset);
        winlog("Axis %2d center      : %08lx\n", j, currentDevice->axis[j].
               center);
        winlog("Axis %2d negative    : %08lx\n",   j, currentDevice->axis[j].
               negative);
        winlog("Axis %2d positive    : %08lx\n",   j, currentDevice->axis[j].
               positive);
      }
    }
    
    currentDevice = NULL;
  }

  if (1) for(int i = 0; i < numDevices; i++)
    pDevices[i].device->Acquire();
  
  return true;
}

bool DirectInput::readDevices()
{
  bool ok = true;
  for(int i = 0; i < numDevices; i++) {
    if(pDevices[i].needed) {
      if(i) {
        ok = readJoystick(i);
      } else
        ok = readKeyboard();
    }
  }
  return ok;
}

u32 DirectInput::readDevice(int which)
{
  u32 res = 0;
  int i = theApp.joypadDefault;
  if(which >= 0 && which <= 3)
    i = which;
  
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_A)]))
    res |= 1;
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_B)]))
    res |= 2;
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_SELECT)]))
    res |= 4;
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_START)]))
    res |= 8;
  if(checkKey(joypad[JOYPAD(i,KEY_RIGHT)]))
    res |= 16;
  if(checkKey(joypad[JOYPAD(i,KEY_LEFT)]))
    res |= 32;
  if(checkKey(joypad[JOYPAD(i,KEY_UP)]))
    res |= 64;
  if(checkKey(joypad[JOYPAD(i,KEY_DOWN)]))
    res |= 128;
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_R)]))
    res |= 256;
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_L)]))
    res |= 512;
  
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_GS)]))
    res |= 4096;

  if(theApp.autoFire) {
    res &= (~theApp.autoFire);
    if(theApp.autoFireToggle)
      res |= theApp.autoFire;
    theApp.autoFireToggle = !theApp.autoFireToggle;
  }

  // disallow L+R or U+D of being pressed at the same time
  if((res & 48) == 48)
    res &= ~16;
  if((res & 192) == 192)
    res &= ~128;

  if(theApp.movieRecording) {
    if(i == theApp.joypadDefault) {
      if(res != theApp.movieLastJoypad) {
        fwrite(&theApp.movieFrame, 1, sizeof(theApp.movieFrame), theApp.movieFile);
        fwrite(&res, 1, sizeof(res), theApp.movieFile);
        theApp.movieLastJoypad = res;
      }
    }
  }
  if(theApp.moviePlaying) {
    if(theApp.movieFrame == theApp.moviePlayFrame) {
      theApp.movieLastJoypad = theApp.movieNextJoypad;
      theApp.movieReadNext();
    }
    res = theApp.movieLastJoypad;
  }
  // we don't record speed up or screen capture buttons
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_SPEED)]) || theApp.speedupToggle)
    res |= 1024;
  if(checkKey(joypad[JOYPAD(i,KEY_BUTTON_CAPTURE)]))
    res |= 2048;

  return res;
}

CString DirectInput::getKeyName(int key)
{
  int d = (key >> 8);
  int k = key & 255;

  DIDEVICEOBJECTINSTANCE di;

  ZeroMemory(&di,sizeof(DIDEVICEOBJECTINSTANCE));

  di.dwSize = sizeof(DIDEVICEOBJECTINSTANCE);
  
  CString winBuffer = winResLoadString(IDS_ERROR);
  
  if(d == 0) {
    pDevices[0].device->GetObjectInfo(&di,key,DIPH_BYOFFSET);
    winBuffer = di.tszName;
  } else {
    if(k < 16) {
      if(k < 4) {
        switch(k) {
        case 0:
          winBuffer.Format(winResLoadString(IDS_JOY_LEFT), d);
          break;
        case 1:
          winBuffer.Format(winResLoadString(IDS_JOY_RIGHT), d);
          break;
        case 2:
          winBuffer.Format(winResLoadString(IDS_JOY_UP), d);
          break;
        case 3:
          winBuffer.Format(winResLoadString(IDS_JOY_DOWN), d);
          break;
        }
      } else {
        pDevices[d].device->GetObjectInfo(&di,
                                          pDevices[d].axis[k>>1].offset,
                                          DIPH_BYOFFSET);
        if(k & 1)
          winBuffer.Format("Joy %d %s +", d, di.tszName);
        else
          winBuffer.Format("Joy %d %s -", d, di.tszName);
      }
    } else if(k < 48) {
      int hat = (k >> 2) & 3;
      pDevices[d].device->GetObjectInfo(&di,
                                        DIJOFS_POV(hat),
                                        DIPH_BYOFFSET);
      char *dir = "up";
      int dd = k & 3;
      if(dd == 1)
        dir = "down";
      else if(dd == 2)
        dir = "right";
      else if(dd == 3)
        dir = "left";
      winBuffer.Format("Joy %d %s %s", d, di.tszName, dir);
    } else {
      pDevices[d].device->GetObjectInfo(&di,
                                        DIJOFS_BUTTON(k-128),
                                        DIPH_BYOFFSET);
      winBuffer.Format(winResLoadString(IDS_JOY_BUTTON),d,di.tszName);
    }
  }

  return winBuffer;
}

void DirectInput::checkKeys()
{
  ::checkKeys();
}

void DirectInput::checkMotionKeys()
{
  if(checkKey(joypad[MOTION(KEY_LEFT)])) {
    theApp.sensorX += 3;
    if(theApp.sensorX > 2197)
      theApp.sensorX = 2197;
    if(theApp.sensorX < 2047)
      theApp.sensorX = 2057;
  } else if(checkKey(joypad[MOTION(KEY_RIGHT)])) {
    theApp.sensorX -= 3;
    if(theApp.sensorX < 1897)
      theApp.sensorX = 1897;
    if(theApp.sensorX > 2047)
      theApp.sensorX = 2037;
  } else if(theApp.sensorX > 2047) {
    theApp.sensorX -= 2;
    if(theApp.sensorX < 2047)
      theApp.sensorX = 2047;
  } else {
    theApp.sensorX += 2;
    if(theApp.sensorX > 2047)
      theApp.sensorX = 2047;
  }
  
  if(checkKey(joypad[MOTION(KEY_UP)])) {
    theApp.sensorY += 3;
    if(theApp.sensorY > 2197)
      theApp.sensorY = 2197;
    if(theApp.sensorY < 2047)
      theApp.sensorY = 2057;
  } else if(checkKey(joypad[MOTION(KEY_DOWN)])) {
    theApp.sensorY -= 3;
    if(theApp.sensorY < 1897)
      theApp.sensorY = 1897;
    if(theApp.sensorY > 2047)
      theApp.sensorY = 2037;
  } else if(theApp.sensorY > 2047) {
    theApp.sensorY -= 2;
    if(theApp.sensorY < 2047)
      theApp.sensorY = 2047;
  } else {
    theApp.sensorY += 2;
    if(theApp.sensorY > 2047)
      theApp.sensorY = 2047;
  }  
}

Input *newDirectInput()
{
  return new DirectInput;
}


void DirectInput::checkDevices()
{
  checkJoypads();
  checkKeyboard();
}

void DirectInput::activate()
{
  for(int i = 0; i < numDevices; i++) {
    if(pDevices != NULL && pDevices[i].device != NULL)
      pDevices[i].device->Acquire();
  }
}

void DirectInput::loadSettings()
{
  winReadKeys();
}

void DirectInput::saveSettings()
{
  winSaveKeys();
}

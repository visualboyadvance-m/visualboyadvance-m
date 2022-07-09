#ifndef _WX_UTIL_H
#define _WX_UTIL_H

#include <wx/event.h>

int getKeyboardKeyCode(const wxKeyEvent& event);

#include <wx/accel.h>

class wxAcceleratorEntryUnicode : public wxAcceleratorEntry
{
  public:
    wxAcceleratorEntryUnicode(wxAcceleratorEntry *accel);
    wxAcceleratorEntryUnicode(int flags=0, int keyCode=0, int cmd=0, wxMenuItem *item=nullptr);
    wxAcceleratorEntryUnicode(wxString uKey, int joy=0, int flags=0, int keyCode=0, int cmd=0, wxMenuItem *item=nullptr);

    void Set(wxString uKey, int joy, int flags, int keyCode, int cmd, wxMenuItem *item=nullptr);

    int GetJoystick() const { return joystick; };
    wxString GetUkey() const { return ukey; };
  private:
    void init(int flags, int keyCode);
    wxString ukey;
    int joystick;
};

#include <unordered_map>
#include <wx/string.h>
#include "widgets/wx/keyedit.h"

class KeyboardInputMap
{
  public:
    static KeyboardInputMap* getInstance();
    static void AddMap(wxString keyStr, int key, int mod);
    static bool GetMap(wxString keyStr, int &key, int &mod);
  private:
    KeyboardInputMap();

    // We want to keep track of this pair for
    // almost all keypresses.
    typedef struct KeyMod {
        int key;
        int mod;
    } KeyMod;

    KeyMod newPair(int key, int mod)
    {
        KeyMod tmp;
        tmp.key = key;
        tmp.mod = mod;
        return tmp;
    }

    // Map accel string to pair
    std::unordered_map<std::wstring, KeyMod> keysMap;
};

#endif

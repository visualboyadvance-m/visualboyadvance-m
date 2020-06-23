#include "wxutil.h"
#include "../common/contains.h"


int getKeyboardKeyCode(wxKeyEvent& event)
{
    int uc = event.GetUnicodeKey();
    if (uc != WXK_NONE) {
        if (uc < 32) { // not all control chars
            switch (uc) {
                case WXK_BACK:
                case WXK_TAB:
                case WXK_RETURN:
                case WXK_ESCAPE:
                    return uc;
                default:
                    return WXK_NONE;
            }
        }
        return uc;
    }
    else {
        return event.GetKeyCode();
    }
}


wxAcceleratorEntryUnicode::wxAcceleratorEntryUnicode(wxAcceleratorEntry *accel)
  : wxAcceleratorEntry(accel->GetFlags(), accel->GetKeyCode(), accel->GetCommand(), accel->GetMenuItem())
{
    init(accel->GetFlags(), accel->GetKeyCode());
}


wxAcceleratorEntryUnicode::wxAcceleratorEntryUnicode(int flags, int keyCode, int cmd, wxMenuItem *item)
  : wxAcceleratorEntry(flags, keyCode, cmd, item)
{
    init(flags, keyCode);
}


wxAcceleratorEntryUnicode::wxAcceleratorEntryUnicode(wxString uKey, int joy, int flags, int keyCode, int cmd, wxMenuItem *item)
  : wxAcceleratorEntry(flags, keyCode, cmd, item)
{
    ukey = uKey;
    joystick = joy;
}


void wxAcceleratorEntryUnicode::Set(wxString uKey, int joy, int flags, int keyCode, int cmd, wxMenuItem *item)
{
    ukey = uKey;
    joystick = joy;
    wxAcceleratorEntry::Set(flags, keyCode, cmd, item);
}


void wxAcceleratorEntryUnicode::init(int flags, int keyCode)
{
    joystick = 0;
    if (!(flags == 0 && keyCode == 0)) {
        ukey.Printf("%d:%d", keyCode, flags);
    }
}


KeyboardInputMap* KeyboardInputMap::getInstance()
{
    static KeyboardInputMap instance;
    return &instance;
}


KeyboardInputMap::KeyboardInputMap(){}


void KeyboardInputMap::AddMap(wxString keyStr, int key, int mod)
{
    KeyboardInputMap* singleton = getInstance();
    singleton->keysMap[keyStr.ToStdWstring()] = singleton->newPair(key, mod);
}


bool KeyboardInputMap::GetMap(wxString keyStr, int &key, int &mod)
{
    KeyboardInputMap* singleton = getInstance();
    if (contains(singleton->keysMap, keyStr.ToStdWstring())) {
        key = singleton->keysMap.at(keyStr.ToStdWstring()).key;
        mod = singleton->keysMap.at(keyStr.ToStdWstring()).mod;
        return true;
    }
    return false;
}

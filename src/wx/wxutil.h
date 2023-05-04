#ifndef _WX_UTIL_H
#define _WX_UTIL_H

#include <vector>

#include <wx/accel.h>
#include <wx/event.h>

#include "config/user-input.h"

int getKeyboardKeyCode(const wxKeyEvent& event);

class wxAcceleratorEntryUnicode : public wxAcceleratorEntry
{
  public:
    wxAcceleratorEntryUnicode(wxAcceleratorEntry *accel);
    wxAcceleratorEntryUnicode(const config::UserInput& input,
                              int cmd = 0,
                              wxMenuItem* item = nullptr);
    wxAcceleratorEntryUnicode(int joy = 0,
                              int flags = 0,
                              int keyCode = 0,
                              int cmd = 0,
                              wxMenuItem* item = nullptr);

    void Set(int joy, int flags, int keyCode, int cmd, wxMenuItem* item = nullptr);

    int GetJoystick() const { return joystick; };

private:
    int joystick;
};

typedef std::vector<wxAcceleratorEntryUnicode> wxAcceleratorEntry_v;

#endif

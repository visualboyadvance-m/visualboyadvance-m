#include "wxutil.h"
#include "../common/contains.h"

int getKeyboardKeyCode(const wxKeyEvent& event) {
    int uc = event.GetUnicodeKey();
    if (uc != WXK_NONE) {
        if (uc < 32) {  // not all control chars
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
    } else {
        return event.GetKeyCode();
    }
}

wxAcceleratorEntryUnicode::wxAcceleratorEntryUnicode(wxAcceleratorEntry* accel)
    : wxAcceleratorEntryUnicode(0,
                                accel->GetFlags(),
                                accel->GetKeyCode(),
                                accel->GetCommand(),
                                accel->GetMenuItem()) {}

wxAcceleratorEntryUnicode::wxAcceleratorEntryUnicode(const config::UserInput& input,
                                                     int cmd,
                                                     wxMenuItem* item)
    : wxAcceleratorEntryUnicode(input.joy(), input.mod(), input.key(), cmd, item) {}

wxAcceleratorEntryUnicode::wxAcceleratorEntryUnicode(int joy,
                                                     int flags,
                                                     int keyCode,
                                                     int cmd,
                                                     wxMenuItem* item)
    : wxAcceleratorEntry(flags, keyCode, cmd, item), joystick(joy) {}

void wxAcceleratorEntryUnicode::Set(int joy, int flags, int keyCode, int cmd, wxMenuItem* item) {
    joystick = joy;
    wxAcceleratorEntry::Set(flags, keyCode, cmd, item);
}

#include "wxutil.h"

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

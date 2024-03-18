#include "background-input.h"

#if defined(__WXMSW__)

#include <windows.h>

#elif defined(__WXMAC__)
#else  // defined(__WXGTK__)

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include "wx/wayland.h"

#endif  // defined(__WXMSW__)

#include <unordered_map>

#define NO_ERROR  (wxThread::ExitCode)0
#define ANY_ERROR (wxThread::ExitCode)1

#if defined(__WXMSW__)

/* The following functions are copied from wxWidgets repo file
 * `src/msw/window.cpp` that convert a Windows Virtual Key into a
 * WX keycode.
 */
int ChooseNormalOrExtended(int lParam, int keyNormal, int keyExtended)
{
    // except that if lParam is 0, it means we don't have real lParam from
    // WM_KEYDOWN but are just translating just a VK constant (e.g. done from
    // msw/treectrl.cpp when processing TVN_KEYDOWN) -- then assume this is a
    // non-numpad (hence extended) key as this is a more common case
    return !lParam || (HIWORD(lParam) & KF_EXTENDED) ? keyExtended : keyNormal;
}

static const std::unordered_map<int, wxKeyCode> kSpecialKeys =
{
    { VK_CANCEL,        WXK_CANCEL },
    { VK_BACK,          WXK_BACK },
    { VK_TAB,           WXK_TAB },
    { VK_CLEAR,         WXK_CLEAR },
    { VK_SHIFT,         WXK_SHIFT },
    { VK_CONTROL,       WXK_CONTROL },
    { VK_MENU ,         WXK_ALT },
    { VK_PAUSE,         WXK_PAUSE },
    { VK_CAPITAL,       WXK_CAPITAL },
    { VK_SPACE,         WXK_SPACE },
    { VK_ESCAPE,        WXK_ESCAPE },
    { VK_SELECT,        WXK_SELECT },
    { VK_PRINT,         WXK_PRINT },
    { VK_EXECUTE,       WXK_EXECUTE },
    { VK_SNAPSHOT,      WXK_SNAPSHOT },
    { VK_HELP,          WXK_HELP },

    { VK_NUMPAD0,       WXK_NUMPAD0 },
    { VK_NUMPAD1,       WXK_NUMPAD1 },
    { VK_NUMPAD2,       WXK_NUMPAD2 },
    { VK_NUMPAD3,       WXK_NUMPAD3 },
    { VK_NUMPAD4,       WXK_NUMPAD4 },
    { VK_NUMPAD5,       WXK_NUMPAD5 },
    { VK_NUMPAD6,       WXK_NUMPAD6 },
    { VK_NUMPAD7,       WXK_NUMPAD7 },
    { VK_NUMPAD8,       WXK_NUMPAD8 },
    { VK_NUMPAD9,       WXK_NUMPAD9 },
    { VK_MULTIPLY,      WXK_NUMPAD_MULTIPLY },
    { VK_ADD,           WXK_NUMPAD_ADD },
    { VK_SUBTRACT,      WXK_NUMPAD_SUBTRACT },
    { VK_DECIMAL,       WXK_NUMPAD_DECIMAL },
    { VK_DIVIDE,        WXK_NUMPAD_DIVIDE },

    { VK_F1,            WXK_F1 },
    { VK_F2,            WXK_F2 },
    { VK_F3,            WXK_F3 },
    { VK_F4,            WXK_F4 },
    { VK_F5,            WXK_F5 },
    { VK_F6,            WXK_F6 },
    { VK_F7,            WXK_F7 },
    { VK_F8,            WXK_F8 },
    { VK_F9,            WXK_F9 },
    { VK_F10,           WXK_F10 },
    { VK_F11,           WXK_F11 },
    { VK_F12,           WXK_F12 },
    { VK_F13,           WXK_F13 },
    { VK_F14,           WXK_F14 },
    { VK_F15,           WXK_F15 },
    { VK_F16,           WXK_F16 },
    { VK_F17,           WXK_F17 },
    { VK_F18,           WXK_F18 },
    { VK_F19,           WXK_F19 },
    { VK_F20,           WXK_F20 },
    { VK_F21,           WXK_F21 },
    { VK_F22,           WXK_F22 },
    { VK_F23,           WXK_F23 },
    { VK_F24,           WXK_F24 },

    { VK_NUMLOCK,       WXK_NUMLOCK },
    { VK_SCROLL,        WXK_SCROLL },

    { VK_LWIN,          WXK_WINDOWS_LEFT },
    { VK_RWIN,          WXK_WINDOWS_RIGHT },
    { VK_APPS,          WXK_WINDOWS_MENU },

    //{ VK_BROWSER_BACK,        WXK_BROWSER_BACK },
    //{ VK_BROWSER_FORWARD,     WXK_BROWSER_FORWARD },
    //{ VK_BROWSER_REFRESH,     WXK_BROWSER_REFRESH },
    //{ VK_BROWSER_STOP,        WXK_BROWSER_STOP },
    //{ VK_BROWSER_SEARCH,      WXK_BROWSER_SEARCH },
    //{ VK_BROWSER_FAVORITES,   WXK_BROWSER_FAVORITES },
    //{ VK_BROWSER_HOME,        WXK_BROWSER_HOME },
    //{ VK_VOLUME_MUTE,         WXK_VOLUME_MUTE },
    //{ VK_VOLUME_DOWN,         WXK_VOLUME_DOWN },
    //{ VK_VOLUME_UP,           WXK_VOLUME_UP },
    //{ VK_MEDIA_NEXT_TRACK,    WXK_MEDIA_NEXT_TRACK },
    //{ VK_MEDIA_PREV_TRACK,    WXK_MEDIA_PREV_TRACK },
    //{ VK_MEDIA_STOP,          WXK_MEDIA_STOP },
    //{ VK_MEDIA_PLAY_PAUSE,    WXK_MEDIA_PLAY_PAUSE },
    //{ VK_LAUNCH_MAIL,         WXK_LAUNCH_MAIL },
    //{ VK_LAUNCH_APP1,         WXK_LAUNCH_APP1 },
    //{ VK_LAUNCH_APP2,         WXK_LAUNCH_APP2 },
};

int VKToWX(WXWORD vk, WXLPARAM lParam, wchar_t *uc)
{
    int wxk;

    // check the table first
    const auto iter = kSpecialKeys.find(vk);
    if (iter != kSpecialKeys.end()) {
        wxk = iter->second;
        if (wxk < WXK_START) {
             // Unicode code for this key is the same as its ASCII code.
             if (uc) *uc = wxk;
        }
        return wxk;
    }

    // keys requiring special handling
    switch ( vk )
    {
        case VK_OEM_1:
        case VK_OEM_PLUS:
        case VK_OEM_COMMA:
        case VK_OEM_MINUS:
        case VK_OEM_PERIOD:
        case VK_OEM_2:
        case VK_OEM_3:
        case VK_OEM_4:
        case VK_OEM_5:
        case VK_OEM_6:
        case VK_OEM_7:
        case VK_OEM_8:
        case VK_OEM_102:
            // MapVirtualKey() returns 0 if it fails to convert the virtual
            // key which nicely corresponds to our WXK_NONE.
            wxk = ::MapVirtualKey(vk, MAPVK_VK_TO_CHAR);

            if ( HIWORD(wxk) & 0x8000 )
            {
                // It's a dead key and we don't return anything at all for them
                // as we simply don't have any way to indicate the difference
                // between e.g. a normal "'" and "'" as a dead key -- and
                // generating the same events for them just doesn't seem like a
                // good idea.
                wxk = WXK_NONE;
            }

            // In any case return this as a Unicode character value.
            if ( uc )
                *uc = wxk;

            // For compatibility with the old non-Unicode code we continue
            // returning key codes for Latin-1 characters directly
            // (normally it would really only make sense to do it for the
            // ASCII characters, not Latin-1 ones).
            if ( wxk > 255 )
            {
                // But for anything beyond this we can only return the key
                // value as a real Unicode character, not a wxKeyCode
                // because this enum values clash with Unicode characters
                // (e.g. WXK_LBUTTON also happens to be U+012C a.k.a.
                // "LATIN CAPITAL LETTER I WITH BREVE").
                wxk = WXK_NONE;
            }
            break;

        // handle extended keys
        case VK_PRIOR:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_PAGEUP, WXK_PAGEUP);
            break;

        case VK_NEXT:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_PAGEDOWN, WXK_PAGEDOWN);
            break;

        case VK_END:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_END, WXK_END);
            break;

        case VK_HOME:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_HOME, WXK_HOME);
            break;

        case VK_LEFT:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_LEFT, WXK_LEFT);
            break;

        case VK_UP:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_UP, WXK_UP);
            break;

        case VK_RIGHT:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_RIGHT, WXK_RIGHT);
            break;

        case VK_DOWN:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_DOWN, WXK_DOWN);
            break;

        case VK_INSERT:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_INSERT, WXK_INSERT);
            break;

        case VK_DELETE:
            wxk = ChooseNormalOrExtended(lParam, WXK_NUMPAD_DELETE, WXK_DELETE);

            if ( uc )
                *uc = WXK_DELETE;
            break;

        case VK_RETURN:
            // don't use ChooseNormalOrExtended() here as the keys are reversed
            // here: numpad enter is the extended one
            wxk = HIWORD(lParam) & KF_EXTENDED ? WXK_NUMPAD_ENTER : WXK_RETURN;

            if ( uc )
                *uc = WXK_RETURN;
            break;

        default:
            if ( (vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z') )
            {
                // A simple alphanumeric key and the values of them coincide in
                // Windows and wx for both ASCII and Unicode codes.
                wxk = vk;
            }
            else // Something we simply don't know about at all.
            {
                wxk = WXK_NONE;
            }

            if ( uc )
                *uc = vk;
    }

    return wxk;
}

#elif defined(__WXMAC__)

#else // defined(__WXGTK__)

/* The following functions are copied from wxWidgets repo file
 * `src/x11/utils.cpp` that convert a XLib keycode into a
 * WX keycode.
 */
static const std::unordered_map<unsigned, int> kKeyMap = {
    #include "x11keymap.h"
};

int wxCharCodeXToWX(unsigned long keySym)
{
    int id;
    switch (keySym)
    {
        case XK_Shift_L:
        case XK_Shift_R:
            id = WXK_SHIFT; break;
        case XK_Control_L:
        case XK_Control_R:
            id = WXK_CONTROL; break;
        case XK_Meta_L:
        case XK_Meta_R:
            id = WXK_ALT; break;
        case XK_Caps_Lock:
            id = WXK_CAPITAL; break;
        case XK_BackSpace:
            id = WXK_BACK; break;
        case XK_Delete:
            id = WXK_DELETE; break;
        case XK_Clear:
            id = WXK_CLEAR; break;
        case XK_Tab:
            id = WXK_TAB; break;
        case XK_numbersign:
            id = '#'; break;
        case XK_Return:
            id = WXK_RETURN; break;
        case XK_Escape:
            id = WXK_ESCAPE; break;
        case XK_Pause:
        case XK_Break:
            id = WXK_PAUSE; break;
        case XK_Num_Lock:
            id = WXK_NUMLOCK; break;
        case XK_Scroll_Lock:
            id = WXK_SCROLL; break;

        case XK_Home:
            id = WXK_HOME; break;
        case XK_End:
            id = WXK_END; break;
        case XK_Left:
            id = WXK_LEFT; break;
        case XK_Right:
            id = WXK_RIGHT; break;
        case XK_Up:
            id = WXK_UP; break;
        case XK_Down:
            id = WXK_DOWN; break;
        case XK_Next:
            id = WXK_PAGEDOWN; break;
        case XK_Prior:
            id = WXK_PAGEUP; break;
        case XK_Menu:
            id = WXK_MENU; break;
        case XK_Select:
            id = WXK_SELECT; break;
        case XK_Cancel:
            id = WXK_CANCEL; break;
        case XK_Print:
            id = WXK_PRINT; break;
        case XK_Execute:
            id = WXK_EXECUTE; break;
        case XK_Insert:
            id = WXK_INSERT; break;
        case XK_Help:
            id = WXK_HELP; break;

        case XK_KP_Multiply:
            id = WXK_NUMPAD_MULTIPLY; break;
        case XK_KP_Add:
            id = WXK_NUMPAD_ADD; break;
        case XK_KP_Subtract:
            id = WXK_NUMPAD_SUBTRACT; break;
        case XK_KP_Divide:
            id = WXK_NUMPAD_DIVIDE; break;
        case XK_KP_Decimal:
            id = WXK_NUMPAD_DECIMAL; break;
        case XK_KP_Equal:
            id = WXK_NUMPAD_EQUAL; break;
        case XK_KP_Space:
            id = WXK_NUMPAD_SPACE; break;
        case XK_KP_Tab:
            id = WXK_NUMPAD_TAB; break;
        case XK_KP_Enter:
            id = WXK_NUMPAD_ENTER; break;
        case XK_KP_0:
            id = WXK_NUMPAD0; break;
        case XK_KP_1:
            id = WXK_NUMPAD1; break;
        case XK_KP_2:
            id = WXK_NUMPAD2; break;
        case XK_KP_3:
            id = WXK_NUMPAD3; break;
        case XK_KP_4:
            id = WXK_NUMPAD4; break;
        case XK_KP_5:
            id = WXK_NUMPAD5; break;
        case XK_KP_6:
            id = WXK_NUMPAD6; break;
        case XK_KP_7:
            id = WXK_NUMPAD7; break;
        case XK_KP_8:
            id = WXK_NUMPAD8; break;
        case XK_KP_9:
            id = WXK_NUMPAD9; break;
        case XK_KP_Insert:
            id = WXK_NUMPAD_INSERT; break;
        case XK_KP_End:
            id = WXK_NUMPAD_END; break;
        case XK_KP_Down:
            id = WXK_NUMPAD_DOWN; break;
        case XK_KP_Page_Down:
            id = WXK_NUMPAD_PAGEDOWN; break;
        case XK_KP_Left:
            id = WXK_NUMPAD_LEFT; break;
        case XK_KP_Right:
            id = WXK_NUMPAD_RIGHT; break;
        case XK_KP_Home:
            id = WXK_NUMPAD_HOME; break;
        case XK_KP_Up:
            id = WXK_NUMPAD_UP; break;
        case XK_KP_Page_Up:
            id = WXK_NUMPAD_PAGEUP; break;
        case XK_KP_Separator:
            id = WXK_NUMPAD_SEPARATOR; break;
        case XK_KP_Delete:
            id = WXK_NUMPAD_DELETE; break;
        case XK_KP_Begin:
            id = WXK_NUMPAD_BEGIN; break;
        case XK_Super_L:
            id = WXK_ALT; break;
        case XK_F1:
            id = WXK_F1; break;
        case XK_F2:
            id = WXK_F2; break;
        case XK_F3:
            id = WXK_F3; break;
        case XK_F4:
            id = WXK_F4; break;
        case XK_F5:
            id = WXK_F5; break;
        case XK_F6:
            id = WXK_F6; break;
        case XK_F7:
            id = WXK_F7; break;
        case XK_F8:
            id = WXK_F8; break;
        case XK_F9:
            id = WXK_F9; break;
        case XK_F10:
            id = WXK_F10; break;
        case XK_F11:
            id = WXK_F11; break;
        case XK_F12:
            id = WXK_F12; break;
        case XK_F13:
            id = WXK_F13; break;
        case XK_F14:
            id = WXK_F14; break;
        case XK_F15:
            id = WXK_F15; break;
        case XK_F16:
            id = WXK_F16; break;
        case XK_F17:
            id = WXK_F17; break;
        case XK_F18:
            id = WXK_F18; break;
        case XK_F19:
            id = WXK_F19; break;
        case XK_F20:
            id = WXK_F20; break;
        case XK_F21:
            id = WXK_F21; break;
        case XK_F22:
            id = WXK_F22; break;
        case XK_F23:
            id = WXK_F23; break;
        case XK_F24:
            id = WXK_F24; break;
        default:
            id = (keySym <= 255) ? (int)keySym : -1;
    }
    return id;
}

int wxUnicodeCharXToWX(unsigned long keySym)
{
    int id = wxCharCodeXToWX(keySym);

    /* first check for Latin-1 characters (1:1 mapping) */
    if (id != WXK_NONE)
        return id;

    /* also check for directly encoded 24-bit UCS characters */
    if ((keySym & 0xff000000) == 0x01000000)
        return keySym & 0x00ffffff;

    const auto iter = kKeyMap.find(keySym);
    if (iter != kKeyMap.end())
        return iter->second;

    return WXK_NONE;
}

#endif  // defined(__WXMSW__)

class BackgroundInput : public wxThread {
public:
    BackgroundInput(wxEvtHandler* handle) : wxThread(wxTHREAD_DETACHED), handler(handle) {}
    ~BackgroundInput();
protected:
    virtual wxThread::ExitCode Entry();
    virtual wxThread::ExitCode Setup();
    virtual wxThread::ExitCode CheckKeyboard();
    virtual void Cleanup();
private:
    wxEvtHandler *handler;
#if defined(__WXMSW__)
    SHORT previousState[0xFF];
#elif defined(__WXMAC__)
#else // defined(__WXGTK__)
    Display *x11display = nullptr;
    char previousState[32];
    char currentState[32];
    int keySymsPerKeycode;
#endif
};

BackgroundInput::~BackgroundInput()
{
    Cleanup();
}

wxThread::ExitCode BackgroundInput::Setup()
{
#if defined(__WXMSW__)
    // https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    for (int i = 0x08; i < 0xFF; ++i) {
        previousState[i] = GetAsyncKeyState(i);
    }
#elif defined(__WXMAC__)
    wxLogError(wxT("Yet to be implemented!"));
    return ANY_ERROR;
#else // defined(__WXGTK__)
    if (IsWayland()) {
        wxLogError(wxT("Wayland does not allow to globally query keypresses for security reasons. \
        Check a better explanation here: https://github.com/albertlauncher/albert/issues/309"));
        return ANY_ERROR;
    }

    x11display = XOpenDisplay(NULL);
    if (x11display == NULL) {
        return ANY_ERROR;
    }
    XQueryKeymap(x11display, previousState);
    int minKeycode, maxKeycode;
    XDisplayKeycodes(x11display, &minKeycode, &maxKeycode);
    XFree(XGetKeyboardMapping(x11display, minKeycode, maxKeycode + 1 - minKeycode, &keySymsPerKeycode));
#endif
    return NO_ERROR;
}

wxThread::ExitCode BackgroundInput::CheckKeyboard()
{
    if (wxWindow::FindFocus()) {
        // if the app has focus, do nothing
        return NO_ERROR;
    }
#if defined(__WXMSW__)
    for (int i = 0x08; i < 0xFF; ++i) {
        SHORT bits = GetAsyncKeyState(i);
        if (bits == previousState[i]) continue;
        wchar_t uc = 0;
        int xKeySym = VKToWX(i, 0, &uc);
        // virtual key "i" is pressed
        if ((bits & 0x8000) && (previousState[i] & 0x8000) == 0) {
            if (handler && !wxWindow::FindFocus()) {
                wxKeyEvent ev(wxEVT_KEY_DOWN);
                ev.m_keyCode = xKeySym;
                handler->AddPendingEvent(ev);
            }
        }
        // virtual key "i" is released
        else if (((bits & 0x8000) == 0) && (previousState[i] & 0x8000)) {
            if (handler && !wxWindow::FindFocus()) {
                wxKeyEvent ev(wxEVT_KEY_UP);
                ev.m_keyCode = xKeySym;
                handler->AddPendingEvent(ev);
            }
        }
        previousState[i] = bits;
    }
#elif defined(__WXMAC__)
#else // defined(__WXGTK__)
    XQueryKeymap(x11display, currentState);
    for (int i = 0; i < 32; ++i) {
        int cte = 8*i;
        if (currentState[i] != previousState[i]) {
            int numCurrent = currentState[i];
            int numPrevious = previousState[i];
            for (int pos = 0; pos < 8; ++pos) {
                int rawKeycode = cte + pos;
                int xKeySymLast = -1;
                // Key was pressed
                if (((numCurrent & 0x01) == 1) && ((numPrevious & 0x01) == 0)) {
                    if (handler && !wxWindow::FindFocus()) {
                        for (int j = 0; j < keySymsPerKeycode; ++j) {
                            KeySym kSym = XkbKeycodeToKeysym(x11display, rawKeycode, 0, j);
                            if (kSym == NoSymbol) break;
                            int xKeySym = wxUnicodeCharXToWX(kSym);
                            if (xKeySym == xKeySymLast || xKeySym == WXK_NONE) break;
                            else xKeySymLast = xKeySym;
                            if (xKeySym >= 'a' && xKeySym <= 'z') xKeySym = xKeySym + 'A' - 'a';
                            //fprintf(stderr, "(%d,%d): %ld - %s --- %d\n", i, j, kSym, XKeysymToString(kSym), xKeySym);
                            wxKeyEvent ev(wxEVT_KEY_DOWN);
                            ev.m_uniChar = xKeySym;
                            ev.m_keyCode = xKeySym;
                            handler->AddPendingEvent(ev);
                        }
                    }
                }
                // Key was released
                else if (((numCurrent & 0x01) == 0) && ((numPrevious & 0x01) == 1)) {
                    if (handler && !wxWindow::FindFocus()) {
                        for (int j = 0; j < keySymsPerKeycode; ++j) {
                            KeySym kSym = XkbKeycodeToKeysym(x11display, rawKeycode, 0, j);
                            if (kSym == NoSymbol) break;
                            int xKeySym = wxUnicodeCharXToWX(kSym);
                            if (xKeySym == xKeySymLast || xKeySym == WXK_NONE) break;
                            else xKeySymLast = xKeySym;
                            if (xKeySym >= 'a' && xKeySym <= 'z') xKeySym = xKeySym + 'A' - 'a';
                            //fprintf(stderr, "(%d,%d): %ld - %s --- %d\n", i, j, kSym, XKeysymToString(kSym), xKeySym);
                            wxKeyEvent ev(wxEVT_KEY_UP);
                            ev.m_uniChar = xKeySym;
                            ev.m_keyCode = xKeySym;
                            handler->AddPendingEvent(ev);
                        }
                    }
                }
                numCurrent /= 2;
                numPrevious /= 2;
            }
        }
        previousState[i] = currentState[i];
    }
#endif
    return NO_ERROR;
}

void BackgroundInput::Cleanup()
{
#if defined(__WXMSW__)
#elif defined(__WXMAC__)
#else // defined(__WXGTK__)
    if (x11display) {
        XCloseDisplay(x11display);
        x11display = nullptr;
    }
#endif
}

wxThread::ExitCode BackgroundInput::Entry()
{
    if (Setup() != NO_ERROR)
        return ANY_ERROR;

    while (!TestDestroy()) {
        if (CheckKeyboard() != NO_ERROR)
            break;
        wxMilliSleep(10);
    }
    Cleanup();
    return NO_ERROR;
}

BackgroundInput *input = nullptr;

void enableKeyboardBackgroundInput(wxEvtHandler* handler)
{
    if (input) return;
    input = new BackgroundInput(handler);
    if (input->Run() != wxTHREAD_NO_ERROR)
    {
        wxLogError(wxT("Failed to create keyboard thread!"));
        delete input;
        input = nullptr;
    }
}

void disableKeyboardBackgroundInput()
{
    if (input && input->IsRunning()) {
        input->Delete();
    }
    input = nullptr;
}

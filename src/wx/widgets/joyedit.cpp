#include "wx/joyedit.h"

#include <wx/tokenzr.h>

#include "config/user-input.h"
#include "opts.h"
#include "strutils.h"
#include "wx/sdljoy.h"

// FIXME: suppport analog/digital flag on per-axis basis

IMPLEMENT_DYNAMIC_CLASS(wxJoyKeyTextCtrl, wxKeyTextCtrl)

BEGIN_EVENT_TABLE(wxJoyKeyTextCtrl, wxKeyTextCtrl)
EVT_SDLJOY(wxJoyKeyTextCtrl::OnJoy)
END_EVENT_TABLE()

void wxJoyKeyTextCtrl::OnJoy(wxJoyEvent& event)
{
    static wxLongLong last_event = 0;

    // Filter consecutive axis motions within 300ms, as this adds two bindings
    // +1/-1 instead of the one intended.
    if ((event.control() == wxJoyControl::AxisPlus ||
         event.control() == wxJoyControl::AxisMinus) &&
        wxGetUTCTimeMillis() - last_event < 300) {
        return;
    }

    last_event = wxGetUTCTimeMillis();

    // Control was unpressed, ignore.
    if (!event.pressed())
        return;

    wxString nv = config::UserInput(event).ToString();

    if (nv.empty())
        return;

    if (multikey) {
        wxString ov = GetValue();

        if (!ov.empty())
            nv = ov + multikey + nv;
    }

    SetValue(nv);

    if (keyenter)
        Navigate();
}

wxString wxJoyKeyTextCtrl::ToString(int mod, int key, int joy, bool isConfig)
{
    if (!joy)
        return wxKeyTextCtrl::ToString(mod, key, isConfig);

    wxString s;
    // Note: wx translates unconditionally (2.8.12, 2.9.1)!
    // So any strings added below must also be translated unconditionally
    s.Printf(("Joy%d-"), joy);
    wxString mk;

    switch (mod) {
    case wxJoyControl::AxisPlus:
        mk.Printf(("Axis%d+"), key);
        break;

    case wxJoyControl::AxisMinus:
        mk.Printf(("Axis%d-"), key);
        break;

    case wxJoyControl::Button:
        mk.Printf(("Button%d"), key);
        break;

    case wxJoyControl::HatNorth:
        mk.Printf(("Hat%dN"), key);
        break;

    case wxJoyControl::HatSouth:
        mk.Printf(("Hat%dS"), key);
        break;

    case wxJoyControl::HatWest:
        mk.Printf(("Hat%dW"), key);
        break;

    case wxJoyControl::HatEast:
        mk.Printf(("Hat%dE"), key);
        break;
    }

    s += mk;
    return s;
}

wxString wxJoyKeyTextCtrl::FromAccelToString(wxAcceleratorEntry_v keys, wxChar sep, bool isConfig)
{
    wxString ret;

    for (size_t i = 0; i < keys.size(); i++) {
        if (i > 0)
            ret += sep;

        wxString key = ToString(keys[i].GetFlags(), keys[i].GetKeyCode(), keys[i].GetJoystick(), isConfig);

        if (key.empty())
            return wxEmptyString;

        ret += key;
    }

    return ret;
}

#include <wx/regex.h>

// only parse regex once
// Note: wx translates unconditionally (2.8.12, 2.9.1)!
// So any strings added below must also be translated unconditionally
// \1 is joy #
static wxRegEx joyre;
// \1 is axis# and \2 is + or -
static wxRegEx axre;
// \1 is button#
static wxRegEx butre;
// \1 is hat#, \3 is N, \4 is S, \5 is E, \6 is W, \7 is NE, \8 is SE,
// \9 is SW, \10 is NW
static wxRegEx hatre;
// use of static wxRegeEx is not thread-safe
static wxCriticalSection recs;

// wx provides no atoi for wxChar
// this is not a universal function; assumes valid number
static int simple_atoi(const wxString& s, int len)
{
    int ret = 0;

    for (int i = 0; i < len; i++)
        ret = ret * 10 + (int)(s[i] - wxT('0'));

    return ret;
}

static void CompileRegex()
{
    // \1 is joy #
    joyre.Compile(("^Joy([0-9]+)[-+]"), wxRE_EXTENDED | wxRE_ICASE);
    // \1 is axis# and \2 is + or -
    axre.Compile(("Axis([0-9]+)([+-])"), wxRE_EXTENDED | wxRE_ICASE);
    // \1 is button#
    butre.Compile(("Button([0-9]+)"), wxRE_EXTENDED | wxRE_ICASE);
    // \1 is hat#, \3 is N, \4 is S, \5 is E, \6 is W
    // This used to support diagonals as discrete input. For compatibility
    // reasons, these have been moved a 1/8 turn counter-clockwise.
    hatre.Compile(("Hat([0-9]+)"
                   "((N|North|U|Up|NE|NorthEast|UR|UpRight)|"
                   "(S|South|D|Down|SW|SouthWest|DL|DownLeft)|"
                   "(E|East|R|Right|SE|SouthEast|DR|DownRight)|"
                   "(W|West|L|Left|NW|NorthWest|UL|UpLeft))"),
                  wxRE_EXTENDED | wxRE_ICASE);
}

static bool ParseJoy(const wxString& s, int len, int& mod, int& key, int& joy)
{
    mod = key = joy = 0;

    if (!len)
        return false;

    wxCriticalSectionLocker lk(recs);
    size_t b, l;

    CompileRegex();
    if (!joyre.Matches(s) || !joyre.GetMatch(&b, &l) || b)
        return false;

    const wxString p = s.Mid(l);
    size_t alen = len - l;
    joyre.GetMatch(&b, &l, 1);
    joy = simple_atoi(s.Mid(b), l);
#define is_ctrl(re) re.Matches(p) && re.GetMatch(&b, &l) && l == alen && !b

    if (is_ctrl(axre)) {
        axre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
        axre.GetMatch(&b, &l, 2);
        mod = p[b] == wxT('+') ? wxJoyControl::AxisPlus : wxJoyControl::AxisMinus;
    } else if (is_ctrl(butre)) {
        butre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
        mod = wxJoyControl::Button;
    } else if (is_ctrl(hatre)) {
        hatre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
#define check_dir(n, d) if (hatre.GetMatch(&b, &l, n) && l > 0) mod = wxJoyControl::Hat##d

        check_dir(3, North);
        else check_dir(4, South);
        else check_dir(5, East);
        else check_dir(6, West);
    } else {
        joy = 0;
        return false;
    }

    return true;
}

bool wxJoyKeyTextCtrl::ParseString(const wxString& s, int len, int& mod, int& key, int& joy)
{
    if (ParseJoy(s, len, mod, key, joy))
        return true;

    return wxKeyTextCtrl::ParseString(s, len, mod, key);
}

bool wxJoyKeyTextCtrl::FromString(const wxString& s, int& mod, int& key, int& joy)
{
    return ParseString(s, s.size(), mod, key, joy);
}

wxAcceleratorEntry_v wxJoyKeyTextCtrl::ToAccelFromString(const wxString& s, wxChar sep)
{
    wxAcceleratorEntry_v ret, empty;
    int mod, key, joy;
    if (s.size() == 0)
        return empty;

    for (const auto& token : strutils::split_with_sep(s, sep)) {
        if (!ParseString(token, token.size(), mod, key, joy))
            return empty;
        ret.insert(ret.begin(), wxAcceleratorEntryUnicode(token, joy, mod, key));
    }
    return ret;
}

IMPLEMENT_CLASS(wxJoyKeyValidator, wxValidator)

bool wxJoyKeyValidator::TransferToWindow()
{
    wxJoyKeyTextCtrl* jk = wxDynamicCast(GetWindow(), wxJoyKeyTextCtrl);

    if (!jk)
        return false;

    jk->SetValue(config::UserInput::SpanToString(gopts.game_control_bindings[val_]));
    return true;
}

bool wxJoyKeyValidator::TransferFromWindow()
{
    wxJoyKeyTextCtrl* jk = wxDynamicCast(GetWindow(), wxJoyKeyTextCtrl);

    if (!jk)
        return false;

    gopts.game_control_bindings[val_] = config::UserInput::FromString(jk->GetValue());
    return true;
}

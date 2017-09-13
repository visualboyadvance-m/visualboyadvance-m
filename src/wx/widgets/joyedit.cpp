#include "wx/joyedit.h"

// FIXME: suppport analog/digital flag on per-axis basis

IMPLEMENT_DYNAMIC_CLASS(wxJoyKeyTextCtrl, wxKeyTextCtrl)

BEGIN_EVENT_TABLE(wxJoyKeyTextCtrl, wxKeyTextCtrl)
EVT_SDLJOY(wxJoyKeyTextCtrl::OnJoy)
END_EVENT_TABLE()

int wxJoyKeyTextCtrl::DigitalButton(wxSDLJoyEvent& event)
{
    int sdlval = event.GetControlValue();
    int sdltype = event.GetControlType();

    switch (sdltype) {
    case WXSDLJOY_AXIS:
        // for val = 0 return arbitrary direction; val means "off"
        return sdlval > 0 ? WXJB_AXIS_PLUS : WXJB_AXIS_MINUS;

    case WXSDLJOY_HAT:

        /* URDL = 1248 */
        switch (sdlval) {
        case 1:
            return WXJB_HAT_N;

        case 2:
            return WXJB_HAT_E;

        case 3:
            return WXJB_HAT_NE;

        case 4:
            return WXJB_HAT_S;

        case 6:
            return WXJB_HAT_SE;

        case 8:
            return WXJB_HAT_W;

        case 9:
            return WXJB_HAT_NW;

        case 12:
            return WXJB_HAT_SW;

        default:
            return WXJB_HAT_N; // arbitrary direction; val = 0 means "off"
        }

    case WXSDLJOY_BUTTON:
        return WXJB_BUTTON;

    default:
        // unknown ctrl type
        return -1;
    }
}

void wxJoyKeyTextCtrl::OnJoy(wxSDLJoyEvent& event)
{
    short val = event.GetControlValue();
    int mod = DigitalButton(event);
    int key = event.GetControlIndex(), joy = event.GetJoy() + 1;

    if (!val || mod < 0)
        return;

    wxString nv = ToString(mod, key, joy);

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

wxString wxJoyKeyTextCtrl::ToString(int mod, int key, int joy)
{
    if (!joy)
        return wxKeyTextCtrl::ToString(mod, key);

    wxString s;
    // Note: wx translates unconditionally (2.8.12, 2.9.1)!
    // So any strings added below must also be translated unconditionally
    s.Printf(_("Joy%d-"), joy);
    wxString mk;

    switch (mod) {
    case WXJB_AXIS_PLUS:
        mk.Printf(_("Axis%d+"), key);
        break;

    case WXJB_AXIS_MINUS:
        mk.Printf(_("Axis%d-"), key);
        break;

    case WXJB_BUTTON:
        mk.Printf(_("Button%d"), key);
        break;

    case WXJB_HAT_N:
        mk.Printf(_("Hat%dN"), key);
        break;

    case WXJB_HAT_S:
        mk.Printf(_("Hat%dS"), key);
        break;

    case WXJB_HAT_W:
        mk.Printf(_("Hat%dW"), key);
        break;

    case WXJB_HAT_E:
        mk.Printf(_("Hat%dE"), key);
        break;

    case WXJB_HAT_NW:
        mk.Printf(_("Hat%dNW"), key);
        break;

    case WXJB_HAT_NE:
        mk.Printf(_("Hat%dNE"), key);
        break;

    case WXJB_HAT_SW:
        mk.Printf(_("Hat%dSW"), key);
        break;

    case WXJB_HAT_SE:
        mk.Printf(_("Hat%dSE"), key);
        break;
    }

    s += mk;
    return s;
}

wxString wxJoyKeyTextCtrl::ToString(wxJoyKeyBinding_v keys, wxChar sep)
{
    wxString ret;

    for (int i = 0; i < keys.size(); i++) {
        if (i > 0)
            ret += sep;

        wxString key = ToString(keys[i].mod, keys[i].key, keys[i].joy);

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
static wxRegEx joyre(_("^Joy([0-9]+)[-+]"), wxRE_EXTENDED | wxRE_ICASE);
// \1 is axis# and \2 is + or -
static wxRegEx axre(_("Axis([0-9]+)([+-])"), wxRE_EXTENDED | wxRE_ICASE);
// \1 is button#
static wxRegEx butre(_("Button([0-9]+)"), wxRE_EXTENDED | wxRE_ICASE);
// \1 is hat#, \3 is N, \4 is S, \5 is E, \6 is W, \7 is NE, \8 is SE,
// \9 is SW, \10 is NW
static wxRegEx hatre(_("Hat([0-9]+)((N|North|U|Up)|(S|South|D|Down)|"
                       "(E|East|R|Right)|(W|West|L|Left)|"
                       "(NE|NorthEast|UR|UpRight)|(SE|SouthEast|DR|DownRight)|"
                       "(SW|SouthWest|DL|DownLeft)|(NW|NorthWest|UL|UpLeft))"),
    wxRE_EXTENDED | wxRE_ICASE);
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

static bool ParseJoy(const wxString& s, int len, int& mod, int& key, int& joy)
{
    mod = key = joy = 0;

    if (!len)
        return false;

    wxCriticalSectionLocker lk(recs);
    size_t b, l;

    if (!joyre.Matches(s) || !joyre.GetMatch(&b, &l) || b)
        return false;

    const wxString p = s.Mid(l);
    int alen = len - l;
    joyre.GetMatch(&b, &l, 1);
    joy = simple_atoi(s.Mid(b), l);
#define is_ctrl(re) re.Matches(p) && re.GetMatch(&b, &l) && l == alen && !b

    if (is_ctrl(axre)) {
        axre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
        axre.GetMatch(&b, &l, 2);
        mod = p[b] == wxT('+') ? WXJB_AXIS_PLUS : WXJB_AXIS_MINUS;
    } else if (is_ctrl(butre)) {
        butre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
        mod = WXJB_BUTTON;
    } else if (is_ctrl(hatre)) {
        hatre.GetMatch(&b, &l, 1);
        key = simple_atoi(p.Mid(b), l);
#define check_dir(n, d) else if (hatre.GetMatch(&b, &l, n) && l > 0) mod = WXJB_HAT_##d

        if (0)
            ;

        check_dir(3, N);
        check_dir(4, S);
        check_dir(5, E);
        check_dir(6, W);
        check_dir(7, NE);
        check_dir(8, SE);
        check_dir(9, SW);
        check_dir(10, NW);
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

wxJoyKeyBinding_v wxJoyKeyTextCtrl::FromString(const wxString& s, wxChar sep)
{
    wxJoyKeyBinding_v ret, empty;
    int mod, key, joy;
    int len = s.size();

    if (!len)
        return empty;

    for (int lastkey = len - 1; (lastkey = s.rfind(sep, lastkey)) != wxString::npos; lastkey--) {
        if (lastkey == len - 1) {
            // sep as accel
            if (!lastkey)
                break;

            if (s[lastkey - 1] == wxT('-') || s[lastkey - 1] == wxT('+') || s[lastkey - 1] == sep)
                continue;
        }

        if (!ParseString(s.Mid(lastkey + 1), len - lastkey - 1, mod, key, joy))
            return empty;

        wxJoyKeyBinding jb = { key, mod, joy };
        ret.insert(ret.begin(), jb);
        len = lastkey;
    }

    if (!ParseString(s, len, mod, key, joy))
        return empty;

    wxJoyKeyBinding jb = { key, mod, joy };
    ret.insert(ret.begin(), jb);
    return ret;
}

IMPLEMENT_CLASS(wxJoyKeyValidator, wxValidator)

bool wxJoyKeyValidator::TransferToWindow()
{
    if (!val)
        return false;

    wxJoyKeyTextCtrl* jk = wxDynamicCast(GetWindow(), wxJoyKeyTextCtrl);

    if (!jk)
        return false;

    jk->SetValue(wxJoyKeyTextCtrl::ToString(*val));
    return true;
}

bool wxJoyKeyValidator::TransferFromWindow()
{
    if (!val)
        return false;

    wxJoyKeyTextCtrl* jk = wxDynamicCast(GetWindow(), wxJoyKeyTextCtrl);

    if (!jk)
        return false;

    *val = wxJoyKeyTextCtrl::FromString(jk->GetValue());
    return true;
}

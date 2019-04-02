#include <wx/log.h>
#include "wx/keyedit.h"

IMPLEMENT_DYNAMIC_CLASS(wxKeyTextCtrl, wxTextCtrl)

BEGIN_EVENT_TABLE(wxKeyTextCtrl, wxTextCtrl)
EVT_KEY_DOWN(wxKeyTextCtrl::OnKeyDown)
EVT_KEY_UP(wxKeyTextCtrl::OnKeyUp)
END_EVENT_TABLE()

void wxKeyTextCtrl::OnKeyDown(wxKeyEvent& event)
{
    lastmod = event.GetModifiers();
    lastkey = event.GetKeyCode();
}

void wxKeyTextCtrl::OnKeyUp(wxKeyEvent& event)
{
    (void)event; // unused param
    int mod = lastmod;
    int key = lastkey;
    lastmod = lastkey = 0;

    // key is only 0 if we missed the keydown event
    // or if we are being shipped pseudo keypress events
    // either way, just ignore
    if (!key)
        return;

    // use unmodified backspace to clear last key, if enabled
    // if blank or backspace is modified, add normally instead
    if (clearable && !mod && key == WXK_BACK && !GetValue().empty()) {
        wxString val = GetValue();
        size_t lastkey = val.rfind(multikey);

        if (lastkey && lastkey != wxString::npos) {
            // if this was actually a ,-accel, delete instead
            if (lastkey == val.size() - 1) {
                lastkey = val.rfind(multikey, lastkey - 1);

                if (lastkey == wxString::npos)
                    lastkey = 0;
            }

            val.resize(lastkey);
            SetValue(val);
        } else
            Clear();
    } else {
        wxString nv = ToString(mod, key);

        if (nv.empty())
            return;

        if (multikey) {
            wxString ov = GetValue();

            if (!ov.empty())
                nv = ov + multikey + nv;
        }

        SetValue(nv);
    }

    if (keyenter)
        Navigate();
}

wxString wxKeyTextCtrl::ToString(int mod, int key)
{
    // wx ignores non-alnum printable chars
    // actually, wx gives an assertion error, so it's best to filter out
    // before passing to ToString()
    bool char_override = key > 32 && key < WXK_START && !wxIsalnum(key);
    // wx also ignores modifiers (and does not report meta at all)
    bool mod_override = key == WXK_SHIFT || key == WXK_CONTROL || key == WXK_ALT || key == WXK_RAW_CONTROL;
    wxAcceleratorEntry ae(mod, char_override || mod_override ? WXK_F1 : key);
    // Note: wx translates unconditionally (2.8.12, 2.9.1)!
    // So any strings added below must also be translated unconditionally
    wxString s = ae.ToString();

    if (char_override || mod_override) {
        size_t l = s.rfind(wxT('-'));

        if (l == wxString::npos)
            l = 0;
        else
            l++;

        s.erase(l);

        switch (key) {
        case WXK_SHIFT:
            s.append(_("SHIFT"));
            break;

        case WXK_ALT:
            s.append(_("ALT"));
            break;

        case WXK_CONTROL:
            s.append(_("CTRL"));
            break;

        // this is the control key on macs
#ifdef __WXMAC__
        case WXK_RAW_CONTROL:
            s.append(_("RAWCTRL"));
            break;
#endif

        default:
            s.append((wxChar)key);
        }
    }

// on Mac, ctrl/meta become xctrl/cmd
// on other, meta is ignored
#ifndef __WXMAC__

    if (mod & wxMOD_META) {
        s.insert(0, _("Meta-"));
    }

#endif

    if (s.empty() || (key != wxT('-') && s[s.size() - 1] == wxT('-'))
                  || (key != wxT('+') && s[s.size() - 1] == wxT('+')))
        // bad key combo; probably also generates an assertion in wx
        return wxEmptyString;

// hacky workaround for bug in wx 3.1+ not parsing key display names, or
// parsing modifiers that aren't a combo correctly
    s.MakeUpper();

    int keys_el_size = sizeof(keys_with_display_names)/sizeof(keys_with_display_names[0]);

    for (int i = 0; i < keys_el_size; i++) {
        wxString name(keys_with_display_names[i].name);
        wxString display_name(keys_with_display_names[i].display_name);
        name.MakeUpper();
        display_name.MakeUpper();

        s.Replace(display_name, name, true);
    }

    return s;
}

wxString wxKeyTextCtrl::ToString(wxAcceleratorEntry_v keys, wxChar sep)
{
    wxString ret;

    for (size_t i = 0; i < keys.size(); i++) {
        if (i > 0)
            ret += sep;

        wxString key = ToString(keys[i].GetFlags(), keys[i].GetKeyCode());

        if (key.empty())
            return wxEmptyString;

        ret += key;
    }

    return ret;
}

bool wxKeyTextCtrl::ParseString(const wxString& s, int len, int& mod, int& key)
{
    mod = key = 0;

    if (!s || !len)
        return false;

    wxString a = wxT('\t');
    a.Append(s.Left(len));
    wxAcceleratorEntry ae;
#ifndef __WXMAC__
#define check_meta(str)                                                        \
    do {                                                                       \
        wxString meta = str;                                                   \
        for (size_t ml = 0; (ml = a.find(meta, ml)) != wxString::npos; ml++) { \
            if (!ml || a[ml - 1] == wxT('-') || a[ml - 1] == wxT('+')) {       \
                mod |= wxMOD_META;                                             \
                a.erase(ml, meta.size());                                      \
                ml = -1;                                                       \
            }                                                                  \
        }                                                                      \
    } while (0)
    check_meta(wxT("Meta-"));
    check_meta(wxT("Meta+"));
    check_meta(_("Meta-"));
    check_meta(_("Meta+"));
#endif

    // wx disallows standalone modifiers
    // unlike ToString(), this generates a debug message rather than
    // an assertion error, so it's easy to ignore and expensive to avoid
    // beforehand.  Instead, check for them on failure
    wxLogNull disable_logging;
    if (!ae.FromString(a)) {
        a.MakeUpper();
#define chk_str(n, k, m)                                                 \
    do {                                                                 \
        wxString t = n;                                                  \
        if (a.size() > t.size() && a.substr(a.size() - t.size()) == t) { \
            a.replace(a.size() - t.size(), t.size(), wxT("F1"));         \
            wxString ss(s);                                              \
            if (ae.FromString(a)) {                                      \
                mod |= ae.GetFlags() | m;                                \
                key = k;                                                 \
                return true;                                             \
            }                                                            \
            a.replace(a.size() - 2, 2, n);                               \
        }                                                                \
    } while (0)
        chk_str(wxT("ALT"), WXK_ALT, wxMOD_ALT);
        chk_str(wxT("SHIFT"), WXK_SHIFT, wxMOD_SHIFT);
        chk_str(wxT("RAWCTRL"), WXK_RAW_CONTROL, wxMOD_RAW_CONTROL);
        chk_str(wxT("RAW_CTRL"), WXK_RAW_CONTROL, wxMOD_RAW_CONTROL);
        chk_str(wxT("RAWCONTROL"), WXK_RAW_CONTROL, wxMOD_RAW_CONTROL);
        chk_str(wxT("RAW_CONTROL"), WXK_RAW_CONTROL, wxMOD_RAW_CONTROL);
        chk_str(_("ALT"), WXK_ALT, wxMOD_ALT);
        chk_str(_("SHIFT"), WXK_SHIFT, wxMOD_SHIFT);
        chk_str(_("RAWCTRL"), WXK_RAW_CONTROL, wxMOD_RAW_CONTROL);
        chk_str(_("RAW_CTRL"), WXK_RAW_CONTROL, wxMOD_RAW_CONTROL);
        chk_str(_("RAWCONTROL"), WXK_RAW_CONTROL, wxMOD_RAW_CONTROL);
        chk_str(_("RAW_CONTROL"), WXK_RAW_CONTROL, wxMOD_RAW_CONTROL);
        chk_str(wxT("CTRL"), WXK_CONTROL, wxMOD_CONTROL);
        chk_str(wxT("CONTROL"), WXK_CONTROL, wxMOD_CONTROL);
        chk_str(_("CTRL"), WXK_CONTROL, wxMOD_CONTROL);
        chk_str(_("CONTROL"), WXK_CONTROL, wxMOD_CONTROL);
        return false;
    }

    mod |= ae.GetFlags();
    key = ae.GetKeyCode();

    // wx makes key lower-case, but key events return upper case
    if (key < WXK_START && wxIslower(key))
        key = wxToupper(key);

    return true;
}

bool wxKeyTextCtrl::FromString(const wxString& s, int& mod, int& key)
{
    return ParseString(s, s.size(), mod, key);
}

wxAcceleratorEntry_v wxKeyTextCtrl::FromString(const wxString& s, wxChar sep)
{
    wxAcceleratorEntry_v ret, empty;
    int mod, key;
    size_t len = s.size();

    for (size_t lastkey = len - 1; (lastkey = s.rfind(sep, lastkey)) != wxString::npos; lastkey--) {
        if (lastkey == len - 1) {
            // sep as accel
            if (!lastkey)
                break;

            if (s[lastkey - 1] == wxT('-') || s[lastkey - 1] == wxT('+') || s[lastkey - 1] == sep)
                continue;
        }

        if (!ParseString(s.Mid(lastkey + 1), len - lastkey - 1, mod, key))
            return empty;

        ret.insert(ret.begin(), wxAcceleratorEntry(mod, key));
        len = lastkey;
    }

    if (!ParseString(s, len, mod, key))
        return empty;

    ret.insert(ret.begin(), wxAcceleratorEntry(mod, key));
    return ret;
}

IMPLEMENT_CLASS(wxKeyValidator, wxValidator)

bool wxKeyValidator::TransferToWindow()
{
    wxKeyTextCtrl* k = wxDynamicCast(GetWindow(), wxKeyTextCtrl);

    if (!k)
        return false;

    k->SetValue(wxKeyTextCtrl::ToString(*val));
    return true;
}

bool wxKeyValidator::TransferFromWindow()
{
    wxKeyTextCtrl* k = wxDynamicCast(GetWindow(), wxKeyTextCtrl);

    if (!k)
        return false;

    *val = wxKeyTextCtrl::FromString(k->GetValue());
    return true;
}

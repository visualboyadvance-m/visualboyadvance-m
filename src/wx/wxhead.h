#ifndef WX_WXHEAD_H
#define WX_WXHEAD_H

// For compilers that support precompilation, includes <wx/wx.h>.
#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

// The following are not pulled in by wx.h

// for some reason, mingw32 wx.h doesn't pull in listctrl by default
#include <wx/config.h>
#include <wx/display.h>
#include <wx/fileconf.h>
#include <wx/listctrl.h>
#include <wx/stdpaths.h>
#include <wx/treectrl.h>
#include <wx/xrc/xmlres.h>
// filehistory.h is separate only in 2.9+
#include <wx/docview.h>

#ifndef NO_OGL
// glcanvas must be included before SFML for MacOSX
// originally, this was confined to drawing.h.
#include <wx/glcanvas.h>
// besides that, other crap gets #defined
#ifdef Status
#undef Status
#endif
#ifdef BadRequest
#undef BadRequest
#endif
#endif

// compatibility with MacOSX 10.5
#if !wxCHECK_VERSION(2, 8, 8)
#define AddFileWithMimeType(a, b, c, m) AddFile(a, b, c)
#define GetItemLabel GetText
#define SetItemLabel SetText
#define GetMenuLabel GetLabelTop
#define GetItemLabelText GetLabel
#endif

// compatibility with wx-2.9
// The only reason I use wxTRANSLATE at all is to get wxT as a side effect.
#if wxCHECK_VERSION(2, 9, 0)
#undef wxTRANSLATE
#define wxTRANSLATE wxT
#endif

// wxGTK (2.8.8+, at least) doesn't store the actual menu item text in m_text.
// This breaks GetText, SetText, GetAccel, SetAccel, and GetLabel;
// GetItemLabel() works, though.
// GetText, SetText, and GetLabel are deprecated, so that's not a problem
// GetAccel is inefficent anyway (often I don't want to convert to wxAccEnt)
// This is a working replacement for SetAccel, at least.

#include "wx/keyedit.h"

static inline void DoSetAccel(wxMenuItem* mi, wxAcceleratorEntry* acc)
{
    wxString lab = mi->GetItemLabel();
    size_t tab = lab.find(wxT('\t'));

    // following short circuit returns are to avoid UI update on no change
    if (tab == wxString::npos && !acc)
        return;

    wxString accs;

    if (acc)
        // actually, use keyedit's ToString(), as it is more reliable
        // and doesn't generate wx assertions
        // accs = acc->ToString();
        accs = wxKeyTextCtrl::ToString(acc->GetFlags(), acc->GetKeyCode());

    if (tab != wxString::npos && accs == lab.substr(tab + 1))
        return;

    if (tab != wxString::npos)
        lab.resize(tab);

    if (acc) {
        lab.append(wxT('\t'));
        lab.append(accs);
    }

    mi->SetItemLabel(lab);
}

// wxrc helpers (for dynamic strings instead of constant)
#define XRCID_D(str) wxXmlResource::GetXRCID(str)
//#define XRCCTRL_D(win, id, type) (wxStaticCast((win).FindWindow(XRCID_D(id)), type))
//#define XRCCTRL_I(win, id, type) (wxStaticCast((win).FindWindow(id), type))
// XRCCTRL is broken.
// In debug mode, it uses wxDynamicCast, which numerous wx classes fail on
// due to not correctly specifying parents in CLASS() declarations
// In standard mode, it does a static cast, which is unsafe for user input
// So instead I'll always do a (slow, possibly unportable) dynamic_cast().
// If your compiler doesn't support rtti, there are other pieces of code where
// I bypassed wx's stuff to use real dynamic_cast as well, so get a better
// compiler.
#undef XRCCTRL
#define XRCCTRL_I(win, id, type) (dynamic_cast<type*>((win).FindWindow(id)))
#define XRCCTRL(win, id, type) XRCCTRL_I(win, XRCID(id), type)
#define XRCCTRL_D(win, id, type) XRCCTRL_I(win, XRCID_D(id), type)

// wxWidgets provides fn_str(), but no mb_fn_str() or equiv.
#define mb_fn_str() mb_str(wxConvFile)

// by default, only 9 recent items
#define wxID_FILE10 (wxID_FILE9 + 1)

#endif /* WX_WXHEAD_H */

#include "viewsupt.h"
#include "wxvbam.h"
#include "wxutil.h"

namespace Viewers {
void Viewer::CloseDlg(wxCloseEvent& ev)
{
    (void)ev; // unused params
    // stop tracking dialog
    MainFrame* f = wxGetApp().frame;

    for (dialog_list_t::iterator i = f->popups.begin();
         i != f->popups.end(); ++i)
        if (*i == this) {
            f->popups.erase(i);
            break;
        }

    // just forwarding this event does not cause system to close window
    // ev.Skip();
    // so do it manually
    Destroy();
}

Viewer::Viewer(const wxString& name)
    : wxDialog()
    , auto_update(false)
{
    dname = name;
    MainFrame* f = wxGetApp().frame;

    // using LoadDialog precludes non-wxDialog nodes, which is good
    // since Viewer is strictly wxDialog-derived.
    if (!wxXmlResource::Get()->LoadDialog(this, f, name))
        baddialog();

    f->popups.push_back(this);
    SetEscapeId(wxID_CLOSE);
    Fit();
}

void Viewer::ActiveCtrl(wxCommandEvent& ev)
{
    wxWindow* ctrl = wxStaticCast(ev.GetEventObject(), wxWindow);
    // TransferDataFromWindow() only operates on children, not self
    // so just simulate
    wxValidator* v = ctrl->GetValidator();

    if (v) {
        v->TransferFromWindow();
        Update();
    }
}

BEGIN_EVENT_TABLE(Viewer, wxDialog)
EVT_CLOSE(Viewer::CloseDlg)
EVT_CHECKBOX(XRCID("AutoUpdate"), Viewer::SetAutoUpdate)
EVT_RADIOBUTTON(wxID_ANY, Viewer::ActiveCtrl)
EVT_SPINCTRL(wxID_ANY, Viewer::ActiveCtrlSpin)
// this does not interfere with/intercept disassemble scrolls
EVT_SCROLL(Viewer::ActiveCtrlScr)
END_EVENT_TABLE()
}

void MainFrame::UpdateViewers()
{
    for (dialog_list_t::iterator i = popups.begin(); i != popups.end(); ++i) {
        Viewers::Viewer* d = static_cast<Viewers::Viewer*>(*i);

        if (d->auto_update)
            d->Update();
    }
}

namespace Viewers {
IMPLEMENT_DYNAMIC_CLASS(DisList, wxPanel)

DisList::DisList()
    : wxPanel()
    , nlines(0)
    , topaddr(0)
    , tc()
    , sb()
    , didinit(false)
    , issel(false)
{
}

void DisList::Refit(int cols)
{
    if (!didinit) {
        tc.Create(this, wxID_ANY, wxEmptyString, wxPoint(0, 0), wxDefaultSize,
            wxTE_READONLY | wxTE_MULTILINE | wxTE_DONTWRAP | wxTE_NOHIDESEL);
        tc.Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(DisList::MouseEvent),
            NULL, this);
        tc.Connect(wxEVT_LEFT_UP, wxMouseEventHandler(DisList::MouseEvent),
            NULL, this);
        tc.Connect(wxEVT_MOTION, wxMouseEventHandler(DisList::MouseEvent),
            NULL, this);
        // FIXME: take tc out of tab order
        sb.Create(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxSB_VERTICAL);
        sb.SetScrollbar(0, 15, 500, 15);
        didinit = true;
    }

    wxString line(wxT('M'), cols);
    // GetBestSize() in wxGTK is pretty damn worthless.  It's surprising
    // autosizing works at all.  wxGTK does not give good values
    // for wxTextCtrl (always 80xht_of_1liner)
    // Given this, we'll fake it using wxStaticText for lineheight
    // and add an extra line for extraheight
    // Just using GetTextExtent for lineheight might not work if extra
    // spacing is usually added between lines.
    wxStaticText st(this, wxID_ANY, line);
    wxSize winsz = st.GetBestSize();
    st.SetLabel(line + wxT('\n') + line);
    wxSize winsz2 = st.GetBestSize();
    lineheight = winsz2.GetHeight() - winsz.GetHeight();
    extraheight = winsz.GetHeight() /* - lineheight */;
    winsz.SetHeight(extraheight + 15 * lineheight);
    winsz.SetWidth(winsz.GetWidth() + sb.GetBestSize().GetWidth());
    SetMinSize(winsz);
    SetSize(winsz);
}

void DisList::MoveSB()
{
    int pos;

    if (topaddr <= 100)
        pos = topaddr;
    else if (topaddr >= maxaddr - 100)
        pos = topaddr - maxaddr + 500;
    else if (topaddr < 1100)
        pos = (topaddr - 100) / 10 + 100;
    else if (topaddr >= maxaddr - 1100)
        pos = (topaddr - maxaddr + 1100) / 10 + 300;
    else // FIXME this pos is very likely wrong... but I cannot trigger it
        pos = 250;

    sb.SetScrollbar(pos, 20, 500, 20);
}

void DisList::MoveView(wxScrollEvent& ev)
{
    int pos = ev.GetPosition();

    if (pos < 100)
        topaddr = pos;
    else if (pos >= 400)
        topaddr = maxaddr + pos - 500;
    else if (ev.GetEventType() == wxEVT_SCROLL_LINEUP) {
        topaddr -= back_size;
        MoveSB();
    } else if (ev.GetEventType() == wxEVT_SCROLL_LINEDOWN) {
        topaddr = addrs[1];
        MoveSB();
    } else if (ev.GetEventType() == wxEVT_SCROLL_PAGEUP) {
        topaddr -= (nlines - 2) * back_size;
        MoveSB();
    } else if (ev.GetEventType() == wxEVT_SCROLL_PAGEDOWN) {
        topaddr = addrs[nlines - 2];
        MoveSB();
    } else if (ev.GetEventType() == wxEVT_SCROLL_THUMBRELEASE) {
        if (pos <= 200)
            topaddr = (pos - 100) * 10 + 100;
        else if (pos >= 300)
            topaddr = (pos - 300) * 10 + maxaddr - 1100;
        else
            // 200 .. 300 -> 1100 .. maxaddr - 1100
            topaddr = (pos - 200) * ((maxaddr - 2200) / 100) + 1100;

        MoveSB();
    } // ignore THUMBTRACK and CHANGED

    RefillNeeded();
}

// generate event; due to ProcessEvent()'s nature, it will return only
// when refill complete
void DisList::RefillNeeded()
{
    wxCommandEvent ev(EVT_REFILL_NEEDED, GetId());
    ev.SetEventObject(this);
    GetEventHandler()->ProcessEvent(ev);
}

// called by parent's refill handler or any other time strings have changed
void DisList::Refill()
{
    MoveSB();
    wxString val;

    for (size_t i = 0; i < (size_t)nlines && i < strings.size(); i++) {
        val += strings[i];
        val += wxT('\n');
    }

    tc.SetValue(val);
    SetSel();
}

// on resize, recompute shown lines and refill if necessary
void DisList::Resize(wxSizeEvent& ev)
{
    (void)ev; // unused params
    if (!didinit) // prevent crash on win32
        return;

    wxSize sz = GetSize();
    int sbw = sb.GetSize().GetWidth();
    sz.SetWidth(sz.GetWidth() - sbw);
    sb.Move(sz.GetWidth(), 0);
    sb.SetSize(sbw, sz.GetHeight());
    nlines = (sz.GetHeight() + lineheight - 1) / lineheight;
    wxString val;
    tc.SetSize(sz.GetWidth(), (nlines + 1) * lineheight + extraheight);

    if ((size_t)nlines > strings.size())
        RefillNeeded();
    else {
        for (size_t i = 0; i < (size_t)nlines && i < strings.size(); i++) {
            val += strings[i];
            val += wxT('\n');
        }

        tc.SetValue(val);
        SetSel();
    }
}

// highlight selected line, if visible
void DisList::SetSel()
{
    tc.SetSelection(0, 0);

    if (!issel)
        return;

    if ((size_t)nlines > addrs.size() || (uint32_t)addrs[0] > seladdr || (uint32_t)addrs[nlines - 1] <= seladdr)
        return;

    for (int i = 0, start = 0; i < nlines; i++) {
        if ((uint32_t)addrs[i + 1] > seladdr) {
            int end = start + strings[i].size() + 1;
// on win32, wx helpfully inserts a CR before every LF
// it also doesn't highlight the whole line, but that's
// not critical
#ifdef __WXMSW__
            start += i;
            end += i + 1;
#endif
            tc.SetSelection(start, end);
            return;
        }

        start += strings[i].size() + 1;
    }
}

void DisList::SetSel(uint32_t addr)
{
    seladdr = addr;
    issel = true;

    if (addrs.size() < 4 || addrs.size() < (size_t)nlines || topaddr > addr || (uint32_t)addrs[addrs.size() - 4] < addr) {
        topaddr = addr;
        strings.clear();
        addrs.clear();
        RefillNeeded();
    } else
        SetSel();
}

BEGIN_EVENT_TABLE(DisList, wxPanel)
EVT_SIZE(DisList::Resize)
EVT_SCROLL(DisList::MoveView)
END_EVENT_TABLE()

DEFINE_EVENT_TYPE(EVT_REFILL_NEEDED)

IMPLEMENT_DYNAMIC_CLASS(MemView, wxPanel)

MemView::MemView()
    : wxPanel()
    , nlines(0)
    , topaddr(0)
    , addrlab(0)
    , disp()
    , sb()
    , didinit(false)
    , selnib(-1)
{
}

void MemView::Refit()
{
    addrlen = maxaddr > 0xffff ? 8 : 4;

    if (!didinit) {
        disp.Create(this, wxID_ANY, wxPoint(0, 0), wxDefaultSize,
            wxBORDER_NONE | wxWANTS_CHARS | wxTAB_TRAVERSAL);
        disp.Connect(wxEVT_PAINT, wxPaintEventHandler(MemView::RepaintEv),
            NULL, this);
        disp.Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(MemView::MouseEvent),
            NULL, this);
        disp.Connect(wxEVT_MOTION, wxMouseEventHandler(MemView::MouseEvent),
            NULL, this);
        disp.Connect(wxEVT_LEFT_UP, wxMouseEventHandler(MemView::MouseEvent),
            NULL, this);
        disp.Connect(wxEVT_CHAR, wxKeyEventHandler(MemView::KeyEvent),
            NULL, this);
        disp.SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
        sb.Create(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxSB_VERTICAL);
        sb.SetScrollbar(0, 15, 500, 15);
    }

    wxClientDC dc(&disp);
    // doesn't seem to inherit font properly
    dc.SetFont(GetFont());
    dc.GetTextExtent(wxT('M'), &charwidth, &charheight);

    if (!didinit) {
        // using 2-phase init doesn't seem to work
        caret = new wxCaret(&disp, charwidth, charheight);
        caret->Hide();
        disp.SetCaret(caret);
        didinit = true;
    }

    wxSize sz(charwidth * (69 + addrlen) + sb.GetBestSize().GetWidth(),
        charheight * 15);
    sz = sz + GetSize() - GetClientSize();
    SetMinSize(sz);
    SetSize(sz);
}

void MemView::MouseEvent(wxMouseEvent& ev)
{
    if (ev.GetEventType() == wxEVT_MOTION && !ev.LeftIsDown())
        return;

    int x = ev.GetX() / charwidth, y = ev.GetY() / charheight;
    x -= addrlen + 3;

    if (x < 0 || y < 0 || y > nlines)
        return;

    int word, nib;
    int nnib = 2 << fmt, nword = 16 >> fmt;
    int preasc = (nnib + 1) * nword + 2;
    isasc = x >= preasc;

    if (isasc) {
        word = (x - preasc) * 2 / nnib;
        nib = (x - preasc) * 2 % nnib;
    } else {
        word = x / (nnib + 1);
        nib = x % (nnib + 1);
        nib = nnib - nib - 1;
    }

    if (nib < 0 || word >= nword)
        return;

    seladdr = topaddr + y * 16;
    selnib = word * nnib + nib;
    Show(seladdr);
}

void MemView::ShowCaret()
{
    if (seladdr < (int)topaddr || seladdr >= (int)topaddr + nlines * 16)
        selnib = -1;

    if (selnib < 0) {
        while (caret->IsVisible())
            caret->Hide();

        if (addrlab)
            addrlab->SetLabel(wxEmptyString);

        return;
    }

    if (addrlab) {
        wxString lab;
        uint32_t addr = seladdr + selnib / 2;

        if (!isasc)
            addr &= ~((1 << fmt) - 1);

        lab.Printf(addrlen == 8 ? wxT("0x%08X") : wxT("0x%04X"), addr);
        addrlab->SetLabel(lab);
    }

    int y = (seladdr - topaddr) / 16;
    int x = addrlen + 3;
    int nnib = 2 << fmt, nword = 16 >> fmt;

    if (isasc)
        x += (nnib + 1) * nword + 2 + selnib / 2;
    else
        x += (nnib + 1) * (selnib / nnib) + nnib - selnib % nnib - 1;

    caret->Move(x * charwidth, y * charheight);

    while (!caret->IsVisible())
        caret->Show();

    disp.SetFocus();
}

void MemView::KeyEvent(wxKeyEvent& ev)
{
    uint32_t k = getKeyboardKeyCode(ev);
    int nnib = 2 << fmt;

    switch (k) {
    case WXK_RIGHT:
    case WXK_NUMPAD_RIGHT:
        if (isasc)
            selnib += 2;
        else if (ev.GetModifiers() == wxMOD_SHIFT)
            selnib += 2 << fmt;
        else if (!(selnib % nnib))
            selnib += nnib + nnib - 1;
        else
            selnib--;

        if (selnib >= 32) {
            if (seladdr == (int)maxaddr - 16)
                selnib = 32 - nnib;
            else {
                selnib -= 32;
                seladdr += 16;
            }
        }

        break;

    case WXK_LEFT:
    case WXK_NUMPAD_LEFT:
        if (isasc)
            selnib -= 2;
        else if (ev.GetModifiers() == wxMOD_SHIFT)
            selnib -= 2 << fmt;
        else if (!(++selnib % nnib))
            selnib -= nnib * 2;

        if (selnib < 0) {
            if (!seladdr)
                selnib = nnib - 1;
            else {
                selnib += 32;
                seladdr -= 16;
            }
        }

        break;

    case WXK_DOWN:
    case WXK_NUMPAD_DOWN:
        if (seladdr < (int)maxaddr - 16)
            seladdr += 16;

        break;

    case WXK_UP:
    case WXK_NUMPAD_UP:
        if (seladdr > 0)
            seladdr -= 16;

        break;

    default:
        if (k > 0x7f || (isasc && !isprint(k)) || (!isasc && !isxdigit(k))) {
            ev.Skip();
            return;
        }

        // location in data array
        int wno = (seladdr - topaddr) / 4 + selnib / 8;
        int bno = (selnib % 8) / 2;
        int nibno = selnib % 2;

        // now that selnib/seladdr isn't needed any more, advance pointer
        if (isasc)
            selnib += 2;
        else if (!(selnib % nnib))
            selnib += nnib + nnib - 1;
        else
            selnib--;

        if (selnib >= 32) {
            if (seladdr == (int)maxaddr - 16)
                selnib = 32 - nnib;
            else {
                selnib -= 32;
                seladdr += 16;
            }
        }

        uint32_t mask, val;

        if (isasc) {
            mask = 0xff << bno * 8;
            val = k << bno * 8;
        } else {
            mask = 8 * (0xf << bno) + 4 * nibno;
            val = isdigit(k) ? k - '0' : tolower(k) + 10 - 'a';
            val <<= bno * 8 + nibno * 4;
        }

        if ((words[wno] & mask) == val)
            break;

        words[wno] = ((words[wno] & ~mask) | val);
        writeaddr = topaddr + 4 * wno;
        val = words[wno];

        switch (fmt) {
        case 0:
            writeval = (val >> bno * 8) & 0xff;
            writeaddr += bno;
            break;

        case 1:
            writeval = (val >> (bno / 2) * 16) & 0xffff;
            writeaddr += bno & ~1;
            break;

        case 2:
            writeval = val;
            break;
        }

        // write value; this will not return until value has been written
        wxCommandEvent ev(EVT_WRITEVAL, GetId());
        ev.SetEventObject(this);
        GetEventHandler()->ProcessEvent(ev);
        // now refresh whole screen.  Really need to make this more
        // efficient some day
        Repaint();
    }

    Show(seladdr);
}

void MemView::MoveSB()
{
    int pos;

    if (topaddr / 16 <= 100) // <= 100
        pos = topaddr / 16;
    else if (topaddr / 16 >= maxaddr / 16 - 100) // >= 400
        pos = topaddr / 16 - maxaddr / 16 + 500;
    else if (topaddr / 16 < 1100) // <= 200
        pos = (topaddr / 16 - 100) / 10 + 100;
    else if (topaddr / 16 >= maxaddr / 16 - 1100) // >= 300
        pos = (topaddr / 16 - maxaddr / 16 + 1100) / 10 + 300;
    else // > 200 && < 300
        pos = ((topaddr / 16) - 1100) / (((maxaddr / 16) - 2200) / 100) + 200;

    sb.SetScrollbar(pos, 20, 500, 20);
}

void MemView::MoveView(wxScrollEvent& ev)
{
    int pos = ev.GetPosition();

    if (pos < 100)
        topaddr = pos * 16;
    else if (pos >= 400)
        topaddr = maxaddr + (pos - 500) * 16;
    else if (ev.GetEventType() == wxEVT_SCROLL_LINEUP) {
        topaddr -= 16;
        MoveSB();
    } else if (ev.GetEventType() == wxEVT_SCROLL_LINEDOWN) {
        topaddr += 16;
        MoveSB();
    } else if (ev.GetEventType() == wxEVT_SCROLL_PAGEUP) {
        topaddr -= (nlines - 2) * 16;
        MoveSB();
    } else if (ev.GetEventType() == wxEVT_SCROLL_PAGEDOWN) {
        topaddr += (nlines - 2) * 16;
        MoveSB();
    } else if (ev.GetEventType() == wxEVT_SCROLL_THUMBRELEASE) {
        if (pos <= 200)
            topaddr = ((pos - 100) * 10 + 100) * 16;
        else if (pos >= 300)
            topaddr = ((pos - 300) * 10 - 1100) * 16 + maxaddr;
        else
            topaddr = ((pos - 200) * ((maxaddr / 16 - 2200) / 100) + 1100) * 16;

        MoveSB();
    } // ignore THUMBTRACK and CHANGED
    // do not interrupt scroll because no event was triggered
    else if (pos <= 200)
        topaddr = ((pos - 100) * 10 + 100) * 16;
    else if (pos >= 300)
        topaddr = ((pos - 300) * 10 - 1100) * 16 + maxaddr;
    else if (pos > 200 && pos < 300)
        topaddr = ((pos - 200) * ((maxaddr / 16 - 2200) / 100) + 1100) * 16;

    RefillNeeded();
}

// generate event; due to ProcessEvent()'s nature, it will return only
// when refill complete
void MemView::RefillNeeded()
{
    wxCommandEvent ev(EVT_REFILL_NEEDED, GetId());
    ev.SetEventObject(this);
    GetEventHandler()->ProcessEvent(ev);
}

// FIXME:  following 3 repainters do not work as intended: there is
// more flickering than necessary and yet still garbage on screen sometimes
// (probably in part from wxCaret)

// called by parent's refill handler or any other time strings have changed
void MemView::Refill()
{
    MoveSB();
    wxClientDC dc(&disp);
    dc.Clear();
    Refill(dc);
    ShowCaret();
}

void MemView::Repaint()
{
    wxClientDC dc(&disp);
    dc.SetBackgroundMode(wxSOLID);
    Refill(dc);
}

void MemView::RepaintEv(wxPaintEvent& ev)
{
    (void)ev; // unused params
    wxPaintDC dc(&disp);
    dc.SetBackgroundMode(wxSOLID);
    Refill(dc);
    ShowCaret();
}

void MemView::Refill(wxDC& dc)
{
    // don't want caret drawing at same time due to timer event
    wxCaretSuspend cs(&disp);
    // doesn't seem to inherit font properly
    dc.SetFont(GetFont());

    for (size_t i = 0; i < (size_t)nlines && i < words.size() / 4; i++) {
        wxString line, word;
        line.Printf(maxaddr > 0xffff ? wxT("%08X   ") : wxT("%04X   "), topaddr + (int)i * 16);

        for (int j = 0; j < 4; j++) {
            uint32_t v = words[i * 4 + j];

            switch (fmt) {
            case 0:
                word.Printf(wxT("%02X %02X %02X %02X "),
                    v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff,
                    (v >> 24) & 0xff);
                break;

            case 1:
                word.Printf(wxT("%04X %04X "), v & 0xffff, (v >> 16) & 0xffff);
                break;

            case 2:
                word.Printf(wxT("%08X "), v);
                break;
            }

            line.append(word);
        }

        line.append(wxT("  "));

        for (int j = 0; j < 4; j++) {
            uint32_t v = words[i * 4 + j];
#define appendc(c) line.append(isascii((c)&0xff) && isprint((c)&0xff) ? (wxChar)((c)&0xff) : wxT('.'))
            appendc(v);
            appendc(v >> 8);
            appendc(v >> 16);
            appendc(v >> 24);
        }

        dc.DrawText(line, 0, i * charheight);
    }

    int lloc = charwidth * ((addrlen + 1) * 2 + 1) / 2;
    dc.DrawLine(lloc, 0, lloc, nlines * charheight);
    lloc = charwidth * (2 * (addrlen + 3 + 32 + 4 + (fmt == 0 ? 3 * 4 : fmt == 1 ? 4 : 0)) + 1) / 2;
    dc.DrawLine(lloc, 0, lloc, nlines * charheight);
}

// on resize, recompute shown lines and refill if necessary
void MemView::Resize(wxSizeEvent& ev)
{
    (void)ev; // unused params
    if (!didinit) // prevent crash on win32
        return;

    wxSize sz = GetClientSize();
    int sbw = sb.GetSize().GetWidth();
    sz.SetWidth(sz.GetWidth() - sbw);
    sb.Move(sz.GetWidth(), 0);
    sb.SetSize(sbw, sz.GetHeight());
    nlines = (sz.GetHeight() + charheight - 1) / charheight;
    wxString val;
    disp.SetSize(sz.GetWidth(), (nlines + 1) * charheight);

    if ((size_t)nlines > words.size() / 4) {
        if (topaddr + nlines * 16 > maxaddr)
            topaddr = maxaddr - nlines * 16 + 1;

        RefillNeeded();
    } else
        Refill();
}

void MemView::ShowAddr(uint32_t addr, bool force_update)
{
    if (addr < topaddr || addr >= topaddr + (nlines - 1) * 16) {
        // align to nearest 16-byte block
        // note that mfc interface only aligns to nearest (1<<fmt)-byte
        uint32_t newtopaddr = addr & ~0xf;

        if (newtopaddr + nlines * 16 > maxaddr)
            newtopaddr = maxaddr - nlines * 16 + 1;

        force_update = newtopaddr != topaddr;
        topaddr = newtopaddr;
    }

    if (force_update) {
        words.clear();
        RefillNeeded();
    } else
        ShowCaret();
}

uint32_t MemView::GetAddr()
{
    if (selnib < 0)
        return topaddr;
    else
        return seladdr + (selnib / 2 & ~((1 << fmt) - 1));
}

BEGIN_EVENT_TABLE(MemView, wxPanel)
EVT_SIZE(MemView::Resize)
EVT_SCROLL(MemView::MoveView)
END_EVENT_TABLE()

DEFINE_EVENT_TYPE(EVT_WRITEVAL)

ColorView::ColorView(wxWindow* parent, wxWindowID id)
    // default for MSW appears to be BORDER_SUNKEN
    : wxControl(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
      r(0),
      g(0),
      b(0)
{
    wxBoxSizer* sz = new wxBoxSizer(wxHORIZONTAL);
    cp = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(75, 75),
        wxBORDER_SUNKEN);
    sz->Add(cp);
    wxGridSizer* gs = new wxGridSizer(2);
    wxStaticText* lab = new wxStaticText(this, wxID_ANY, _("R:"));
    gs->Add(lab, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    rt = new wxStaticText(this, wxID_ANY, wxT("255"), wxDefaultPosition,
#if !defined(__WXGTK__)
        wxDefaultSize, wxST_NO_AUTORESIZE);
#else
        wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
#endif // !defined(__WXGTK__)
    gs->Add(rt, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    lab = new wxStaticText(this, wxID_ANY, _("G:"));
    gs->Add(lab, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    gt = new wxStaticText(this, wxID_ANY, wxT("255"), wxDefaultPosition,
#if !defined(__WXGTK__)
        wxDefaultSize, wxST_NO_AUTORESIZE);
#else
        wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
#endif // !defined(__WXGTK__)
    gs->Add(gt, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    lab = new wxStaticText(this, wxID_ANY, _("B:"));
    gs->Add(lab, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    bt = new wxStaticText(this, wxID_ANY, wxT("255"), wxDefaultPosition,
#if !defined(__WXGTK__)
        wxDefaultSize, wxST_NO_AUTORESIZE);
#else
        wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
#endif // !defined(__WXGTK__)
    gs->Add(bt, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    sz->Add(gs);
    sz->Layout();
    SetSizerAndFit(sz);
    SetRGB(-1, -1, -1);
}

void ColorView::SetRGB(int r, int g, int b)
{
    if (r == -1 || g == -1 || b == -1) {
        cp->SetBackgroundColour(wxNullColour);
        rt->SetLabel(wxT(""));
        gt->SetLabel(wxT(""));
        bt->SetLabel(wxT(""));
        return;
    }

    cp->SetBackgroundColour(wxColour(r, g, b));
    wxString s;
    // FIXME: make shift an option; currently hard-coded to rgb555
    s.Printf(wxT("%d"), r >> 3);
    rt->SetLabel(s);
    s.Printf(wxT("%d"), g >> 3);
    gt->SetLabel(s);
    s.Printf(wxT("%d"), b >> 3);
    bt->SetLabel(s);
}

IMPLEMENT_DYNAMIC_CLASS(PixView, wxPanel);

BEGIN_EVENT_TABLE(PixView, wxPanel)
EVT_PAINT(PixView::Redraw)
EVT_LEFT_UP(PixView::SelPoint)
END_EVENT_TABLE()

bool PixView::InitBMP(int w, int h, ColorView* cv)
{
    im = wxImage(w, h);
    bm = new wxBitmap(im);

    if (!bm)
        return false;

    selx = sely = -1;
    cview = cv;
    return true;
}

void PixView::SetData(const unsigned char* data, int stride, int x, int y)
{
    if (!bm)
        return;

    ox = x;
    oy = y;

    if (!data) {
        im.SetRGB(wxRect(0, 0, im.GetWidth(), im.GetHeight()), 0, 0, 0);
        selx = sely = -1;
    } else {
        data += (y * stride + x) * 3;

        for (y = 0; y < im.GetHeight(); y++) {
            for (x = 0; x < im.GetWidth(); x++) {
                im.SetRGB(x, y, *data, data[1], data[2]);
                data += 3;
            }

            data += 3 * (stride - x);
        }
    }

    delete bm;
    bm = new wxBitmap(im);

    if (selx >= 0 && cview)
        cview->SetRGB(im.GetRed(selx, sely), im.GetGreen(selx, sely),
            im.GetBlue(selx, sely));
    else if (cview)
        cview->SetRGB(-1, -1, -1);

    Refresh();
}

void PixView::SetSel(int x, int y, bool desel_cview_update)
{
    if (x >= ox && y >= oy && x - ox < im.GetWidth() && y - oy < im.GetHeight()) {
        int oselx = selx, osely = sely;
        selx = x - ox;
        sely = y - oy;

        if (selx != oselx || sely != osely) {
            if (cview)
                cview->SetRGB(im.GetRed(selx, sely), im.GetGreen(selx, sely),
                    im.GetBlue(selx, sely));

            Refresh();
        }
    } else {
        bool r = selx >= 0 && sely >= 0;
        selx = sely = -1;

        if (r) {
            if (cview && desel_cview_update)
                cview->SetRGB(-1, -1, -1);

            Refresh();
        }
    }
}

void PixView::Redraw(wxPaintEvent& ev)
{
    (void)ev; // unused params
    if (!bm)
        return;

    wxPaintDC dc(this);
    double sx, sy;
    int w, h;
    GetClientSize(&w, &h);
    sx = (double)w / im.GetWidth();
    sy = (double)h / im.GetHeight();
    dc.SetUserScale(sx, sy);
    dc.DrawBitmap(*bm, 0, 0, false);
    dc.SetUserScale(sx / 8, sy / 8);
    // grid color is hard-coded to gray
    dc.SetPen(*wxGREY_PEN);

    // grid is only on top/left, just like win32 port
    for (int y = 0; y < im.GetHeight(); y++)
        dc.DrawLine(0, y * 8, im.GetWidth() * 8, y * 8);

    for (int x = 0; x < im.GetWidth(); x++)
        dc.DrawLine(x * 8, 0, x * 8, im.GetHeight() * 8);

    if (selx >= 0) {
        // sel color is hard-coded to red
        dc.SetPen(*wxRED_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(selx * 8, sely * 8, 8, 8);
    }
}

void PixView::SelPoint(wxMouseEvent& ev)
{
    if (!bm)
        return;

    int w, h;
    GetClientSize(&w, &h);
    wxPoint p = ev.GetPosition();

    if (p.x < 0 || p.x >= w || p.y < 0 || p.y >= h) {
        bool r = selx >= 0 && sely >= 0;
        selx = sely = -1;

        if (r) {
            Refresh();

            if (cview)
                cview->SetRGB(-1, -1, -1);
        }

        return;
    }

    int oselx = selx, osely = sely;
    selx = p.x * im.GetWidth() / w;
    sely = p.y * im.GetHeight() / h;

    if (selx != oselx || sely != osely) {
        Refresh();

        if (cview)
            cview->SetRGB(im.GetRed(selx, sely), im.GetGreen(selx, sely),
                im.GetBlue(selx, sely));
    }
}

IMPLEMENT_DYNAMIC_CLASS(PixViewEvt, PixView)
void PixViewEvt::SetData(const unsigned char* data, int stride, int x,
    int y)
{
    PixView::SetData(data, stride, x, y);

    if (selx >= 0 && sely >= 0)
        click();
}

void PixViewEvt::SelPoint(wxMouseEvent& ev)
{
    PixView::SelPoint(ev);
    click();
}

void PixViewEvt::click()
{
    wxMouseEvent ev;
    ev.SetEventType(EVT_COMMAND_GFX_CLICK);
    ev.ResumePropagation(wxEVENT_PROPAGATE_MAX);
    ev.SetEventObject(this);
    ev.SetId(GetId());
    //ev.SetX(selx);
    ev.m_x = selx;
    //ev.SetY(sely);
    ev.m_y = sely;
    GetEventHandler()->ProcessEvent(ev);
}

IMPLEMENT_DYNAMIC_CLASS(GfxPanel, wxPanel)
void GfxPanel::DrawBitmap(wxPaintEvent& ev)
{
    (void)ev; // unused params
    if (!bm)
        return;

    wxPaintDC dc(this);
    wxSize sz = GetClientSize();
    double scalex = (double)sz.GetWidth() / bmw;
    double scaley = (double)sz.GetHeight() / bmh;
    dc.SetUserScale(scalex, scaley);
    dc.DrawBitmap(*bm, 0, 0);

    if (selx >= 0) {
        if (selx > bmw - 4 || sely > bmh - 4)
            pv->SetData(NULL, 0, 0, 0);
        else
            pv->SetData(im->GetData(), im->GetWidth(), selx - 4, sely - 4);
    }
}

void GfxPanel::Click(wxMouseEvent& ev)
{
    DoSel(ev, ev.GetEventType() == wxEVT_LEFT_DOWN);
}

void GfxPanel::MouseMove(wxMouseEvent& ev)
{
    if (!ev.LeftIsDown()) {
        ev.Skip();
        return;
    }

    DoSel(ev);
}

void GfxPanel::DoSel(wxMouseEvent& ev, bool force)
{
    if (!bm)
        return;

    int x = ev.GetX(), y = ev.GetY();

    if (x < 0 || y < 0)
        return;

    wxSize sz = GetClientSize();

    if (x > sz.GetWidth() || y > sz.GetHeight())
        return;

    x = x * bmw / sz.GetWidth();
    y = y * bmh / sz.GetHeight();
    wxMouseEvent ev2(ev);
    ev2.SetEventType(EVT_COMMAND_GFX_CLICK);
    ev2.ResumePropagation(wxEVENT_PROPAGATE_MAX);
    ev2.SetEventObject(this);
    ev2.SetId(GetId());
    //ev2.SetX(x);
    ev2.m_x = x;
    //ev2.SetY(y);
    ev2.m_y = y;
    GetEventHandler()->ProcessEvent(ev2);

    if (x < 4)
        x = 4;
    else if (x > bmw - 4)
        x = bmw - 4;

    if (y < 4)
        y = 4;
    else if (y > bmh - 4)
        y = bmh - 4;

    if (force || selx != x || sely != y) {
        selx = x;
        sely = y;
        pv->SetData(im->GetData(), im->GetWidth(), selx - 4, sely - 4);
    }
}

DEFINE_EVENT_TYPE(EVT_COMMAND_GFX_CLICK)

BEGIN_EVENT_TABLE(GfxPanel, wxPanel)
EVT_PAINT(GfxPanel::DrawBitmap)
EVT_LEFT_DOWN(GfxPanel::Click)
EVT_LEFT_UP(GfxPanel::Click)
EVT_MOTION(GfxPanel::MouseMove)
END_EVENT_TABLE()

GfxViewer::GfxViewer(const wxString& dname, int maxw, int maxh)
    : Viewer(dname)
    , image(maxw, maxh)
{
    gv = XRCCTRL(*this, "GfxView", GfxPanel);

    if (!gv || !(gvs = dynamic_cast<wxScrolledWindow*>(gv->GetParent())))
        baddialog();

    gvs->SetMinSize(gvs->GetSize());
    gvs->SetScrollRate(1, 1);
    gv->SetSize(maxw, maxh);
    gvs->SetVirtualSize(maxw, maxh);
    gv->im = &image;
    gv->bmw = maxw;
    gv->bmh = maxh;
    ColorView* cv;
    colorctrl(cv, "Color");
    pixview(gv->pv, "Zoom", 8, 8, cv);
    str = XRCCTRL(*this, "Stretch", wxCheckBox);

    if (!str)
        baddialog();
}

void GfxViewer::ChangeBMP()
{
    delete gv->bm;
    gv->bm = new wxBitmap(image);
    gv->Refresh();
}

void GfxViewer::BMPSize(int w, int h)
{
    if (gv->bmw != w || gv->bmh != h) {
        gv->bmw = w;
        gv->bmh = h;

        if (!str->GetValue()) {
            gv->SetSize(w, h);
            gvs->SetVirtualSize(gv->GetSize());
        }
    }
}

void GfxViewer::StretchTog(wxCommandEvent& ev)
{
    (void)ev; // unused params
    wxSize sz;

    if (str->GetValue()) {
        // first time to remove scrollbars
        gvs->SetVirtualSize(gvs->GetClientSize());
        // second time to expand to outer edges
        sz = gvs->GetClientSize();
    } else
        sz = wxSize(gv->bmw, gv->bmh);

    gv->SetSize(sz);
    gvs->SetVirtualSize(sz);
}

void GfxViewer::SaveBMP(wxCommandEvent& ev)
{
    (void)ev; // unused params
    GameArea* panel = wxGetApp().frame->GetPanel();
    bmp_save_dir = wxGetApp().frame->GetGamePath(gopts.scrshot_dir);
    // no attempt is made here to translate the dialog type name
    // it's just a suggested name, anyway
    wxString def_name = panel->game_name() + wxT('-') + dname;
    def_name.resize(def_name.size() - 6); // strlen("Viewer")

    if (captureFormat)
        def_name += wxT(".bmp");
    else
        def_name += wxT(".png");

    wxFileDialog dlg(GetGrandParent(), _("Select output file"), bmp_save_dir, def_name,
        _("PNG images|*.png|BMP images|*.bmp"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    dlg.SetFilterIndex(captureFormat);
    int ret = dlg.ShowModal();
    bmp_save_dir = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxBitmap obmp = gv->bm->GetSubBitmap(wxRect(0, 0, gv->bmw, gv->bmh));
    wxString fn = dlg.GetPath();
    wxBitmapType fmt = dlg.GetFilterIndex() ? wxBITMAP_TYPE_BMP : wxBITMAP_TYPE_PNG;

    if (fn.size() > 4) {
        if (wxString(fn.substr(fn.size() - 4)).IsSameAs(wxT(".bmp"), false))
            fmt = wxBITMAP_TYPE_BMP;
        else if (wxString(fn.substr(fn.size() - 4)).IsSameAs(wxT(".png"), false))
            fmt = wxBITMAP_TYPE_PNG;
    }

    obmp.SaveFile(fn, fmt);
}

void GfxViewer::RefreshEv(wxCommandEvent& ev)
{
    (void)ev; // unused params
    Update();
}

wxString GfxViewer::bmp_save_dir = wxEmptyString;

BEGIN_EVENT_TABLE(GfxViewer, Viewer)
EVT_CHECKBOX(XRCID("Stretch"), GfxViewer::StretchTog)
EVT_BUTTON(XRCID("Refresh"), GfxViewer::RefreshEv)
EVT_BUTTON(XRCID("Save"), GfxViewer::SaveBMP)
EVT_BUTTON(XRCID("SaveGBOAM"), GfxViewer::SaveBMP)
EVT_BUTTON(XRCID("SaveGBAOAM"), GfxViewer::SaveBMP)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(DispCheckBox, wxCheckBox)

BEGIN_EVENT_TABLE(DispCheckBox, wxCheckBox)
EVT_LEFT_DOWN(DispCheckBox::MouseEvent)
EVT_LEFT_UP(DispCheckBox::MouseEvent)
END_EVENT_TABLE()
}

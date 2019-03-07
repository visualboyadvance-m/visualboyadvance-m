
// initialize menus & dialogs, etc.
// for most of the prefs dialogs, all code resides here in the form of
// event handlers & validators
// other non-viewer dialogs are at least validated enough that they won't crash
// viewer dialogs are not commonly used, so they are initialized on demand

#include "wxvbam.h"

#include <algorithm>
#include <stdexcept>
#include <typeinfo>

#include <wx/checkedlistctrl.h>
#include <wx/clrpicker.h>
#include <wx/dir.h>
#include <wx/filepicker.h>
#include <wx/progdlg.h>
#include <wx/spinctrl.h>
#include <wx/valnum.h>
#include <wx/stockitem.h>
#include <wx/tokenzr.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

#include "../common/ConfigManager.h"
#include "../gba/CheatSearch.h"

// The program icon, in case it's missing from .xrc (MSW gets it from .rc file)
#if !defined(__WXMSW__) && !defined(__WXPM__)
// ImageMagick makes the name wxvbam, but wx expects wxvbam_xpm
#define wxvbam wxvbam_xpm
const
#include "xrc/vbam.xpm"
#undef wxvbam
#endif

    // this is supposed to happen automatically if a parent is marked recursive
    // but some dialogs don't do it (propertydialog?)
    // so go ahead and mark all dialogs for fully recursive validation
    static void
    mark_recursive(wxWindowBase* w)
{
    w->SetExtraStyle(w->GetExtraStyle() | wxWS_EX_VALIDATE_RECURSIVELY);
    wxWindowList l = w->GetChildren();

    for (wxWindowList::iterator ch = l.begin(); ch != l.end(); ++ch)
        mark_recursive(*ch);
}

#if (wxMAJOR_VERSION < 3)
#define GetXRCDialog(n) \
    wxStaticCast(wxGetApp().frame->FindWindow(XRCID(n)), wxDialog)
#else
#define GetXRCDialog(n) \
    wxStaticCast(wxGetApp().frame->FindWindowByName(n), wxDialog)
#endif

// Event handlers must be methods of wxEvtHandler-derived objects

// manage the network link dialog
#ifndef NO_LINK
static class NetLink_t : public wxEvtHandler {
public:
    wxDialog* dlg;
    int n_players;
    bool server;
    NetLink_t()
        : n_players(2)
        , server(false)
    {
    }
    wxButton* okb;
    void ServerOKButton(wxCommandEvent& ev)
    {
        okb->SetLabel(_("Start!"));
    }
    void ClientOKButton(wxCommandEvent& ev)
    {
        okb->SetLabel(_("Connect"));
    }
    // attached to OK, so skip when OK
    void NetConnect(wxCommandEvent& ev)
    {
        static const int length = 256;

        if (!dlg->Validate() || !dlg->TransferDataFromWindow())
            return;

        if (!server) {
            bool valid = SetLinkServerHost(gopts.link_host.mb_str());

            if (!valid) {
                wxMessageBox(_("You must enter a valid host name"),
                    _("Host name invalid"), wxICON_ERROR | wxOK);
                return;
            }
        }

        linkNumPlayers = n_players;
        update_opts(); // save fast flag and client host
        // Close any previous link
        CloseLink();
        wxString connmsg;
        wxString title;
        SetLinkTimeout(linkTimeout);
        EnableSpeedHacks(linkHacks);
        EnableLinkServer(server, linkNumPlayers - 1);

        if (server) {
            char host[length];
            GetLinkServerHost(host, length);
            title.Printf(_("Waiting for clients..."));
            connmsg.Printf(_("Server IP address is: %s\n"), wxString(host, wxConvLibc).mb_str());
        } else {
            title.Printf(_("Waiting for connection..."));
            connmsg.Printf(_("Connecting to %s\n"), gopts.link_host.mb_str());
        }

        // Init link
        MainFrame* mf = wxGetApp().frame;
        ConnectionState state = InitLink(mf->GetConfiguredLinkMode());

        // Display a progress dialog while the connection is establishing
        if (state == LINK_NEEDS_UPDATE) {
            wxProgressDialog pdlg(title, connmsg,
                100, dlg, wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME);

            while (state == LINK_NEEDS_UPDATE) {
                // Ask the core for updates
                char message[length];
                state = ConnectLinkUpdate(message, length);
                connmsg = wxString(message, wxConvLibc);

                // Does the user want to abort?
                if (!pdlg.Pulse(connmsg)) {
                    state = LINK_ABORT;
                }
            }
        }

        // The user canceled the connection attempt
        if (state == LINK_ABORT) {
            CloseLink();
        }

        // Something failed during init
        if (state == LINK_ERROR) {
            CloseLink();
            wxLogError(_("Error occurred.\nPlease try again."));
        }

        if (GetLinkMode() != LINK_DISCONNECTED) {
            connmsg.Replace(wxT("\n"), wxT(" "));
            systemScreenMessage(connmsg);
            ev.Skip(); // all OK
        }
    }
} net_link_handler;
#endif

// manage the cheat list dialog
static class CheatList_t : public wxEvtHandler {
public:
    wxDialog* dlg;
    wxCheckedListCtrl* list;
    wxListItem item0, item1;
    int col1minw;
    wxString cheatdir, cheatfn, deffn;
    bool isgb;
    bool* dirty;

    // add/edit dialog
    wxString ce_desc;
    wxString ce_codes;
    wxChoice* ce_type_ch;
    wxControl* ce_codes_tc;
    int ce_type;

    void Reload()
    {
        list->DeleteAllItems();
        Reload(0);
    }

    void Reload(int start)
    {
        if (isgb) {
            for (int i = start; i < gbCheatNumber; i++) {
                item0.SetId(i);
                item0.SetText(wxString(gbCheatList[i].cheatCode, wxConvLibc));
                list->InsertItem(item0);
                item1.SetId(i);
                item1.SetText(wxString(gbCheatList[i].cheatDesc, wxConvUTF8));
                list->SetItem(item1);
                list->Check(i, gbCheatList[i].enabled);
            }
        } else {
            for (int i = start; i < cheatsNumber; i++) {
                item0.SetId(i);
                item0.SetText(wxString(cheatsList[i].codestring, wxConvLibc));
                list->InsertItem(item0);
                item1.SetId(i);
                item1.SetText(wxString(cheatsList[i].desc, wxConvUTF8));
                list->SetItem(item1);
                list->Check(i, cheatsList[i].enabled);
            }
        }

        AdjustDescWidth();
    }

    void Tool(wxCommandEvent& ev)
    {
        switch (ev.GetId()) {
        case wxID_OPEN: {
            wxFileDialog subdlg(dlg, _("Select cheat file"), cheatdir, cheatfn,
                _("VBA cheat lists (*.clt)|*.clt|CHT cheat lists (*.cht)|*.cht"),
                wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            int ret = subdlg.ShowModal();
            cheatdir = subdlg.GetDirectory();
            cheatfn = subdlg.GetPath();

            if (ret != wxID_OK)
                break;

            bool cld;

            if (isgb)
                cld = gbCheatsLoadCheatList(cheatfn.mb_fn_str());
            else {
                if (cheatfn.EndsWith(wxT(".clt"))) {
                    cld = cheatsLoadCheatList(cheatfn.mb_fn_str());

                    if (cld) {
                        *dirty = cheatfn != deffn;
                        systemScreenMessage(_("Loaded cheats"));
                    } else
                        *dirty = true; // attempted load always clears
                } else {
                    // cht format
                    wxFileInputStream input(cheatfn);
                    wxTextInputStream text(input, wxT("\x09"), wxConvUTF8);
                    wxString cheat_desc = wxT("");

                    while (input.IsOk() && !input.Eof()) {
                        wxString line = text.ReadLine().Trim();

                        if (line.Contains(wxT("[")) && !line.Contains(wxT("="))) {
                            cheat_desc = line.AfterFirst('[').BeforeLast(']');
                        }

                        if (line.Contains(wxT("=")) && cheat_desc != wxT("GameInfo")) {
                            while ((input.IsOk() && !input.Eof()) && (line.EndsWith(wxT(";")) || line.EndsWith(wxT(",")))) {
                                line = line + text.ReadLine().Trim();
                            }

                            ParseChtLine(cheat_desc, line);
                        }
                    }

                    *dirty = true;
                }
            }

            Reload();
        } break;

        case wxID_SAVE: {
            wxFileDialog subdlg(dlg, _("Select cheat file"), cheatdir,
                cheatfn, _("VBA cheat lists (*.clt)|*.clt"),
                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
            int ret = subdlg.ShowModal();
            cheatdir = subdlg.GetDirectory();
            cheatfn = subdlg.GetPath();

            if (ret != wxID_OK)
                break;

            // note that there is no way to test for succes of save
            if (isgb)
                gbCheatsSaveCheatList(cheatfn.mb_fn_str());
            else
                cheatsSaveCheatList(cheatfn.mb_fn_str());

            if (cheatfn == deffn)
                *dirty = false;

            systemScreenMessage(_("Saved cheats"));
        } break;

        case wxID_ADD: {
            int ncheats = isgb ? gbCheatNumber : cheatsNumber;
            ce_codes = wxEmptyString;
            wxDialog* subdlg = GetXRCDialog("CheatEdit");
            dlg->SetWindowStyle(wxCAPTION | wxRESIZE_BORDER);

            if (gopts.keep_on_top)
                subdlg->SetWindowStyle(subdlg->GetWindowStyle() | wxSTAY_ON_TOP);
            else
                subdlg->SetWindowStyle(subdlg->GetWindowStyle() & ~wxSTAY_ON_TOP);

            subdlg->ShowModal();
            AddCheat();
            Reload(ncheats);
        } break;

        case wxID_REMOVE: {
            bool asked = false, restore;

            for (int i = list->GetItemCount() - 1; i >= 0; i--)
                if (list->GetItemState(i, wxLIST_STATE_SELECTED)) {
                    list->DeleteItem(i);

                    if (isgb)
                        gbCheatRemove(i);
                    else {
                        if (!asked) {
                            asked = true;
                            restore = wxMessageBox(_("Restore old values?"),
                                          _("Removing cheats"),
                                          wxYES_NO | wxICON_QUESTION)
                                == wxYES;
                        }

                        cheatsDelete(i, restore);
                    }
                }
        } break;

        case wxID_CLEAR:
            if (isgb) {
                if (gbCheatNumber) {
                    *dirty = true;
                    gbCheatRemoveAll();
                }
            } else {
                if (cheatsNumber) {
                    bool restore = wxMessageBox(_("Restore old values?"),
                                       _("Removing cheats"),
                                       wxYES_NO | wxICON_QUESTION)
                        == wxYES;
                    *dirty = true;
                    cheatsDeleteAll(restore);
                }
            }

            Reload();
            break;

        case wxID_SELECTALL:
            // FIXME: probably ought to limit to selected items if any items
            // are selected
            *dirty = true;

            if (isgb) {
                int i;

                for (i = 0; i < gbCheatNumber; i++)
                    if (!gbCheatList[i].enabled)
                        break;

                if (i < gbCheatNumber)
                    for (; i < gbCheatNumber; i++) {
                        gbCheatEnable(i);
                        list->Check(i, true);
                    }
                else
                    for (i = 0; i < gbCheatNumber; i++) {
                        gbCheatDisable(i);
                        list->Check(i, false);
                    }
            } else {
                int i;

                for (i = 0; i < cheatsNumber; i++)
                    if (!cheatsList[i].enabled)
                        break;

                if (i < cheatsNumber)
                    for (; i < cheatsNumber; i++) {
                        cheatsEnable(i);
                        list->Check(i, true);
                    }
                else
                    for (i = 0; i < cheatsNumber; i++) {
                        cheatsDisable(i);
                        list->Check(i, false);
                    }
            }

            break;
        }
    }

    void Check(wxListEvent& ev)
    {
        int ch = ev.GetIndex();

        if (isgb) {
            if (!gbCheatList[ch].enabled) {
                gbCheatEnable(ev.GetIndex());
                *dirty = true;
            }
        } else {
            if (!cheatsList[ch].enabled) {
                cheatsEnable(ev.GetIndex());
                *dirty = true;
            }
        }
    }

    void UnCheck(wxListEvent& ev)
    {
        int ch = ev.GetIndex();

        if (isgb) {
            if (gbCheatList[ch].enabled) {
                gbCheatDisable(ev.GetIndex());
                *dirty = true;
            }
        } else {
            if (cheatsList[ch].enabled) {
                cheatsDisable(ev.GetIndex());
                *dirty = true;
            }
        }
    }

    void AddCheat()
    {
        wxStringTokenizer tk(ce_codes.MakeUpper());

        while (tk.HasMoreTokens()) {
            wxString tok = tk.GetNextToken();

            if (isgb) {
                if (!ce_type)
                    gbAddGsCheat(tok.mb_str(), ce_desc.mb_str());
                else
                    gbAddGgCheat(tok.mb_str(), ce_desc.mb_str());
            } else {
                // Flashcart CHT format
                if (tok.Contains(wxT("="))) {
                    ParseChtLine(ce_desc, tok);
                }
                // Generic Code
                else if (tok.Contains(wxT(":")))
                    cheatsAddCheatCode(tok.mb_str(), ce_desc.mb_str());
                // following determination of type by lengths is
                // same used by win32 and gtk code
                // and like win32/gtk code, user-chosen fmt is ignored
                else if (tok.size() == 12) {
                    tok = tok.substr(0, 8) + wxT(' ') + tok.substr(8);
                    cheatsAddCBACode(tok.mb_str(), ce_desc.mb_str());
                } else if (tok.size() == 16)
                    // not sure why 1-tok is !v3 and 2-tok is v3..
                    cheatsAddGSACode(tok.mb_str(), ce_desc.mb_str(), false);
                // CBA codes are assumed to be N+4, and anything else
                // is assumed to be GSA v3 (although I assume the
                // actual formats should be 8+4 and 8+8)
                else {
                    if (!tk.HasMoreTokens()) {
                        // throw an error appropriate to chosen type
                        if (ce_type == 1) // GSA
                            cheatsAddGSACode(tok.mb_str(), ce_desc.mb_str(), false);
                        else
                            cheatsAddCBACode(tok.mb_str(), ce_desc.mb_str());
                    } else {
                        wxString tok2 = tk.GetNextToken();

                        if (tok2.size() == 4) {
                            tok += wxT(' ') + tok2;
                            cheatsAddCBACode(tok.mb_str(), ce_desc.mb_str());
                        } else {
                            tok += tok2;
                            cheatsAddGSACode(tok.mb_str(), ce_desc.mb_str(), true);
                        }
                    }
                }
            }
        }
    }

    void ParseChtLine(wxString desc, wxString tok);

    void Edit(wxListEvent& ev)
    {
        int id = ev.GetIndex();
        // GetItem() followed by GetText doesn't work, so retrieve from
        // source
        wxString odesc, ocode;
        bool ochecked;
        int otype;

        if (isgb) {
            ochecked = gbCheatList[id].enabled;
            ce_codes = ocode = wxString(gbCheatList[id].cheatCode, wxConvLibc);
            ce_desc = odesc = wxString(gbCheatList[id].cheatDesc, wxConvUTF8);

            if (ce_codes.find(wxT('-')) == wxString::npos)
                otype = ce_type = 0;
            else
                otype = ce_type = 1;
        } else {
            ochecked = cheatsList[id].enabled;
            ce_codes = ocode = wxString(cheatsList[id].codestring, wxConvLibc);
            ce_desc = odesc = wxString(cheatsList[id].desc, wxConvUTF8);

            if (ce_codes.find(wxT(':')) != wxString::npos)
                otype = ce_type = 0;
            else if (ce_codes.find(wxT(' ')) == wxString::npos)
                otype = ce_type = 1;
            else
                otype = ce_type = 2;
        }

        wxDialog* subdlg = GetXRCDialog("CheatEdit");
        dlg->SetWindowStyle(wxCAPTION | wxRESIZE_BORDER);

        if (gopts.keep_on_top)
            subdlg->SetWindowStyle(subdlg->GetWindowStyle() | wxSTAY_ON_TOP);
        else
            subdlg->SetWindowStyle(subdlg->GetWindowStyle() & ~wxSTAY_ON_TOP);

        if (subdlg->ShowModal() != wxID_OK)
            return;

        if (otype != ce_type || ocode != ce_codes) {
            // vba core certainly doesn't make this easy
            // there is no "change" function, so the only way to retain
            // the old order is to delete this and all subsequent items, and
            // then re-add them
            // The MFC code got around this by not even supporting edits on
            // gba codes (which have order dependencies) and just forcing
            // edited codes to the rear on gb codes.
            // It might be safest to only support desc edits, and force the
            // user to re-enter codes to change them
            int ncodes = isgb ? gbCheatNumber : cheatsNumber;

            if (ncodes > id + 1) {
                wxString codes[MAX_CHEATS];
                wxString descs[MAX_CHEATS];
                bool checked[MAX_CHEATS];
                bool v3[MAX_CHEATS];

                for (int i = id + 1; i < ncodes; i++) {
                    codes[i - id - 1] = wxString(isgb ? gbCheatList[i].cheatCode : cheatsList[i].codestring,
                        wxConvLibc);
                    descs[i - id - 1] = wxString(isgb ? gbCheatList[i].cheatDesc : cheatsList[i].desc,
                        wxConvUTF8);
                    checked[i - id - 1] = isgb ? gbCheatList[i].enabled : cheatsList[i].enabled;
                    v3[i - id - 1] = isgb ? false : cheatsList[i].code == 257;
                }

                for (int i = ncodes - 1; i >= id; i--) {
                    list->DeleteItem(i);

                    if (isgb)
                        gbCheatRemove(i);
                    else
                        cheatsDelete(i, cheatsList[i].enabled);
                }

                AddCheat();

                if (!ochecked) {
                    if (isgb)
                        gbCheatDisable(id);
                    else
                        cheatsDisable(id);
                }

                for (int i = id + 1; i < ncodes; i++) {
                    ce_codes = codes[i - id - 1];
                    ce_desc = descs[i - id - 1];

                    if (isgb) {
                        if (ce_codes.find(wxT('-')) == wxString::npos)
                            ce_type = 0;
                        else
                            ce_type = 1;
                    } else {
                        if (ce_codes.find(wxT(':')) != wxString::npos)
                            ce_type = 0;
                        else if (ce_codes.find(wxT(' ')) == wxString::npos) {
                            ce_type = 1;

                            if (v3[i - id - 1])
                                ce_codes.insert(8, 1, wxT(' '));
                        } else {
                            ce_type = 2;
                        }
                    }

                    AddCheat();

                    if (!checked[i - id - 1]) {
                        if (isgb)
                            gbCheatDisable(i);
                        else
                            cheatsDisable(i);
                    }
                }
            } else {
                list->DeleteItem(id);

                if (isgb)
                    gbCheatRemove(id);
                else
                    cheatsDelete(id, cheatsList[id].enabled);

                AddCheat();

                if (!ochecked) {
                    if (isgb)
                        gbCheatDisable(id);
                    else
                        cheatsDisable(id);
                }
            }

            Reload(id);
        } else if (ce_desc != odesc) {
            *dirty = true;
            char* p = isgb ? gbCheatList[id].cheatDesc : cheatsList[id].desc;
            strncpy(p, ce_desc.mb_str(), sizeof(cheatsList[0].desc));
            p[sizeof(cheatsList[0].desc) - 1] = 0;
            item1.SetId(id);
            item1.SetText(wxString(p, wxConvUTF8));
            list->SetItem(item1);
        }
    }

    void AdjustDescWidth()
    {
        // why is it so hard to get an accurate measurement out of wx?
        // on msw, wxLIST_AUTOSIZE might actually be accurate.  On wxGTK,
        // and probably wxMAC (both of which use generic impl) wrong
        // font is used both for rendering (col 0's font) and for
        // wxLIST_AUTOSIZE calculation (the widget's font).
        // The only way to defeat this is to calculate size manually
        // Instead, this just allows user to set max size, and retains
        // it.
        int ow = list->GetColumnWidth(1);
        list->SetColumnWidth(1, wxLIST_AUTOSIZE);
        int cw = list->GetColumnWidth(1);
        // subtracted in renderer from width avail for text
        // but not added in wxLIST_AUTOSIZE
        cw += 8;

        if (cw < col1minw)
            cw = col1minw;

        if (cw < ow)
            cw = ow;

        list->SetColumnWidth(1, cw);
    }
} cheat_list_handler;

void CheatList_t::ParseChtLine(wxString desc, wxString tok)
{
    wxString cheat_opt = tok.BeforeFirst(wxT('='));
    wxString cheat_set = tok.AfterFirst(wxT('='));
    wxStringTokenizer addr_tk(cheat_set.MakeUpper(), wxT(";"));

    while (addr_tk.HasMoreTokens()) {
        wxString addr_token = addr_tk.GetNextToken();
        wxString cheat_addr = addr_token.BeforeFirst(wxT(','));
        wxString values = addr_token.AfterFirst(wxT(','));
        wxString cheat_desc = desc + wxT(":") + cheat_opt;
        wxString cheat_line;
        wxString cheat_value;
        uint32_t address = 0;
        uint32_t value = 0;
        sscanf(cheat_addr.mb_str(), "%8x", &address);

        if (address < 0x40000)
            address += 0x2000000;
        else
            address += 0x3000000 - 0x40000;

        wxStringTokenizer value_tk(values.MakeUpper(), wxT(","));

        while (value_tk.HasMoreTokens()) {
            wxString value_token = value_tk.GetNextToken();
            sscanf(value_token.mb_str(), "%2x", &value);
            cheat_line.Printf(wxT("%08X"), address);
            cheat_value.Printf(wxT("%02X"), value);
            cheat_line = cheat_line + wxT(":") + cheat_value;
            cheatsAddCheatCode(cheat_line.mb_str(), cheat_desc.mb_str());
            address++;
        }
    }
}

// onshow handler for above, in the form of an overzealous validator
class CheatListFill : public wxValidator {
public:
    CheatListFill()
        : wxValidator()
    {
    }
    CheatListFill(const CheatListFill& e)
        : wxValidator()
    {
    }
    wxObject* Clone() const { return new CheatListFill(*this); }
    bool TransferFromWindow() { return true; }
    bool Validate(wxWindow* p) { return true; }
    bool TransferToWindow()
    {
        CheatList_t& clh = cheat_list_handler;
        GameArea* panel = wxGetApp().frame->GetPanel();
        clh.isgb = panel->game_type() == IMAGE_GB;
        clh.dirty = &panel->cheats_dirty;
        clh.cheatfn = panel->game_name() + wxT(".clt");
        clh.cheatdir = panel->game_dir();
        clh.deffn = wxFileName(clh.cheatdir, clh.cheatfn).GetFullPath();
        clh.Reload();
        clh.ce_desc = wxEmptyString;
        wxChoice* ch = clh.ce_type_ch;
        ch->Clear();

        if (clh.isgb) {
            ch->Append(_("GameShark"));
            ch->Append(_("GameGenie"));
        } else {
            ch->Append(_("Generic Code"));
            ch->Append(_("GameShark Advance"));
            ch->Append(_("CodeBreaker Advance"));
            ch->Append(_("Flashcart CHT"));
        }

        ch->SetSelection(0);
        return true;
    }
};

// manage the cheat search dialog
enum cf_vfmt {
    CFVFMT_SD,
    CFVFMT_UD,
    CFVFMT_UH
};

// virtual ListCtrl for cheat search results
class CheatListCtrl : public wxListCtrl {
public:
    wxArrayInt addrs;
    int cap_size; // size in effect when addrs were generated
    int count8, count16, count32; // number of aligned addresses in addrs
    wxString OnGetItemText(long item, long column) const;

    DECLARE_DYNAMIC_CLASS(CheatListCtrl)
};

IMPLEMENT_DYNAMIC_CLASS(CheatListCtrl, wxListCtrl);

static class CheatFind_t : public wxEvtHandler {
public:
    wxDialog* dlg;
    int valsrc, size, op, fmt;
    int ofmt, osize;
    wxString val_s;
    wxTextCtrl* val_tc;
    CheatListCtrl* list;

    // for enable/disable
    wxRadioButton *old_rb, *val_rb;
    wxControl *update_b, *clear_b, *add_b;

    bool isgb;

    // add dialog
    wxString ca_desc, ca_val;
    wxTextCtrl* ca_val_tc;
    wxControl *ca_fmt, *ca_addr;

    CheatFind_t()
        : wxEvtHandler()
        , valsrc(0)
        , size(0)
        , op(0)
        , fmt(0)
        , val_s()
    {
    }
    ~CheatFind_t()
    {
        // not that it matters to anyone but mem leak detectors..
        cheatSearchCleanup(&cheatSearchData);
    }

    void Search(wxCommandEvent& ev)
    {
        dlg->TransferDataFromWindow();

        if (!valsrc && val_s.empty()) {
            wxLogError(_("Number cannot be empty"));
            return;
        }

        if (!cheatSearchData.count)
            ResetSearch(ev);

        if (valsrc)
            cheatSearch(&cheatSearchData, op, size, fmt == CFVFMT_SD);
        else
            cheatSearchValue(&cheatSearchData, op, size, fmt == CFVFMT_SD,
                SignedValue());

        Deselect();
        list->addrs.clear();
        list->count8 = list->count16 = list->count32 = 0;
        list->cap_size = size;

        for (int i = 0; i < cheatSearchData.count; i++) {
            CheatSearchBlock* block = &cheatSearchData.blocks[i];

            for (int j = 0; j < block->size; j += (1 << size)) {
                if (IS_BIT_SET(block->bits, j)) {
                    list->addrs.push_back((i << 28) + j);

                    if (!(j & 1))
                        list->count16++;

                    if (!(j & 3))
                        list->count32++;

// since listctrl is virtual, it should be able to handle
// at least 256k results, which is about the most you
// will ever get
#if 0

					if (list->addrs.size() > 1000)
					{
						wxLogError(_("Search produced %d results.  Please refine better"),
						           list->addrs.size());
						list->addrs.clear();
						return;
					}

#endif
                }
            }
        }

        if (list->addrs.empty()) {
            wxLogError(_("Search produced no results"));
            // no point in keeping empty search results around
            ResetSearch(ev);

            if (old_rb->GetValue()) {
                val_rb->SetValue(true);
                // SetValue doesn't generate an event
                val_tc->Enable();
            }

            old_rb->Disable();
            update_b->Disable();
            clear_b->Disable();
        } else {
            switch (size) {
            case BITS_32:
                list->count16 = list->count32 * 2;

            // fall through
            case BITS_16:
                list->count8 = list->count16 * 2;
                break;

            case BITS_8:
                list->count8 = list->addrs.size();
            }

            old_rb->Enable();
            update_b->Enable();
            clear_b->Enable();
        }

        list->SetItemCount(list->addrs.size());
        list->Refresh();
    }

    void UpdateVals(wxCommandEvent& ev)
    {
        if (cheatSearchData.count) {
            cheatSearchUpdateValues(&cheatSearchData);

            if (list->count8)
                list->Refresh();

            update_b->Disable();
        }
    }

    void ResetSearch(wxCommandEvent& ev)
    {
        if (!cheatSearchData.count) {
            CheatSearchBlock* block = cheatSearchData.blocks;

            if (isgb) {
                block->offset = 0xa000;

                if (gbRam)
                    block->data = gbRam;
                else
                    block->data = &gbMemory[0xa000];

                block->saved = (uint8_t*)malloc(gbRamSize);
                block->size = gbRamSize;
                block->bits = (uint8_t*)malloc(gbRamSize >> 3);

                if (gbCgbMode) {
                    block++;
                    block->offset = 0xc000;
                    block->data = &gbMemory[0xc000];
                    block->saved = (uint8_t*)malloc(0x1000);
                    block->size = 0x1000;
                    block->bits = (uint8_t*)malloc(0x1000 >> 3);
                    block++;
                    block->offset = 0xd000;
                    block->data = gbWram;
                    block->saved = (uint8_t*)malloc(0x8000);
                    block->size = 0x8000;
                    block->bits = (uint8_t*)malloc(0x8000 >> 3);
                } else {
                    block++;
                    block->offset = 0xc000;
                    block->data = &gbMemory[0xc000];
                    block->saved = (uint8_t*)malloc(0x2000);
                    block->size = 0x2000;
                    block->bits = (uint8_t*)malloc(0x2000 >> 3);
                }
            } else {
                block->size = 0x40000;
                block->offset = 0x2000000;
                block->bits = (uint8_t*)malloc(0x40000 >> 3);
                block->data = workRAM;
                block->saved = (uint8_t*)malloc(0x40000);
                block++;
                block->size = 0x8000;
                block->offset = 0x3000000;
                block->bits = (uint8_t*)malloc(0x8000 >> 3);
                block->data = internalRAM;
                block->saved = (uint8_t*)malloc(0x8000);
            }

            cheatSearchData.count = (int)((block + 1) - cheatSearchData.blocks);
        }

        cheatSearchStart(&cheatSearchData);

        if (list->count8) {
            Deselect();
            list->count8 = list->count16 = list->count32 = 0;
            list->addrs.clear();
            list->SetItemCount(0);
            list->Refresh();

            if (old_rb->GetValue()) {
                val_rb->SetValue(true);
                // SetValue doesn't generate an event
                val_tc->Enable();
            }

            old_rb->Disable();
            update_b->Disable();
            clear_b->Disable();
        }
    }

    void Deselect()
    {
        int idx = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

        if (idx >= 0)
            list->SetItemState(idx, 0, wxLIST_STATE_SELECTED);

        add_b->Disable();
    }

    void Select(wxListEvent& ev)
    {
        add_b->Enable(list->GetItemState(ev.GetIndex(), wxLIST_STATE_SELECTED) != 0);
    }

    void AddCheatB(wxCommandEvent& ev)
    {
        int idx = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

        if (idx >= 0)
            AddCheat(idx);
    }

    void AddCheatL(wxListEvent& ev)
    {
        AddCheat(ev.GetIndex());
    }

    void AddCheat(int idx)
    {
        wxString addr_s = list->OnGetItemText(idx, 0);
        ca_addr->SetLabel(addr_s);
        wxString s;

        switch (size) {
        case BITS_8:
            s = _("8-bit ");
            break;

        case BITS_16:
            s = _("16-bit ");
            break;

        case BITS_32:
            s = _("32-bit ");
            break;
        }

        switch (fmt) {
        case CFVFMT_SD:
            s += _("signed decimal");
            break;

        case CFVFMT_UD:
            s += _("unsigned decimal");
            break;

        case CFVFMT_UH:
            s += _("unsigned hexadecimal");
            break;
        }

        ca_fmt->SetLabel(s);
        // probably pointless (but inoffensive) to suggest a value
        ca_val = list->OnGetItemText(idx, 2); // sugest "New" value
        SetValVal(ca_val_tc);
        wxDialog* subdlg = GetXRCDialog("CheatAdd");
        dlg->SetWindowStyle(wxCAPTION | wxRESIZE_BORDER);

        if (gopts.keep_on_top)
            subdlg->SetWindowStyle(subdlg->GetWindowStyle() | wxSTAY_ON_TOP);
        else
            subdlg->SetWindowStyle(subdlg->GetWindowStyle() & ~wxSTAY_ON_TOP);

        if (subdlg->ShowModal() != wxID_OK)
            return;

        if (ca_val.empty()) {
            wxLogError(_("Number cannot be empty"));
            return;
        }

        uint32_t val = GetValue(ca_val, fmt);

        if (isgb) {
            long bank, addr;
            addr_s.ToLong(&bank, 16);
            addr_s.erase(0, 3);
            addr_s.ToLong(&addr, 16);

            if (addr >= 0xd000)
                bank += 0x90;
            else
                bank = 1;

            for (int i = 0; i < (1 << size); i++) {
                addr_s.Printf(wxT("%02X%02X%02X%02X"), bank, val & 0xff,
                    addr & 0xff, addr >> 8);
                gbAddGsCheat(addr_s.mb_str(), ca_desc.mb_str());
                val >>= 8;
                addr++;
            }
        } else {
            wxString s;

            switch (size) {
            case BITS_8:
                s.Printf(wxT(":%02X"), val);
                break;

            case BITS_16:
                s.Printf(wxT(":%04X"), val);
                break;

            case BITS_32:
                s.Printf(wxT(":%08X"), val);
                break;
            }

            addr_s.append(s);
            cheatsAddCheatCode(addr_s.mb_str(), ca_desc.mb_str());
        }
    }

    void SetValVal(wxTextCtrl* tc)
    {
        wxTextValidator* v = wxStaticCast(tc->GetValidator(), wxTextValidator);

        switch (fmt) {
        case CFVFMT_SD:
            v->SetIncludes(val_sigdigits);
            break;

        case CFVFMT_UD:
            v->SetIncludes(val_unsdigits);
            break;

        case CFVFMT_UH:
            v->SetIncludes(val_hexdigits);
            break;
        }
    }

    uint32_t GetValue(wxString& s, int fmt)
    {
        long val;
        // FIXME: probably ought to throw an error if ToLong
        // returns false or val is out of range
        s.ToLong(&val, fmt == CFVFMT_UH ? 16 : 10);

        if (size != BITS_32)
            val &= size == BITS_8 ? 0xff : 0xffff;

        return val;
    }

    uint32_t GetValue(int fmt)
    {
        return GetValue(val_s, fmt);
    }

    uint32_t GetValue()
    {
        return GetValue(fmt);
    }

    int32_t SignedValue(wxString& s, int fmt)
    {
        int32_t val = GetValue(s, fmt);

        if (fmt == CFVFMT_SD) {
            if (size == BITS_8)
                val = (int32_t)(int8_t)val;
            else if (size == BITS_16)
                val = (int32_t)(int16_t)val;
        }

        return val;
    }

    int32_t SignedValue(int fmt)
    {
        return SignedValue(val_s, fmt);
    }

    int32_t SignedValue()
    {
        return SignedValue(fmt);
    }

    void FormatValue(int32_t val, wxString& s)
    {
        if (fmt != CFVFMT_SD && size != BITS_32)
            val &= size == BITS_8 ? 0xff : 0xffff;

        switch (fmt) {
        case CFVFMT_SD:
            s.Printf(wxT("%d"), val);
            break;

        case CFVFMT_UD:
            s.Printf(wxT("%u"), val);
            break;

        case CFVFMT_UH:
            switch (size) {
            case BITS_8:
                s.Printf(wxT("%02X"), val);
                break;

            case BITS_16:
                s.Printf(wxT("%04X"), val);
                break;

            case BITS_32:
                s.Printf(wxT("%08X"), val);
                break;
            }
        }
    }

    void UpdateView(wxCommandEvent& ev)
    {
        dlg->TransferDataFromWindow();

        if (ofmt != fmt && !val_s.empty()) {
            int32_t val = GetValue(ofmt);

            switch (fmt) {
            case CFVFMT_SD:
                switch (size) {
                case BITS_8:
                    val = (int32_t)(int8_t)val;
                    break;

                case BITS_16:
                    val = (int32_t)(int16_t)val;
                }

                val_s.Printf(wxT("%d"), val);
                break;

            case CFVFMT_UD:
                val_s.Printf(wxT("%u"), val);
                break;

            case CFVFMT_UH:
                val_s.Printf(wxT("%x"), val);
                break;
            }

            val_tc->SetValue(val_s);
        }

        if (ofmt != fmt)
            SetValVal(val_tc);

        if (list->count8 && osize != size) {
            switch (size) {
            case BITS_32:
                list->SetItemCount(list->count32);
                break;

            case BITS_16:
                list->SetItemCount(list->count16);
                break;

            case BITS_8:
                list->SetItemCount(list->count8);
                break;
            }
        }

        if (ofmt != fmt || osize != size)
            list->Refresh();

        ofmt = fmt;
        osize = size;
    }

    void EnableVal(wxCommandEvent& ev)
    {
        val_tc->Enable(ev.GetId() == XRCID("SpecificValue"));
    }

} cheat_find_handler;

// clear cheat find dialog between games
void MainFrame::ResetCheatSearch()
{
    CheatFind_t& cfh = cheat_find_handler;
    cfh.fmt = cfh.size = cfh.op = cfh.valsrc = 0;
    cfh.val_s = wxEmptyString;
    cfh.Deselect();
    cfh.list->SetItemCount(0);
    cfh.list->count8 = cfh.list->count16 = cfh.list->count32 = 0;
    cfh.list->addrs.clear();
    cfh.ca_desc = wxEmptyString;
    cheatSearchCleanup(&cheatSearchData);
}

// onshow handler for above, in the form of an overzealous validator
class CheatFindFill : public wxValidator {
public:
    CheatFindFill()
        : wxValidator()
    {
    }
    CheatFindFill(const CheatFindFill& e)
        : wxValidator()
    {
    }
    wxObject* Clone() const { return new CheatFindFill(*this); }
    bool TransferFromWindow() { return true; }
    bool Validate(wxWindow* p) { return true; }
    bool TransferToWindow()
    {
        CheatFind_t& cfh = cheat_find_handler;
        GameArea* panel = wxGetApp().frame->GetPanel();
        cfh.isgb = panel->game_type() == IMAGE_GB;
        cfh.val_tc->Enable(!cfh.valsrc);
        cfh.ofmt = cfh.fmt;
        cfh.SetValVal(cfh.val_tc);
        return true;
    }
};

// the implementation of the virtual list ctrl for search results
// requires CheatFind_t to be implemented
wxString CheatListCtrl::OnGetItemText(long item, long column) const
{
    wxString s;
    CheatFind_t& cfh = cheat_find_handler;
    // allowing GUI to change format after search makes this a little
    // more complicated than necessary...
    int off = 0;
    int size = cfh.size;

    if (cap_size > size) {
        off = (item & ((1 << (cap_size - size)) - 1)) << size;
        item >>= cap_size - size;
    } else if (cap_size < size) {
        for (int i = 0; i < addrs.size(); i++) {
            if (!(addrs[i] & ((1 << size) - 1)) && !item--) {
                item = i;
                break;
            }
        }
    }

    CheatSearchBlock* block = &cheatSearchData.blocks[addrs[item] >> 28];
    off += addrs[item] & 0xfffffff;

    switch (column) {
    case 0: // address
        if (cfh.isgb) {
            int bank = 0;
            int addr = block->offset;

            if (block->offset == 0xa000) {
                bank = off / 0x2000;
                addr += off % 0x2000;
            } else if (block->offset == 0xd000) {
                bank = off / 0x1000;
                addr += off % 0x1000;
            } else
                addr += off;

            s.Printf(wxT("%02X:%04X"), bank, addr);
        } else
            s.Printf(wxT("%08X"), block->offset + off);

        break;

    case 1: // old
        cfh.FormatValue(cheatSearchSignedRead(block->saved, off, size), s);
        break;

    case 2: // new
        cfh.FormatValue(cheatSearchSignedRead(block->data, off, size), s);
        break;
    }

    return s;
}

// these are the choices for canned colors; their order must match the
// names in the choice control
static const uint16_t defaultPalettes[][8] = {
    {
        // Standard
        0x7FFF, 0x56B5, 0x318C, 0x0000, 0x7FFF, 0x56B5, 0x318C, 0x0000,
    },
    {
        // Blue Sea
        0x6200, 0x7E10, 0x7C10, 0x5000, 0x6200, 0x7E10, 0x7C10, 0x5000,
    },
    {
        // Dark Night
        0x4008, 0x4000, 0x2000, 0x2008, 0x4008, 0x4000, 0x2000, 0x2008,
    },
    {
        // Green Forest
        0x43F0, 0x03E0, 0x4200, 0x2200, 0x43F0, 0x03E0, 0x4200, 0x2200,
    },
    {
        // Hot Desert
        0x43FF, 0x03FF, 0x221F, 0x021F, 0x43FF, 0x03FF, 0x221F, 0x021F,
    },
    {
        // Pink Dreams
        0x621F, 0x7E1F, 0x7C1F, 0x2010, 0x621F, 0x7E1F, 0x7C1F, 0x2010,
    },
    {
        // Weird Colors
        0x621F, 0x401F, 0x001F, 0x2010, 0x621F, 0x401F, 0x001F, 0x2010,
    },
    {
        // Real GB Colors
        0x1B8E, 0x02C0, 0x0DA0, 0x1140, 0x1B8E, 0x02C0, 0x0DA0, 0x1140,
    },
    {
        // Real 'GB on GBASP' Colors
        0x7BDE, /*0x23F0*/ 0x5778, /*0x5DC0*/ 0x5640, 0x0000, 0x7BDE, /*0x3678*/ 0x529C, /*0x0980*/ 0x2990, 0x0000,
    }
};

// manage the GB color prefs' canned color selecter
static class GBColorConfig_t : public wxEvtHandler {
public:
    wxWindow* p;
    wxChoice* c;
    wxColourPickerCtrl* cp[8];
    int pno;
    void ColorSel(wxCommandEvent& ev)
    {
        if (ev.GetSelection() > 0) {
            const uint16_t* color = defaultPalettes[ev.GetSelection() - 1];

            for (int i = 0; i < 8; i++, color++)
                cp[i]->SetColour(wxColor(((*color << 3) & 0xf8),
                    ((*color >> 2) & 0xf8),
                    ((*color >> 7) & 0xf8)));
        }
    }
    void ColorReset(wxCommandEvent& ev)
    {
        const uint16_t* color = &systemGbPalette[pno * 8];

        for (int i = 0; i < 8; i++, color++)
            cp[i]->SetColour(wxColor(((*color << 3) & 0xf8),
                ((*color >> 2) & 0xf8),
                ((*color >> 7) & 0xf8)));
    }

    void ColorButton(wxCommandEvent& ev)
    {
        c->SetSelection(0);
    }
} GBColorConfigHandler[3];

// disable controls if a GBA game is not loaded
class GBACtrlEnabler : public wxValidator {
public:
    GBACtrlEnabler()
        : wxValidator()
    {
    }
    GBACtrlEnabler(const GBACtrlEnabler& e)
        : wxValidator()
    {
    }
    wxObject* Clone() const { return new GBACtrlEnabler(*this); }
    bool TransferFromWindow() { return true; }
    bool Validate(wxWindow* p) { return true; }
    bool TransferToWindow()
    {
        GetWindow()->Enable(wxGetApp().frame->GetPanel()->game_type() == IMAGE_GBA);
        return true;
    }
};

// manage save game area settings for GBA prefs
static class BatConfig_t : public wxEvtHandler {
public:
    wxChoice *type, *size;
    void ChangeType(wxCommandEvent& ev)
    {
        int i = ev.GetSelection();
        size->Enable(!i || i == 3); // automatic/flash
    }
    void Detect(wxCommandEvent& ev)
    {
        uint32_t sz = wxGetApp().frame->GetPanel()->game_size();
        utilGBAFindSave(sz);
        type->SetSelection(saveType);

        if (saveType == GBA_SAVE_FLASH) {
            size->SetSelection(flashSize == 0x20000 ? 1 : 0);
            size->Enable();
        } else {
            size->Disable();
        }
    }
} BatConfigHandler;

// manage the sound prefs dialog
static class SoundConfig_t : public wxEvtHandler {
public:
    wxSlider *vol, *bufs;
    wxControl* bufinfo;
    int lastapi;
    wxChoice* dev;
    wxControl *umix, *hwacc;
    wxArrayString dev_ids;

    void FullVol(wxCommandEvent& ev)
    {
        vol->SetValue(100);
    }
    void AdjustFrames(int count)
    {
        wxString s;
        s.Printf(_("%d frames = %.2f ms"), count, (double)count / 60.0 * 1000.0);
        bufinfo->SetLabel(s);
    }
    void AdjustFramesEv(wxCommandEvent& ev)
    {
        AdjustFrames(bufs->GetValue());
    }

    bool FillDev(int api)
    {
        dev->Clear();
        dev->Append(_("Default device"));
        dev_ids.clear();
        wxArrayString names;

        switch (api) {
        case AUD_SDL:
            break;
#ifndef NO_OAL

        case AUD_OPENAL:
            if (!GetOALDevices(names, dev_ids))
                return false;

            break;
#endif
#ifdef __WXMSW__

        case AUD_DIRECTSOUND:
            if (!(GetDSDevices(names, dev_ids)))
                return false;

            break;
#ifndef NO_XAUDIO2

        case AUD_XAUDIO2:
            if (!GetXA2Devices(names, dev_ids))
                return false;

            break;
#endif
#ifndef NO_FAUDIO

        case AUD_FAUDIO:
            if (!GetFADevices(names, dev_ids))
                return false;

            break;
#endif
#endif
        }

        dev->SetSelection(0);

        for (int i = 0; i < names.size(); i++) {
            dev->Append(names[i]);

            if (api == gopts.audio_api && gopts.audio_dev == dev_ids[i])
                dev->SetSelection(i + 1);
        }

        umix->Enable(api == AUD_XAUDIO2);
        hwacc->Enable(api == AUD_DIRECTSOUND);
        lastapi = api;
        return true;
    }
    void SetAPI(wxCommandEvent& ev)
    {
        int api = gopts.audio_api;
        wxValidator* v = wxStaticCast(ev.GetEventObject(), wxWindow)->GetValidator();
        v->TransferFromWindow();
        int newapi = gopts.audio_api;
        gopts.audio_api = api;

        if (newapi == lastapi)
            return;

        gopts.audio_dev = "";
        FillDev(newapi);
    }
} sound_config_handler;

// Validator/widget filler for sound device selector & time indicator
class SoundConfigLoad : public wxValidator {
public:
    SoundConfigLoad()
        : wxValidator()
    {
    }
    SoundConfigLoad(const SoundConfigLoad& e)
        : wxValidator()
    {
    }
    wxObject* Clone() const { return new SoundConfigLoad(*this); }
    bool Validate(wxWindow* p) { return true; }
    bool TransferToWindow()
    {
        SoundConfig_t& sch = sound_config_handler;
        sch.FillDev(gopts.audio_api);
        sch.AdjustFrames(gopts.audio_buffers);
        return true;
    }
    bool TransferFromWindow()
    {
        SoundConfig_t& sch = sound_config_handler;
        int devs = sch.dev->GetSelection();

        if (devs <= 0)
            gopts.audio_dev = wxEmptyString;
        else
            gopts.audio_dev = sch.dev_ids[devs - 1];

        return true;
    }
};

// manage the joypad prefs' per-panel default/clear buttons
static class JoyPadConfig_t : public wxEvtHandler {
public:
    wxWindow* p;
    void JoypadConfigButtons(wxCommandEvent& ev)
    {
        bool clear = ev.GetId() == XRCID("Clear");

        for (int i = 0; i < NUM_KEYS; i++) {
            wxJoyKeyTextCtrl* tc = XRCCTRL_D(*p, joynames[i], wxJoyKeyTextCtrl);

            if (clear)
                tc->SetValue(wxEmptyString);
            else {
                wxJoyKeyBinding_v a;

                if (defkeys[i * 2].key)
                    a.push_back(defkeys[i * 2]);

                if (defkeys[i * 2 + 1].joy)
                    a.push_back(defkeys[i * 2 + 1]);

                tc->SetValue(wxJoyKeyTextCtrl::ToString(a));
            }
        }
    }
} JoyPadConfigHandler[4];

// manage fullscreen mode widget
// technically, it's more than a validator: it modifies the widget as well
class ScreenModeList : public wxValidator {
public:
    ScreenModeList()
        : wxValidator()
    {
    }
    ScreenModeList(const ScreenModeList& e)
        : wxValidator()
    {
    }
    wxObject* Clone() const { return new ScreenModeList(*this); }
    bool Validate(wxWindow* p) { return true; }
    bool TransferToWindow()
    {
        wxChoice* c = wxStaticCast(GetWindow(), wxChoice);
        wxDisplay d(wxDisplay::GetFromWindow(c->GetParent()));
        c->Clear();
        int modeno = 0, bestmode = 0;
        int bm_bpp = 0;
        c->Append(_("Desktop mode"));
        // probably ought to just disable this whole control on UNIX/X11 since
        // wxDisplay is so broken.
        vm = d.GetModes();
        wxString s;

        for (int i = 0; i < vm.size(); i++) {
            s.Printf(_("%d x %d - %dbpp @ %dHz"), vm[i].w, vm[i].h, vm[i].bpp, vm[i].refresh);
            c->Append(s);

            if (!modeno && gopts.fs_mode.w == vm[i].w && gopts.fs_mode.h == vm[i].h) {
                if (gopts.fs_mode.bpp == vm[i].bpp && gopts.fs_mode.refresh == vm[i].refresh)
                    modeno = i + 1;
                else if (vm[i].bpp == gopts.fs_mode.bpp && bm_bpp != gopts.fs_mode.bpp) {
                    bestmode = i + 1;
                    bm_bpp = vm[i].bpp;
                } else if (bm_bpp != gopts.fs_mode.bpp && bm_bpp != 32 && vm[i].bpp == 32) {
                    bm_bpp = vm[i].bpp;
                    bestmode = i + 1;
                } else if (bm_bpp != gopts.fs_mode.bpp && bm_bpp < 24 && vm[i].bpp == 24) {
                    bm_bpp = vm[i].bpp;
                    bestmode = i + 1;
                } else if (bm_bpp != gopts.fs_mode.bpp && bm_bpp < 24 && bm_bpp != 16 && vm[i].bpp == 16) {
                    bm_bpp = vm[i].bpp;
                    bestmode = i + 1;
                } else if (!bm_bpp) {
                    bm_bpp = vm[i].bpp;
                    bestmode = i + 1;
                }
            }
        }

        if (!modeno && bestmode)
            modeno = bestmode;

        c->SetSelection(modeno);
        return true;
    }
    bool TransferFromWindow()
    {
        int bestmode = wxStaticCast(GetWindow(), wxChoice)->GetSelection();

        if (!bestmode)
            gopts.fs_mode.h = gopts.fs_mode.w = gopts.fs_mode.bpp = gopts.fs_mode.refresh = 0;
        else
            gopts.fs_mode = vm[bestmode - 1];

        return true;
    }

private:
    wxArrayVideoModes vm;
};

// enable plugin-related iff filter choice is plugin
class PluginEnabler : public wxValidator {
public:
    PluginEnabler()
        : wxValidator()
    {
    }
    PluginEnabler(const PluginEnabler& e)
        : wxValidator()
    {
    }
    wxObject* Clone() const { return new PluginEnabler(*this); }
    bool TransferFromWindow() { return true; }
    bool Validate(wxWindow* p) { return true; }
    bool TransferToWindow()
    {
        GetWindow()->Enable(gopts.filter == FF_PLUGIN);
        return true;
    }
};

// The same, but as an event handler
static class PluginEnable_t : public wxEvtHandler {
public:
    wxWindow *lab, *ch;
    void ToggleChoice(wxCommandEvent& ev)
    {
        bool en = ev.GetSelection() == FF_PLUGIN;
        lab->Enable(en);
        ch->Enable(en);
    }
} PluginEnableHandler;

// fill in plugin list
class PluginListFiller : public PluginEnabler {
public:
    PluginListFiller(wxDialog* parent, wxControl* lab, wxChoice* ch)
        : PluginEnabler()
        , txt(lab)
        , dlg(parent)
        , plugins()
        , filtch(ch)
    {
    }
    PluginListFiller(const PluginListFiller& e)
        : PluginEnabler()
        , txt(e.txt)
        , dlg(e.dlg)
        , plugins(e.plugins)
        , filtch(e.filtch)
    {
    }
    wxObject* Clone() const { return new PluginListFiller(*this); }
    bool Validate(wxWindow* p) { return true; }
    bool TransferToWindow()
    {
        PluginEnabler::TransferToWindow();
        wxChoice* ch = wxStaticCast(GetWindow(), wxChoice);
        ch->Clear();
        ch->Append(_("None"));
        plugins.clear();
        const wxString plpath = wxGetApp().GetPluginsDir();
        wxDir::GetAllFiles(plpath, &plugins, wxT("*.rpi"), wxDIR_FILES | wxDIR_DIRS);

        for (int i = 0; i < plugins.size(); i++) {
            wxDynamicLibrary dl(plugins[i], wxDL_VERBATIM | wxDL_NOW);
            RENDPLUG_GetInfo GetInfo;
            const RENDER_PLUGIN_INFO* rpi;

            if (dl.IsLoaded() && (GetInfo = (RENDPLUG_GetInfo)dl.GetSymbol(wxT("RenderPluginGetInfo"))) &&
                // note that in actual kega fusion plugins, rpi->Output is
                // unused (as is rpi->Handle)
                dl.GetSymbol(wxT("RenderPluginOutput")) && (rpi = GetInfo()) &&
                // FIXME: maybe this should be >= RPI_VERISON
                (rpi->Flags & 0xff) == RPI_VERSION &&
                // RPI_565_SUPP is not supported
                // although it would be possible
                // and it would make Cairo more efficient
                (rpi->Flags & (RPI_555_SUPP | RPI_888_SUPP))) {
                wxFileName fn(plugins[i]);
                wxString s = fn.GetName();
                s += wxT(": ");
                s += wxString(rpi->Name, wxConvUTF8, sizeof(rpi->Name));
                fn.MakeRelativeTo(plpath);
                plugins[i] = fn.GetFullPath();
                ch->Append(s);

                if (plugins[i] == gopts.filter_plugin)
                    ch->SetSelection(i + 1);
            } else
                plugins.RemoveAt(i--);
        }

        if (ch->GetCount() == 1) {
            // this is probably the only place the user can find out where
            // to put the plugins...  it depends on where program was
            // installed, and of course OS
            wxString msg;
            msg.Printf(_("No usable rpi plugins found in %s"), plpath.mb_str());
            systemScreenMessage(msg);
            ch->Hide();
            txt->Hide();
            int cursel = filtch->GetSelection();

            if (cursel == FF_PLUGIN)
                cursel = 0;

            if (filtch->GetCount() == FF_PLUGIN + 1) {
                filtch->Delete(FF_PLUGIN);
                // apparently wxgtk loses selection after this, even
                // if selection was not FF_PLUGIN
                filtch->SetSelection(cursel);
            }
        } else {
            ch->Show();
            txt->Show();

            if (filtch->GetCount() < FF_PLUGIN + 1)
                filtch->Append(_("Plugin"));
        }

        // FIXME: this isn't enough.  It only resizes 2nd time around
        dlg->Fit();
        return true;
    }
    bool TransferFromWindow()
    {
        wxChoice* ch = wxStaticCast(GetWindow(), wxChoice);

        if (ch->GetCount() == 1) {
            gopts.filter_plugin = wxEmptyString;

            // this happens if "Plugin" was selected and the entry was
            // subsequently removed
            if (ch->GetSelection() < 0)
                ch->SetSelection(0);

            if (gopts.filter < 0)
                gopts.filter = 0;
        } else {
            int n = ch->GetSelection();

            if (n > 0)
                gopts.filter_plugin = plugins[n - 1];
            else {
                if (filtch->GetSelection() == FF_PLUGIN) {
                    wxMessageBox(_("Please select a plugin or a different filter"),
                        _("Plugin selection error"), wxOK | wxICON_ERROR);
                    return false;
                }

                gopts.filter_plugin = wxEmptyString;
            }
        }

        return true;
    }

private:
    wxDialog* dlg;
    wxControl* txt;
    wxChoice* filtch;
    wxArrayString plugins;
};

// this is the cmd table index for the accel tree ctrl
// one of the "benefits" of using TreeItemData is that we have to
// malloc them all, because treectrl destructor will free them all
// that means we can't use e.g. a single static table of len ncmds
class TreeInt : public wxTreeItemData {
public:
    TreeInt(int i)
        : wxTreeItemData()
    {
        val = i;
    }
    int val;
};

// Convert a tree selection ID to a name
// root
//   parent
//     item
static bool treeid_to_name(int id, wxString& name, wxTreeCtrl* tc,
    const wxTreeItemId& parent, int lev = 0)
{
    wxTreeItemIdValue cookie;

    for (wxTreeItemId tid = tc->GetFirstChild(parent, cookie); tid.IsOk();
         tid = tc->GetNextChild(parent, cookie)) {
        const TreeInt* ti = static_cast<const TreeInt*>(tc->GetItemData(tid));

        if (ti && ti->val == id) {
            name = wxString(wxT(' '), 2 * lev) + tc->GetItemText(tid);
            return true;
        }

        if (treeid_to_name(id, name, tc, tid, lev + 1)) {
            name = wxString(wxT(' '), 2 * lev) + tc->GetItemText(tid) + wxT('\n') + name;
            return true;
        }
    }

    return false;
}

// for sorting accels by command ID
static bool cmdid_lt(const wxAcceleratorEntry& a, const wxAcceleratorEntry& b)
{
    return a.GetCommand() < b.GetCommand();
}

// manage the accel editor dialog
static class AccelConfig_t : public wxEvtHandler {
public:
    wxTreeCtrl* tc;
    wxControlWithItems* lb;
    wxAcceleratorEntry_v user_accels, accels;
    wxWindow *asb, *remb;
    wxKeyTextCtrl* key;
    wxControl* curas;

    // since this is not the actual dialog, derived from wxDialog, which is
    // the normal way of doing things, do init on the show event instead of
    // constructor
    void Init(wxShowEvent& ev)
    {
#if wxCHECK_VERSION(2, 9, 0)
#define GetShow IsShown
#endif
        ev.Skip();

        if (!ev.GetShow())
            return;

        lb->Clear();
        tc->Unselect();
        tc->ExpandAll();
        user_accels = gopts.accels;
        key->SetValue(wxT(""));
        asb->Enable(false);
        remb->Enable(false);
        curas->SetLabel(wxT(""));
        accels = wxGetApp().frame->get_accels(user_accels);
    }

    // on OK, save the accels in gopts
    void Set(wxCommandEvent& ev)
    {
        // opts.cpp assumes that gopts.accels entries with same command ID
        // are contiguous, so sort first
        std::sort(gopts.accels.begin(), gopts.accels.end(), cmdid_lt);
        gopts.accels = user_accels;
        wxGetApp().frame->set_global_accels();
        ev.Skip();
    }

    // After selecting item in command list, fill in key list
    // and maybe enable asb
    void CommandSel(wxTreeEvent& ev)
    {
        // wxTreeCtrl *tc = wxStaticCast(evt.GetEventObject(), wxTreeCtrl);
        // can't use wxStaticCast; wxTreeItemData does not derive from wxObject
        const TreeInt* id = static_cast<const TreeInt*>(tc->GetItemData(ev.GetItem()));

        if (!id) {
            ev.Veto();
            return;
        }

        if (ev.GetEventType() == wxEVT_COMMAND_TREE_SEL_CHANGING) {
            ev.Skip();
            return;
        }

        lb->Clear();
        remb->Enable(false);
        asb->Enable(!key->GetValue().empty());
        int cmd = id->val;

        for (int i = 0; i < accels.size(); i++)
            if (accels[i].GetCommand() == cmdtab[cmd].cmd_id)
                lb->Append(wxKeyTextCtrl::ToString(accels[i].GetFlags(),
                    accels[i].GetKeyCode()));
    }

    // after selecting a key in key list, enable Remove button
    void KeySel(wxCommandEvent& ev)
    {
        remb->Enable(lb->GetSelection() != wxNOT_FOUND);
    }

    // remove selected binding
    void Remove(wxCommandEvent& ev)
    {
        int lsel = lb->GetSelection();

        if (lsel == wxNOT_FOUND)
            return;

        wxString selstr = lb->GetString(lsel);
        int selmod, selkey;

        if (!wxKeyTextCtrl::FromString(selstr, selmod, selkey))
            return; // this should never happen

        remb->Enable(false);

        // if this key is currently in the shortcut field, clear out curas
        if (selstr == key->GetValue())
            curas->SetLabel(wxT(""));

        lb->Delete(lsel);

        // first drop from user accels, if applicable
        for (wxAcceleratorEntry_v::iterator i = user_accels.begin();
             i < user_accels.end(); ++i)
            if (i->GetFlags() == selmod && i->GetKeyCode() == selkey) {
                user_accels.erase(i);
                break;
            }

        // if it's a system accel, disable by assigning to NOOP
        wxAcceleratorEntry_v& sys_accels = wxGetApp().frame->sys_accels;

        for (int i = 0; i < sys_accels.size(); i++)
            if (sys_accels[i].GetFlags() == selmod && sys_accels[i].GetKeyCode() == selkey) {
                wxAcceleratorEntry ne(selmod, selkey, XRCID("NOOP"));
                user_accels.push_back(ne);
            }

        // finally, remove from accels instead of recomputing
        for (wxAcceleratorEntry_v::iterator i = accels.begin();
             i < accels.end(); ++i)
            if (i->GetFlags() == selmod && i->GetKeyCode() == selkey) {
                accels.erase(i);
                break;
            }
    }

    // wipe out all user bindings
    void ResetAll(wxCommandEvent& ev)
    {
        if (user_accels.empty() || wxMessageBox(_("This will clear all user-defined accelerators.  Are you sure?"), _("Confirm"), wxYES_NO) != wxYES)
            return;

        user_accels.clear();
        accels = wxGetApp().frame->sys_accels;
        tc->Unselect();
        lb->Clear();
        // rather than recomputing curas, just clear it
        key->SetValue(wxT(""));
        curas->SetLabel(wxT(""));
    }

    // remove old key binding, add new key binding, and update GUI
    void Assign(wxCommandEvent& ev)
    {
        wxTreeItemId csel = tc->GetSelection();
        wxString accel = key->GetValue();

        if (!csel.IsOk() || accel.empty())
            return;

        int acmod, ackey;

        if (!wxKeyTextCtrl::FromString(accel, acmod, ackey))
            return; // this should never happen

        for (int i = 0; i < lb->GetCount(); i++)
            if (lb->GetString(i) == accel)
                return; // ignore attempts to add twice

        lb->Append(accel);

        // first drop from user accels, if applicable
        for (wxAcceleratorEntry_v::iterator i = user_accels.begin();
             i < user_accels.end(); ++i)
            if (i->GetFlags() == acmod && i->GetKeyCode() == ackey) {
                user_accels.erase(i);
                break;
            }

        // then assign to this command
        const TreeInt* id = static_cast<const TreeInt*>(tc->GetItemData(csel));
        wxAcceleratorEntry ne(acmod, ackey, cmdtab[id->val].cmd_id);
        user_accels.push_back(ne);
        // now assigned to this cmd...
        wxString lab;
        treeid_to_name(id->val, lab, tc, tc->GetRootItem());
        curas->SetLabel(lab);
        // finally, instead of recomputing accels, just append new accel
        accels.push_back(ne);
    }

    // update curas and maybe enable asb
    void CheckKey(wxCommandEvent& ev)
    {
        wxString nkey = key->GetValue();

        if (nkey.empty()) {
            curas->SetLabel(wxT(""));
            asb->Enable(false);
            return;
        }

        int acmod, ackey;

        if (!wxKeyTextCtrl::FromString(nkey, acmod, ackey)) {
            // this should never happen
            key->SetValue(wxT(""));
            asb->Enable(false);
            return;
        }

        asb->Enable(tc->GetSelection().IsOk());
        int cmd = -1;

        for (int i = 0; i < accels.size(); i++)
            if (accels[i].GetFlags() == acmod && accels[i].GetKeyCode() == ackey) {
                int cmdid = accels[i].GetCommand();

                for (cmd = 0; cmd < ncmds; cmd++)
                    if (cmdid == cmdtab[cmd].cmd_id)
                        break;

                break;
            }

        if (cmd < 0 || cmdtab[cmd].cmd_id == XRCID("NOOP")) {
            curas->SetLabel(wxT(""));
            return;
        }

        wxString lab;
        treeid_to_name(cmd, lab, tc, tc->GetRootItem());
        curas->SetLabel(lab);
    }
} accel_config_handler;

// build initial accel tree control from menu
void MainFrame::add_menu_accels(wxTreeCtrl* tc, wxTreeItemId& parent, wxMenu* menu)
{
    wxMenuItemList mil = menu->GetMenuItems();

    for (wxMenuItemList::iterator mi = mil.begin(); mi != mil.end(); ++mi) {
        if ((*mi)->IsSeparator()) {
            tc->AppendItem(parent, wxT("-----"));
        } else if ((*mi)->IsSubMenu()) {
            wxTreeItemId id = tc->AppendItem(parent, (*mi)->GetItemLabelText());
            add_menu_accels(tc, id, (*mi)->GetSubMenu());

            if ((*mi)->GetSubMenu() == recent) {
                for (int i = wxID_FILE1; i <= wxID_FILE10; i++) {
                    int cmdid;

                    for (cmdid = 0; cmdid < ncmds; cmdid++)
                        if (cmdtab[cmdid].cmd_id == i)
                            break;

                    TreeInt* val = new TreeInt(cmdid);
                    tc->AppendItem(id, cmdtab[cmdid].name, -1, -1, val);
                }
            }
        } else {
            int mid = (*mi)->GetId();

            if (mid >= wxID_FILE1 && mid <= wxID_FILE10)
                continue;

            int cmdid;

            for (cmdid = 0; cmdid < ncmds; cmdid++)
                if (cmdtab[cmdid].cmd_id == mid)
                    break;

            if (cmdid == ncmds)
                continue; // bad menu item; should inform user really

            TreeInt* val = new TreeInt(cmdid);
            // ugh.  There has to be a better way...
            // perhaps make XRCID ranges a requirement for load/save st?
            // but then if the user overides main menu, that req. is broken..
            wxString txt = (*mi)->GetItemLabelText();

            for (int i = 0; i < 10; i++)
                if (*mi == loadst_mi[i] || *mi == savest_mi[i]) {
                    txt = cmdtab[i].name;
                    break;
                }

            tc->AppendItem(parent, txt, -1, -1, val);
        }
    }
}

// manage throttle spinctrl/canned setting choice interaction
static class ThrottleCtrl_t : public wxEvtHandler {
public:
    wxSpinCtrl* thr;
    wxChoice* thrsel;

    // set thrsel from thr
    void SetThrottleSel(wxSpinEvent& evt)
    {
        DoSetThrottleSel(thr->GetValue());
    }

    void DoSetThrottleSel(uint32_t val)
    {
        if (val <= 600)
            thrsel->SetSelection(std::round((double)val / 25));
        else
            thrsel->SetSelection(100 / 25);
    }

    // set thr from thrsel
    void SetThrottle(wxCommandEvent& evt)
    {
        uint32_t val = thrsel->GetSelection() * 25;

        if (val <= 600)
            thr->SetValue(val);
        else
            thr->SetValue(100);
    }

    // since this is not the actual dialog, derived from wxDialog, which is
    // the normal way of doing things, do init on the show event instead of
    // constructor
    // could've also used a validator, I guess...
    void Init(wxShowEvent& ev)
    {
        ev.Skip();
        DoSetThrottleSel(throttle);
    }
} throttle_ctrl;

// manage speedup key frame skip spinctrl/canned setting choice interaction
static class SpeedupFrameSkipCtrl_t : public wxEvtHandler {
public:
    wxSpinCtrl* speedup_frame_skip_spin;
    wxChoice* speedup_frame_skip_sel;
    void SetSpeedupFrameSkipSel(wxSpinEvent& evt);
    void DoSetSpeedupFrameSkipSel(uint32_t val);
    void SetSpeedupFrameSkip(wxCommandEvent& evt);
    void Init(wxShowEvent& ev);
} speedup_frame_skip_ctrl;

// manage speedup key throttle spinctrl/canned setting choice interaction
static class SpeedupThrottleCtrl_t : public wxEvtHandler {
public:
    wxSpinCtrl* speedup_throttle_spin;
    wxChoice* speedup_throttle_sel;

    // set speedup_throttle_sel from speedup_throttle
    void SetSpeedupThrottleSel(wxSpinEvent& evt)
    {
        DoSetSpeedupThrottleSel(speedup_throttle_spin->GetValue());
    }

    void DoSetSpeedupThrottleSel(uint32_t val)
    {
        if (val > 0 && val <= 600) {
            speedup_throttle_sel->SetSelection(std::round((double)val / 25));
            speedup_frame_skip_ctrl.DoSetSpeedupFrameSkipSel(0);
            wxCommandEvent nil;
            speedup_frame_skip_ctrl.SetSpeedupFrameSkip(nil);
        }
        else
            speedup_throttle_sel->SetSelection(0);
    }

    // set speedup_throttle from speedup_throttle_sel
    void SetSpeedupThrottle(wxCommandEvent& evt)
    {
        uint32_t val = speedup_throttle_sel->GetSelection() * 25;

        if (val > 0 && val <= 600) {
            speedup_throttle_spin->SetValue(val);
            speedup_frame_skip_ctrl.DoSetSpeedupFrameSkipSel(0);
            wxCommandEvent nil;
            speedup_frame_skip_ctrl.SetSpeedupFrameSkip(nil);
        }
        else
            speedup_throttle_spin->SetValue(0);
    }

    void Init(wxShowEvent& ev)
    {
        ev.Skip();
        DoSetSpeedupThrottleSel(speedup_throttle);
    }
} speedup_throttle_ctrl;

// set speedup_frame_skip_sel from speedup_frame_skip
void SpeedupFrameSkipCtrl_t::SetSpeedupFrameSkipSel(wxSpinEvent& evt)
{
    DoSetSpeedupFrameSkipSel(speedup_frame_skip_spin->GetValue());
}

void SpeedupFrameSkipCtrl_t::DoSetSpeedupFrameSkipSel(uint32_t val)
{
    if (val > 0 && val <= 30) {
        speedup_frame_skip_sel->SetSelection(val);
        speedup_throttle_ctrl.DoSetSpeedupThrottleSel(0);
        wxCommandEvent nil;
        speedup_throttle_ctrl.SetSpeedupThrottle(nil);
    }
    else
        speedup_frame_skip_sel->SetSelection(0);
}

// set speedup_frame_skip from speedup_frame_skip_sel
void SpeedupFrameSkipCtrl_t::SetSpeedupFrameSkip(wxCommandEvent& evt)
{
    uint32_t val = speedup_frame_skip_sel->GetSelection();

    if (val > 0 && val <= 30) {
        speedup_frame_skip_spin->SetValue(val);
        speedup_throttle_ctrl.DoSetSpeedupThrottleSel(0);
        wxCommandEvent nil;
        speedup_throttle_ctrl.SetSpeedupThrottle(nil);
    }
    else
        speedup_frame_skip_spin->SetValue(0);
}

void SpeedupFrameSkipCtrl_t::Init(wxShowEvent& ev)
{
    ev.Skip();
    DoSetSpeedupFrameSkipSel(speedup_frame_skip);
}

/////////////////////////////
//Check if a pointer from the XRC file is valid. If it's not, throw an error telling the user.
template <typename T>
void CheckThrowXRCError(T pointer, const wxString& name)
{
    if (pointer == NULL) {
        std::string errormessage = "Unable to load a \"";
        errormessage += typeid(pointer).name();
        errormessage += "\" from the builtin xrc file: ";
        errormessage += name.mb_str();
        throw std::runtime_error(errormessage);
    }
}

template <typename T>
void CheckThrowXRCError(T pointer, const char* name)
{
    if (pointer == NULL) {
        std::string errormessage = "Unable to load a \"";
        errormessage += typeid(pointer).name();
        errormessage += "\" from the builtin xrc file: ";
        errormessage += name;
        throw std::runtime_error(errormessage);
    }
}

wxDialog* MainFrame::LoadXRCDialog(const char* name)
{
    wxString dname = wxString::FromUTF8(name);
    wxDialog* dialog = wxXmlResource::Get()->LoadDialog(this, dname);
    CheckThrowXRCError(dialog, name);
/* wx-2.9.1 doesn't set parent for propertysheetdialogs for some reason */
/* this will generate a gtk warning but it is necessary for later */
/* retrieval using FindWindow() */
#if (wxMAJOR_VERSION < 3)

    if (!dialog->GetParent())
        dialog->Reparent(this);

#endif
    mark_recursive(dialog);
    return dialog;
}

wxDialog* MainFrame::LoadXRCropertySheetDialog(const char* name)
{
    wxString dname = wxString::FromUTF8(name);
    //Seems like the only way to do this
    wxObject* anObject = wxXmlResource::Get()->LoadObject(this, dname, wxEmptyString);
    wxDialog* dialog = dynamic_cast<wxDialog*>(anObject);
    CheckThrowXRCError(dialog, name);
/* wx-2.9.1 doesn't set parent for propertysheetdialogs for some reason */
/* this will generate a gtk warning but it is necessary for later */
/* retrieval using FindWindow() */
#if (wxMAJOR_VERSION < 3)

    if (!dialog->GetParent())
        dialog->Reparent(this);

#endif
    mark_recursive(dialog);
    return dialog;
}

//This just adds some error checking to the wx XRCCTRL macro
template <typename T>
T* SafeXRCCTRL(wxWindow* parent, const char* name)
{
    wxString dname = wxString::FromUTF8(name);
    //This is needed to work around a bug in XRCCTRL
    wxString Ldname = dname;
    T* output = XRCCTRL_D(*parent, dname, T);
    CheckThrowXRCError(output, name);
    return output;
}

template <typename T>
T* SafeXRCCTRL(wxWindow* parent, const wxString& name)
{
    T* output = XRCCTRL_D(*parent, name, T);
    CheckThrowXRCError(output, name);
    return output;
}

///Get an object, and set the appropriate validator
//T is the object type, and V is the validator type
template <typename T, typename V>
T* GetValidatedChild(wxWindow* parent, const char* name, V validator)
{
    T* child = SafeXRCCTRL<T>(parent, name);
    child->SetValidator(validator);
    return child;
}

wxAcceleratorEntry_v MainFrame::get_accels(wxAcceleratorEntry_v user_accels)
{
    // set global accelerators
    // first system
    wxAcceleratorEntry_v accels = sys_accels;

    // then user overrides
    // silently keep only last defined binding
    // same horribly inefficent O(n*m) search for duplicates as above..
    for (int i = 0; i < user_accels.size(); i++) {
        const wxAcceleratorEntry& ae = user_accels[i];

        for (wxAcceleratorEntry_v::iterator e = accels.begin(); e < accels.end(); ++e)
            if (ae.GetFlags() == e->GetFlags() && ae.GetKeyCode() == e->GetKeyCode()) {
                accels.erase(e);
                break;
            }

        accels.push_back(ae);
    }

    return accels;
}

void MainFrame::set_global_accels()
{
    wxAcceleratorEntry_v accels = get_accels(gopts.accels);
    // this is needed on Wine/win32 to support accels for close & quit
    wxGetApp().accels = accels;

    // Update menus; this probably takes the longest
    // as a side effect, any system-defined accels that weren't already in
    // the menus will be added now

    // first, zero out menu item on all accels
    for (int i = 0; i < accels.size(); i++)
        accels[i].Set(accels[i].GetFlags(), accels[i].GetKeyCode(),
            accels[i].GetCommand());

    // yet another O(n*m) loop.  I really ought to sort the accel arrays
    for (int i = 0; i < ncmds; i++) {
        wxMenuItem* mi = cmdtab[i].mi;

        if (!mi)
            continue;

        // only *last* accelerator is made visible in menu
        // and is flagged as such by setting menu item in accel
        // the last is chosen so menu overrides non-menu and user overrides
        // system
        int cmd = cmdtab[i].cmd_id;
        int last_accel = -1;

        for (int j = 0; j < accels.size(); j++)
            if (cmd == accels[j].GetCommand())
                last_accel = j;

        if (last_accel >= 0) {
            DoSetAccel(mi, &accels[last_accel]);
            accels[last_accel].Set(accels[last_accel].GetFlags(),
                accels[last_accel].GetKeyCode(),
                accels[last_accel].GetCommand(), mi);
        } else
            // clear out user-cleared menu items
            DoSetAccel(mi, NULL);
    }

    // Finally, install a global accelerator table for any non-menu accels
    int len = 0;

    for (int i = 0; i < accels.size(); i++)
        if (!accels[i].GetMenuItem())
            len++;

    if (len) {
        wxAcceleratorEntry tab[1000];

        for (int i = 0, j = 0; i < accels.size(); i++)
            if (!accels[i].GetMenuItem())
                tab[j++] = accels[i];

        wxAcceleratorTable atab(len, tab);
        // set the table on the panel, where focus usually is
        // otherwise accelerators are lost sometimes
        panel->SetAcceleratorTable(atab);
    } else
        panel->SetAcceleratorTable(wxNullAcceleratorTable);

    // save recent accels
    for (int i = 0; i < 10; i++)
        recent_accel[i] = wxAcceleratorEntry();

    for (int i = 0; i < accels.size(); i++)
        if (accels[i].GetCommand() >= wxID_FILE1 && accels[i].GetCommand() <= wxID_FILE10)
            recent_accel[accels[i].GetCommand() - wxID_FILE1] = accels[i];

    SetRecentAccels();
}

void MainFrame::MenuOptionBool(const char* menuName, bool& field)
{
    int id = wxXmlResource::GetXRCID(wxString(menuName, wxConvUTF8));

    for (int i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != id)
            continue;

        checkable_mi[i].boolopt = &field;
        checkable_mi[i].mi->Check(field);
        break;
    }
}

void MainFrame::MenuOptionIntMask(const char* menuName, int& field, int mask)
{
    int id = wxXmlResource::GetXRCID(wxString(menuName, wxConvUTF8));
    int value = mask;

    for (int i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != id)
            continue;

        checkable_mi[i].intopt = &field;
        checkable_mi[i].mask = mask;
        checkable_mi[i].val = value;
        checkable_mi[i].mi->Check((field & mask) == value);
        break;
    }
}

void MainFrame::MenuOptionIntRadioValue(const char* menuName, int& field, int value)
{
    int id = wxXmlResource::GetXRCID(wxString(menuName, wxConvUTF8));

    for (int i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != id)
            continue;

        checkable_mi[i].intopt = &field;
        checkable_mi[i].val = field;
        checkable_mi[i].mi->Check(field == value);
        break;
    }
}

// The Windows icon loading code is from:
//
// https://stackoverflow.com/questions/17949693/windows-volume-mixer-icon-size-is-too-large/46310786#46310786
//
// This works around a Windows bug where the icon is too large in the
// app-specific volume controls.

#ifdef __WXMSW__
    #include <windows.h>
    #include <versionhelpers.h>
    #include <commctrl.h>
    #include <wx/msw/private.h>
    typedef int (WINAPI *func_LoadIconWithScaleDown)(HINSTANCE, LPCWSTR, int, int, HICON*);
#endif

void MainFrame::BindAppIcon() {
#ifdef __WXMSW__
    if (IsWindowsVistaOrGreater()) {
        wxDynamicLibrary comctl32("comctl32", wxDL_DEFAULT | wxDL_QUIET);
        func_LoadIconWithScaleDown load_icon_scaled = reinterpret_cast<func_LoadIconWithScaleDown>(comctl32.GetSymbol("LoadIconWithScaleDown"));
        int icon_set_count = 0;

        HICON hIconLg;
        if (load_icon_scaled && SUCCEEDED(load_icon_scaled(wxGetInstance(), _T("AAAAA_MAINICON"), ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), &hIconLg))) {
            ::SendMessage(GetHandle(), WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIconLg));
            ++icon_set_count;
        }
        HICON hIconSm;
        if (load_icon_scaled && SUCCEEDED(load_icon_scaled(wxGetInstance(), _T("AAAAA_MAINICON"), ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), &hIconSm))) {
            ::SendMessage(GetHandle(), WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSm));
            ++icon_set_count;
        }

        if (icon_set_count == 2) return;
    }
    // otherwise fall back to Wx method of setting icon
#endif
    wxIcon icon = wxXmlResource::Get()->LoadIcon(wxT("MainIcon"));

    if (!icon.IsOk()) {
        wxLogInfo(_("Main icon not found"));
        icon = wxICON(wxvbam);
    }

    SetIcon(icon);
}

// If there is a menubar, store all special menuitems
#define XRCITEM_I(id) menubar->FindItem(id, NULL)
#define XRCITEM_D(s) XRCITEM_I(XRCID_D(s))
#define XRCITEM(s) XRCITEM_D(wxT(s))

bool MainFrame::BindControls()
{
    // Make sure display panel present and correct type
    panel = XRCCTRL(*this, "DisplayArea", GameArea);

    if (!panel) {
        wxLogError(_("Main display panel not found"));
        return false;
    }

    panel->SetMainFrame(this);

    panel->AdjustSize(false);
    // only the panel does idle events (the emulator loop)
    // however, do not enable until end of init, since errors will start
    // the idle loop on wxGTK
    wxIdleEvent::SetMode(wxIDLE_PROCESS_SPECIFIED);

    BindAppIcon();

    // NOOP if no status area
    SetStatusText(wxT(""));

    // Prepare system accel table
    for (int i = 0; i < num_def_accels; i++)
        sys_accels.push_back(default_accels[i]);

    wxMenuBar* menubar = GetMenuBar();
    ctx_menu = NULL;

    if (menubar) {
#if 0 // doesn't work in 2.9 at all (causes main menu to malfunction)
		// to fix, recursively copy entire menu insted of just copying
		// menubar.  This means that every saved menu item must also be
		// saved twice...  A lot of work for a mostly worthless feature.
		// If you want the menu, just exit full-screen mode.
		// Either that, or add an option to retain the regular
		// menubar in full-screen mode
		// create a context menu for fullscreen mode
		// FIXME: on gtk port, this gives Gtk-WARNING **:
		//   gtk_menu_attach_to_widget(): menu already attached to GtkMenuItem
		// but it works anyway
		// Note: menu default accelerators (e.g. alt-f for file menu) don't
		// work with context menu (and can never work, since there is no
		// way to pop up a submenu)
		// It would probably be better, in the end, to use a collapsed menu
		// bar (either Amiga-style press RMB to make appear, or Windows
		// collapsed toolbar-style move mouse to within a pixel of top to
		// make appear).  Not supported in wx without a lot of work, though.
		// Maybe this feature should just be dropped; the user would simply
		// have to exit fullscreen mode to use the menu.
		ctx_menu = new wxMenu();

		for (int i = 0; i < menubar->GetMenuCount(); i++)
			ctx_menu->AppendSubMenu(menubar->GetMenu(i), menubar->GetMenuLabel(i));

#endif

        // save all menu items in the command table
        for (int i = 0; i < ncmds; i++) {
            wxMenuItem* mi = cmdtab[i].mi = XRCITEM_I(cmdtab[i].cmd_id);
// remove unsupported commands first
#ifdef NO_FFMPEG

            if (cmdtab[i].mask_flags & (CMDEN_SREC | CMDEN_NSREC | CMDEN_VREC | CMDEN_NVREC)) {
                if (mi)
                    mi->GetMenu()->Remove(mi);

                cmdtab[i].cmd_id = XRCID("NOOP");
                cmdtab[i].mi = NULL;
                continue;
            }

#endif
#ifndef GBA_LOGGING

            if (cmdtab[i].cmd_id == XRCID("Logging")) {
                if (mi)
                    mi->GetMenu()->Remove(mi);

                cmdtab[i].cmd_id = XRCID("NOOP");
                cmdtab[i].mi = NULL;
                continue;
            }

#endif
#ifdef NO_LINK

            if (cmdtab[i].cmd_id == XRCID("LanLink") || cmdtab[i].cmd_id == XRCID("LinkType0Nothing") || cmdtab[i].cmd_id == XRCID("LinkType1Cable") || cmdtab[i].cmd_id == XRCID("LinkType2Wireless") || cmdtab[i].cmd_id == XRCID("LinkType3GameCube") || cmdtab[i].cmd_id == XRCID("LinkType4Gameboy") || cmdtab[i].cmd_id == XRCID("LinkAuto") || cmdtab[i].cmd_id == XRCID("SpeedOn") || cmdtab[i].cmd_id == XRCID("LinkProto") || cmdtab[i].cmd_id == XRCID("LinkConfigure")) {
                if (mi)
                    mi->GetMenu()->Remove(mi);

                cmdtab[i].cmd_id = XRCID("NOOP");
                cmdtab[i].mi = NULL;
                continue;
            }

#endif

            if (mi) {
                // wxgtk provides no way to retrieve stock label/accel
                // and does not override wxGetStockLabel()
                // as of 2.8.12/2.9.1
                // so override with wx's stock label  <sigh>
                // at least you still get gtk's stock icon
                if (mi->GetItemLabel().empty())
                    mi->SetItemLabel(wxGetStockLabel(mi->GetId(),
                        wxSTOCK_WITH_MNEMONIC | wxSTOCK_WITH_ACCELERATOR));

                // add accelerator to global accel table
                wxAcceleratorEntry* a = mi->GetAccel();

                if (a) {
                    a->Set(a->GetFlags(), a->GetKeyCode(), cmdtab[i].cmd_id, mi);

                    // only add it if not already there
                    for (wxAcceleratorEntry_v::iterator e = sys_accels.begin();
                         e < sys_accels.end(); ++e)
                        if (a->GetFlags() == e->GetFlags() && a->GetKeyCode() == e->GetKeyCode()) {
                            if (e->GetMenuItem()) {
                                wxLogInfo(_("Duplicate menu accelerator: %s for %s and %s; keeping first"),
                                    wxKeyTextCtrl::ToString(a->GetFlags(), a->GetKeyCode()).mb_str(),
                                    e->GetMenuItem()->GetItemLabelText().mb_str(),
                                    mi->GetItemLabelText().mb_str());
                                delete a;
                                a = 0;
                            } else {
                                if (e->GetCommand() != a->GetCommand()) {
                                    int cmd;

                                    for (cmd = 0; cmd < ncmds; cmd++)
                                        if (cmdtab[cmd].cmd_id == e->GetCommand())
                                            break;

                                    wxLogInfo(_("Menu accelerator %s for %s overrides default for %s ; keeping menu"),
                                        wxKeyTextCtrl::ToString(a->GetFlags(), a->GetKeyCode()).mb_str(),
                                        mi->GetItemLabelText().mb_str(),
                                        cmdtab[cmd].cmd);
                                }

                                sys_accels.erase(e);
                            }

                            break;
                        }

                    if (a)
                        sys_accels.push_back(*a);
                    else
                        // strip from label so user isn't confused
                        DoSetAccel(mi, NULL);
                }

                // store checkable items
                if (mi->IsCheckable()) {
                    checkable_mi_t cmi = { cmdtab[i].cmd_id, mi };
                    checkable_mi.push_back(cmi);

                    for (int j = 0; j < num_opts; j++) {
                        wxString menuName = wxString(opts[j].cmd, wxConvUTF8);

                        if (menuName == cmdtab[i].cmd) {
                            if (opts[j].intopt)
                                MenuOptionIntMask(opts[j].cmd, *opts[j].intopt, (1 << 0));
                            else if (opts[j].boolopt)
                                MenuOptionBool(opts[j].cmd, *opts[j].boolopt);
                        }
                    }
                }
            }
        }

        // if a recent menu is present, save its location
        wxMenuItem* recentmi = XRCITEM("RecentMenu");

        if (recentmi && recentmi->IsSubMenu()) {
            recent = recentmi->GetSubMenu();
            gopts.recent->UseMenu(recent);
            gopts.recent->AddFilesToMenu();
        } else
            recent = NULL;

        // if save/load state menu items present, save their locations
        for (int i = 0; i < 10; i++) {
            wxString n;
            n.Printf(wxT("LoadGame%02d"), i + 1);
            loadst_mi[i] = XRCITEM_D(n);
            n.Printf(wxT("SaveGame%02d"), i + 1);
            savest_mi[i] = XRCITEM_D(n);
        }
    } else {
        recent = NULL;

        for (int i = 0; i < 10; i++)
            loadst_mi[i] = savest_mi[i] = NULL;
    }

    // just setting to UNLOAD_CMDEN_KEEP is invalid
    // so just set individual flags here
    cmd_enable = CMDEN_NGDB_ANY | CMDEN_NREC_ANY;
    update_state_ts(true);

    // set pointers for checkable menu items
    // and set initial checked status
    if (checkable_mi.size()) {
        MenuOptionBool("RecentFreeze", gopts.recent_freeze);
        MenuOptionBool("Pause", paused);
        MenuOptionIntMask("SoundChannel1", gopts.sound_en, (1 << 0));
        MenuOptionIntMask("SoundChannel2", gopts.sound_en, (1 << 1));
        MenuOptionIntMask("SoundChannel3", gopts.sound_en, (1 << 2));
        MenuOptionIntMask("SoundChannel4", gopts.sound_en, (1 << 3));
        MenuOptionIntMask("DirectSoundA", gopts.sound_en, (1 << 8));
        MenuOptionIntMask("DirectSoundB", gopts.sound_en, (1 << 9));
        MenuOptionIntMask("VideoLayersBG0", layerSettings, (1 << 8));
        MenuOptionIntMask("VideoLayersBG1", layerSettings, (1 << 9));
        MenuOptionIntMask("VideoLayersBG2", layerSettings, (1 << 10));
        MenuOptionIntMask("VideoLayersBG3", layerSettings, (1 << 11));
        MenuOptionIntMask("VideoLayersOBJ", layerSettings, (1 << 12));
        MenuOptionIntMask("VideoLayersWIN0", layerSettings, (1 << 13));
        MenuOptionIntMask("VideoLayersWIN1", layerSettings, (1 << 14));
        MenuOptionIntMask("VideoLayersOBJWIN", layerSettings, (1 << 15));
        MenuOptionBool("CheatsAutoSaveLoad", gopts.autoload_cheats);
        MenuOptionIntMask("CheatsEnable", cheatsEnabled, 1);
        MenuOptionIntMask("KeepSaves", skipSaveGameBattery, 1);
        MenuOptionIntMask("KeepCheats", skipSaveGameCheats, 1);
        MenuOptionBool("LoadGameAutoLoad", gopts.autoload_state);
        MenuOptionIntMask("JoypadAutofireA", autofire, KEYM_A);
        MenuOptionIntMask("JoypadAutofireB", autofire, KEYM_B);
        MenuOptionIntMask("JoypadAutofireL", autofire, KEYM_LEFT);
        MenuOptionIntMask("JoypadAutofireR", autofire, KEYM_RIGHT);
        MenuOptionBool("EmulatorSpeedupToggle", turbo);
        MenuOptionIntRadioValue("LinkType0Nothing", gopts.gba_link_type, 0);
        MenuOptionIntRadioValue("LinkType1Cable", gopts.gba_link_type, 1);
        MenuOptionIntRadioValue("LinkType2Wireless", gopts.gba_link_type, 2);
        MenuOptionIntRadioValue("LinkType3GameCube", gopts.gba_link_type, 3);
        MenuOptionIntRadioValue("LinkType4Gameboy", gopts.gba_link_type, 4);
    }

    for (int i = 0; i < checkable_mi.size(); i++)
        if (!checkable_mi[i].boolopt && !checkable_mi[i].intopt) {
            wxLogError(_("Invalid menu item %s; removing"),
                checkable_mi[i].mi->GetItemLabelText().mb_str());
            checkable_mi[i].mi->GetMenu()->Remove(checkable_mi[i].mi);
            checkable_mi[i].mi = NULL;
        }

    set_global_accels();

    // preload and verify all resource dialogs
    // this will take init time and memory, but catches errors in xrc sooner
    // note that the only verification done is to ensure no crashes.  It's the
    // user's responsibility to ensure that the GUI works as intended after
    // modifications
    try {
        wxDialog* d = NULL;
        //// displayed during run
        d = LoadXRCDialog("GBPrinter");
        // just verify preview window & mag sel present
        {
            wxPanel* prev;
            prev = SafeXRCCTRL<wxPanel>(d, "Preview");

            if (!wxDynamicCast(prev->GetParent(), wxScrolledWindow))
                throw std::runtime_error("Unable to load a dialog control from the builtin xrc file: Preview");

            SafeXRCCTRL<wxControlWithItems>(d, "Magnification");
            d->Fit();
        }
        //// File menu
        d = LoadXRCDialog("GBAROMInfo");
        // just verify fields present
        wxControl* lab;
#define getlab(n) lab = SafeXRCCTRL<wxControl>(d, n)
        getlab("Title");
        getlab("GameCode");
        getlab("MakerCode");
        getlab("MakerName");
        getlab("UnitCode");
        getlab("DeviceType");
        getlab("Version");
        getlab("CRC");
        d->Fit();
        d = LoadXRCDialog("GBROMInfo");
        // just verify fields present
        getlab("Title");
        getlab("MakerCode");
        getlab("MakerName");
        getlab("UnitCode");
        getlab("DeviceType");
        getlab("Version");
        getlab("CRC");
        getlab("Color");
        getlab("ROMSize");
        getlab("RAMSize");
        getlab("DestCode");
        getlab("LicCode");
        getlab("Checksum");
        d->Fit();
        d = LoadXRCDialog("CodeSelect");
        // just verify list present
        SafeXRCCTRL<wxControlWithItems>(d, "CodeList");
        d->Fit();
        d = LoadXRCDialog("ExportSPS");
        // just verify text fields present
        SafeXRCCTRL<wxTextCtrl>(d, "Title");
        SafeXRCCTRL<wxTextCtrl>(d, "Description");
        SafeXRCCTRL<wxTextCtrl>(d, "Notes");
        d->Fit();
//// Emulation menu
#ifndef NO_LINK
        d = LoadXRCDialog("NetLink");
#endif
        wxRadioButton* rb;
#define getrbi(n, o, v)                              \
    do {                                             \
        rb = SafeXRCCTRL<wxRadioButton>(d, n);       \
        rb->SetValidator(wxBoolIntValidator(&o, v)); \
    } while (0)
#define getrbb(n, o)                              \
    do {                                          \
        rb = SafeXRCCTRL<wxRadioButton>(d, n);    \
        rb->SetValidator(wxGenericValidator(&o)); \
    } while (0)
#define getrbbr(n, o)                             \
    do {                                          \
        rb = SafeXRCCTRL<wxRadioButton>(d, n);    \
        rb->SetValidator(wxBoolRevValidator(&o)); \
    } while (0)
        wxBoolEnValidator* benval;
        wxBoolEnHandler* ben;
#define getbe(n, o, cv, t, wt)                                        \
    do {                                                              \
        cv = SafeXRCCTRL<t>(d, n);                                    \
        cv->SetValidator(wxBoolEnValidator(&o));                      \
        benval = wxStaticCast(cv->GetValidator(), wxBoolEnValidator); \
        static wxBoolEnHandler _ben;                                  \
        ben = &_ben;                                                  \
        wx##wt##BoolEnHandlerConnect(cv, wxID_ANY, _ben);             \
    } while (0)
        // brenval & friends are here just to allow yes/no radioboxes in place
        // of checkboxes.  A lot of work for little benefit.
        wxBoolRevEnValidator* brenval;
#define getbre(n, o, cv, t, wt)                                           \
    do {                                                                  \
        cv = SafeXRCCTRL<t>(d, n);                                        \
        cv->SetValidator(wxBoolRevEnValidator(&o));                       \
        brenval = wxStaticCast(cv->GetValidator(), wxBoolRevEnValidator); \
        wx##wt##BoolEnHandlerConnect(rb, wxID_ANY, *ben);                 \
    } while (0)
#define addbe(n)                       \
    do {                               \
        ben->controls.push_back(n);    \
        benval->controls.push_back(n); \
    } while (0)
#define addrbe(n)                       \
    do {                                \
        addbe(n);                       \
        brenval->controls.push_back(n); \
    } while (0)
#define addber(n, r)                   \
    do {                               \
        ben->controls.push_back(n);    \
        ben->reverse.push_back(r);     \
        benval->controls.push_back(n); \
        benval->reverse.push_back(r);  \
    } while (0)
#define addrber(n, r)                   \
    do {                                \
        addber(n, r);                   \
        brenval->controls.push_back(n); \
        brenval->reverse.push_back(r);  \
    } while (0)
#define getrbbe(n, o) getbe(n, o, rb, wxRadioButton, RBE)
#define getrbbd(n, o) getbre(n, o, rb, wxRadioButton, RBD)
        wxTextCtrl* tc;
#define gettc(n, o)                                           \
    do {                                                      \
        tc = SafeXRCCTRL<wxTextCtrl>(d, n);                   \
        tc->SetValidator(wxTextValidator(wxFILTER_NONE, &o)); \
    } while (0)
#define getdtc(n, o)                                          \
    do {                                                      \
        tc = SafeXRCCTRL<wxTextCtrl>(d, n);                   \
        tc->SetValidator(wxPositiveDoubleValidator(&o));      \
    } while (0)
#ifndef NO_LINK
        {
            net_link_handler.dlg = d;
            net_link_handler.n_players = linkNumPlayers;
            getrbbe("Server", net_link_handler.server);
            getrbbd("Client", net_link_handler.server);
            getlab("PlayersLab");
            addrber(lab, false);
            getrbi("Link2P", net_link_handler.n_players, 2);
            addrber(rb, false);
            getrbi("Link3P", net_link_handler.n_players, 3);
            addrber(rb, false);
            getrbi("Link4P", net_link_handler.n_players, 4);
            addrber(rb, false);
            getlab("ServerIPLab");
            addrber(lab, true);
            gettc("ServerIP", gopts.link_host);
            addrber(tc, true);
            wxWindow* okb = d->FindWindow(wxID_OK);

            if (okb) // may be gone if style guidlines removed it
            {
                net_link_handler.okb = wxStaticCast(okb, wxButton);
                d->Connect(XRCID("Server"), wxEVT_COMMAND_RADIOBUTTON_SELECTED,
                    wxCommandEventHandler(NetLink_t::ServerOKButton),
                    NULL, &net_link_handler);
                d->Connect(XRCID("Client"), wxEVT_COMMAND_RADIOBUTTON_SELECTED,
                    wxCommandEventHandler(NetLink_t::ClientOKButton),
                    NULL, &net_link_handler);
            }

            // this should intercept wxID_OK before the dialog handler gets it
            d->Connect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(NetLink_t::NetConnect),
                NULL, &net_link_handler);
            d->Fit();
        }
#endif
        d = LoadXRCDialog("CheatList");
        {
            cheat_list_handler.dlg = d;
            d->SetEscapeId(wxID_OK);
            wxCheckedListCtrl* cl;
            cl = SafeXRCCTRL<wxCheckedListCtrl>(d, "Cheats");

            if (!cl->Init())
                throw std::runtime_error("Unable to initialize the Cheats dialog control from the builtin xrc file!");

            cheat_list_handler.list = cl;
            cl->SetValidator(CheatListFill());
            cl->InsertColumn(0, _("Code"));
            // can't just set font for whole column; must set in each
            // individual item
            wxFont of = cl->GetFont();
            // of.SetFamily(wxFONTFAMILY_MODERN);  // doesn't work (no font change)
            wxFont f(of.GetPointSize(), wxFONTFAMILY_MODERN, of.GetStyle(),
                of.GetWeight());
            cheat_list_handler.item0.SetFont(f);
            cheat_list_handler.item0.SetColumn(0);
            cl->InsertColumn(1, _("Description"));
            // too bad I can't just set the size to windowwidth - other cols
            // default width is header width, but using following will probably
            // make it 80 pixels wide regardless
            // cl->SetColumnWidth(1, wxLIST_AUTOSIZE_USEHEADER);
            cheat_list_handler.col1minw = cl->GetColumnWidth(1);
            // on wxGTK, column 1 seems to inherit column 0's font regardless
            // of requested font
            cheat_list_handler.item1.SetFont(cl->GetFont());
            cheat_list_handler.item1.SetColumn(1);
#if 0
			// the ideal way to set col 0's width would be to use
			// wxLIST_AUTOSIZE after setting value to a sample:
			cheat_list_handler.item0.SetText(wxT("00000000 00000000"));
			cl->InsertItem(cheat_list_handler.item0);
			cl->SetColumnWidth(0, wxLIST_AUTOSIZE);
			cl->RemoveItem(0);
#else
            // however, the generic listctrl implementation uses the wrong
            // font to determine width (window vs. item), and does not
            // calculate the margins the same way in calculation vs. actual
            // drawing.  so calculate manually, using knowledge of underlying
            // code.  This is highly version-unportable, but better than using
            // buggy wx code..
            int w, h;
            cl->GetImageList(wxIMAGE_LIST_SMALL)->GetSize(0, w, h);
            w += 5; // IMAGE_MARGIN_IN_REPORT_MODE
            // following is missing from wxLIST_AUTOSIZE
            w += 8; // ??? subtracted from width avail for text
            {
                int charwidth, charheight;
                wxClientDC dc(cl);
                // following is item font instead of window font,
                // and so is missing from wxLIST_AUTOSIZE
                dc.SetFont(f);
                dc.GetTextExtent(wxT('M'), &charwidth, &charheight);
                w += (8 + 1 + 8) * charwidth;
            }
            cl->SetColumnWidth(0, w);
#endif
            d->Connect(wxEVT_COMMAND_TOOL_CLICKED,
                wxCommandEventHandler(CheatList_t::Tool),
                NULL, &cheat_list_handler);
            d->Connect(wxEVT_COMMAND_LIST_ITEM_CHECKED,
                wxListEventHandler(CheatList_t::Check),
                NULL, &cheat_list_handler);
            d->Connect(wxEVT_COMMAND_LIST_ITEM_UNCHECKED,
                wxListEventHandler(CheatList_t::UnCheck),
                NULL, &cheat_list_handler);
            d->Connect(wxEVT_COMMAND_LIST_ITEM_ACTIVATED,
                wxListEventHandler(CheatList_t::Edit),
                NULL, &cheat_list_handler);
            d->Fit();
        }
        d = LoadXRCDialog("CheatEdit");
        wxChoice* ch;
        {
            // d->Reparent(cheat_list_handler.dlg); // broken
            ch = GetValidatedChild<wxChoice, wxGenericValidator>(d, "Type", wxGenericValidator(&cheat_list_handler.ce_type));
            cheat_list_handler.ce_type_ch = ch;
            gettc("Desc", cheat_list_handler.ce_desc);
            tc->SetMaxLength(sizeof(cheatsList[0].desc) - 1);
            gettc("Codes", cheat_list_handler.ce_codes);
            cheat_list_handler.ce_codes_tc = tc;
            d->Fit();
        }
        d = LoadXRCDialog("CheatCreate");
        {
            cheat_find_handler.dlg = d;
            d->SetEscapeId(wxID_OK);
            CheatListCtrl* list;
            list = SafeXRCCTRL<CheatListCtrl>(d, "CheatList");
            cheat_find_handler.list = list;
            list->SetValidator(CheatFindFill());
            list->InsertColumn(0, _("Address"));
            list->InsertColumn(1, _("Old Value"));
            list->InsertColumn(2, _("New Value"));
            getrbi("EQ", cheat_find_handler.op, SEARCH_EQ);
            getrbi("NE", cheat_find_handler.op, SEARCH_NE);
            getrbi("LT", cheat_find_handler.op, SEARCH_LT);
            getrbi("LE", cheat_find_handler.op, SEARCH_LE);
            getrbi("GT", cheat_find_handler.op, SEARCH_GT);
            getrbi("GE", cheat_find_handler.op, SEARCH_GE);
#define cf_make_update()                                \
    rb->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,     \
        wxCommandEventHandler(CheatFind_t::UpdateView), \
        NULL, &cheat_find_handler)
            getrbi("Size8", cheat_find_handler.size, BITS_8);
            cf_make_update();
            getrbi("Size16", cheat_find_handler.size, BITS_16);
            cf_make_update();
            getrbi("Size32", cheat_find_handler.size, BITS_32);
            cf_make_update();
            getrbi("Signed", cheat_find_handler.fmt, CFVFMT_SD);
            cf_make_update();
            getrbi("Unsigned", cheat_find_handler.fmt, CFVFMT_UD);
            cf_make_update();
            getrbi("Hexadecimal", cheat_find_handler.fmt, CFVFMT_UH);
            cf_make_update();
#define cf_make_valen()                                \
    rb->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,    \
        wxCommandEventHandler(CheatFind_t::EnableVal), \
        NULL, &cheat_find_handler)
            getrbi("OldValue", cheat_find_handler.valsrc, 1);
            cf_make_valen();
            cheat_find_handler.old_rb = rb;
            rb->Disable();
            getrbi("SpecificValue", cheat_find_handler.valsrc, 0);
            cf_make_valen();
            cheat_find_handler.val_rb = rb;
            gettc("Value", cheat_find_handler.val_s);
            cheat_find_handler.val_tc = tc;
            wxStaticCast(tc->GetValidator(), wxTextValidator)->SetStyle(wxFILTER_INCLUDE_CHAR_LIST);
#define cf_button(n, f)                                \
    d->Connect(XRCID(n), wxEVT_COMMAND_BUTTON_CLICKED, \
        wxCommandEventHandler(CheatFind_t::f),         \
        NULL, &cheat_find_handler);
#define cf_enbutton(n, v)                                   \
    do {                                                    \
        cheat_find_handler.v = SafeXRCCTRL<wxButton>(d, n); \
        cheat_find_handler.v->Disable();                    \
    } while (0)
            cf_button("Search", Search);
            cf_button("Update", UpdateVals);
            cf_enbutton("Update", update_b);
            cf_button("Clear", ResetSearch);
            cf_enbutton("Clear", clear_b);
            cf_button("AddCheat", AddCheatB);
            cf_enbutton("AddCheat", add_b);
            d->Connect(wxEVT_COMMAND_LIST_ITEM_ACTIVATED,
                wxListEventHandler(CheatFind_t::AddCheatL),
                NULL, &cheat_find_handler);
            d->Connect(wxEVT_COMMAND_LIST_ITEM_SELECTED,
                wxListEventHandler(CheatFind_t::Select),
                NULL, &cheat_find_handler);
            d->Fit();
        }
        d = LoadXRCDialog("CheatAdd");
        {
            // d->Reparent(cheat_find_handler.dlg); // broken
            gettc("Desc", cheat_find_handler.ca_desc);
            tc->SetMaxLength(sizeof(cheatsList[0].desc) - 1);
            gettc("Value", cheat_find_handler.ca_val);
            cheat_find_handler.ca_val_tc = tc;
            // MFC interface used this for cheat list's generic code adder as well,
            // and made format selectable in interface.  I think the plain
            // interface is good enough, even though the format for GB cheats
            // is non-obvious.  Therefore, the format is now just a read-only
            // field.
            getlab("Format");
            cheat_find_handler.ca_fmt = lab;
            getlab("Address");
            cheat_find_handler.ca_addr = lab;
            d->Fit();
        }
        //// config menu
        d = LoadXRCDialog("GeneralConfig");
        wxCheckBox* cb;
#define getcbb(n, o)                              \
    do {                                          \
        cb = SafeXRCCTRL<wxCheckBox>(d, n);       \
        cb->SetValidator(wxGenericValidator(&o)); \
    } while (0)
#define getcbi(n, o)                                 \
    do {                                             \
        cb = SafeXRCCTRL<wxCheckBox>(d, n);          \
        cb->SetValidator(wxBoolIntValidator(&o, 1)); \
    } while (0)
        wxSpinCtrl* sc;
#define getsc(n, o)                               \
    do {                                          \
        sc = SafeXRCCTRL<wxSpinCtrl>(d, n);       \
        sc->SetValidator(wxGenericValidator(&o)); \
    } while (0)
#define getsc_uint(n, o)                               \
    do {                                          \
        sc = SafeXRCCTRL<wxSpinCtrl>(d, n);       \
        sc->SetValidator(wxUIntValidator(&o)); \
    } while (0)
        {
            // Online Auto Update check frequency
            getrbi("UpdateNever", gopts.onlineupdates, 0);
            getrbi("UpdateDaily", gopts.onlineupdates, 1);
            getrbi("UpdateWeekly", gopts.onlineupdates, 7);
            getrbi("PNG", captureFormat, 0);
            getrbi("BMP", captureFormat, 1);
            getsc("RewindInterval", gopts.rewind_interval);
            getsc_uint("Throttle", throttle);
            throttle_ctrl.thr = sc;
            throttle_ctrl.thrsel = SafeXRCCTRL<wxChoice>(d, "ThrottleSel");
            throttle_ctrl.thr->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED,
                wxSpinEventHandler(ThrottleCtrl_t::SetThrottleSel),
                NULL, &throttle_ctrl);
            throttle_ctrl.thrsel->Connect(wxEVT_COMMAND_CHOICE_SELECTED,
                wxCommandEventHandler(ThrottleCtrl_t::SetThrottle),
                NULL, &throttle_ctrl);
            d->Connect(wxEVT_SHOW, wxShowEventHandler(ThrottleCtrl_t::Init),
                NULL, &throttle_ctrl);
            d->Fit();
        }
        // SpeedUp Key Config
        d = LoadXRCDialog("SpeedupConfig");
        {
            getsc_uint("SpeedupThrottle", speedup_throttle);
            speedup_throttle_ctrl.speedup_throttle_spin = sc;
            speedup_throttle_ctrl.speedup_throttle_sel = SafeXRCCTRL<wxChoice>(d, "SpeedupThrottleSel");
            speedup_throttle_ctrl.speedup_throttle_spin->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED,
                wxSpinEventHandler(SpeedupThrottleCtrl_t::SetSpeedupThrottleSel),
                NULL, &speedup_throttle_ctrl);
            speedup_throttle_ctrl.speedup_throttle_sel->Connect(wxEVT_COMMAND_CHOICE_SELECTED,
                wxCommandEventHandler(SpeedupThrottleCtrl_t::SetSpeedupThrottle),
                NULL, &speedup_throttle_ctrl);
            d->Connect(wxEVT_SHOW, wxShowEventHandler(SpeedupThrottleCtrl_t::Init),
                NULL, &speedup_throttle_ctrl);
            d->Fit();

            getsc_uint("SpeedupFrameSkip", speedup_frame_skip);
            speedup_frame_skip_ctrl.speedup_frame_skip_spin = sc;
            speedup_frame_skip_ctrl.speedup_frame_skip_sel = SafeXRCCTRL<wxChoice>(d, "SpeedupFrameSkipSel");
            speedup_frame_skip_ctrl.speedup_frame_skip_spin->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED,
                wxSpinEventHandler(SpeedupFrameSkipCtrl_t::SetSpeedupFrameSkipSel),
                NULL, &speedup_frame_skip_ctrl);
            speedup_frame_skip_ctrl.speedup_frame_skip_sel->Connect(wxEVT_COMMAND_CHOICE_SELECTED,
                wxCommandEventHandler(SpeedupFrameSkipCtrl_t::SetSpeedupFrameSkip),
                NULL, &speedup_frame_skip_ctrl);
            d->Connect(wxEVT_SHOW, wxShowEventHandler(SpeedupFrameSkipCtrl_t::Init),
                NULL, &speedup_frame_skip_ctrl);
            d->Fit();
        }
#define getcbbe(n, o) getbe(n, o, cb, wxCheckBox, CB)
        wxBoolIntEnValidator* bienval;
#define getbie(n, o, v, cv, t, wt)                                        \
    do {                                                                  \
        cv = SafeXRCCTRL<t>(d, n);                                        \
        cv->SetValidator(wxBoolIntEnValidator(&o, v, v));                 \
        bienval = wxStaticCast(cv->GetValidator(), wxBoolIntEnValidator); \
        static wxBoolEnHandler _ben;                                      \
        ben = &_ben;                                                      \
        wx##wt##BoolEnHandlerConnect(cv, wxID_ANY, _ben);                 \
    } while (0)
#define addbie(n)                       \
    do {                                \
        ben->controls.push_back(n);     \
        bienval->controls.push_back(n); \
    } while (0)
#define addbier(n, r)                   \
    do {                                \
        ben->controls.push_back(n);     \
        ben->reverse.push_back(r);      \
        bienval->controls.push_back(n); \
        bienval->reverse.push_back(r);  \
    } while (0)
#define getcbie(n, o, v) getbie(n, o, v, cb, wxCheckBox, CB)
        wxFilePickerCtrl* fp;
#define getfp(n, o)                                     \
    do {                                                \
        fp = SafeXRCCTRL<wxFilePickerCtrl>(d, n);       \
        fp->SetValidator(wxFileDirPickerValidator(&o)); \
    } while (0)
        d = LoadXRCropertySheetDialog("GameBoyConfig");
        {
            /// System and Peripherals
            ch = GetValidatedChild<wxChoice, wxGenericValidator>(d, "System", wxGenericValidator(&gbEmulatorType));
            // "Display borders" corresponds to 2 variables, so it is handled
            // in command handler.  Plus making changes might require resizing
            // game area.  Validation only here.
            SafeXRCCTRL<wxChoice>(d, "Borders");
            /// Boot ROM
            getfp("BootRom", gopts.gb_bios);
            getlab("BootRomLab");
            getfp("CBootRom", gopts.gbc_bios);
            getlab("CBootRomLab");
            /// Custom Colors
            //getcbi("Color", gbColorOption);
            wxFarRadio* r = NULL;

            for (int i = 0; i < 3; i++) {
                wxString pn;
                // NOTE: wx2.9.1 behaves differently for referenced nodes
                // than 2.8!  Unless there is an actual child node, the ID field
                // will not be overwritten.  This means that there should be a
                // dummy child node (e.g. position=(0,0)).  If you get
                // "Unable to load dialog GameBoyConfig from resources", this is
                // probably the reason.
                pn.Printf(wxT("cp%d"), i + 1);
                wxWindow* w = SafeXRCCTRL<wxWindow>(d, pn);
                GBColorConfigHandler[i].p = w;
                GBColorConfigHandler[i].pno = i;
                wxFarRadio* cb = SafeXRCCTRL<wxFarRadio>(w, "UsePalette");

                if (r)
                    cb->SetGroup(r);
                else
                    r = cb;

                cb->SetValidator(wxBoolIntValidator(&gbPaletteOption, i));
                ch = SafeXRCCTRL<wxChoice>(w, "ColorSet");
                GBColorConfigHandler[i].c = ch;

                for (int j = 0; j < 8; j++) {
                    wxString s;
                    s.Printf(wxT("Color%d"), j);
                    wxColourPickerCtrl* cp = SafeXRCCTRL<wxColourPickerCtrl>(w, s);
                    GBColorConfigHandler[i].cp[j] = cp;
                    cp->SetValidator(wxColorValidator(&systemGbPalette[i * 8 + j]));
                }

                w->Connect(wxEVT_COMMAND_CHOICE_SELECTED,
                    wxCommandEventHandler(GBColorConfig_t::ColorSel),
                    NULL, &GBColorConfigHandler[i]);
                w->Connect(XRCID("Reset"), wxEVT_COMMAND_BUTTON_CLICKED,
                    wxCommandEventHandler(GBColorConfig_t::ColorReset),
                    NULL, &GBColorConfigHandler[i]);
                w->Connect(wxID_ANY, wxEVT_COMMAND_COLOURPICKER_CHANGED,
                    wxCommandEventHandler(GBColorConfig_t::ColorButton),
                    NULL, &GBColorConfigHandler[i]);
            }

            d->Fit();
        }
        d = LoadXRCropertySheetDialog("GameBoyAdvanceConfig");
        {
            /// System and peripherals
            ch = GetValidatedChild<wxChoice, wxGenericValidator>(d, "SaveType", wxGenericValidator(&cpuSaveType));
            BatConfigHandler.type = ch;
            ch = GetValidatedChild<wxChoice, wxGenericValidator>(d, "FlashSize", wxGenericValidator(&winFlashSize));
            BatConfigHandler.size = ch;
            d->Connect(XRCID("SaveType"), wxEVT_COMMAND_CHOICE_SELECTED,
                wxCommandEventHandler(BatConfig_t::ChangeType),
                NULL, &BatConfigHandler);
#define getgbaw(n)                             \
    do {                                       \
        wxWindow* w = d->FindWindow(XRCID(n)); \
        CheckThrowXRCError(w, n);              \
        w->SetValidator(GBACtrlEnabler());     \
    } while (0)
            getgbaw("Detect");
            d->Connect(XRCID("Detect"), wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(BatConfig_t::Detect),
                NULL, &BatConfigHandler);
            /// Boot ROM
            getfp("BootRom", gopts.gba_bios);
            getlab("BootRomLab");
            /// Game Overrides
            getgbaw("GameSettings");
            // the rest must be filled in by command handler; just validate
            SafeXRCCTRL<wxTextCtrl>(d, "Comment");
            SafeXRCCTRL<wxChoice>(d, "OvRTC");
            SafeXRCCTRL<wxChoice>(d, "OvSaveType");
            SafeXRCCTRL<wxChoice>(d, "OvFlashSize");
            SafeXRCCTRL<wxChoice>(d, "OvMirroring");
            d->Fit();
        }
        d = LoadXRCropertySheetDialog("DisplayConfig");
        {
            /// Speed
            // AutoSkip/FrameSkip are 2 controls for 1 value.  Needs post-process
            // to ensure checkbox not ignored
            getsc("FrameSkip", frameSkip);
            getlab("FrameSkipLab");
            int fs = frameSkip;

            if (fs >= 0)
                systemFrameSkip = fs;

            /// On-Screen Display
            ch = GetValidatedChild<wxChoice, wxGenericValidator>(d, "SpeedIndicator", wxGenericValidator(&showSpeed));
            /// Zoom
            getdtc("DefaultScale", gopts.video_scale);
            // this was a choice, but I'd rather not have to make an off-by-one
            // validator just for this, and spinctrl is good enough.
            getsc("MaxScale", maxScale);
            /// Basic
            getrbi("OutputSimple", gopts.render_method, RND_SIMPLE);
            getrbi("OutputQuartz2D", gopts.render_method, RND_QUARTZ2D);
#if !defined(__WXMAC__)
            rb->Hide();
#endif
            getrbi("OutputOpenGL", gopts.render_method, RND_OPENGL);
#ifdef NO_OGL
            rb->Hide();
#endif
#ifdef __WXGTK__
            // wxGLCanvas segfaults on Wayland
            if (wxGetApp().UsingWayland()) {
                rb->Hide();
            }
#endif
            getrbi("OutputDirect3D", gopts.render_method, RND_DIRECT3D);
#if !defined(__WXMSW__) || defined(NO_D3D) || 1 // not implemented
            rb->Hide();
#endif
            ch = GetValidatedChild<wxChoice, wxGenericValidator>(d, "Filter", wxGenericValidator(&gopts.filter));
            // these two are filled and/or hidden at dialog load time
            wxControl* pll;
            wxChoice* pl;
            pll = SafeXRCCTRL<wxControl>(d, "PluginLab");
            pl = SafeXRCCTRL<wxChoice>(d, "Plugin");
            pll->SetValidator(PluginEnabler());
            pl->SetValidator(PluginListFiller(d, pll, ch));
            PluginEnableHandler.lab = pll;
            PluginEnableHandler.ch = pl;
            ch->Connect(wxEVT_COMMAND_CHOICE_SELECTED,
                wxCommandEventHandler(PluginEnable_t::ToggleChoice),
                NULL, &PluginEnableHandler);
            ch = GetValidatedChild<wxChoice, wxGenericValidator>(d, "IFB", wxGenericValidator(&gopts.ifb));
            d->Fit();
        }
        d = LoadXRCropertySheetDialog("SoundConfig");
        wxSlider* sl;
#define getsl(n, o)                               \
    do {                                          \
        sl = SafeXRCCTRL<wxSlider>(d, n);         \
        sl->SetValidator(wxGenericValidator(&o)); \
    } while (0)
        {
            /// Basic
            getsl("Volume", gopts.sound_vol);
            sound_config_handler.vol = sl;
            d->Connect(XRCID("Volume100"), wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(SoundConfig_t::FullVol),
                NULL, &sound_config_handler);
            ch = GetValidatedChild<wxChoice, wxGenericValidator>(d, "Rate", wxGenericValidator(&gopts.sound_qual));
/// Advanced
#define audapi_rb(n, v)                                   \
    do {                                                  \
        getrbi(n, gopts.audio_api, v);                    \
        rb->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED,   \
            wxCommandEventHandler(SoundConfig_t::SetAPI), \
            NULL, &sound_config_handler);                 \
    } while (0)
            audapi_rb("SDL", AUD_SDL);
            audapi_rb("OpenAL", AUD_OPENAL);
#ifdef NO_OAL
            rb->Hide();
#endif
            audapi_rb("DirectSound", AUD_DIRECTSOUND);
#ifndef __WXMSW__
            rb->Hide();
#endif
            audapi_rb("XAudio2", AUD_XAUDIO2);
#if !defined(__WXMSW__) || defined(NO_XAUDIO2)
            rb->Hide();
#endif
            audapi_rb("FAudio", AUD_FAUDIO);
#ifdef NO_FAUDIO
            rb->Hide();
#endif
            sound_config_handler.dev = SafeXRCCTRL<wxChoice>(d, "Device");
            sound_config_handler.dev->SetValidator(SoundConfigLoad());
            getcbb("Upmix", gopts.upmix);
            sound_config_handler.umix = cb;
#if !defined(__WXMSW__) || defined(NO_XAUDIO2)
            cb->Hide();
#endif
            getcbb("HWAccel", gopts.dsound_hw_accel);
            sound_config_handler.hwacc = cb;
#ifndef __WXMSW__
            cb->Hide();
#endif
            getsl("Buffers", gopts.audio_buffers);
            sound_config_handler.bufs = sl;
            getlab("BuffersInfo");
            sound_config_handler.bufinfo = lab;
            sl->Connect(wxEVT_SCROLL_CHANGED,
                wxCommandEventHandler(SoundConfig_t::AdjustFramesEv),
                NULL, &sound_config_handler);
            sl->Connect(wxEVT_SCROLL_THUMBTRACK,
                wxCommandEventHandler(SoundConfig_t::AdjustFramesEv),
                NULL, &sound_config_handler);
            sound_config_handler.AdjustFrames(10);
            /// Game Boy
            SafeXRCCTRL<wxPanel>(d, "GBEnhanceSoundDep");
            getsl("GBEcho", gopts.gb_echo);
            getsl("GBStereo", gopts.gb_stereo);
            /// Game Boy Advance
            getsl("GBASoundFiltering", gopts.gba_sound_filter);
            d->Fit();
        }
        wxDirPickerCtrl* dp;
#define getdp(n, o)                                     \
    do {                                                \
        dp = SafeXRCCTRL<wxDirPickerCtrl>(d, n);        \
        dp->SetValidator(wxFileDirPickerValidator(&o)); \
    } while (0)
        d = LoadXRCDialog("DirectoriesConfig");
        {
            getdp("GBARoms", gopts.gba_rom_dir);
            getdp("GBRoms", gopts.gb_rom_dir);
            getdp("GBCRoms", gopts.gbc_rom_dir);
            getdp("BatSaves", gopts.battery_dir);
            getdp("StateSaves", gopts.state_dir);
            getdp("Screenshots", gopts.scrshot_dir);
            getdp("Recordings", gopts.recording_dir);
            d->Fit();
        }
        wxDialog* joyDialog = LoadXRCropertySheetDialog("JoypadConfig");
        wxFarRadio* r = 0;

        for (int i = 0; i < 4; i++) {
            wxString pn;
            // NOTE: wx2.9.1 behaves differently for referenced nodes
            // than 2.8!  Unless there is an actual child node, the ID field
            // will not be overwritten.  This means that there should be a
            // dummy child node (e.g. position=(0,0)).  If you get
            // "Unable to load dialog JoypadConfig from resources", this is
            // probably the reason.
            pn.Printf(wxT("joy%d"), i + 1);
            wxWindow* w = SafeXRCCTRL<wxWindow>(joyDialog, pn);
            wxFarRadio* cb;
            cb = SafeXRCCTRL<wxFarRadio>(w, "DefaultConfig");

            if (r)
                cb->SetGroup(r);
            else
                r = cb;

            cb->SetValidator(wxBoolIntValidator(&gopts.default_stick, i + 1));
            wxWindow *prev = NULL, *prevp = NULL;

            for (int j = 0; j < NUM_KEYS; j++) {
                wxJoyKeyTextCtrl* tc = XRCCTRL_D(*w, joynames[j], wxJoyKeyTextCtrl);
                CheckThrowXRCError(tc, joynames[j]);
                wxWindow* p = tc->GetParent();

                if (p == prevp)
                    tc->MoveAfterInTabOrder(prev);

                prev = tc;
                prevp = p;
                tc->SetValidator(wxJoyKeyValidator(&gopts.joykey_bindings[i][j]));
            }

            JoyPadConfigHandler[i].p = w;
            w->Connect(XRCID("Defaults"), wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(JoyPadConfig_t::JoypadConfigButtons),
                NULL, &JoyPadConfigHandler[i]);
            w->Connect(XRCID("Clear"), wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(JoyPadConfig_t::JoypadConfigButtons),
                NULL, &JoyPadConfigHandler[i]);
            joyDialog->Fit();
        }

#ifndef NO_LINK
        d = LoadXRCDialog("LinkConfig");
        {
            getlab("LinkTimeoutLab");
            addbe(lab);
            getsc("LinkTimeout", linkTimeout);
            addbe(sc);
            d->Fit();
        }
#endif
        d = LoadXRCDialog("AccelConfig");
        {
            wxTreeCtrl* tc;
            tc = SafeXRCCTRL<wxTreeCtrl>(d, "Commands");
            accel_config_handler.tc = tc;
            wxControlWithItems* lb;
            lb = SafeXRCCTRL<wxControlWithItems>(d, "Current");
            accel_config_handler.lb = lb;
            accel_config_handler.asb = SafeXRCCTRL<wxButton>(d, "Assign");
            accel_config_handler.remb = SafeXRCCTRL<wxButton>(d, "Remove");
            accel_config_handler.key = SafeXRCCTRL<wxKeyTextCtrl>(d, "Shortcut");
            accel_config_handler.curas = SafeXRCCTRL<wxControl>(d, "AlreadyThere");
            accel_config_handler.key->MoveBeforeInTabOrder(accel_config_handler.asb);
            accel_config_handler.key->SetMultikey(0);
            accel_config_handler.key->SetClearable(false);
            wxTreeItemId rid = tc->AddRoot(wxT("root"));

            if (menubar) {
                wxTreeItemId mid = tc->AppendItem(rid, _("Menu commands"));

                for (int i = 0; i < menubar->GetMenuCount(); i++) {
#if wxCHECK_VERSION(2, 8, 8)
                    wxTreeItemId id = tc->AppendItem(mid, menubar->GetMenuLabelText(i));
#else
                    // 2.8.4 has no equivalent for GetMenuLabelText()
                    wxString txt = menubar->GetMenuLabel(i);
                    txt.Replace(wxT("&"), wxT(""));
                    wxTreeItemId id = tc->AppendItem(mid, txt);
#endif
                    add_menu_accels(tc, id, menubar->GetMenu(i));
                }
            }

            wxTreeItemId oid;
            int noop_id = XRCID("NOOP");

            for (int i = 0; i < ncmds; i++) {
                if (cmdtab[i].mi || (recent && cmdtab[i].cmd_id >= wxID_FILE1 && cmdtab[i].cmd_id <= wxID_FILE10) || cmdtab[i].cmd_id == noop_id)
                    continue;

                if (!oid.IsOk())
                    oid = tc->AppendItem(rid, _("Other commands"));

                TreeInt* val = new TreeInt(i);
                tc->AppendItem(oid, cmdtab[i].name, -1, -1, val);
            }

            tc->ExpandAll();
            // FIXME: make this actually show the entire line w/o scrolling
            // BestSize cuts off on rhs; MaxSize is completely invalid
            wxSize sz = tc->GetBestSize();

            if (sz.GetHeight() > 200)
                sz.SetHeight(200);

            tc->SetSize(sz);
            sz.SetWidth(-1); // maybe allow it to become bigger
            tc->SetSizeHints(sz, sz);
            int w, h;
            lb->GetTextExtent(wxT("CTRL-ALT-SHIFT-ENTER"), &w, &h);
            sz.Set(w, h);
            lb->SetMinSize(sz);
            sz.Set(0, 0);
            wxControl* curas = accel_config_handler.curas;

            for (int i = 0; i < ncmds; i++) {
                wxString labs;
                treeid_to_name(i, labs, tc, tc->GetRootItem());
                curas->GetTextExtent(labs, &w, &h);

                if (w > sz.GetWidth())
                    sz.SetWidth(w);

                if (h > sz.GetHeight())
                    sz.SetHeight(h);
            }

            curas->SetSize(sz);
            curas->SetSizeHints(sz);
            tc->Connect(wxEVT_COMMAND_TREE_SEL_CHANGING,
                wxTreeEventHandler(AccelConfig_t::CommandSel),
                NULL, &accel_config_handler);
            tc->Connect(wxEVT_COMMAND_TREE_SEL_CHANGED,
                wxTreeEventHandler(AccelConfig_t::CommandSel),
                NULL, &accel_config_handler);
            d->Connect(wxEVT_SHOW, wxShowEventHandler(AccelConfig_t::Init),
                NULL, &accel_config_handler);
            d->Connect(wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(AccelConfig_t::Set),
                NULL, &accel_config_handler);
            d->Connect(XRCID("Assign"), wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(AccelConfig_t::Assign),
                NULL, &accel_config_handler);
            d->Connect(XRCID("Remove"), wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(AccelConfig_t::Remove),
                NULL, &accel_config_handler);
            d->Connect(XRCID("ResetAll"), wxEVT_COMMAND_BUTTON_CLICKED,
                wxCommandEventHandler(AccelConfig_t::ResetAll),
                NULL, &accel_config_handler);
            lb->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
                wxCommandEventHandler(AccelConfig_t::KeySel),
                NULL, &accel_config_handler);
            d->Connect(XRCID("Shortcut"), wxEVT_COMMAND_TEXT_UPDATED,
                wxCommandEventHandler(AccelConfig_t::CheckKey),
                NULL, &accel_config_handler);
            d->Fit();
        }
    } catch (std::exception& e) {
        wxLogError(wxString::FromUTF8(e.what()));
        return false;
    }

    //// Debug menu
    // actually, the viewers can be instantiated multiple times.
    // since they're for debugging, it's probably OK to just detect errors
    // at popup time.
    // The only one that can only be popped up once is logging, so allocate
    // and check it already.
    logdlg = new LogDialog;
// activate OnDropFile event handler
#if !defined(__WXGTK__) || wxCHECK_VERSION(2, 8, 10)
    // may not actually do anything, but verfied to work w/ Linux/Nautilus
    DragAcceptFiles(true);
#endif

    // delayed fullscreen
    if (wxGetApp().pending_fullscreen)
        panel->ShowFullScreen(true);

    MainFrame* mf = wxGetApp().frame;

    if (gopts.statusbar)
        mf->GetStatusBar()->Show();
    else
        mf->GetStatusBar()->Hide();

    if (gopts.keep_on_top)
        mf->SetWindowStyle(mf->GetWindowStyle() | wxSTAY_ON_TOP);
    else
        mf->SetWindowStyle(mf->GetWindowStyle() & ~wxSTAY_ON_TOP);

#ifndef NO_LINK
    LinkMode linkMode = GetConfiguredLinkMode();

    if (linkMode == LINK_GAMECUBE_DOLPHIN) {
        bool isv = !gopts.link_host.empty();

        if (isv) {
            isv = SetLinkServerHost(gopts.link_host.mb_str());
        }

        if (!isv) {
            wxLogError(_("JoyBus host invalid; disabling"));
        } else {
            linkMode = LINK_DISCONNECTED;
        }
    }

    ConnectionState linkState = InitLink(linkMode);

    if (linkState != LINK_OK) {
        CloseLink();
    }

    if (GetLinkMode() != LINK_DISCONNECTED) {
        cmd_enable |= CMDEN_LINK_ANY;
        SetLinkTimeout(linkTimeout);
        EnableSpeedHacks(linkHacks);
    }

    EnableNetworkMenu();
#endif
    enable_menus();
    panel->SetFrameTitle();
    // All OK; activate idle loop
    panel->SetExtraStyle(panel->GetExtraStyle() | wxWS_EX_PROCESS_IDLE);
    return true;
}

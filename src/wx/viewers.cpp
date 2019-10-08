
// these are all the viewer dialogs except for the ones with graphical areas
// they can be instantiated multiple times

#include "../common/cstdint.h"

#include "../gba/armdis.h"
#include "viewsupt.h"
#include "wxvbam.h"
#include <wx/ffile.h>
#include <wx/vlbox.h>

// avoid exporting classes
namespace Viewers {
#define gethex(v, n, l)                                 \
    do {                                                \
        v = XRCCTRL(*this, n, wxTextCtrl);              \
        if (!v)                                         \
            baddialog();                                \
        wxTextValidator hv(wxFILTER_INCLUDE_CHAR_LIST); \
        hv.SetIncludes(val_hexdigits);                  \
        v->SetValidator(hv);                            \
        v->SetMaxLength(l);                             \
    } while (0)

class DisassembleViewer : public Viewer {
public:
    DisassembleViewer()
        : Viewer(wxT("Disassemble"))
    {
        gethex(goto_addr, "GotoAddress", 8);
        goto_addr->SetFocus();

        for (int i = 0; i < 17; i++) {
            // it is unfortunately impossible to reliably assign
            // number ranges to XRC IDs, so the string name has to be
            // used every time
            wxString n;
            n.Printf(wxT("R%d"), i);
            regv[i] = XRCCTRL_D(*this, n, wxControl);

            if (!regv[i])
                baddialog();
        }

#define flagctrl(n)                         \
    do {                                    \
        n = XRCCTRL(*this, #n, wxCheckBox); \
        if (!n)                             \
            baddialog();                    \
    } while (0)
#define regctrl(n)                            \
    do {                                      \
        n##v = XRCCTRL(*this, #n, wxControl); \
        if (!n##v)                            \
            baddialog();                      \
    } while (0)
        flagctrl(N);
        flagctrl(Z);
        flagctrl(C);
        flagctrl(V);
        flagctrl(I);
        flagctrl(F);
        flagctrl(T);
        regctrl(Mode);
        dis = XRCCTRL(*this, "Disassembly", DisList);

        if (!dis)
            baddialog();

        // refit listing for longest line
        dis->Refit(70);
        Fit();
        SetMinSize(GetSize());
        dis->maxaddr = (uint32_t)~0;
        dismode = 0;
        GotoPC();
    }
    void Update()
    {
        GotoPC();
    }
    void Next(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        CPULoop(1);
        GotoPC();
    }
    void Goto(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        wxString as = goto_addr->GetValue();

        if (!as.size())
            return;

        long a;
        as.ToLong(&a, 16);
        dis->SetSel(a);
        UpdateDis();
    }
    // wx-2.8.4 or MacOSX compiler can't resolve overloads in evt table
    void GotoPCEv(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        GotoPC();
    }
    void GotoPC()
    {
#if 0

		// this is what the win32 interface used
		if (armState)
			dis->SetSel(armNextPC - 16);
		else
			dis->SetSel(armNextPC - 8);

		// doesn't make sense, though.  Maybe it's just trying to keep the
		// sel 4 instructions below top...
#endif
        dis->SetSel(armNextPC);
        UpdateDis();
    }
    void RefreshCmd(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        UpdateDis();
    }
    void UpdateDis()
    {
        N->SetValue(reg[16].I & 0x80000000);
        Z->SetValue(reg[16].I & 0x40000000);
        C->SetValue(reg[16].I & 0x20000000);
        V->SetValue(reg[16].I & 0x10000000);
        I->SetValue(reg[16].I & 0x00000080);
        F->SetValue(reg[16].I & 0x00000040);
        T->SetValue(reg[16].I & 0x00000020);
        wxString s;
        s.Printf(wxT("%02X"), reg[16].I & 0x1f);
        Modev->SetLabel(s);

        for (int i = 0; i < 17; i++) {
            s.Printf(wxT("%08X"), reg[i].I);
            regv[i]->SetLabel(s);
        }
    }

    void RefillListEv(wxCommandEvent& ev)
    {
        (void)ev; // unused params
        // what an unsafe calling convention
        // examination of disArm shows that max len is 69 chars
        // (e.g. 0x081cb6db), and I assume disThumb is shorter
        char buf[4096];
        dis->strings.clear();
        dis->addrs.clear();
        uint32_t addr = dis->topaddr;
        bool arm = dismode == 1 || (armState && dismode != 2);
        dis->back_size = arm ? 4 : 2;

        for (int i = 0; i < dis->nlines; i++) {
            dis->addrs.push_back(addr);

            if (arm)
                addr += disArm(addr, buf, 4096, DIS_VIEW_CODE | DIS_VIEW_ADDRESS);
            else
                addr += disThumb(addr, buf, 4096, DIS_VIEW_CODE | DIS_VIEW_ADDRESS);

            dis->strings.push_back(wxString(buf, wxConvLibc));
        }

        dis->Refill();
    }

    DisList* dis;
    wxTextCtrl* goto_addr;
    wxCheckBox *N, *Z, *C, *V, *I, *F, *T;
    int dismode;
    wxControl *regv[17], *Modev;
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(DisassembleViewer, Viewer)
EVT_COMMAND(wxID_ANY, EVT_REFILL_NEEDED, DisassembleViewer::RefillListEv)
EVT_BUTTON(XRCID("Goto"), DisassembleViewer::Goto)
EVT_TEXT_ENTER(XRCID("GotoAddress"), DisassembleViewer::Goto)
EVT_BUTTON(XRCID("GotoPC"), DisassembleViewer::GotoPCEv)
EVT_BUTTON(XRCID("Next"), DisassembleViewer::Next)
EVT_BUTTON(XRCID("Refresh"), DisassembleViewer::RefreshCmd)
END_EVENT_TABLE()

class GBDisassembleViewer : public Viewer {
public:
    GBDisassembleViewer()
        : Viewer(wxT("GBDisassemble"))
    {
        gethex(goto_addr, "GotoAddress", 4);
        goto_addr->SetFocus();
        regctrl(AF);
        regctrl(BC);
        regctrl(DE);
        regctrl(HL);
        regctrl(SP);
        regctrl(PC);
        regctrl(LY);
        regctrl(IFF);
        flagctrl(Z);
        flagctrl(N);
        flagctrl(H);
        flagctrl(C);
        dis = XRCCTRL(*this, "Disassembly", DisList);

        if (!dis)
            baddialog();

        // refit listing for longest line
        dis->Refit(26);
        Fit();
        SetMinSize(GetSize());
        dis->maxaddr = (uint32_t)~0;
        GotoPC();
    }
    void Update()
    {
        GotoPC();
    }
    void Next(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        gbEmulate(1);
        GotoPC();
    }
    void Goto(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        wxString as = goto_addr->GetValue();

        if (!as.size())
            return;

        long a;
        as.ToLong(&a, 16);
        dis->SetSel(a);
        UpdateDis();
    }
    // wx-2.8.4 or MacOSX compiler can't resolve overloads in evt table
    void GotoPCEv(wxCommandEvent& ev)
    {
        (void)ev; // unused params
        GotoPC();
    }
    void GotoPC()
    {
        dis->SetSel(PC.W);
        UpdateDis();
    }
    void RefreshCmd(wxCommandEvent& ev)
    {
        (void)ev; // unused params
        UpdateDis();
    }
    void UpdateDis()
    {
        Z->SetValue(AF.B.B0 & GB_Z_FLAG);
        N->SetValue(AF.B.B0 & GB_N_FLAG);
        H->SetValue(AF.B.B0 & GB_H_FLAG);
        C->SetValue(AF.B.B0 & GB_C_FLAG);
#define grv16(n, val)                    \
    do {                                 \
        wxString s;                      \
        s.Printf(wxT("%04X"), (int)val); \
        n##v->SetLabel(s);               \
    } while (0)
#define rv16(n) grv16(n, n.W)
#define srv16(n) grv16(n, n)
        rv16(AF);
        rv16(BC);
        rv16(DE);
        rv16(HL);
        rv16(SP);
        rv16(PC);
        srv16(IFF);
        wxString s;
        s.Printf(wxT("%02X"), register_LY);
        LYv->SetLabel(s);
    }

    void RefillListEv(wxCommandEvent& ev)
    {
        (void)ev; // unused params
        // what an unsafe calling convention
        // examination of gbDis shows that max len is 26 chars
        // (e.g. 0xe2)
        char buf[30];
        uint16_t addr = dis->topaddr;
        dis->strings.clear();
        dis->addrs.clear();
        dis->back_size = 1;

        for (int i = 0; i < dis->nlines; i++) {
            dis->addrs.push_back(addr);
            addr += gbDis(buf, addr);
            dis->strings.push_back(wxString(buf, wxConvLibc));
        }

        dis->Refill();
    }

    DisList* dis;
    wxTextCtrl* goto_addr;
    wxControl *AFv, *BCv, *DEv, *HLv, *SPv, *PCv, *LYv, *IFFv;
    wxCheckBox *Z, *N, *H, *C;
    DECLARE_EVENT_TABLE()
};
BEGIN_EVENT_TABLE(GBDisassembleViewer, Viewer)
EVT_COMMAND(wxID_ANY, EVT_REFILL_NEEDED, GBDisassembleViewer::RefillListEv)
EVT_BUTTON(XRCID("Goto"), GBDisassembleViewer::Goto)
EVT_TEXT_ENTER(XRCID("GotoAddress"), GBDisassembleViewer::Goto)
EVT_BUTTON(XRCID("GotoPC"), GBDisassembleViewer::GotoPCEv)
EVT_BUTTON(XRCID("Next"), GBDisassembleViewer::Next)
EVT_BUTTON(XRCID("Refresh"), GBDisassembleViewer::RefreshCmd)
END_EVENT_TABLE()
}

void MainFrame::Disassemble(void)
{
    switch (panel->game_type()) {
    case IMAGE_GBA:
        LoadXRCViewer(Disassemble);
        break;

    case IMAGE_GB:
        LoadXRCViewer(GBDisassemble);
        break;

    case IMAGE_UNKNOWN:
        // do nothing
        break;
    }
}

// for CPUWriteHalfWord
// and CPURead... below
#include "../gba/GBAinline.h"

namespace Viewers {
#include "ioregs.h"
class IOViewer : public Viewer {
public:
    IOViewer()
        : Viewer(wxT("IOViewer"))
    {
        for (int i = 0; i < 16; i++) {
            wxString s;
            s.Printf(wxT("B%d"), i);
            bit[i] = XRCCTRL_D(*this, s, wxCheckBox);
            s.append(wxT("lab"));
            bitlab[i] = XRCCTRL_D(*this, s, wxControl);

            if (!bit[i] || !bitlab[i])
                baddialog();
        }

        addr = XRCCTRL(*this, "Address", wxChoice);
        val = XRCCTRL(*this, "Value", wxControl);

        if (!addr || !val)
            baddialog();

        addr->Clear();
        wxString longline = lline;
        int lwidth = 0;

        for (long unsigned int i = 0; i < NUM_IOREGS; i++) {
            addr->Append(wxGetTranslation(ioregs[i].name));

            // find longest label
            // this is probably horribly expensive
            // cache for entire app...
            // and while at it, translate all the strings
            if (!lline) {
                for (int j = 0; j < 16; j++) {
                    if (ioregs[i].bits[j][0]) {
                        ioregs[i].bits[j] = wxGetTranslation(ioregs[i].bits[j]);
                        int w, h;
                        bitlab[0]->GetTextExtent(ioregs[i].bits[j], &w, &h);

                        if (w > lwidth) {
                            lwidth = w;
                            longline = ioregs[i].bits[j];
                        }
                    }
                }
            }
        }

        if (!lline)
            lline = longline;

        bitlab[0]->SetLabel(lline);
        Fit();
        addr->SetSelection(0);
        Select(0);
    }

    void SelectEv(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        Select(addr->GetSelection());
    }

    void Select(int sel)
    {
        int i;
        uint16_t mask;

        for (mask = 1, i = 0; mask; mask <<= 1, i++) {
            bit[i]->Enable(mask & ioregs[sel].write);
            bitlab[i]->SetLabel(ioregs[sel].bits[i]);
        }

        Update(sel);
    }

    void Update()
    {
        Update(addr->GetSelection());
    }

    void Update(int sel)
    {
        uint16_t* addr = ioregs[sel].address ? ioregs[sel].address : (uint16_t*)&ioMem[ioregs[sel].offset];
        uint16_t mask, reg = *addr;
        int i;

        for (mask = 1, i = 0; mask; mask <<= 1, i++)
            bit[i]->SetValue(mask & reg);

        wxString s;
        s.Printf(wxT("%04X"), reg);
        val->SetLabel(s);
    }

    void CheckBit(wxCommandEvent& ev)
    {
        for (int i = 0; i < 16; i++)
            if (ev.GetEventObject() == bit[i]) {
                // it'd be faster to store the value and just flip
                // the bit, but it's easier this way
                uint16_t mask, reg = 0;
                int j;

                for (mask = 1, j = 0; mask; mask <<= 1, j++)
                    if (bit[j]->GetValue())
                        reg |= mask;

                wxString s;
                s.Printf(wxT("%04X"), reg);
                val->SetLabel(s);
                return;
            }

        ev.Skip();
    }

    void RefreshEv(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        Update();
    }

    void Apply(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        int sel = addr->GetSelection();
        uint16_t* addr = ioregs[sel].address ? ioregs[sel].address : (uint16_t*)&ioMem[ioregs[sel].offset];
        uint16_t mask, reg = *addr;
        reg &= ~ioregs[sel].write;
        int i;

        for (mask = 1, i = 0; mask; mask <<= 1, i++) {
            if ((mask & ioregs[sel].write) && bit[i]->GetValue())
                reg |= mask;
        }

        CPUWriteHalfWord(0x4000000 + ioregs[sel].offset, reg);
        Update(sel);
    }

    static wxString lline;
    wxChoice* addr;
    wxControl* val;
    wxCheckBox* bit[16];
    wxControl* bitlab[16];
    DECLARE_EVENT_TABLE()
};
wxString IOViewer::lline;
BEGIN_EVENT_TABLE(IOViewer, Viewer)
EVT_BUTTON(XRCID("Refresh"), IOViewer::RefreshEv)
EVT_BUTTON(wxID_APPLY, IOViewer::Apply)
EVT_CHOICE(XRCID("Address"), IOViewer::SelectEv)
EVT_CHECKBOX(wxID_ANY, IOViewer::CheckBit)
END_EVENT_TABLE()
}

void MainFrame::IOViewer()
{
    LoadXRCViewer(IO);
}

#define getlogf(n, val)                                                 \
    do {                                                                \
        wxCheckBox* cb = XRCCTRL(*this, n, wxCheckBox);                 \
        if (!cb)                                                        \
            baddialog();                                                \
        cb->SetValidator(wxBoolIntValidator(&systemVerbose, val, val)); \
    } while (0)
LogDialog::LogDialog()
{
    const wxString dname = wxT("Logging");

    if (!wxXmlResource::Get()->LoadDialog(this, wxGetApp().frame, dname))
        baddialog();

    SetEscapeId(wxID_OK);
    getlogf("SWI", VERBOSE_SWI);
    getlogf("UnalignedMemory", VERBOSE_UNALIGNED_MEMORY);
    getlogf("IllWrite", VERBOSE_ILLEGAL_WRITE);
    getlogf("IllRead", VERBOSE_ILLEGAL_READ);
    getlogf("DMA0", VERBOSE_DMA0);
    getlogf("DMA1", VERBOSE_DMA1);
    getlogf("DMA2", VERBOSE_DMA2);
    getlogf("DMA3", VERBOSE_DMA3);
    getlogf("UndefInstruction", VERBOSE_UNDEFINED);
    getlogf("AGBPrint", VERBOSE_AGBPRINT);
    getlogf("SoundOut", VERBOSE_SOUNDOUTPUT);
    log = XRCCTRL(*this, "Log", wxTextCtrl);

    if (!log)
        baddialog();

    Fit();
}

void LogDialog::Update()
{
    wxString l = wxGetApp().log;
    log->SetValue(l);
    log->ShowPosition(l.size() > 2 ? l.size() - 2 : 0);
}

void LogDialog::Save(wxCommandEvent& ev)
{
    (void)ev; // unused params
    static wxString logdir = wxEmptyString, def_name = wxEmptyString;

    if (def_name.empty())
        def_name = wxGetApp().frame->GetPanel()->game_name() + wxT(".log");

    wxString pats = _("Text files (*.txt;*.log)|*.txt;*.log|");
    pats.append(wxALL_FILES);
    wxFileDialog dlg(this, _("Select output file"), logdir, def_name,
        pats, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    int ret = dlg.ShowModal();
    def_name = dlg.GetPath();
    logdir = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxFFile f(def_name, wxT("w"));

    if (f.IsOpened()) {
        f.Write(wxGetApp().log);
        f.Close();
    }
}

void LogDialog::Clear(wxCommandEvent& ev)
{
    (void)ev; // unused params
    wxGetApp().log.clear();
    Update();
}

BEGIN_EVENT_TABLE(LogDialog, wxDialog)
EVT_BUTTON(wxID_SAVE, LogDialog::Save)
EVT_BUTTON(XRCID("Clear"), LogDialog::Clear)
EVT_CHECKBOX(wxID_ANY, Viewers::Viewer::ActiveCtrl)
END_EVENT_TABLE()

// These are what mfc interface used.  Maybe it would be safer
// to avoid the Quick() routines in favor of the long ones...
#define CPUWriteByteQuick(addr, b) \
    ::map[(addr) >> 24].address[(addr) & ::map[(addr) >> 24].mask] = (b)
#define CPUWriteHalfWordQuick(addr, b) \
    WRITE16LE((uint16_t*)&::map[(addr) >> 24].address[(addr) & ::map[(addr) >> 24].mask], b)
#define CPUWriteMemoryQuick(addr, b) \
    WRITE32LE((uint32_t*)&::map[(addr) >> 24].address[(addr) & ::map[(addr) >> 24].mask], b)
#define GBWriteByteQuick(addr, b) \
    *((uint8_t*)&gbMemoryMap[(addr) >> 12][(addr)&0xfff]) = (b)
#define GBWriteHalfWordQuick(addr, b) \
    WRITE16LE((uint16_t*)&gbMemoryMap[(addr) >> 12][(addr)&0xfff], b)
#define GBWriteMemoryQuick(addr, b) \
    WRITE32LE((uint32_t*)&gbMemoryMap[(addr) >> 12][(addr)&0xfff], b)
#define GBReadMemoryQuick(addr) \
    READ32LE((uint32_t*)&gbMemoryMap[(addr) >> 12][(addr)&0xfff])

namespace Viewers {
static wxString memsave_dir = wxEmptyString;
class MemViewerBase : public Viewer {
public:
    MemViewerBase(uint32_t max)
        : Viewer(wxT("MemViewer"))
    {
        if (!(mv = XRCCTRL(*this, "MemView", MemView)))
            baddialog();

        if (!(bs = XRCCTRL(*this, "BlockStart", wxChoice)))
            baddialog();

        bs->Append(wxT(""));
        bs->SetFocus();
        mv->fmt = max > 0xffff ? 2 : 1;
        getradio(, "Fmt8", mv->fmt, 0);
        getradio(, "Fmt16", mv->fmt, 1);
        getradio(, "Fmt32", mv->fmt, 2);
        mv->maxaddr = max;
        addrlen = max > 0xffff ? 8 : 4;
        gethex(goto_addr, "GotoAddress", addrlen);
        wxControl* addr = XRCCTRL(*this, "CurAddress", wxControl);

        if (!addr)
            baddialog();

        addr->SetLabel(addrlen == 8 ? wxT("0xWWWWWWWW") : wxT("0xWWWW"));
        // refit listing for longest line
        mv->Refit();
        Fit();
        // don't let address display resize when window size changes
        addr->SetMinSize(addr->GetSize());
        mv->addrlab = addr;
        SetMinSize(GetSize());
        Goto(0);
        // initialize load/save support dialog already
        {
            const wxString dname = wxT("MemSelRegion");
            selregion = wxXmlResource::Get()->LoadDialog(this, dname);

            if (!selregion)
                baddialog();

#define this selregion // for gethex()
            gethex(selreg_addr, "Address", addrlen);
            gethex(selreg_len, "Size", addrlen);
#undef this
            selreg_lenlab = XRCCTRL(*selregion, "SizeLab", wxControl);
            selregion->Fit();
        }
    }
    void BlockStart(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        unsigned long l;
        bs->GetStringSelection().ToULong(&l, 0);
        Goto(l);
    }
    void GotoEv(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        unsigned long l;
        wxString v = goto_addr->GetValue();

        if (v.empty())
            return;

        v.ToULong(&l, 16);
        Goto(l);
    }
    void Goto(uint32_t addr)
    {
        mv->ShowAddr(addr, true);
    }
    void RefreshCmd(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        Update();
    }

    virtual void Update() {}

    void Load(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        if (memsave_fn.empty())
            memsave_fn = wxGetApp().frame->GetPanel()->game_name() + wxT(".dmp");

        wxString pats = _("Memory dumps (*.dmp;*.bin)|*.dmp;*.bin|");
        pats.append(wxALL_FILES);
        wxFileDialog dlg(this, _("Select memory dump file"), memsave_dir, memsave_fn,
            pats, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        int ret = dlg.ShowModal();
        memsave_fn = dlg.GetPath();
        memsave_dir = dlg.GetDirectory();

        if (ret != wxID_OK)
            return;

        wxFileName fn(memsave_fn);

        if (!fn.IsFileReadable()) {
            wxLogError(wxT("Can't open file %s"), memsave_fn.c_str());
            return;
        }

        unsigned long addr, len = fn.GetSize().ToULong();

        if (!len)
            return;

        wxString s;
        s.Printf(addrlen == 4 ? wxT("%04X") : wxT("%08X"), mv->GetAddr());
        selreg_addr->SetValue(s);
        selreg_len->Disable();
        selreg_lenlab->Disable();
        s.Printf(addrlen == 4 ? wxT("%04X") : wxT("%08X"), len);
        selreg_len->SetValue(s);
        selregion->SetWindowStyle(wxCAPTION | wxRESIZE_BORDER);

        if (gopts.keep_on_top)
            selregion->SetWindowStyle(selregion->GetWindowStyle() | wxSTAY_ON_TOP);
        else
            selregion->SetWindowStyle(selregion->GetWindowStyle() & ~wxSTAY_ON_TOP);

        if (selregion->ShowModal() != wxID_OK)
            return;

        selreg_addr->GetValue().ToULong(&addr, 16);
        MemLoad(memsave_fn, addr, len);
    }

    virtual void MemLoad(wxString& name, uint32_t addr, uint32_t len) = 0;

    void Save(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        wxString s;
        s.Printf(addrlen == 4 ? wxT("%04X") : wxT("%08X"), mv->GetAddr());
        selreg_addr->SetValue(s);
        selreg_len->Enable();
        selreg_lenlab->Enable();
        selreg_len->SetValue(wxEmptyString);
        selregion->SetWindowStyle(wxCAPTION | wxRESIZE_BORDER);

        if (gopts.keep_on_top)
            selregion->SetWindowStyle(selregion->GetWindowStyle() | wxSTAY_ON_TOP);
        else
            selregion->SetWindowStyle(selregion->GetWindowStyle() & ~wxSTAY_ON_TOP);

        if (selregion->ShowModal() != wxID_OK)
            return;

        unsigned long addr, len;
        selreg_addr->GetValue().ToULong(&addr, 16);
        selreg_len->GetValue().ToULong(&len, 16);

        if (memsave_fn.empty())
            memsave_fn = wxGetApp().frame->GetPanel()->game_name() + wxT(".dmp");

        wxString pats = _("Memory dumps (*.dmp;*.bin)|*.dmp;*.bin|");
        pats.append(wxALL_FILES);
        wxFileDialog dlg(this, _("Select output file"), memsave_dir, memsave_fn,
            pats, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        int ret = dlg.ShowModal();
        memsave_dir = dlg.GetDirectory();
        memsave_fn = dlg.GetPath();

        if (ret != wxID_OK)
            return;

        MemSave(memsave_fn, addr, len);
    }

    virtual void MemSave(wxString& name, uint32_t addr, uint32_t len) = 0;

protected:
    int addrlen;
    wxChoice* bs;
    wxTextCtrl* goto_addr;
    MemView* mv;

    wxDialog* selregion;
    wxTextCtrl *selreg_addr, *selreg_len;
    wxControl* selreg_lenlab;
    wxString memsave_fn;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MemViewerBase, Viewer)
EVT_CHOICE(XRCID("BlockStart"), MemViewerBase::BlockStart)
EVT_BUTTON(XRCID("Goto"), MemViewerBase::GotoEv)
EVT_TEXT_ENTER(XRCID("GotoAddress"), MemViewerBase::GotoEv)
EVT_BUTTON(XRCID("Refresh"), MemViewerBase::RefreshCmd)
EVT_BUTTON(wxID_SAVE, MemViewerBase::Save)
EVT_BUTTON(wxID_OPEN, MemViewerBase::Load)
END_EVENT_TABLE()

class MemViewer : public MemViewerBase {
public:
    MemViewer()
        : MemViewerBase(~0)
    {
        bs->Append(_("0x00000000 - BIOS"));
        bs->Append(_("0x02000000 - WRAM"));
        bs->Append(_("0x03000000 - IRAM"));
        bs->Append(_("0x04000000 - I/O"));
        bs->Append(_("0x05000000 - PALETTE"));
        bs->Append(_("0x06000000 - VRAM"));
        bs->Append(_("0x07000000 - OAM"));
        bs->Append(_("0x08000000 - ROM"));
        bs->SetSelection(1);
        Fit();
    }

    // wx-2.8.4 or MacOSX compiler can't resolve overloads in evt table
    void RefillListEv(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        Update();
    }

    void Update()
    {
        uint32_t addr = mv->topaddr;
        mv->words.resize(mv->nlines * 4);

        for (int i = 0; i < mv->nlines; i++) {
            if (i && !addr)
                break;

            for (int j = 0; j < 4; j++, addr += 4)
                mv->words[i * 4 + j] = CPUReadMemoryQuick(addr);
        }

        mv->Refill();
    }

    void WriteVal(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        switch (mv->fmt) {
        case 0:
            CPUWriteByteQuick(mv->writeaddr, mv->writeval);
            break;

        case 1:
            CPUWriteHalfWordQuick(mv->writeaddr, mv->writeval);
            break;

        case 2:
            CPUWriteMemoryQuick(mv->writeaddr, mv->writeval);
            break;
        }
    }

    void MemLoad(wxString& name, uint32_t addr, uint32_t len)
    {
        wxFFile f(name, wxT("rb"));

        if (!f.IsOpened())
            return;

        // this does the equivalent of the CPUWriteMemoryQuick()
        while (len > 0) {
            memoryMap m = map[addr >> 24];
            uint32_t off = addr & m.mask;
            int wlen = (off + len) > m.mask ? m.mask + 1 - off : len;
            wlen = f.Read(m.address + off, wlen);

            if (wlen < 0)
                return; // FIXME: give error

            len -= wlen;
            addr += wlen;
        }
    }

    void MemSave(wxString& name, uint32_t addr, uint32_t len)
    {
        wxFFile f(name, wxT("wb"));

        if (!f.IsOpened())
            return;

        // this does the equivalent of the CPUReadMemoryQuick()
        while (len > 0) {
            memoryMap m = map[addr >> 24];
            uint32_t off = addr & m.mask;
            int wlen = (off + len) > m.mask ? m.mask + 1 - off : len;
            wlen = f.Write(m.address + off, wlen);

            if (wlen < 0)
                return; // FIXME: give error

            len -= wlen;
            addr += wlen;
        }
    }

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MemViewer, MemViewerBase)
EVT_COMMAND(wxID_ANY, EVT_REFILL_NEEDED, MemViewer::RefillListEv)
EVT_COMMAND(wxID_ANY, EVT_WRITEVAL, MemViewer::WriteVal)
END_EVENT_TABLE()

class GBMemViewer : public MemViewerBase {
public:
    GBMemViewer()
        : MemViewerBase((uint16_t)~0)
    {
        bs->Append(_("0x0000 - ROM"));
        bs->Append(_("0x4000 - ROM"));
        bs->Append(_("0x8000 - VRAM"));
        bs->Append(_("0xA000 - SRAM"));
        bs->Append(_("0xC000 - RAM"));
        bs->Append(_("0xD000 - WRAM"));
        bs->Append(_("0xFF00 - I/O"));
        bs->Append(_("0xFF80 - RAM"));
        bs->SetSelection(1);
        Fit();
    }

    // wx-2.8.4 or MacOSX compiler can't resolve overloads in evt table
    void RefillListEv(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        Update();
    }

    void Update()
    {
        uint32_t addr = mv->topaddr;
        mv->words.resize(mv->nlines * 4);

        for (int i = 0; i < mv->nlines; i++) {
            if (i && !(uint16_t)addr)
                break;

            for (int j = 0; j < 4; j++, addr += 4)
                mv->words[i * 4 + j] = GBReadMemoryQuick(addr);
        }

        mv->Refill();
    }

    void WriteVal(wxCommandEvent& ev)
    {
	(void)ev; // unused params
        switch (mv->fmt) {
        case 0:
            GBWriteByteQuick(mv->writeaddr, mv->writeval);
            break;

        case 1:
            GBWriteHalfWordQuick(mv->writeaddr, mv->writeval);
            break;

        case 2:
            GBWriteMemoryQuick(mv->writeaddr, mv->writeval);
            break;
        }
    }

    void MemLoad(wxString& name, uint32_t addr, uint32_t len)
    {
        wxFFile f(name, wxT("rb"));

        if (!f.IsOpened())
            return;

        // this does the equivalent of the GBWriteMemoryQuick()
        while (len > 0) {
            uint8_t* maddr = gbMemoryMap[addr >> 12];
            uint32_t off = addr & 0xfff;
            int wlen = (off + len) > 0xfff ? 0x1000 - off : len;
            wlen = f.Read(maddr + off, wlen);

            if (wlen < 0)
                return; // FIXME: give error

            len -= wlen;
            addr += wlen;
        }
    }

    void MemSave(wxString& name, uint32_t addr, uint32_t len)
    {
        wxFFile f(name, wxT("wb"));

        if (!f.IsOpened())
            return;

        // this does the equivalent of the GBReadMemoryQuick()
        while (len > 0) {
            uint8_t* maddr = gbMemoryMap[addr >> 12];
            uint32_t off = addr & 0xfff;
            int wlen = (off + len) > 0xfff ? 0x1000 - off : len;
            wlen = f.Write(maddr + off, wlen);

            if (wlen < 0)
                return; // FIXME: give error

            len -= wlen;
            addr += wlen;
        }
    }

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(GBMemViewer, MemViewerBase)
EVT_COMMAND(wxID_ANY, EVT_REFILL_NEEDED, GBMemViewer::RefillListEv)
EVT_COMMAND(wxID_ANY, EVT_WRITEVAL, GBMemViewer::WriteVal)
END_EVENT_TABLE()
}

void MainFrame::MemViewer()
{
    switch (panel->game_type()) {
    case IMAGE_GBA:
        LoadXRCViewer(Mem);
        break;

    case IMAGE_GB:
        LoadXRCViewer(GBMem);
        break;

    default:
        break;
    }
}

#include "../common/ConfigManager.h"
#include "../common/SoundSDL.h"
#include "wxvbam.h"
#include "SDL.h"
#include <wx/ffile.h>
#include <wx/generic/prntdlgg.h>
#include <wx/print.h>
#include <wx/printdlg.h>

// These should probably be in vbamcore
int systemVerbose;
int systemFrameSkip;

int systemRedShift;
int systemGreenShift;
int systemBlueShift;
int systemColorDepth;
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
#define gs555(x) (x | (x << 5) | (x << 10))
uint16_t systemGbPalette[24] = {
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0
};
int RGB_LOW_BITS_MASK;

// these are local, though.
int joypress[4], autofire;
static int sensorx[4], sensory[4], sensorz[4];
bool pause_next;
bool turbo;

// and this is from MFC interface
bool soundBufferLow;

void systemMessage(int id, const char* fmt, ...)
{
    static char* buf = NULL;
    static int buflen = 80;
    va_list args;
    // auto-conversion of wxCharBuffer to const char * seems broken
    // so save underlying wxCharBuffer (or create one of none is used)
    wxCharBuffer _fmt(wxString(wxGetTranslation(wxString(fmt, wxConvLibc))).mb_str());

    if (!buf) {
        buf = (char*)malloc(buflen);

        if (!buf)
            exit(1);
    }

    while (1) {
        va_start(args, fmt);
        int needsz = vsnprintf(buf, buflen, _fmt.data(), args);
        va_end(args);

        if (needsz < buflen)
            break;

        while (buflen <= needsz)
            buflen *= 2;

        free(buf);
        buf = (char*)malloc(buflen);

        if (!buf)
            exit(1);
    }

    wxLogError(wxT("%s"), wxString(buf, wxConvLibc).mb_str());
}

static int frames = 0;

void systemDrawScreen()
{
    frames++;
    MainFrame* mf = wxGetApp().frame;
    mf->UpdateViewers();
    // FIXME: Sm60FPS crap and sondBufferLow crap
    GameArea* ga = mf->GetPanel();
#ifndef NO_FFMPEG

    if (ga)
        ga->AddFrame(pix);

#endif

    if (ga && ga->panel)
        ga->panel->DrawArea(&pix);
}

// record a game "movie"
// actually just game save state combined with a keystroke log
// doesn't work in GB "multiplayer" mode (only records default joypad)
//
//  <name>.vmv = keystroke log; all values little-endian ints:
//     <version>.32 = 1
//     for every joypad change (init to 0) and once at end of movie {
//        <timestamp>.32 = frames since start of movie
//        <joypad>.32 = default joypad reading at that time
//     }
//  <name>.vm0 = saved state

wxFFile game_file;
bool game_recording, game_playback;
uint32_t game_frame;
uint32_t game_joypad;

void systemStartGameRecording(const wxString& fname)
{
    GameArea* panel = wxGetApp().frame->GetPanel();

    if (!panel || panel->game_type() == IMAGE_UNKNOWN || !panel->emusys->emuWriteState) {
        wxLogError(_("No game in progress to record"));
        return;
    }

    systemStopGamePlayback();
    wxString fn = fname;

    if (fn.size() < 4 || !wxString(fn.substr(fn.size() - 4)).IsSameAs(wxT(".vmv"), false))
        fn.append(wxT(".vmv"));

    uint32_t version = 1;

    if (!game_file.Open(fn, wxT("wb")) || game_file.Write(&version, sizeof(version)) != sizeof(version)) {
        wxLogError(_("Cannot open output file %s"), fname.mb_str());
        return;
    }

    fn[fn.size() - 1] = wxT('0');

    if (!panel->emusys->emuWriteState(fn.mb_fn_str())) {
        wxLogError(_("Error writing game recording"));
        game_file.Close();
        return;
    }

    game_frame = 0;
    game_joypad = 0;
    game_recording = true;
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~(CMDEN_NGREC | CMDEN_GPLAY | CMDEN_NGPLAY);
    mf->cmd_enable |= CMDEN_GREC;
    mf->enable_menus();
}

void systemStopGameRecording()
{
    if (!game_recording)
        return;

    if (game_file.Write(&game_frame, sizeof(game_frame)) != sizeof(game_frame) || game_file.Write(&game_joypad, sizeof(game_joypad)) != sizeof(game_joypad) || !game_file.Close())
        wxLogError(_("Error writing game recording"));

    game_recording = false;
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~CMDEN_GREC;
    mf->cmd_enable |= CMDEN_NGREC | CMDEN_NGPLAY;
    mf->enable_menus();
}

uint32_t game_next_frame, game_next_joypad;

void systemStartGamePlayback(const wxString& fname)
{
    GameArea* panel = wxGetApp().frame->GetPanel();

    if (!panel || panel->game_type() == IMAGE_UNKNOWN || !panel->emusys->emuReadState) {
        wxLogError(_("No game in progress to record"));
        return;
    }

    if (game_recording) {
        wxLogError(_("Cannot play game recording while recording"));
        return;
    }

    systemStopGamePlayback();
    wxString fn = fname;

    if (fn.size() < 4 || !wxString(fn.substr(fn.size() - 4)).IsSameAs(wxT(".vmv"), false))
        fn.append(wxT(".vmv"));

    uint32_t version;

    if (!game_file.Open(fn, wxT("rb")) || game_file.Read(&version, sizeof(version)) != sizeof(version) || wxUINT32_SWAP_ON_BE(version) != 1) {
        wxLogError(_("Cannot open recording file %s"), fname.mb_str());
        return;
    }

    uint32_t gf, jp;

    if (game_file.Read(&gf, sizeof(gf)) != sizeof(gf) || game_file.Read(&jp, sizeof(jp)) != sizeof(jp)) {
        wxLogError(_("Error reading game recording"));
        game_file.Close();
        return;
    }

    game_next_frame = wxUINT32_SWAP_ON_BE(gf);
    game_next_joypad = wxUINT32_SWAP_ON_BE(jp);
    fn[fn.size() - 1] = wxT('0');

    if (!panel->emusys->emuReadState(fn.mb_fn_str())) {
        wxLogError(_("Error reading game recording"));
        game_file.Close();
        return;
    }

    game_frame = 0;
    game_joypad = 0;
    game_playback = true;
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~(CMDEN_NGREC | CMDEN_GREC | CMDEN_NGPLAY);
    mf->cmd_enable |= CMDEN_GPLAY;
    mf->enable_menus();
}

void systemStopGamePlayback()
{
    if (!game_playback)
        return;

    game_file.Close();
    game_playback = false;
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~CMDEN_GPLAY;
    mf->cmd_enable |= CMDEN_NGREC | CMDEN_NGPLAY;
    mf->enable_menus();
}

// updates the joystick data (done in background using wxSDLJoy)
bool systemReadJoypads()
{
    return true;
}

// return information about the given joystick, -1 for default joystick
uint32_t systemReadJoypad(int joy)
{
    if (joy < 0 || joy > 3)
        joy = gopts.default_stick - 1;

    uint32_t ret = joypress[joy];

    if (turbo)
        ret |= KEYM_SPEED;

    uint32_t af = autofire;

    if (ret & KEYM_AUTO_A) {
        ret |= KEYM_A;
        af |= KEYM_A;
    }

    if (ret & KEYM_AUTO_B) {
        ret |= KEYM_B;
        af |= KEYM_B;
    }

    static int autofire_trigger = 1;
    static bool autofire_state = true;
    uint32_t af_but = af & ret;

    if (af_but) {
        if (!autofire_state)
            ret &= ~af_but;

        if (!--autofire_trigger) {
            autofire_trigger = gopts.autofire_rate;
            autofire_state = !autofire_state;
        }
    } else {
        autofire_state = true;
        autofire_trigger = gopts.autofire_rate;
    }

    // disallow opposite directionals simultaneously
    ret &= ~((ret & (KEYM_LEFT | KEYM_DOWN | KEYM_MOTION_DOWN | KEYM_MOTION_RIGHT)) >> 1);
    ret &= REALKEY_MASK;

    if (game_recording) {
        uint32_t rret = ret & ~(KEYM_SPEED | KEYM_CAPTURE);

        if (rret != game_joypad) {
            game_joypad = rret;
            uint32_t gf = wxUINT32_SWAP_ON_BE(game_frame);
            uint32_t jp = wxUINT32_SWAP_ON_BE(game_joypad);

            if (game_file.Write(&gf, sizeof(gf)) != sizeof(gf) || game_file.Write(&jp, sizeof(jp)) != sizeof(jp)) {
                game_file.Close();
                game_recording = false;
                wxLogError(_("Error writing game recording"));
            }
        }
    } else if (game_playback) {
        while (game_frame >= game_next_frame) {
            game_joypad = game_next_joypad;
            uint32_t gf, jp;

            if (game_file.Read(&gf, sizeof(gf)) != sizeof(gf) || game_file.Read(&jp, sizeof(jp)) != sizeof(jp)) {
                game_file.Close();
                game_playback = false;
                wxString msg(_("Playback ended"));
                systemScreenMessage(msg);
                break;
            }

            game_next_frame = wxUINT32_SWAP_ON_BE(gf);
            game_next_joypad = wxUINT32_SWAP_ON_BE(jp);
        }

        ret = game_joypad;
    }

    return ret;
}

void systemShowSpeed(int speed)
{
    MainFrame* f = wxGetApp().frame;
    wxString s;
    s.Printf(_("%d%%(%d, %d fps)"), speed, systemFrameSkip, frames * speed / 100);

    switch (showSpeed) {
    case SS_NONE:
        f->GetPanel()->osdstat.clear();
        break;

    case SS_PERCENT:
        f->GetPanel()->osdstat.Printf(_("%d%%"), speed);
        break;

    case SS_DETAILED:
        f->GetPanel()->osdstat = s;
        break;
    }

    wxGetApp().frame->SetStatusText(s, 1);
    frames = 0;
}

int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

void system10Frames(int rate)
{
    GameArea* panel = wxGetApp().frame->GetPanel();
    int fs = frameSkip;

    if (fs < 0) {
        // I don't know why this algorithm isn't in common somewhere
        // as is, I copied it from SDL
        static uint32_t prevclock = 0;
        static int speedadj = 0;
        uint32_t t = systemGetClock();

        if (!panel->was_paused && prevclock && (t - prevclock) != 10000 / rate) {
            int speed = t == prevclock ? 100 * 10000 / rate - (t - prevclock) : 100;

            // why 98??
            if (speed >= 98)
                speedadj++;
            else if (speed < 80)
                speedadj -= (90 - speed) / 5;
            else
                speedadj--;

            if (speedadj >= 3) {
                speedadj = 0;

                if (systemFrameSkip > 0)
                    systemFrameSkip--;
            } else if (speedadj <= -2) {
                speedadj += 2;

                if (systemFrameSkip < 9)
                    systemFrameSkip++;
            }
        }

        prevclock = t;
        panel->was_paused = false;
    }

    if (gopts.rewind_interval) {
        if (!panel->rewind_time)
            panel->rewind_time = gopts.rewind_interval * 6;
        else if (!--panel->rewind_time)
            panel->do_rewind = true;
    }

    if (--systemSaveUpdateCounter == SYSTEM_SAVE_NOT_UPDATED)
        panel->SaveBattery();
    else if (systemSaveUpdateCounter < SYSTEM_SAVE_NOT_UPDATED)
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
}

void systemFrame()
{
    if (game_recording || game_playback)
        game_frame++;
}

// technically, num is ignored in favor of finding the first
// available slot
void systemScreenCapture(int num)
{
    GameArea* panel = wxGetApp().frame->GetPanel();
    wxFileName fn = wxFileName(wxGetApp().frame->GetGamePath(gopts.scrshot_dir), wxEmptyString);

    do {
        wxString bfn;
        bfn.Printf(wxT("%s%02d"), panel->game_name().mb_str(),
            num++);

        if (captureFormat == 0)
            bfn.append(wxT(".png"));
        else // if(gopts.cap_format == 1)
            bfn.append(wxT(".bmp"));

        fn.SetFullName(bfn);
    } while (fn.FileExists());

    fn.Mkdir(0777, wxPATH_MKDIR_FULL);

    if (captureFormat == 0)
        panel->emusys->emuWritePNG(fn.GetFullPath().mb_fn_str());
    else // if(gopts.cap_format == 1)
        panel->emusys->emuWriteBMP(fn.GetFullPath().mb_fn_str());

    wxString msg;
    msg.Printf(_("Wrote snapshot %s"), fn.GetFullPath().mb_str());
    systemScreenMessage(msg);
}

void systemSaveOldest()
{
    // I need to be implemented
}

void systemLoadRecent()
{
    // I need to be implemented
}

uint32_t systemGetClock()
{
    return wxGetApp().timer.Time();
}

void systemCartridgeRumble(bool) {}

static uint8_t sensorDarkness = 0xE8; // total darkness (including daylight on rainy days)

uint8_t systemGetSensorDarkness()
{
    return sensorDarkness;
}

void systemUpdateSolarSensor()
{
    uint8_t sun = 0x0; //sun = 0xE8 - 0xE8 (case 0 and default)
    int level = sunBars / 10;

    switch (level) {
    case 1:
        sun = 0xE8 - 0xE0;
        break;

    case 2:
        sun = 0xE8 - 0xDA;
        break;

    case 3:
        sun = 0xE8 - 0xD0;
        break;

    case 4:
        sun = 0xE8 - 0xC8;
        break;

    case 5:
        sun = 0xE8 - 0xC0;
        break;

    case 6:
        sun = 0xE8 - 0xB0;
        break;

    case 7:
        sun = 0xE8 - 0xA0;
        break;

    case 8:
        sun = 0xE8 - 0x88;
        break;

    case 9:
        sun = 0xE8 - 0x70;
        break;

    case 10:
        sun = 0xE8 - 0x50;
        break;

    default:
        break;
    }

    sensorDarkness = 0xE8 - sun;
}

void systemUpdateMotionSensor()
{
    for (int i = 0; i < 4; i++) {
        if (!sensorx[i])
            sensorx[i] = 2047;

        if (!sensory[i])
            sensory[i] = 2047;

        if (joypress[i] & KEYM_MOTION_LEFT) {
            sunBars--;

            if (sunBars < 1)
                sunBars = 1;

            sensorx[i] += 3;

            if (sensorx[i] > 2197)
                sensorx[i] = 2197;

            if (sensorx[i] < 2047)
                sensorx[i] = 2057;
        } else if (joypress[i] & KEYM_MOTION_RIGHT) {
            sunBars++;

            if (sunBars > 100)
                sunBars = 100;

            sensorx[i] -= 3;

            if (sensorx[i] < 1897)
                sensorx[i] = 1897;

            if (sensorx[i] > 2047)
                sensorx[i] = 2037;
        } else if (sensorx[i] > 2047) {
            sensorx[i] -= 2;

            if (sensorx[i] < 2047)
                sensorx[i] = 2047;
        } else {
            sensorx[i] += 2;

            if (sensorx[i] > 2047)
                sensorx[i] = 2047;
        }

        if (joypress[i] & KEYM_MOTION_UP) {
            sensory[i] += 3;

            if (sensory[i] > 2197)
                sensory[i] = 2197;

            if (sensory[i] < 2047)
                sensory[i] = 2057;
        } else if (joypress[i] & KEYM_MOTION_DOWN) {
            sensory[i] -= 3;

            if (sensory[i] < 1897)
                sensory[i] = 1897;

            if (sensory[i] > 2047)
                sensory[i] = 2037;
        } else if (sensory[i] > 2047) {
            sensory[i] -= 2;

            if (sensory[i] < 2047)
                sensory[i] = 2047;
        } else {
            sensory[i] += 2;

            if (sensory[i] > 2047)
                sensory[i] = 2047;
        }

        const int lowZ = -1800;
        const int centerZ = 0;
        const int highZ = 1800;
        const int accelZ = 3;

        if (joypress[i] & KEYM_MOTION_IN) {
            sensorz[i] += accelZ;

            if (sensorz[i] > highZ)
                sensorz[i] = highZ;

            if (sensorz[i] < centerZ)
                sensorz[i] = centerZ + (accelZ * 300);
        } else if (joypress[i] & KEYM_MOTION_OUT) {
            sensorz[i] -= accelZ;

            if (sensorz[i] < lowZ)
                sensorz[i] = lowZ;

            if (sensorz[i] > centerZ)
                sensorz[i] = centerZ - (accelZ * 300);
        } else if (sensorz[i] > centerZ) {
            sensorz[i] -= (accelZ * 100);

            if (sensorz[i] < centerZ)
                sensorz[i] = centerZ;
        } else {
            sensorz[i] += (accelZ * 100);

            if (sensorz[i] > centerZ)
                sensorz[i] = centerZ;
        }
    }

    systemUpdateSolarSensor();
}

int systemGetSensorX()
{
    return sensorx[gopts.default_stick - 1];
}

int systemGetSensorY()
{
    return sensory[gopts.default_stick - 1];
}

int systemGetSensorZ()
{
    return sensorz[gopts.default_stick - 1] / 10;
}

class PrintDialog : public wxEvtHandler, public wxPrintout {
public:
    PrintDialog(const uint16_t* data, int lines, bool cont);
    ~PrintDialog();
    int ShowModal()
    {
        dlg->SetWindowStyle(wxCAPTION | wxRESIZE_BORDER);

        if (gopts.keep_on_top)
            dlg->SetWindowStyle(dlg->GetWindowStyle() | wxSTAY_ON_TOP);
        else
            dlg->SetWindowStyle(dlg->GetWindowStyle() & ~wxSTAY_ON_TOP);

        CheckPointer(wxGetApp().frame);
        return wxGetApp().frame->ShowModal(dlg);
    }

private:
    void DoSave(wxCommandEvent&);
    void DoPrint(wxCommandEvent&);
    void ChangeMag(wxCommandEvent&);
    void ShowImg(wxPaintEvent&);
    bool OnPrintPage(int pno);
    void OnPreparePrinting();
    bool HasPage(int pno) { return pno <= npw * nph; }
    void GetPageInfo(int* minp, int* maxp, int* pfrom, int* pto)
    {
        *minp = 1;
        *maxp = npw * nph;
        *pfrom = 1;
        *pto = 1;
    }

    wxDialog* dlg;
    wxPanel* p;
    wxImage img;
    wxBitmap* bmp;
    wxControlWithItems* mag;

    static wxPrintData* printdata;
    static wxPageSetupDialogData* pagedata;
    wxRect margins;
    int npw, nph;

    DECLARE_CLASS(PrintDialog)
};

IMPLEMENT_CLASS(PrintDialog, wxEvtHandler)

PrintDialog::PrintDialog(const uint16_t* data, int lines, bool cont)
    : img(160, lines)
    , npw(1)
    , nph(1)
    , wxPrintout(wxGetApp().frame->GetPanel()->game_name() + wxT(" Printout"))
{
    dlg = wxStaticCast(wxGetApp().frame->FindWindow(XRCID("GBPrinter")), wxDialog);
    p = XRCCTRL(*dlg, "Preview", wxPanel);
    wxScrolledWindow* pp = wxStaticCast(p->GetParent(), wxScrolledWindow);
    wxSize sz(320, lines * 2);
    p->SetSize(sz);
    pp->SetVirtualSize(sz);

    if (lines > 144)
        sz.SetHeight(144 * 2);

    pp->SetSizeHints(sz, sz); // keep sizer from messing with size
    pp->SetSize(sz);
    pp->SetScrollRate(1, 1);
    // why is this so difficult?
    // I want the display area to be 320x288, not the area w/ scrollbars
    wxSize csz = pp->GetClientSize();
    sz += sz - csz;
    pp->SetSizeHints(sz, sz); // keep sizer from messing with size
    pp->SetSize(sz);
    dlg->Fit();
    // what a waste.  wxImage is brain-dead rgb24
    data += 162; // top border

    for (int y = 0; y < lines; y++) {
        for (int x = 0; x < 160; x++) {
            uint16_t d = *data++;
            img.SetRGB(x, y, ((d >> 10) & 0x1f) << 3, ((d >> 5) & 0x1f) << 3,
                (d & 0x1f) << 3);
        }

        data += 2; // rhs border
    }

    bmp = new wxBitmap(img.Scale(320, 2 * lines, wxIMAGE_QUALITY_HIGH));
    p->Connect(wxEVT_PAINT, wxPaintEventHandler(PrintDialog::ShowImg),
        NULL, this);
    dlg->Connect(wxID_SAVE, wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(PrintDialog::DoSave), NULL, this);
    dlg->Connect(wxID_PRINT, wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(PrintDialog::DoPrint), NULL, this);
    dlg->Connect(XRCID("Magnification"), wxEVT_COMMAND_CHOICE_SELECTED,
        wxCommandEventHandler(PrintDialog::ChangeMag), NULL, this);
    mag = XRCCTRL(*dlg, "Magnification", wxControlWithItems);
    mag->SetSelection(1);
    wxWindow* w = dlg->FindWindow(cont ? wxID_OK : wxID_SAVE);

    if (w)
        w->SetFocus();

    wxButton* cb = wxStaticCast(dlg->FindWindow(wxID_CANCEL), wxButton);

    if (cb)
        cb->SetLabel(_("&Discard"));
}

PrintDialog::~PrintDialog()
{
    p->Disconnect(wxID_ANY, wxEVT_PAINT);
    dlg->Disconnect(wxID_PRINT);
    dlg->Disconnect(wxID_SAVE);
    dlg->Disconnect(XRCID("Magnification"));
    delete bmp;
}

void PrintDialog::ShowImg(wxPaintEvent& evt)
{
    wxPaintDC dc(wxStaticCast(evt.GetEventObject(), wxWindow));
    dc.DrawBitmap(*bmp, 0, 0);
}

void PrintDialog::ChangeMag(wxCommandEvent& evt)
{
    int m = mag->GetSelection() + 1;
    wxScrolledWindow* pp = wxStaticCast(p->GetParent(), wxScrolledWindow);
    wxSize sz(m * 160, m * img.GetHeight());
    delete bmp;
    bmp = new wxBitmap(img.Scale(sz.GetWidth(), sz.GetHeight(), wxIMAGE_QUALITY_HIGH));
    p->SetSize(sz);
    pp->SetVirtualSize(sz);
    //    pp->Refresh();
}

static wxString prsav_path;
void PrintDialog::DoSave(wxCommandEvent&)
{
    wxString pats = _("Image files (*.bmp;*.jpg;*.png)|*.bmp;*.jpg;*.png|");
    pats.append(wxALL_FILES);
    wxString dn = wxGetApp().frame->GetPanel()->game_name();

    if (captureFormat == 0)
        dn.append(wxT(".png"));
    else // if(gopts.cap_format == 1)
        dn.append(wxT(".bmp"));

    wxFileDialog fdlg(dlg, _("Save printer image to"), prsav_path, dn,
        pats, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    int ret = fdlg.ShowModal();
    prsav_path = fdlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxString of = fdlg.GetPath();
    int m = mag->GetSelection() + 1;
    wxImage scimg = img.Scale(m * 160, m * img.GetHeight(), wxIMAGE_QUALITY_HIGH);

    if (scimg.SaveFile(of)) {
        wxString msg;
        msg.Printf(_("Wrote printer output to %s"), of.mb_str());
        systemScreenMessage(msg);
        wxButton* cb = wxStaticCast(dlg->FindWindow(wxID_CANCEL), wxButton);

        if (cb) {
            cb->SetLabel(_("&Close"));
            cb->SetFocus();
        }
    }
}

wxPrintData* PrintDialog::printdata = NULL;
wxPageSetupDialogData* PrintDialog::pagedata = NULL;

void PrintDialog::OnPreparePrinting()
{
    margins = GetLogicalPageMarginsRect(*pagedata);
    // strange... y is always negative
    margins.y = -margins.y;
    int w = bmp->GetWidth();
    int h = bmp->GetHeight();
    npw = (w + margins.width - 1) / margins.width;
    nph = (h + margins.height - 1) / margins.height;
}

bool PrintDialog::OnPrintPage(int pno)
{
    wxDC* dc = GetDC();
    dc->SetClippingRegion(margins);
    int xoff = (pno - 1) % npw;
    int yoff = (pno - 1) / npw;
    xoff *= margins.width;
    yoff *= margins.height;
    dc->DrawBitmap(*bmp, margins.x - xoff, margins.y - yoff);
    return true;
}

void PrintDialog::DoPrint(wxCommandEvent&)
{
    if (!printdata)
        printdata = new wxPrintData;

    if (!pagedata)
        pagedata = new wxPageSetupDialogData(*printdata);

// wxGTK-2.8.12/gnome-2.18.8: can't set margins in page setup
// wxMAC: need to pop up 2 dialogs to get margins
// why even bother using standard dialogs, then?
// well, maybe under msw, where generic dialog may not even exist...
#ifndef __WXMSW__
    wxGenericPageSetupDialog psdlg2(dlg, pagedata);
    psdlg2.ShowModal();
    *printdata = psdlg2.GetPageSetupDialogData().GetPrintData();
    *pagedata = psdlg2.GetPageSetupDialogData();
#else
    wxPageSetupDialog psdlg(dlg, pagedata);
    psdlg.ShowModal();
    *printdata = psdlg.GetPageSetupDialogData().GetPrintData();
    *pagedata = psdlg.GetPageSetupDialogData();
#ifdef __WXMAC__
    // FIXME: is this necessary?  useful? functional?
    wxMacPageMarginsDialog pmdlg(dlg, pagedata);
    pmdlg.ShowModal();
    *printdata = pmdlg.GetPageSetupDialogData().GetPrintData();
    *pagedata = pmdlg.GetPageSetupDialogData();
#endif
#endif
    wxPrintDialogData prdlg(*printdata);
    wxPrinter printer(&prdlg);

    if (printer.Print(dlg, this)) {
        wxString msg = _("Printed");
        systemScreenMessage(msg);
        wxButton* cb = wxStaticCast(dlg->FindWindow(wxID_CANCEL), wxButton);

        if (cb) {
            cb->SetLabel(_("&Close"));
            cb->SetFocus();
        }

        *printdata = printer.GetPrintDialogData().GetPrintData();
    }
}

void systemGbPrint(uint8_t* data, int len, int pages, int feed, int pal, int cont)
{
    ModalPause mp; // this might take a while, so signal a pause
    GameArea* panel = wxGetApp().frame->GetPanel();
    static uint16_t* accum_prdata;
    static int accum_prdata_len = 0, accum_prdata_size = 0;
    static uint16_t prdata[162 * 145] = { 0 };
    int lines = len / 40;
    uint16_t* out = prdata + 162; // 1-pix top border

    for (int y = 0; y < lines / 8; y++) {
        for (int x = 0; x < 160 / 8; x++) {
            for (int k = 0; k < 8; k++) {
                int a = *data++;
                int b = *data++;

                for (int j = 0, mask = 0x80; j < 8; j++, mask >>= 1) {
                    int c = ((a & mask) | ((b & mask) << 1)) >> (7 - j);

                    // the guide I read said 0 is on top, but if I do that,
                    // output seems reversed.  So either 11 in pal means
                    // white, or 00 in c means bits 0-1 in pal.
                    // the guide I read also said 00 is white and 11 is black
                    // I'm going with "00 in c means bits 0-1 in pal"
                    if (pal != 0xe4) // 11 10 01 00
                        c = (pal & (3 << c * 2)) >> c * 2;

                    out[j + k * 162] = systemGbPalette[c];
                }
            }

            out += 8;
        }

        out += 2 + 7 * 162; // 2-pix rhs border
    }

    // assume no bottom margin means "more coming"
    // probably ought to make this time out somehow
    // or at the very least dump when the game state changes
    uint16_t* to_print = prdata;

    if ((gopts.print_auto_page && !(feed & 15)) || accum_prdata_len) {
        if (!accum_prdata_len)
            accum_prdata_len = 162; // top border

        accum_prdata_len += lines * 162;

        if (accum_prdata_size < accum_prdata_len) {
            if (!accum_prdata_size)
                accum_prdata = (uint16_t*)calloc(accum_prdata_len, 2);
            else
                accum_prdata = (uint16_t*)realloc(accum_prdata, accum_prdata_len * 2);

            accum_prdata_size = accum_prdata_len;
        }

        memcpy(accum_prdata + accum_prdata_len - lines * 162, prdata + 162,
            lines * 162 * 2);

        if (gopts.print_auto_page && !(feed & 15))
            return;

        to_print = accum_prdata;
        lines = accum_prdata_len / 162 - 1;
        accum_prdata_len = 0;
    }

    if (gopts.print_screen_cap) {
        wxFileName fn = wxFileName(wxGetApp().frame->GetGamePath(gopts.scrshot_dir), wxEmptyString);
        int num = 1;

        do {
            wxString bfn;
            bfn.Printf(wxT("%s-print%02d"), panel->game_name().mb_str(),
                num++);

            if (captureFormat == 0)
                bfn.append(wxT(".png"));
            else // if(gopts.cap_format == 1)
                bfn.append(wxT(".bmp"));

            fn.SetFullName(bfn);
        } while (fn.FileExists());

        fn.Mkdir(0777, wxPATH_MKDIR_FULL);
        int d = systemColorDepth;
        int rs = systemRedShift, bs = systemBlueShift, gs = systemGreenShift;
        systemColorDepth = 16;
        systemRedShift = 10;
        systemGreenShift = 5;
        systemBlueShift = 0;
        wxString of = fn.GetFullPath();
        bool ret = captureFormat == 0 ? utilWritePNGFile(of.mb_fn_str(), 160, lines, (uint8_t*)to_print) : utilWriteBMPFile(of.mb_fn_str(), 160, lines, (uint8_t*)to_print);

        if (ret) {
            wxString msg;
            msg.Printf(_("Wrote printer output to %s"), of.mb_str());
            systemScreenMessage(msg);
        }

        systemColorDepth = d;
        systemRedShift = rs;
        systemGreenShift = gs;
        systemBlueShift = bs;
        return;
    }

    PrintDialog dlg(to_print, lines, !(feed & 15));
    int ret = dlg.ShowModal();

    if (ret == wxID_OK) {
        accum_prdata_len = (lines + 1) * 162;

        if (to_print != accum_prdata) {
            if (accum_prdata_size < accum_prdata_len) {
                if (!accum_prdata_size)
                    accum_prdata = (uint16_t*)calloc(accum_prdata_len, 2);
                else
                    accum_prdata = (uint16_t*)realloc(accum_prdata, accum_prdata_len * 2);

                accum_prdata_size = accum_prdata_len;
            }

            memcpy(accum_prdata, to_print, accum_prdata_len * 2);
        }
    }
}

void systemScreenMessage(const wxString& msg)
{
    if (wxGetApp().frame && wxGetApp().frame->IsShown()) {
        wxPuts(msg);
        MainFrame* f = wxGetApp().frame;
        GameArea* panel = f->GetPanel();

        if (!panel->osdtext.empty())
            f->PopStatusText();

        f->PushStatusText(msg);
        panel->osdtext = msg;
        panel->osdtime = systemGetClock();
    }
}

void systemScreenMessage(const char* msg)
{
    systemScreenMessage(wxString(msg, wxConvLibc));
}

bool systemCanChangeSoundQuality()
{
#ifndef NO_FFMPEG

    if (emulating) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        if (panel)
            return !panel->IsRecording();
    }

#endif
    return wxGetApp().IsMainLoopRunning();
}

bool systemPauseOnFrame()
{
    if (pause_next) {
        pause_next = false;
        MainFrame* fr = wxGetApp().frame;
        fr->GetPanel()->Pause();
        return true;
    }

    return false;
}

void systemGbBorderOn()
{
    GameArea* panel = wxGetApp().frame->GetPanel();

    if (panel)
        panel->AddBorder();
}

class SoundDriver;
SoundDriver* systemSoundInit()
{
    soundShutdown();

    switch (gopts.audio_api) {
    case AUD_SDL:
        return new SoundSDL();
#ifndef NO_OAL

    case AUD_OPENAL:
        return newOpenAL();
#endif
#ifdef __WXMSW__

    case AUD_DIRECTSOUND:
        return newDirectSound();
#ifndef NO_XAUDIO2

    case AUD_XAUDIO2:
        return newXAudio2_Output();
#endif
#endif

    default:
        gopts.audio_api = 0;
    }

    return 0;
}

void systemOnWriteDataToSoundBuffer(const uint16_t* finalWave, int length)
{
#ifndef NO_FFMPEG
    GameArea* panel = wxGetApp().frame->GetPanel();

    if (panel)
        panel->AddFrame(finalWave, length);

#endif
}

void systemOnSoundShutdown()
{
}

extern int (*remoteSendFnc)(char*, int);
extern int (*remoteRecvFnc)(char*, int);
extern void (*remoteCleanUpFnc)();

#ifndef __WXMSW__
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/poll.h>

static wxString pty_slave;
static int pty_master = -1;

static int debugReadPty(char* buf, int len)
{
    while (1) {
        struct pollfd fd;
        fd.fd = pty_master;
        fd.events = POLLIN;

        if (poll(&fd, 1, 200) != 0)
            return read(pty_master, buf, len);
        else
            return -2; // try to let repaints & such run
    }
}

static int debugWritePty(/* const */ char* buf, int len)
{
    return write(pty_master, buf, len);
}

static void debugClosePty()
{
    if (pty_master >= 0) {
        close(pty_master);
        pty_master = -1;
    }
}

bool debugOpenPty()
{
    if (pty_master >= 0) // should never happen
        close(pty_master);

    const char* slave_name;

    if ((pty_master = posix_openpt(O_RDWR | O_NOCTTY)) < 0 || grantpt(pty_master) < 0 || unlockpt(pty_master) < 0 || !(slave_name = ptsname(pty_master))) {
        wxLogError(_("Error opening pseudo tty: %s"), wxString(strerror(errno),
                                                          wxConvLibc)
                                                          .mb_str());

        if (pty_master >= 0) {
            close(pty_master);
            pty_master = -1;
        }

        return false;
    }

    pty_slave = wxString(slave_name, wxConvLibc);
    remoteSendFnc = debugWritePty;
    remoteRecvFnc = debugReadPty;
    remoteCleanUpFnc = debugClosePty;
    return true;
}

const wxString& debugGetSlavePty()
{
    return pty_slave;
}

bool debugWaitPty()
{
    if (pty_master < 0)
        return false;

    struct pollfd fd;
    fd.fd = pty_master;
    fd.events = POLLIN;
    return poll(&fd, 1, 100) > 0;
}
#endif

#include <wx/socket.h>

static wxSocketServer* debug_server = NULL;
wxSocketBase* debug_remote = NULL;

static int debugReadSock(char* buf, int len)
{
    debug_remote->SetFlags(wxSOCKET_BLOCK);
    MainFrame* f = wxGetApp().frame;
    // WaitForRead does not obey wxSOCKET_BLOCK
    // It's hard to prevent menu command selection while GUI is active
    // so this will likely fail if the user isn't careful
    // the only fix in place is to prevent recursive idle loop
    f->StartModal();
    bool done = debug_remote->WaitForRead(0, 200);
    f->StopModal();

    if (!done)
        return -2;

    debug_remote->Read(buf, len);

    if (debug_remote->Error())
        return -1;

    return debug_remote->LastCount();
}

static int debugWriteSock(char* buf, int len)
{
    debug_remote->SetFlags(wxSOCKET_WAITALL | wxSOCKET_BLOCK);
    debug_remote->Write(buf, len);

    if (debug_remote->Error())
        return -1;

    return debug_remote->LastCount();
}

static void debugCloseSock()
{
    delete debug_remote;
    debug_remote = NULL;
    delete debug_server;
    debug_server = NULL;
}

bool debugStartListen(int port)
{
    delete debug_server; // should never be necessary
    delete debug_remote;
    debug_remote = NULL;
    wxIPV4address addr;
    addr.Service(port);
    addr.AnyAddress(); // probably ought to have a flag to select any/localhost
    debug_server = new wxSocketServer(addr, wxSOCKET_REUSEADDR);
    remoteSendFnc = debugWriteSock;
    remoteRecvFnc = debugReadSock;
    remoteCleanUpFnc = debugCloseSock;

    if (debug_server->IsOk())
        return true;

    wxLogError(_("Error setting up server socket (%d)"), debug_server->LastError());
    return false;
}

bool debugWaitSocket()
{
    if (!debug_server)
        return false;

    if (debug_remote)
        return true;

    debug_server->WaitForAccept(0, 100);
    debug_remote = debug_server->Accept(false);
    return debug_remote != NULL;
}

void log(const char* defaultMsg, ...)
{
    static FILE* out = NULL;
    va_list valist;
    char buf[2048];
    va_start(valist, defaultMsg);
    vsnprintf(buf, 2048, defaultMsg, valist);
    va_end(valist);
    wxGetApp().log.append(wxString(buf, wxConvLibc));

    if (wxGetApp().IsMainLoopRunning()) {
        LogDialog* d = wxGetApp().frame->logdlg;

        if (d && d->IsShown()) {
            d->Update();
        }

        systemScreenMessage(buf);
    }

    if (out == NULL) {
        // FIXME: this should be an option
        wxFileName trace_log(wxGetApp().GetConfigurationPath(), wxT("trace.log"));
        out = fopen(trace_log.GetFullPath().mb_str(), "w");

        if (!out)
            return;
    }

    va_start(valist, defaultMsg);
    vfprintf(out, defaultMsg, valist);
    va_end(valist);
}

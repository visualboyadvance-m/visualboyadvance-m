#include "wxvbam.h"

#include <algorithm>
#include <wx/aboutdlg.h>
#include <wx/ffile.h>
#include <wx/numdlg.h>
#include <wx/progdlg.h>
#include <wx/regex.h>
#include <wx/sstream.h>
#include <wx/url.h>
#include <wx/wfstream.h>
#include <wx/msgdlg.h>

#include "../common/version_cpp.h"
#include "../gb/gbPrinter.h"
#include "../gba/agbprint.h"
#include "config/option.h"

#if (wxMAJOR_VERSION < 3)
#define GetXRCDialog(n) \
    wxStaticCast(wxGetApp().frame->FindWindow(XRCID(n)), wxDialog)
#else
#define GetXRCDialog(n) \
    wxStaticCast(wxGetApp().frame->FindWindowByName(n), wxDialog)
#endif

void GDBBreak(MainFrame* mf);

bool cmditem_lt(const struct cmditem& cmd1, const struct cmditem& cmd2)
{
    return wxStrcmp(cmd1.cmd, cmd2.cmd) < 0;
}

void MainFrame::GetMenuOptionBool(const wxString& menuName, bool* field)
{
    assert(field);
    *field = !*field;
    int id = wxXmlResource::GetXRCID(menuName);

    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != id)
            continue;

        *field = checkable_mi[i].mi->IsChecked();
        break;
    }
}

void MainFrame::GetMenuOptionConfig(const wxString& menu_name,
                                    const config::OptionID& option_id) {
    config::Option* option = config::Option::ByID(option_id);
    assert(option);

    int id = wxXmlResource::GetXRCID(menu_name);
    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != id)
            continue;

        const bool is_checked = checkable_mi[i].mi->IsChecked();
        switch (option->type()) {
            case config::Option::Type::kBool:
                option->SetBool(is_checked);
                break;
            case config::Option::Type::kInt:
                option->SetInt(is_checked);
                break;
            default:
                assert(false);
                return;
        }
        break;
    }
}

void MainFrame::GetMenuOptionInt(const wxString& menuName, int* field, int mask)
{
    assert(field);
    int value = mask;
    bool is_checked = ((*field) & (mask)) != (value);
    int id = wxXmlResource::GetXRCID(menuName);

    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != id)
            continue;

        is_checked = checkable_mi[i].mi->IsChecked();
        break;
    }

    *field = ((*field) & ~(mask)) | (is_checked ? (value) : 0);
}

void MainFrame::SetMenuOption(const wxString& menuName, int value)
{
    int id = wxXmlResource::GetXRCID(menuName);

    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != id)
            continue;

        checkable_mi[i].mi->Check(value);
        break;
    }
}

static void toggleBooleanVar(bool *menuValue, bool *globalVar)
{
    if (*menuValue == *globalVar) // used accelerator
        *globalVar = !(*globalVar);
    else // used menu item
        *globalVar = *menuValue;
}

static void toggleBitVar(bool *menuValue, int *globalVar, int mask)
{
    bool isEnabled = ((*globalVar) & (mask)) != (mask);
    if (*menuValue == isEnabled)
        *globalVar = ((*globalVar) & ~(mask)) | (!isEnabled ? (mask) : 0);
    else
        *globalVar = ((*globalVar) & ~(mask)) | (*menuValue ? (mask) : 0);
    *menuValue = ((*globalVar) & (mask)) != (mask);
}

//// File menu

static int open_ft = 0;
static wxString open_dir;

EVT_HANDLER(wxID_OPEN, "Open ROM...")
{
    open_dir = wxGetApp().GetAbsolutePath(gopts.gba_rom_dir);
    // FIXME: ignore if non-existent or not a dir
    wxString pats = _(
        "GameBoy Advance Files (*.agb;*.gba;*.bin;*.elf;*.mb;*.zip;*.7z;*.rar)|"
        "*.agb;*.gba;*.bin;*.elf;*.mb;"
        "*.agb.gz;*.gba.gz;*.bin.gz;*.elf.gz;*.mb.gz;"
        "*.agb.z;*.gba.z;*.bin.z;*.elf.z;*.mb.z;"
        "*.zip;*.7z;*.rar"
        "|GameBoy Files (*.dmg;*.gb;*.gbc;*.cgb;*.sgb;*.zip;*.7z;*.rar)|"
        "*.dmg;*.gb;*.gbc;*.cgb;*.sgb;"
        "*.dmg.gz;*.gb.gz;*.gbc.gz;*.cgb.gz;*.sgb.gz;"
        "*.dmg.z;*.gb.z;*.gbc.z;*.cgb.z;*.sgb.z;"
        "*.zip;*.7z;*.rar"
        "|");
    pats.append(wxALL_FILES);
    wxFileDialog dlg(this, _("Open ROM file"), open_dir, wxT(""),
        pats,
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    dlg.SetFilterIndex(open_ft);

    if (ShowModal(&dlg) == wxID_OK)
        wxGetApp().pending_load = dlg.GetPath();

    open_ft = dlg.GetFilterIndex();
    open_dir = dlg.GetDirectory();
}

EVT_HANDLER(OpenGB, "Open GB...")
{
    open_dir = wxGetApp().GetAbsolutePath(gopts.gb_rom_dir);
    // FIXME: ignore if non-existent or not a dir
    wxString pats = _(
        "GameBoy Files (*.dmg;*.gb;*.gbc;*.cgb;*.sgb;*.zip;*.7z;*.rar)|"
        "*.dmg;*.gb;*.gbc;*.cgb;*.sgb;"
        "*.dmg.gz;*.gb.gz;*.gbc.gz;*.cgb.gz;*.sgb.gz;"
        "*.dmg.z;*.gb.z;*.gbc.z;*.cgb.z;*.sgb.z;"
        "*.zip;*.7z;*.rar"
        "|");
    pats.append(wxALL_FILES);
    wxFileDialog dlg(this, _("Open GB ROM file"), open_dir, wxT(""),
        pats,
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    dlg.SetFilterIndex(open_ft);

    if (ShowModal(&dlg) == wxID_OK)
        wxGetApp().pending_load = dlg.GetPath();

    open_ft = dlg.GetFilterIndex();
    open_dir = dlg.GetDirectory();
}

EVT_HANDLER(OpenGBC, "Open GBC...")
{
    open_dir = wxGetApp().GetAbsolutePath(gopts.gbc_rom_dir);
    // FIXME: ignore if non-existent or not a dir
    wxString pats = _(
        "GameBoy Color Files (*.dmg;*.gb;*.gbc;*.cgb;*.sgb;*.zip;*.7z;*.rar)|"
        "*.dmg;*.gb;*.gbc;*.cgb;*.sgb;"
        "*.dmg.gz;*.gb.gz;*.gbc.gz;*.cgb.gz;*.sgb.gz;"
        "*.dmg.z;*.gb.z;*.gbc.z;*.cgb.z;*.sgb.z;"
        "*.zip;*.7z;*.rar"
        "|");
    pats.append(wxALL_FILES);
    wxFileDialog dlg(this, _("Open GBC ROM file"), open_dir, wxT(""),
        pats,
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    dlg.SetFilterIndex(open_ft);

    if (ShowModal(&dlg) == wxID_OK)
        wxGetApp().pending_load = dlg.GetPath();

    open_ft = dlg.GetFilterIndex();
    open_dir = dlg.GetDirectory();
}

EVT_HANDLER(RecentReset, "Reset recent ROM list")
{
    // only save config if there were items to remove
    if (gopts.recent->GetCount()) {
        while (gopts.recent->GetCount())
            gopts.recent->RemoveFileFromHistory(0);

        wxFileConfig* cfg = wxGetApp().cfg;
        cfg->SetPath(wxT("/Recent"));
        gopts.recent->Save(*cfg);
        cfg->SetPath(wxT("/"));
        cfg->Flush();
    }
}

EVT_HANDLER(RecentFreeze, "Freeze recent ROM list (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("RecentFreeze", &menuPress);
    toggleBooleanVar(&menuPress, &gopts.recent_freeze);
    SetMenuOption("RecentFreeze", gopts.recent_freeze ? 1 : 0);
    update_opts();
}

// following 10 should really be a single ranged handler
// former names: Recent01 .. Recent10
EVT_HANDLER(wxID_FILE1, "Load recent ROM 1")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(0));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE2, "Load recent ROM 2")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(1));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE3, "Load recent ROM 3")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(2));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE4, "Load recent ROM 4")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(3));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE5, "Load recent ROM 5")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(4));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE6, "Load recent ROM 6")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(5));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE7, "Load recent ROM 7")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(6));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE8, "Load recent ROM 8")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(7));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE9, "Load recent ROM 9")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(8));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

EVT_HANDLER(wxID_FILE10, "Load recent ROM 10")
{
    panel->LoadGame(gopts.recent->GetHistoryFile(9));

#ifndef NO_DEBUGGER
    if (gdbBreakOnLoad)
        GDBBreak();
#endif
}

static const struct rom_maker {
    const wxString code, name;
} makers[] = {
    { wxT("01"), wxT("Nintendo") },
    { wxT("02"), wxT("Rocket Games") },
    { wxT("08"), wxT("Capcom") },
    { wxT("09"), wxT("Hot B Co.") },
    { wxT("0A"), wxT("Jaleco") },
    { wxT("0B"), wxT("Coconuts Japan") },
    { wxT("0C"), wxT("Coconuts Japan/G.X.Media") },
    { wxT("0H"), wxT("Starfish") },
    { wxT("0L"), wxT("Warashi Inc.") },
    { wxT("0N"), wxT("Nowpro") },
    { wxT("0P"), wxT("Game Village") },
    { wxT("13"), wxT("Electronic Arts Japan") },
    { wxT("18"), wxT("Hudson Soft Japan") },
    { wxT("19"), wxT("S.C.P.") },
    { wxT("1A"), wxT("Yonoman") },
    { wxT("1G"), wxT("SMDE") },
    { wxT("1P"), wxT("Creatures Inc.") },
    { wxT("1Q"), wxT("TDK Deep Impresion") },
    { wxT("20"), wxT("Destination Software") },
    { wxT("22"), wxT("VR 1 Japan") },
    { wxT("25"), wxT("San-X") },
    { wxT("28"), wxT("Kemco Japan") },
    { wxT("29"), wxT("Seta") },
    { wxT("2H"), wxT("Ubisoft Japan") },
    { wxT("2K"), wxT("NEC InterChannel") },
    { wxT("2L"), wxT("Tam") },
    { wxT("2M"), wxT("Jordan") },
    { wxT("2N"), wxT("Smilesoft") },
    { wxT("2Q"), wxT("Mediakite") },
    { wxT("36"), wxT("Codemasters") },
    { wxT("37"), wxT("GAGA Communications") },
    { wxT("38"), wxT("Laguna") },
    { wxT("39"), wxT("Telstar Fun and Games") },
    { wxT("41"), wxT("Ubi Soft Entertainment") },
    { wxT("42"), wxT("Sunsoft") },
    { wxT("47"), wxT("Spectrum Holobyte") },
    { wxT("49"), wxT("IREM") },
    { wxT("4D"), wxT("Malibu Games") },
    { wxT("4F"), wxT("Eidos/U.S. Gold") },
    { wxT("4J"), wxT("Fox Interactive") },
    { wxT("4K"), wxT("Time Warner Interactive") },
    { wxT("4Q"), wxT("Disney") },
    { wxT("4S"), wxT("Black Pearl") },
    { wxT("4X"), wxT("GT Interactive") },
    { wxT("4Y"), wxT("RARE") },
    { wxT("4Z"), wxT("Crave Entertainment") },
    { wxT("50"), wxT("Absolute Entertainment") },
    { wxT("51"), wxT("Acclaim") },
    { wxT("52"), wxT("Activision") },
    { wxT("53"), wxT("American Sammy Corp.") },
    { wxT("54"), wxT("Take 2 Interactive") },
    { wxT("55"), wxT("Hi Tech") },
    { wxT("56"), wxT("LJN LTD.") },
    { wxT("58"), wxT("Mattel") },
    { wxT("5A"), wxT("Mindscape/Red Orb Ent.") },
    { wxT("5C"), wxT("Taxan") },
    { wxT("5D"), wxT("Midway") },
    { wxT("5F"), wxT("American Softworks") },
    { wxT("5G"), wxT("Majesco Sales Inc") },
    { wxT("5H"), wxT("3DO") },
    { wxT("5K"), wxT("Hasbro") },
    { wxT("5L"), wxT("NewKidCo") },
    { wxT("5M"), wxT("Telegames") },
    { wxT("5N"), wxT("Metro3D") },
    { wxT("5P"), wxT("Vatical Entertainment") },
    { wxT("5Q"), wxT("LEGO Media") },
    { wxT("5S"), wxT("Xicat Interactive") },
    { wxT("5T"), wxT("Cryo Interactive") },
    { wxT("5W"), wxT("Red Storm Ent./BKN Ent.") },
    { wxT("5X"), wxT("Microids") },
    { wxT("5Z"), wxT("Conspiracy Entertainment Corp.") },
    { wxT("60"), wxT("Titus Interactive Studios") },
    { wxT("61"), wxT("Virgin Interactive") },
    { wxT("62"), wxT("Maxis") },
    { wxT("64"), wxT("LucasArts Entertainment") },
    { wxT("67"), wxT("Ocean") },
    { wxT("69"), wxT("Electronic Arts") },
    { wxT("6E"), wxT("Elite Systems Ltd.") },
    { wxT("6F"), wxT("Electro Brain") },
    { wxT("6G"), wxT("The Learning Company") },
    { wxT("6H"), wxT("BBC") },
    { wxT("6J"), wxT("Software 2000") },
    { wxT("6L"), wxT("BAM! Entertainment") },
    { wxT("6M"), wxT("Studio 3") },
    { wxT("6Q"), wxT("Classified Games") },
    { wxT("6S"), wxT("TDK Mediactive") },
    { wxT("6U"), wxT("DreamCatcher") },
    { wxT("6V"), wxT("JoWood Productions") },
    { wxT("6W"), wxT("SEGA") },
    { wxT("6X"), wxT("Wannado Edition") },
    { wxT("6Y"), wxT("LSP") },
    { wxT("6Z"), wxT("ITE Media") },
    { wxT("70"), wxT("Infogrames") },
    { wxT("71"), wxT("Interplay") },
    { wxT("72"), wxT("JVC Musical Industries Inc") },
    { wxT("73"), wxT("Parker Brothers") },
    { wxT("75"), wxT("SCI") },
    { wxT("78"), wxT("THQ") },
    { wxT("79"), wxT("Accolade") },
    { wxT("7A"), wxT("Triffix Ent. Inc.") },
    { wxT("7C"), wxT("Microprose Software") },
    { wxT("7D"), wxT("Universal Interactive Studios") },
    { wxT("7F"), wxT("Kemco") },
    { wxT("7G"), wxT("Rage Software") },
    { wxT("7H"), wxT("Encore") },
    { wxT("7J"), wxT("Zoo") },
    { wxT("7K"), wxT("BVM") },
    { wxT("7L"), wxT("Simon & Schuster Interactive") },
    { wxT("7M"), wxT("Asmik Ace Entertainment Inc./AIA") },
    { wxT("7N"), wxT("Empire Interactive") },
    { wxT("7Q"), wxT("Jester Interactive") },
    { wxT("7T"), wxT("Scholastic") },
    { wxT("7U"), wxT("Ignition Entertainment") },
    { wxT("7W"), wxT("Stadlbauer") },
    { wxT("80"), wxT("Misawa") },
    { wxT("83"), wxT("LOZC") },
    { wxT("8B"), wxT("Bulletproof Software") },
    { wxT("8C"), wxT("Vic Tokai Inc.") },
    { wxT("8J"), wxT("General Entertainment") },
    { wxT("8N"), wxT("Success") },
    { wxT("8P"), wxT("SEGA Japan") },
    { wxT("91"), wxT("Chun Soft") },
    { wxT("92"), wxT("Video System") },
    { wxT("93"), wxT("BEC") },
    { wxT("96"), wxT("Yonezawa/S'pal") },
    { wxT("97"), wxT("Kaneko") },
    { wxT("99"), wxT("Victor Interactive Software") },
    { wxT("9A"), wxT("Nichibutsu/Nihon Bussan") },
    { wxT("9B"), wxT("Tecmo") },
    { wxT("9C"), wxT("Imagineer") },
    { wxT("9F"), wxT("Nova") },
    { wxT("9H"), wxT("Bottom Up") },
    { wxT("9L"), wxT("Hasbro Japan") },
    { wxT("9N"), wxT("Marvelous Entertainment") },
    { wxT("9P"), wxT("Keynet Inc.") },
    { wxT("9Q"), wxT("Hands-On Entertainment") },
    { wxT("A0"), wxT("Telenet") },
    { wxT("A1"), wxT("Hori") },
    { wxT("A4"), wxT("Konami") },
    { wxT("A6"), wxT("Kawada") },
    { wxT("A7"), wxT("Takara") },
    { wxT("A9"), wxT("Technos Japan Corp.") },
    { wxT("AA"), wxT("JVC") },
    { wxT("AC"), wxT("Toei Animation") },
    { wxT("AD"), wxT("Toho") },
    { wxT("AF"), wxT("Namco") },
    { wxT("AG"), wxT("Media Rings Corporation") },
    { wxT("AH"), wxT("J-Wing") },
    { wxT("AK"), wxT("KID") },
    { wxT("AL"), wxT("MediaFactory") },
    { wxT("AP"), wxT("Infogrames Hudson") },
    { wxT("AQ"), wxT("Kiratto. Ludic Inc") },
    { wxT("B0"), wxT("Acclaim Japan") },
    { wxT("B1"), wxT("ASCII") },
    { wxT("B2"), wxT("Bandai") },
    { wxT("B4"), wxT("Enix") },
    { wxT("B6"), wxT("HAL Laboratory") },
    { wxT("B7"), wxT("SNK") },
    { wxT("B9"), wxT("Pony Canyon Hanbai") },
    { wxT("BA"), wxT("Culture Brain") },
    { wxT("BB"), wxT("Sunsoft") },
    { wxT("BD"), wxT("Sony Imagesoft") },
    { wxT("BF"), wxT("Sammy") },
    { wxT("BG"), wxT("Magical") },
    { wxT("BJ"), wxT("Compile") },
    { wxT("BL"), wxT("MTO Inc.") },
    { wxT("BN"), wxT("Sunrise Interactive") },
    { wxT("BP"), wxT("Global A Entertainment") },
    { wxT("BQ"), wxT("Fuuki") },
    { wxT("C0"), wxT("Taito") },
    { wxT("C2"), wxT("Kemco") },
    { wxT("C3"), wxT("Square Soft") },
    { wxT("C5"), wxT("Data East") },
    { wxT("C6"), wxT("Tonkin House") },
    { wxT("C8"), wxT("Koei") },
    { wxT("CA"), wxT("Konami/Palcom/Ultra") },
    { wxT("CB"), wxT("Vapinc/NTVIC") },
    { wxT("CC"), wxT("Use Co.,Ltd.") },
    { wxT("CD"), wxT("Meldac") },
    { wxT("CE"), wxT("FCI/Pony Canyon") },
    { wxT("CF"), wxT("Angel") },
    { wxT("CM"), wxT("Konami Computer Entertainment Osaka") },
    { wxT("CP"), wxT("Enterbrain") },
    { wxT("D1"), wxT("Sofel") },
    { wxT("D2"), wxT("Quest") },
    { wxT("D3"), wxT("Sigma Enterprises") },
    { wxT("D4"), wxT("Ask Kodansa") },
    { wxT("D6"), wxT("Naxat") },
    { wxT("D7"), wxT("Copya System") },
    { wxT("D9"), wxT("Banpresto") },
    { wxT("DA"), wxT("TOMY") },
    { wxT("DB"), wxT("LJN Japan") },
    { wxT("DD"), wxT("NCS") },
    { wxT("DF"), wxT("Altron Corporation") },
    { wxT("DH"), wxT("Gaps Inc.") },
    { wxT("DN"), wxT("ELF") },
    { wxT("E2"), wxT("Yutaka") },
    { wxT("E3"), wxT("Varie") },
    { wxT("E5"), wxT("Epoch") },
    { wxT("E7"), wxT("Athena") },
    { wxT("E8"), wxT("Asmik Ace Entertainment Inc.") },
    { wxT("E9"), wxT("Natsume") },
    { wxT("EA"), wxT("King Records") },
    { wxT("EB"), wxT("Atlus") },
    { wxT("EC"), wxT("Epic/Sony Records") },
    { wxT("EE"), wxT("IGS") },
    { wxT("EL"), wxT("Spike") },
    { wxT("EM"), wxT("Konami Computer Entertainment Tokyo") },
    { wxT("EN"), wxT("Alphadream Corporation") },
    { wxT("F0"), wxT("A Wave") },
    { wxT("G1"), wxT("PCCW") },
    { wxT("G4"), wxT("KiKi Co Ltd") },
    { wxT("G5"), wxT("Open Sesame Inc.") },
    { wxT("G6"), wxT("Sims") },
    { wxT("G7"), wxT("Broccoli") },
    { wxT("G8"), wxT("Avex") },
    { wxT("G9"), wxT("D3 Publisher") },
    { wxT("GB"), wxT("Konami Computer Entertainment Japan") },
    { wxT("GD"), wxT("Square-Enix") },
    { wxT("HY"), wxT("Sachen") }
};
#define num_makers (sizeof(makers) / sizeof(makers[0]))
static bool maker_lt(const rom_maker& r1, const rom_maker& r2)
{
    return wxStrcmp(r1.code, r2.code) < 0;
}

void SetDialogLabel(wxDialog* dlg, const wxString& id, wxString ts, size_t l)
{
    (void)l; // unused params
    ts.Replace(wxT("&"), wxT("&&"), true);
    (dynamic_cast<wxControl*>((*dlg).FindWindow(wxXmlResource::GetXRCID(id))))->SetLabel(ts);
}

EVT_HANDLER_MASK(RomInformation, "ROM information...", CMDEN_GB | CMDEN_GBA)
{
    wxString s;
#define setlab(id)                            \
    do {                                      \
        /* SetLabelText is not in 2.8 */      \
        s.Replace(wxT("&"), wxT("&&"), true); \
        XRCCTRL(*dlg, id, wxControl)          \
            ->SetLabel(s);                    \
    } while (0)
#define setblab(id, b)                          \
    do {                                        \
        s.Printf(wxT("%02x"), (unsigned int)b); \
        setlab(id);                             \
    } while (0)
#define setblabs(id, b, ts)                              \
    do {                                                 \
        s.Printf(wxT("%02x (%s)"), (unsigned int)b, ts.c_str()); \
        setlab(id);                                      \
    } while (0)
#define setlabs(id, ts, l)                               \
    do {                                                 \
        s = wxString((const char*)&(ts), wxConvLibc, l); \
        setlab(id);                                      \
    } while (0)

    switch (panel->game_type()) {
    case IMAGE_GB: {
        wxDialog* dlg = GetXRCDialog("GBROMInfo");
        setlabs("Title", gbRom[0x134], 15);
        setblab("Color", gbRom[0x143]);

        if (gbRom[0x14b] == 0x33)
            s = wxString((const char*)&gbRom[0x144], wxConvUTF8, 2);
        else
            s.Printf(wxT("%02x"), gbRom[0x14b]);

        setlab("MakerCode");
        const rom_maker m = { s, wxString() }, *rm;
        rm = std::lower_bound(&makers[0], &makers[num_makers], m, maker_lt);

        if (rm < &makers[num_makers] && !wxStrcmp(m.code, rm->code))
            s = rm->name;
        else
            s = _("Unknown");

        setlab("MakerName");
        setblab("UnitCode", gbRom[0x146]);
        wxString type;

        switch (gbRom[0x147]) {
        case 0x00:
            type = _("ROM");
            break;

        case 0x01:
            type = _("ROM+MBC1");
            break;

        case 0x02:
            type = _("ROM+MBC1+RAM");
            break;

        case 0x03:
            type = _("ROM+MBC1+RAM+BATT");
            break;

        case 0x05:
            type = _("ROM+MBC2");
            break;

        case 0x06:
            type = _("ROM+MBC2+BATT");
            break;

        case 0x0b:
            type = _("ROM+MMM01");
            break;

        case 0x0c:
            type = _("ROM+MMM01+RAM");
            break;

        case 0x0d:
            type = _("ROM+MMM01+RAM+BATT");
            break;

        case 0x0f:
            type = _("ROM+MBC3+TIMER+BATT");
            break;

        case 0x10:
            type = _("ROM+MBC3+TIMER+RAM+BATT");
            break;

        case 0x11:
            type = _("ROM+MBC3");
            break;

        case 0x12:
            type = _("ROM+MBC3+RAM");
            break;

        case 0x13:
            type = _("ROM+MBC3+RAM+BATT");
            break;

        case 0x19:
            type = _("ROM+MBC5");
            break;

        case 0x1a:
            type = _("ROM+MBC5+RAM");
            break;

        case 0x1b:
            type = _("ROM+MBC5+RAM+BATT");
            break;

        case 0x1c:
            type = _("ROM+MBC5+RUMBLE");
            break;

        case 0x1d:
            type = _("ROM+MBC5+RUMBLE+RAM");
            break;

        case 0x1e:
            type = _("ROM+MBC5+RUMBLE+RAM+BATT");
            break;

        case 0x22:
            type = _("ROM+MBC7+BATT");
            break;

        case 0x55:
            type = _("GameGenie");
            break;

        case 0x56:
            type = _("GameShark V3.0");
            break;

        case 0xfc:
            type = _("ROM+POCKET CAMERA");
            break;

        case 0xfd:
            type = _("ROM+BANDAI TAMA5");
            break;

        case 0xfe:
            type = _("ROM+HuC-3");
            break;

        case 0xff:
            type = _("ROM+HuC-1");
            break;

        default:
            type = _("Unknown");
        }

        setblabs("DeviceType", gbRom[0x147], type);

        switch (gbRom[0x148]) {
        case 0:
            type = wxT("32K");
            break;

        case 1:
            type = wxT("64K");
            break;

        case 2:
            type = wxT("128K");
            break;

        case 3:
            type = wxT("256K");
            break;

        case 4:
            type = wxT("512K");
            break;

        case 5:
            type = wxT("1M");
            break;

        case 6:
            type = wxT("2M");
            break;

        case 7:
            type = wxT("4M");
            break;

        default:
            type = _("Unknown");
        }

        setblabs("ROMSize", gbRom[0x148], type);

        switch (gbRom[0x149]) {
        case 0:
            type = _("None");
            break;

        case 1:
            type = wxT("2K");
            break;

        case 2:
            type = wxT("8K");
            break;

        case 3:
            type = wxT("32K");
            break;

        case 4:
            type = wxT("128K");
            break;

        case 5:
            type = wxT("64K");
            break;
        }

        setblabs("RAMSize", gbRom[0x149], type);
        setblab("DestCode", gbRom[0x14a]);
        setblab("LicCode", gbRom[0x14b]);
        setblab("Version", gbRom[0x14c]);
        uint8_t crc = 25;

        for (int i = 0x134; i < 0x14d; i++)
            crc += gbRom[i];

        crc = 256 - crc;
        s.Printf(wxT("%02x (%02x)"), crc, gbRom[0x14d]);
        setlab("CRC");
        uint16_t crc16 = 0;

        for (int i = 0; i < gbRomSize; i++)
            crc16 += gbRom[i];

        crc16 -= gbRom[0x14e] + gbRom[0x14f];
        s.Printf(wxT("%04x (%04x)"), crc16, gbRom[0x14e] * 256 + gbRom[0x14f]);
        setlab("Checksum");
        dlg->Fit();
        ShowModal(dlg);
    } break;

    case IMAGE_GBA: {
        IdentifyRom();
        wxDialog* dlg = GetXRCDialog("GBAROMInfo");
        wxString rom_crc32;
        rom_crc32.Printf(wxT("%08X"), panel->rom_crc32);
        SetDialogLabel(dlg, wxT("Title"), panel->rom_name, 30);
        setlabs("IntTitle", rom[0xa0], 12);
        SetDialogLabel(dlg, wxT("Scene"), panel->rom_scene_rls_name, 30);
        SetDialogLabel(dlg, wxT("Release"), panel->rom_scene_rls, 4);
        SetDialogLabel(dlg, wxT("CRC32"), rom_crc32, 8);
        setlabs("GameCode", rom[0xac], 4);
        setlabs("MakerCode", rom[0xb0], 2);
        const rom_maker m = { s, wxString() }, *rm;
        rm = std::lower_bound(&makers[0], &makers[num_makers], m, maker_lt);

        if (rm < &makers[num_makers] && !wxStrcmp(m.code, rm->code))
            s = rm->name;
        else
            s = _("Unknown");

        setlab("MakerName");
        setblab("UnitCode", rom[0xb3]);
        s.Printf(wxT("%02x"), (unsigned int)rom[0xb4]);

        if (rom[0xb4] & 0x80)
            s.append(wxT(" (DACS)"));

        setlab("DeviceType");
        setblab("Version", rom[0xbc]);
        uint8_t crc = 0x19;

        for (int i = 0xa0; i < 0xbd; i++)
            crc += rom[i];

        crc = -crc;
        s.Printf(wxT("%02x (%02x)"), crc, rom[0xbd]);
        setlab("CRC");
        dlg->Fit();
        ShowModal(dlg);
    } break;

    default:
        break;
    }
}

EVT_HANDLER_MASK(ResetLoadingDotCodeFile, "Reset Loading e-Reader Dot Code", CMDEN_GBA)
{
    ResetLoadDotCodeFile();
}

EVT_HANDLER_MASK(SetLoadingDotCodeFile, "Load e-Reader Dot Code...", CMDEN_GBA)
{
    static wxString loaddotcodefile_path;
    wxFileDialog dlg(this, _("Select Dot Code file"), loaddotcodefile_path, wxEmptyString,
        _(
                         "e-Reader Dot Code (*.bin;*.raw)|"
                         "*.bin;*.raw"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    int ret = ShowModal(&dlg);

    if (ret != wxID_OK)
        return;

    loaddotcodefile_path = dlg.GetPath();
    SetLoadDotCodeFile(UTF8(loaddotcodefile_path));
}

EVT_HANDLER_MASK(ResetSavingDotCodeFile, "Reset Saving e-Reader Dot Code", CMDEN_GBA)
{
    ResetLoadDotCodeFile();
}

EVT_HANDLER_MASK(SetSavingDotCodeFile, "Save e-Reader Dot Code...", CMDEN_GBA)
{
    static wxString savedotcodefile_path;
    wxFileDialog dlg(this, _("Select Dot Code file"), savedotcodefile_path, wxEmptyString,
        _(
                         "e-Reader Dot Code (*.bin;*.raw)|"
                         "*.bin;*.raw"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    int ret = ShowModal(&dlg);

    if (ret != wxID_OK)
        return;

    savedotcodefile_path = dlg.GetPath();
    SetSaveDotCodeFile(UTF8(savedotcodefile_path));
}

static wxString batimp_path;

EVT_HANDLER_MASK(ImportBatteryFile, "Import battery file...", CMDEN_GB | CMDEN_GBA)
{
    if (!batimp_path.size())
        batimp_path = panel->bat_dir();

    wxFileDialog dlg(this, _("Select battery file"), batimp_path, wxEmptyString,
        _("Battery file (*.sav)|*.sav|Flash save (*.dat)|*.dat"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    int ret = ShowModal(&dlg);
    batimp_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxString fn = dlg.GetPath();
    ret = wxMessageBox(_("Importing a battery file will erase any saved games (permanently after the next write).  Do you want to continue?"),
        _("Confirm import"), wxYES_NO | wxICON_EXCLAMATION);

    if (ret == wxYES) {
        wxString msg;

        if (panel->emusys->emuReadBattery(UTF8(fn)))
            msg.Printf(_("Loaded battery %s"), fn.wc_str());
        else
            msg.Printf(_("Error loading battery %s"), fn.wc_str());

        systemScreenMessage(msg);
    }
}

EVT_HANDLER_MASK(ImportGamesharkCodeFile, "Import GameShark code file...", CMDEN_GB | CMDEN_GBA)
{
    static wxString path;
    wxFileDialog dlg(this, _("Select code file"), path, wxEmptyString,
        panel->game_type() == IMAGE_GBA ? _("Gameshark Code File (*.spc;*.xpc)|*.spc;*.xpc") : _("Gameshark Code File (*.gcf)|*.gcf"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    int ret = ShowModal(&dlg);
    path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxString fn = dlg.GetPath();
    ret = wxMessageBox(_("Importing a code file will replace any loaded cheats.  Do you want to continue?"),
        _("Confirm import"), wxYES_NO | wxICON_EXCLAMATION);

    if (ret == wxYES) {
        wxString msg;
        bool res;

        if (panel->game_type() == IMAGE_GB)
            // FIXME: this routine will not work on big-endian systems
            // if the underlying file format is little-endian
            // (fix in gb/gbCheats.cpp)
            res = gbCheatReadGSCodeFile(UTF8(fn));
        else {
            // need to select game first
            wxFFile f(fn, wxT("rb"));

            if (!f.IsOpened()) {
                wxLogError(_("Cannot open file %s"), fn.c_str());
                return;
            }

            // FIXME: in my code, I assume file format is little-endian
            // however, in core code, it is assumed to be native-endian
            uint32_t len;
            char buf[14];

            if (f.Read(&len, sizeof(len)) != sizeof(len) || wxUINT32_SWAP_ON_BE(len) != 14 || f.Read(buf, 14) != 14 || memcmp(buf, "SharkPortCODES", 14)) {
                wxLogError(_("Unsupported code file %s"), fn.c_str());
                return;
            }

            f.Seek(0x1e);

            if (f.Read(&len, sizeof(len)) != sizeof(len))
                len = 0;

            uint32_t game = 0;

            if (len > 1) {
                wxDialog* seldlg = GetXRCDialog("CodeSelect");
                wxControlWithItems* lst = XRCCTRL(*seldlg, "CodeList", wxControlWithItems);
                lst->Clear();

                while (len-- > 0) {
                    uint32_t slen;

                    if (f.Read(&slen, sizeof(slen)) != sizeof(slen) || slen > 1024) // arbitrary upper bound
                        break;

                    char buf[1024];

                    if (f.Read(buf, slen) != slen)
                        break;

                    lst->Append(wxString(buf, wxConvLibc, slen));
                    uint32_t ncodes;

                    if (f.Read(&ncodes, sizeof(ncodes)) != sizeof(ncodes))
                        break;

                    for (; ncodes > 0; ncodes--) {
                        if (f.Read(&slen, sizeof(slen)) != sizeof(slen))
                            break;

                        f.Seek(slen, wxFromCurrent);

                        if (f.Read(&slen, sizeof(slen)) != sizeof(slen))
                            break;

                        f.Seek(slen + 4, wxFromCurrent);

                        if (f.Read(&slen, sizeof(slen)) != sizeof(slen))
                            break;

                        f.Seek(slen * 12, wxFromCurrent);
                    }
                }

                int sel = ShowModal(seldlg);

                if (sel != wxID_OK)
                    return;

                game = lst->GetSelection();

                if ((int)game == wxNOT_FOUND)
                    game = 0;
            }

            bool v3 = fn.size() >= 4 && wxString(fn.substr(fn.size() - 4)).IsSameAs(wxT(".xpc"), false);
            // FIXME: this routine will not work on big-endian systems
            // if the underlying file format is little-endian
            // (fix in gba/Cheats.cpp)
            res = cheatsImportGSACodeFile(UTF8(fn), game, v3);
        }

        if (res)
            msg.Printf(_("Loaded code file %s"), fn.wc_str());
        else
            msg.Printf(_("Error loading code file %s"), fn.wc_str());

        systemScreenMessage(msg);
    }
}

static wxString gss_path;

EVT_HANDLER_MASK(ImportGamesharkActionReplaySnapshot,
    "Import GameShark Action Replay snapshot...", CMDEN_GB | CMDEN_GBA)
{
    wxFileDialog dlg(this, _("Select snapshot file"), gss_path, wxEmptyString,
        panel->game_type() == IMAGE_GBA ? _("GS & PAC Snapshots (*.sps;*.xps)|*.sps;*.xps|GameShark SP Snapshots (*.gsv)|*.gsv") : _("Gameboy Snapshot (*.gbs)|*.gbs"),
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    int ret = ShowModal(&dlg);
    gss_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxString fn = dlg.GetPath();
    ret = wxMessageBox(_("Importing a snapshot file will erase any saved games (permanently after the next write).  Do you want to continue?"),
        _("Confirm import"), wxYES_NO | wxICON_EXCLAMATION);

    if (ret == wxYES) {
        wxString msg;
        bool res;

        if (panel->game_type() == IMAGE_GB)
            res = gbReadGSASnapshot(UTF8(fn));
        else {
            bool gsv = fn.size() >= 4 && wxString(fn.substr(fn.size() - 4)).IsSameAs(wxT(".gsv"), false);

            if (gsv)
                // FIXME: this will fail on big-endian machines if
                // file format is little-endian
                // fix in GBA.cpp
                res = CPUReadGSASPSnapshot(UTF8(fn));
            else
                // FIXME: this will fail on big-endian machines if
                // file format is little-endian
                // fix in GBA.cpp
                res = CPUReadGSASnapshot(UTF8(fn));
        }

        if (res)
            msg.Printf(_("Loaded snapshot file %s"), fn.wc_str());
        else
            msg.Printf(_("Error loading snapshot file %s"), fn.wc_str());

        systemScreenMessage(msg);
    }
}

EVT_HANDLER_MASK(ExportBatteryFile, "Export battery file...", CMDEN_GB | CMDEN_GBA)
{
    if (!batimp_path.size())
        batimp_path = panel->bat_dir();

    wxFileDialog dlg(this, _("Select battery file"), batimp_path, wxEmptyString,
        _("Battery file (*.sav)|*.sav|Flash save (*.dat)|*.dat"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    int ret = ShowModal(&dlg);
    batimp_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxString fn = dlg.GetPath();
    wxString msg;

    if (panel->emusys->emuWriteBattery(UTF8(fn)))
        msg.Printf(_("Wrote battery %s"), fn.wc_str());
    else
        msg.Printf(_("Error writing battery %s"), fn.wc_str());

    systemScreenMessage(msg);
}

EVT_HANDLER_MASK(ExportGamesharkSnapshot, "Export GameShark snapshot...", CMDEN_GBA)
{
    if (eepromInUse) {
        wxLogError(_("EEPROM saves cannot be exported"));
        return;
    }

    wxString def_name = panel->game_name();
    def_name.append(wxT(".sps"));
    wxFileDialog dlg(this, _("Select snapshot file"), gss_path, def_name,
        _("Gameshark Snapshot (*.sps)|*.sps"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    int ret = ShowModal(&dlg);
    gss_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxString fn = dlg.GetPath();
    wxDialog* infodlg = GetXRCDialog("ExportSPS");
    wxTextCtrl *tit = XRCCTRL(*infodlg, "Title", wxTextCtrl),
               *dsc = XRCCTRL(*infodlg, "Description", wxTextCtrl),
               *n = XRCCTRL(*infodlg, "Notes", wxTextCtrl);
    tit->SetValue(wxString((const char*)&rom[0xa0], wxConvLibc, 12));
    dsc->SetValue(wxDateTime::Now().Format(wxT("%c")));
    n->SetValue(_("Exported from VisualBoyAdvance-M"));

    if (ShowModal(infodlg) != wxID_OK)
        return;

    wxString msg;

    // FIXME: this will fail on big-endian machines if file format is
    // little-endian
    // fix in GBA.cpp
    if (CPUWriteGSASnapshot(fn.utf8_str(), tit->GetValue().utf8_str(),
            dsc->GetValue().utf8_str(), n->GetValue().utf8_str()))
        msg.Printf(_("Saved snapshot file %s"), fn.wc_str());
    else
        msg.Printf(_("Error saving snapshot file %s"), fn.wc_str());

    systemScreenMessage(msg);
}

EVT_HANDLER_MASK(ScreenCapture, "Screen capture...", CMDEN_GB | CMDEN_GBA)
{
    wxString scap_path = GetGamePath(gopts.scrshot_dir);
    wxString def_name = panel->game_name();

    if (captureFormat == 0)
        def_name.append(wxT(".png"));
    else
        def_name.append(wxT(".bmp"));

    wxFileDialog dlg(this, _("Select output file"), scap_path, def_name,
        _("PNG images|*.png|BMP images|*.bmp"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    dlg.SetFilterIndex(captureFormat);
    int ret = ShowModal(&dlg);
    scap_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxString fn = dlg.GetPath();
    int fmt = dlg.GetFilterIndex();

    if (fn.size() >= 4) {
        if (wxString(fn.substr(fn.size() - 4)).IsSameAs(wxT(".bmp"), false))
            fmt = 1;
        else if (wxString(fn.substr(fn.size() - 4)).IsSameAs(wxT(".png"), false))
            fmt = 0;
    }

    if (fmt == 0)
        panel->emusys->emuWritePNG(UTF8(fn));
    else
        panel->emusys->emuWriteBMP(UTF8(fn));

    wxString msg;
    msg.Printf(_("Wrote snapshot %s"), fn.wc_str());
    systemScreenMessage(msg);
}

EVT_HANDLER_MASK(RecordSoundStartRecording, "Start sound recording...", CMDEN_NSREC)
{
#ifndef NO_FFMPEG
    static wxString sound_exts;
    static int sound_extno;
    static wxString sound_path;

    if (!sound_exts.size()) {
        sound_extno = -1;
        int extno = 0;

        std::vector<char *> fmts = recording::getSupAudNames();
        std::vector<char *> exts = recording::getSupAudExts();

        for (size_t i = 0; i < fmts.size(); ++i)
        {
            sound_exts.append(wxString(fmts[i], wxConvLibc));
            sound_exts.append(_(" files ("));
            wxString ext(exts[i], wxConvLibc);
            ext.Replace(wxT(","), wxT(";*."));
            ext.insert(0, wxT("*."));

            if (sound_extno < 0 && ext.find(wxT("*.wav")) != wxString::npos)
                sound_extno = extno;

            sound_exts.append(ext);
            sound_exts.append(wxT(")|"));
            sound_exts.append(ext);
            sound_exts.append(wxT('|'));
            extno++;
        }

        sound_exts.append(wxALL_FILES);

        if (sound_extno < 0)
            sound_extno = extno;
    }

    sound_path = GetGamePath(gopts.recording_dir);
    wxString def_name = panel->game_name();
    wxString extoff = sound_exts;

    for (int i = 0; i < sound_extno; i++) {
        extoff = extoff.Mid(extoff.Find(wxT('|')) + 1);
        extoff = extoff.Mid(extoff.Find(wxT('|')) + 1);
    }

    extoff = extoff.Mid(extoff.Find(wxT('|')) + 2); // skip *
    def_name += extoff.Left(wxStrcspn(extoff, wxT(";|")));
    wxFileDialog dlg(this, _("Select output file"), sound_path, def_name,
        sound_exts, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    dlg.SetFilterIndex(sound_extno);
    int ret = ShowModal(&dlg);
    sound_extno = dlg.GetFilterIndex();
    sound_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    panel->StartSoundRecording(dlg.GetPath());
#endif
}

EVT_HANDLER_MASK(RecordSoundStopRecording, "Stop sound recording", CMDEN_SREC)
{
#ifndef NO_FFMPEG
    panel->StopSoundRecording();
#endif
}

EVT_HANDLER_MASK(RecordAVIStartRecording, "Start video recording...", CMDEN_NVREC)
{
#ifndef NO_FFMPEG
    static wxString vid_exts;
    static int vid_extno;
    static wxString vid_path;

    if (!vid_exts.size()) {
        vid_extno = -1;
        int extno = 0;

        std::vector<char *> fmts = recording::getSupVidNames();
        std::vector<char *> exts = recording::getSupVidExts();

        for (size_t i = 0; i < fmts.size(); ++i)
        {
            vid_exts.append(wxString(fmts[i], wxConvLibc));
            vid_exts.append(_(" files ("));
            wxString ext(exts[i], wxConvLibc);
            ext.Replace(wxT(","), wxT(";*."));
            ext.insert(0, wxT("*."));

            if (vid_extno < 0 && ext.find(wxT("*.avi")) != wxString::npos)
                vid_extno = extno;

            vid_exts.append(ext);
            vid_exts.append(wxT(")|"));
            vid_exts.append(ext);
            vid_exts.append(wxT('|'));
            extno++;
        }

        vid_exts.append(wxALL_FILES);

        if (vid_extno < 0)
            vid_extno = extno;
    }

    vid_path = GetGamePath(gopts.recording_dir);
    wxString def_name = panel->game_name();
    wxString extoff = vid_exts;

    for (int i = 0; i < vid_extno; i++) {
        extoff = extoff.Mid(extoff.Find(wxT('|')) + 1);
        extoff = extoff.Mid(extoff.Find(wxT('|')) + 1);
    }

    extoff = extoff.Mid(extoff.Find(wxT('|')) + 2); // skip *
    def_name += extoff.Left(wxStrcspn(extoff, wxT(";|")));
    wxFileDialog dlg(this, _("Select output file"), vid_path, def_name,
        vid_exts, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    dlg.SetFilterIndex(vid_extno);
    int ret = ShowModal(&dlg);
    vid_extno = dlg.GetFilterIndex();
    vid_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    panel->StartVidRecording(dlg.GetPath());
#endif
}

EVT_HANDLER_MASK(RecordAVIStopRecording, "Stop video recording", CMDEN_VREC)
{
#ifndef NO_FFMPEG
    panel->StopVidRecording();
#endif
}

static wxString mov_path;

EVT_HANDLER_MASK(RecordMovieStartRecording, "Start game recording...", CMDEN_NGREC)
{
    static wxString mov_exts;
    static int mov_extno;
    static wxString mov_path;

    if (!mov_exts.size()) {
        mov_extno = -1;
        int extno = 0;

        std::vector<char*> fmts = getSupMovNamesToRecord();
        std::vector<char*> exts = getSupMovExtsToRecord();

        for (auto&& fmt : fmts)
        {
            mov_exts.append(wxString(fmt, wxConvLibc));
            mov_exts.append(_(" files ("));
            wxString ext(exts[extno], wxConvLibc);
            ext.Replace(wxT(","), wxT(";*."));
            ext.insert(0, wxT("*."));

            if (mov_extno < 0 && ext.find(wxT("*.vmv")) != wxString::npos)
                mov_extno = extno;

            mov_exts.append(ext);
            mov_exts.append(wxT(")|"));
            mov_exts.append(ext);
            mov_exts.append(wxT('|'));
            extno++;
        }

        if (mov_extno < 0)
            mov_extno = extno;
    }

    mov_path = GetGamePath(gopts.recording_dir);
    wxString def_name = panel->game_name();
    wxString extoff = mov_exts;

    for (int i = 0; i < mov_extno; i++) {
        extoff = extoff.Mid(extoff.Find(wxT('|')) + 1);
        extoff = extoff.Mid(extoff.Find(wxT('|')) + 1);
    }

    extoff = extoff.Mid(extoff.Find(wxT('|')) + 2); // skip *
    def_name += extoff.Left(wxStrcspn(extoff, wxT(";|")));
    wxFileDialog dlg(this, _("Select output file"), mov_path, def_name,
        mov_exts, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    dlg.SetFilterIndex(mov_extno);
    int ret = ShowModal(&dlg);
    mov_extno = dlg.GetFilterIndex();
    mov_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    systemStartGameRecording(dlg.GetPath(), getSupMovFormatsToRecord()[mov_extno]);
}

EVT_HANDLER_MASK(RecordMovieStopRecording, "Stop game recording", CMDEN_GREC)
{
    systemStopGameRecording();
}

EVT_HANDLER_MASK(PlayMovieStartPlaying, "Start playing movie...", CMDEN_NGREC | CMDEN_NGPLAY)
{
    static wxString mov_exts;
    static int mov_extno;
    static wxString mov_path;

    if (!mov_exts.size()) {
        mov_extno = -1;
        int extno = 0;

        std::vector<char*> fmts = getSupMovNamesToPlayback();
        std::vector<char*> exts = getSupMovExtsToPlayback();

        for (size_t i = 0; i < fmts.size(); ++i)
        {
            mov_exts.append(wxString(fmts[i], wxConvLibc));
            mov_exts.append(_(" files ("));
            wxString ext(exts[i], wxConvLibc);
            ext.Replace(wxT(","), wxT(";*."));
            ext.insert(0, wxT("*."));

            if (mov_extno < 0 && ext.find(wxT("*.vmv")) != wxString::npos)
                mov_extno = extno;

            mov_exts.append(ext);
            mov_exts.append(wxT(")|"));
            mov_exts.append(ext);
            mov_exts.append(wxT('|'));
            extno++;
        }

        if (mov_extno < 0)
            mov_extno = extno;
    }

    mov_path = GetGamePath(gopts.recording_dir);
    systemStopGamePlayback();
    wxString def_name = panel->game_name();
    wxString extoff = mov_exts;

    for (int i = 0; i < mov_extno; i++) {
        extoff = extoff.Mid(extoff.Find(wxT('|')) + 1);
        extoff = extoff.Mid(extoff.Find(wxT('|')) + 1);
    }

    extoff = extoff.Mid(extoff.Find(wxT('|')) + 2); // skip *
    def_name += extoff.Left(wxStrcspn(extoff, wxT(";|")));
    wxFileDialog dlg(this, _("Select file"), mov_path, def_name,
        mov_exts, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    dlg.SetFilterIndex(mov_extno);
    int ret = ShowModal(&dlg);
    mov_extno = dlg.GetFilterIndex();
    mov_path = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    systemStartGamePlayback(dlg.GetPath(), getSupMovFormatsToPlayback()[mov_extno]);
}

EVT_HANDLER_MASK(PlayMovieStopPlaying, "Stop playing movie", CMDEN_GPLAY)
{
    systemStopGamePlayback();
}

// formerly Close
EVT_HANDLER_MASK(wxID_CLOSE, "Close", CMDEN_GB | CMDEN_GBA)
{
    panel->UnloadGame();
}

// formerly Exit
EVT_HANDLER(wxID_EXIT, "Exit")
{
    Close(false);
}

// Emulation menu
EVT_HANDLER(Pause, "Pause (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("Pause", &menuPress);
    toggleBooleanVar(&menuPress, &paused);
    SetMenuOption("Pause", paused ? 1 : 0);

    if (paused)
        panel->Pause();
    else if (!IsPaused())
        panel->Resume();

    // undo next-frame's zeroing of frameskip
    int fs = frameSkip;

    if (fs >= 0)
        systemFrameSkip = fs;
}

// new
EVT_HANDLER_MASK(EmulatorSpeedupToggle, "Turbo mode (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    GetMenuOptionBool("EmulatorSpeedupToggle", &menuPress);
    toggleBooleanVar(&menuPress, &turbo);
    SetMenuOption("EmulatorSpeedupToggle", turbo ? 1 : 0);
}

EVT_HANDLER_MASK(Reset, "Reset", CMDEN_GB | CMDEN_GBA)
{
    panel->emusys->emuReset();
    // systemScreenMessage("Reset");
}

EVT_HANDLER(ToggleFullscreen, "Full screen (toggle)")
{
    panel->ShowFullScreen(!IsFullScreen());
}

EVT_HANDLER(JoypadAutofireA, "Autofire A (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("JoypadAutofireA", &menuPress);
    toggleBitVar(&menuPress, &autofire, KEYM_A);
    SetMenuOption("JoypadAutofireA", menuPress ? 1 : 0);
    GetMenuOptionInt("JoypadAutofireA", &autofire, KEYM_A);
}

EVT_HANDLER(JoypadAutofireB, "Autofire B (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("JoypadAutofireB", &menuPress);
    toggleBitVar(&menuPress, &autofire, KEYM_B);
    SetMenuOption("JoypadAutofireB", menuPress ? 1 : 0);
    GetMenuOptionInt("JoypadAutofireB", &autofire, KEYM_B);
}

EVT_HANDLER(JoypadAutofireL, "Autofire L (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("JoypadAutofireL", &menuPress);
    toggleBitVar(&menuPress, &autofire, KEYM_L);
    SetMenuOption("JoypadAutofireL", menuPress ? 1 : 0);
    GetMenuOptionInt("JoypadAutofireL", &autofire, KEYM_L);
}

EVT_HANDLER(JoypadAutofireR, "Autofire R (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("JoypadAutofireR", &menuPress);
    toggleBitVar(&menuPress, &autofire, KEYM_R);
    SetMenuOption("JoypadAutofireR", menuPress ? 1 : 0);
    GetMenuOptionInt("JoypadAutofireR", &autofire, KEYM_R);
}

EVT_HANDLER(JoypadAutoholdUp, "Autohold Up (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdUp";
    int keym = KEYM_UP;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdDown, "Autohold Down (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdDown";
    int keym = KEYM_DOWN;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdLeft, "Autohold Left (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdLeft";
    int keym = KEYM_LEFT;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdRight, "Autohold Right (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdRight";
    int keym = KEYM_RIGHT;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdA, "Autohold A (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdA";
    int keym = KEYM_A;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdB, "Autohold B (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdB";
    int keym = KEYM_B;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdL, "Autohold L (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdL";
    int keym = KEYM_L;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdR, "Autohold R (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdR";
    int keym = KEYM_R;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdSelect, "Autohold Select (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdSelect";
    int keym = KEYM_SELECT;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

EVT_HANDLER(JoypadAutoholdStart, "Autohold Start (toggle)")
{
    bool menuPress = false;
    char keyName[] = "JoypadAutoholdStart";
    int keym = KEYM_START;
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &autohold, keym);
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &autohold, keym);
}

#include "background-input.h"

EVT_HANDLER(AllowKeyboardBackgroundInput, "Allow keyboard background input (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("AllowKeyboardBackgroundInput", &menuPress);
    toggleBooleanVar(&menuPress, &allowKeyboardBackgroundInput);
    SetMenuOption("AllowKeyboardBackgroundInput", allowKeyboardBackgroundInput ? 1 : 0);

    disableKeyboardBackgroundInput();

    if (allowKeyboardBackgroundInput) {
        if (panel && panel->panel) {
            enableKeyboardBackgroundInput(panel->panel->GetWindow());
        }
    }

    update_opts();
}

EVT_HANDLER(AllowJoystickBackgroundInput, "Allow joystick background input (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("AllowJoystickBackgroundInput", &menuPress);
    toggleBooleanVar(&menuPress, &allowJoystickBackgroundInput);
    SetMenuOption("AllowJoystickBackgroundInput", allowJoystickBackgroundInput ? 1 : 0);

    update_opts();
}

EVT_HANDLER_MASK(LoadGameRecent, "Load most recent save", CMDEN_SAVST)
{
    panel->LoadState();
}

EVT_HANDLER(LoadGameAutoLoad, "Auto load most recent save (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("LoadGameAutoLoad", &menuPress);
    toggleBooleanVar(&menuPress, &gopts.autoload_state);
    SetMenuOption("LoadGameAutoLoad", gopts.autoload_state ? 1 : 0);
    update_opts();
}

EVT_HANDLER_MASK(LoadGame01, "Load saved state 1", CMDEN_SAVST)
{
    panel->LoadState(1);
}

EVT_HANDLER_MASK(LoadGame02, "Load saved state 2", CMDEN_SAVST)
{
    panel->LoadState(2);
}

EVT_HANDLER_MASK(LoadGame03, "Load saved state 3", CMDEN_SAVST)
{
    panel->LoadState(3);
}

EVT_HANDLER_MASK(LoadGame04, "Load saved state 4", CMDEN_SAVST)
{
    panel->LoadState(4);
}

EVT_HANDLER_MASK(LoadGame05, "Load saved state 5", CMDEN_SAVST)
{
    panel->LoadState(5);
}

EVT_HANDLER_MASK(LoadGame06, "Load saved state 6", CMDEN_SAVST)
{
    panel->LoadState(6);
}

EVT_HANDLER_MASK(LoadGame07, "Load saved state 7", CMDEN_SAVST)
{
    panel->LoadState(7);
}

EVT_HANDLER_MASK(LoadGame08, "Load saved state 8", CMDEN_SAVST)
{
    panel->LoadState(8);
}

EVT_HANDLER_MASK(LoadGame09, "Load saved state 9", CMDEN_SAVST)
{
    panel->LoadState(9);
}

EVT_HANDLER_MASK(LoadGame10, "Load saved state 10", CMDEN_SAVST)
{
    panel->LoadState(10);
}

static wxString st_dir;

EVT_HANDLER_MASK(Load, "Load state...", CMDEN_GB | CMDEN_GBA)
{
    if (st_dir.empty())
        st_dir = panel->state_dir();

    wxFileDialog dlg(this, _("Select state file"), st_dir, wxEmptyString,
        _("VisualBoyAdvance saved game files|*.sgm"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    int ret = ShowModal(&dlg);
    st_dir = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    panel->LoadState(dlg.GetPath());
}

// new
EVT_HANDLER(KeepSaves, "Do not load battery saves (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("KeepSaves", &menuPress);
    toggleBitVar(&menuPress, &skipSaveGameBattery, 1);
    SetMenuOption("KeepSaves", menuPress ? 1 : 0);
    GetMenuOptionInt("KeepSaves", &skipSaveGameBattery, 1);
    update_opts();
}

// new
EVT_HANDLER(KeepCheats, "Do not change cheat list (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("KeepCheats", &menuPress);
    toggleBitVar(&menuPress, &skipSaveGameCheats, 1);
    SetMenuOption("KeepCheats", menuPress ? 1 : 0);
    GetMenuOptionInt("KeepCheats", &skipSaveGameCheats, 1);
    update_opts();
}

EVT_HANDLER_MASK(SaveGameOldest, "Save state to oldest slot", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState();
}

EVT_HANDLER_MASK(SaveGame01, "Save state 1", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(1);
}

EVT_HANDLER_MASK(SaveGame02, "Save state 2", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(2);
}

EVT_HANDLER_MASK(SaveGame03, "Save state 3", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(3);
}

EVT_HANDLER_MASK(SaveGame04, "Save state 4", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(4);
}

EVT_HANDLER_MASK(SaveGame05, "Save state 5", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(5);
}

EVT_HANDLER_MASK(SaveGame06, "Save state 6", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(6);
}

EVT_HANDLER_MASK(SaveGame07, "Save state 7", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(7);
}

EVT_HANDLER_MASK(SaveGame08, "Save state 8", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(8);
}

EVT_HANDLER_MASK(SaveGame09, "Save state 9", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(9);
}

EVT_HANDLER_MASK(SaveGame10, "Save state 10", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(10);
}

EVT_HANDLER_MASK(Save, "Save state as...", CMDEN_GB | CMDEN_GBA)
{
    if (st_dir.empty())
        st_dir = panel->state_dir();

    wxFileDialog dlg(this, _("Select state file"), st_dir, wxEmptyString,
        _("VisualBoyAdvance saved game files|*.sgm"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    int ret = ShowModal(&dlg);
    st_dir = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    panel->SaveState(dlg.GetPath());
}

static int state_slot = 0;

// new
EVT_HANDLER_MASK(LoadGameSlot, "Load current state slot", CMDEN_GB | CMDEN_GBA)
{
    panel->LoadState(state_slot + 1);
}

// new
EVT_HANDLER_MASK(SaveGameSlot, "Save current state slot", CMDEN_GB | CMDEN_GBA)
{
    panel->SaveState(state_slot + 1);
}

// new
EVT_HANDLER_MASK(IncrGameSlot, "Increase state slot number", CMDEN_GB | CMDEN_GBA)
{
    state_slot = (state_slot + 1) % 10;

    wxString msg;
    msg.Printf(_("Current state slot #%d"), state_slot);
    systemScreenMessage(msg);
}

// new
EVT_HANDLER_MASK(DecrGameSlot, "Decrease state slot number", CMDEN_GB | CMDEN_GBA)
{
    state_slot = (state_slot + 9) % 10;

    wxString msg;
    msg.Printf(_("Current state slot #%d"), state_slot);
    systemScreenMessage(msg);
}

// new
EVT_HANDLER_MASK(IncrGameSlotSave, "Increase state slot number and save", CMDEN_GB | CMDEN_GBA)
{
    state_slot = (state_slot + 1) % 10;
    panel->SaveState(state_slot + 1);

    wxString msg;
    msg.Printf(_("Current state slot #%d"), state_slot);
    systemScreenMessage(msg);
}

EVT_HANDLER_MASK(Rewind, "Rewind", CMDEN_REWIND)
{
    MainFrame* mf = wxGetApp().frame;
    GameArea* panel = mf->GetPanel();
    int rew_st = (panel->next_rewind_state + NUM_REWINDS - 1) % NUM_REWINDS;

    // if within 5 seconds of last one, and > 1 state, delete last state & move back
    // FIXME: 5 should actually be user-configurable
    // maybe instead of 5, 10% of rewind_interval
    if (panel->num_rewind_states > 1 && (gopts.rewind_interval <= 5 || (int)panel->rewind_time / 6 > gopts.rewind_interval - 5)) {
        --panel->num_rewind_states;
        panel->next_rewind_state = rew_st;

        if (gopts.rewind_interval > 5)
            rew_st = (rew_st + NUM_REWINDS - 1) % NUM_REWINDS;
    }

    panel->emusys->emuReadMemState(&panel->rewind_mem[rew_st * REWIND_SIZE],
        REWIND_SIZE);
    InterframeCleanup();
    // FIXME: if(paused) blank screen
    panel->do_rewind = false;
    panel->rewind_time = gopts.rewind_interval * 6;
    //    systemScreenMessage(_("Rewinded"));
}

EVT_HANDLER_MASK(CheatsList, "List cheats...", CMDEN_GB | CMDEN_GBA)
{
    wxDialog* dlg = GetXRCDialog("CheatList");
    ShowModal(dlg);
}

EVT_HANDLER_MASK(CheatsSearch, "Create cheat...", CMDEN_GB | CMDEN_GBA)
{
    wxDialog* dlg = GetXRCDialog("CheatCreate");
    ShowModal(dlg);
}

// new
EVT_HANDLER(CheatsAutoSaveLoad, "Auto save/load cheats (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("CheatsAutoSaveLoad", &menuPress);
    toggleBooleanVar(&menuPress, &gopts.autoload_cheats);
    SetMenuOption("CheatsAutoSaveLoad", gopts.autoload_cheats ? 1 : 0);
    update_opts();
}

// was CheatsDisable
// changed for convenience to match internal variable functionality
EVT_HANDLER(CheatsEnable, "Enable cheats (toggle)")
{
    bool menuPress = false;
    GetMenuOptionBool("CheatsEnable", &menuPress);
    toggleBitVar(&menuPress, &cheatsEnabled, 1);
    SetMenuOption("CheatsEnable", menuPress ? 1 : 0);
    GetMenuOptionInt("CheatsEnable", &cheatsEnabled, 1);
    update_opts();
}

EVT_HANDLER(ColorizerHack, "Enable Colorizer Hack (toggle)")
{
    int val = 0;
    GetMenuOptionInt("ColorizerHack", &val, 1);

    if (val == 1 && useBiosFileGB == 1) {
        wxLogError(_("Cannot use Colorizer Hack when GB BIOS File is enabled."));
        val = 0;
        SetMenuOption("ColorizerHack", 0);
    }

    colorizerHack = val;

    update_opts();
}

// Debug menu
EVT_HANDLER_MASK(VideoLayersBG0, "Video layer BG0 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "VideoLayersBG0";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &layerSettings, (1 << 8));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &layerSettings, (1 << 8));
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(VideoLayersBG1, "Video layer BG1 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "VideoLayersBG1";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &layerSettings, (1 << 9));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &layerSettings, (1 << 9));
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(VideoLayersBG2, "Video layer BG2 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "VideoLayersBG2";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &layerSettings, (1 << 10));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &layerSettings, (1 << 10));
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(VideoLayersBG3, "Video layer BG3 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "VideoLayersBG3";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &layerSettings, (1 << 11));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &layerSettings, (1 << 11));
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(VideoLayersOBJ, "Video layer OBJ (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "VideoLayersOBJ";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &layerSettings, (1 << 12));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &layerSettings, (1 << 12));
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(VideoLayersWIN0, "Video layer WIN0 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "VideoLayersWIN0";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &layerSettings, (1 << 13));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &layerSettings, (1 << 13));
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(VideoLayersWIN1, "Video layer WIN1 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "VideoLayersWIN1";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &layerSettings, (1 << 14));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &layerSettings, (1 << 14));
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(VideoLayersOBJWIN, "Video layer OBJWIN (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "VideoLayersOBJWIN";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &layerSettings, (1 << 15));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &layerSettings, (1 << 15));
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(VideoLayersReset, "Show all video layers", CMDEN_GB | CMDEN_GBA)
{
#define set_vl(s)                                     \
    do {                                              \
        int id = XRCID(s);                            \
        for (size_t i = 0; i < checkable_mi.size(); i++) \
            if (checkable_mi[i].cmd == id) {          \
                checkable_mi[i].mi->Check(true);      \
                break;                                \
            }                                         \
    } while (0)
    layerSettings = 0x7f00;
    layerEnable = DISPCNT & layerSettings;
    set_vl("VideoLayersBG0");
    set_vl("VideoLayersBG1");
    set_vl("VideoLayersBG2");
    set_vl("VideoLayersBG3");
    set_vl("VideoLayersOBJ");
    set_vl("VideoLayersWIN0");
    set_vl("VideoLayersWIN1");
    set_vl("VideoLayersOBJWIN");
    CPUUpdateRenderBuffers(false);
}

EVT_HANDLER_MASK(SoundChannel1, "Sound Channel 1 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "SoundChannel1";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &gopts.sound_en, (1 << 0));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &gopts.sound_en, (1 << 0));
    soundSetEnable(gopts.sound_en);
    update_opts();
}

EVT_HANDLER_MASK(SoundChannel2, "Sound Channel 2 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "SoundChannel2";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &gopts.sound_en, (1 << 1));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &gopts.sound_en, (1 << 1));
    soundSetEnable(gopts.sound_en);
    update_opts();
}

EVT_HANDLER_MASK(SoundChannel3, "Sound Channel 3 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "SoundChannel3";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &gopts.sound_en, (1 << 2));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &gopts.sound_en, (1 << 2));
    soundSetEnable(gopts.sound_en);
    update_opts();
}

EVT_HANDLER_MASK(SoundChannel4, "Sound Channel 4 (toggle)", CMDEN_GB | CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "SoundChannel4";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &gopts.sound_en, (1 << 3));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &gopts.sound_en, (1 << 3));
    soundSetEnable(gopts.sound_en);
    update_opts();
}

EVT_HANDLER_MASK(DirectSoundA, "Direct Sound A (toggle)", CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "DirectSoundA";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &gopts.sound_en, (1 << 8));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &gopts.sound_en, (1 << 8));
    soundSetEnable(gopts.sound_en);
    update_opts();
}

EVT_HANDLER_MASK(DirectSoundB, "Direct Sound B (toggle)", CMDEN_GBA)
{
    bool menuPress = false;
    char keyName[] = "DirectSoundB";
    GetMenuOptionBool(keyName, &menuPress);
    toggleBitVar(&menuPress, &gopts.sound_en, (1 << 9));
    SetMenuOption(keyName, menuPress ? 1 : 0);
    GetMenuOptionInt(keyName, &gopts.sound_en, (1 << 9));
    soundSetEnable(gopts.sound_en);
    update_opts();
}

EVT_HANDLER(ToggleSound, "Enable/disable all sound channels")
{
    bool en = gopts.sound_en == 0;
    gopts.sound_en = en ? 0x30f : 0;
    SetMenuOption("SoundChannel1", en);
    SetMenuOption("SoundChannel2", en);
    SetMenuOption("SoundChannel3", en);
    SetMenuOption("SoundChannel4", en);
    SetMenuOption("DirectSoundA", en);
    SetMenuOption("DirectSoundB", en);
    soundSetEnable(gopts.sound_en);
    update_opts();
    systemScreenMessage(en ? _("Sound enabled") : _("Sound disabled"));
}

EVT_HANDLER(IncreaseVolume, "Increase volume")
{
    gopts.sound_vol += 5;

    if (gopts.sound_vol > 200)
        gopts.sound_vol = 200;

    update_opts();
    soundSetVolume((float)gopts.sound_vol / 100.0);
    wxString msg;
    msg.Printf(_("Volume: %d%%"), gopts.sound_vol);
    systemScreenMessage(msg);
}

EVT_HANDLER(DecreaseVolume, "Decrease volume")
{
    gopts.sound_vol -= 5;

    if (gopts.sound_vol < 0)
        gopts.sound_vol = 0;

    update_opts();
    soundSetVolume((float)gopts.sound_vol / 100.0);
    wxString msg;
    msg.Printf(_("Volume: %d%%"), gopts.sound_vol);
    systemScreenMessage(msg);
}

EVT_HANDLER_MASK(NextFrame, "Next Frame", CMDEN_GB | CMDEN_GBA)
{
    SetMenuOption("Pause", true);
    paused = true;
    pause_next = true;

    if (!IsPaused())
        panel->Resume();

    systemFrameSkip = 0;
}

EVT_HANDLER_MASK(Disassemble, "Disassemble...", CMDEN_GB | CMDEN_GBA)
{
    Disassemble();
}

EVT_HANDLER(Logging, "Logging...")
{
    wxDialog* dlg = wxGetApp().frame->logdlg;
    dlg->SetWindowStyle(wxCAPTION | wxRESIZE_BORDER);

    if (gopts.keep_on_top)
        dlg->SetWindowStyle(dlg->GetWindowStyle() | wxSTAY_ON_TOP);
    else
        dlg->SetWindowStyle(dlg->GetWindowStyle() & ~wxSTAY_ON_TOP);

    dlg->Show();
    dlg->Raise();
}

EVT_HANDLER_MASK(IOViewer, "I/O Viewer...", CMDEN_GBA)
{
    IOViewer();
}

EVT_HANDLER_MASK(MapViewer, "Map Viewer...", CMDEN_GB | CMDEN_GBA)
{
    MapViewer();
}

EVT_HANDLER_MASK(MemoryViewer, "Memory Viewer...", CMDEN_GB | CMDEN_GBA)
{
    MemViewer();
}

EVT_HANDLER_MASK(OAMViewer, "OAM Viewer...", CMDEN_GB | CMDEN_GBA)
{
    OAMViewer();
}

EVT_HANDLER_MASK(PaletteViewer, "Palette Viewer...", CMDEN_GB | CMDEN_GBA)
{
    PaletteViewer();
}

EVT_HANDLER_MASK(TileViewer, "Tile Viewer...", CMDEN_GB | CMDEN_GBA)
{
    TileViewer();
}

#ifndef NO_DEBUGGER
extern int remotePort;

int GetGDBPort(MainFrame* mf)
{
    ModalPause mp;
    return wxGetNumberFromUser(
#ifdef __WXMSW__
        wxEmptyString,
#else
        _("Set to 0 for pseudo tty"),
#endif
        _("Port to wait for connection:"),
        _("GDB Connection"), gdbPort,
#ifdef __WXMSW__
        1025,
#else
        0,
#endif
        65535, mf);
}
#endif

EVT_HANDLER(DebugGDBPort, "Configure port...")
{
#ifndef NO_DEBUGGER
    int port_selected = GetGDBPort(this);

    if (port_selected != -1) {
        gdbPort = port_selected;
        update_opts();
    }
#endif
}

EVT_HANDLER(DebugGDBBreakOnLoad, "Break on load")
{
#ifndef NO_DEBUGGER
    GetMenuOptionInt("DebugGDBBreakOnLoad", &gdbBreakOnLoad, 1);
    update_opts();
#endif
}

#ifndef NO_DEBUGGER
void MainFrame::GDBBreak()
{
    ModalPause mp;

    if (gdbPort == 0) {
        int port_selected = GetGDBPort(this);

        if (port_selected != -1) {
            gdbPort = port_selected;
            update_opts();
        }
    }

    if (gdbPort > 0) {
        if (!remotePort) {
            wxString msg;
#ifndef __WXMSW__

            if (!gdbPort) {
                if (!debugOpenPty())
                    return;

                msg.Printf(_("Waiting for connection at %s"), debugGetSlavePty().wc_str());
            } else
#endif
            {
                if (!debugStartListen(gdbPort))
                    return;

                msg.Printf(_("Waiting for connection on port %d"), gdbPort);
            }

            wxProgressDialog dlg(_("Waiting for GDB..."), msg, 100, this,
                wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME);
            bool connected = false;

            while (dlg.Pulse()) {
#ifndef __WXMSW__

                if (!gdbPort)
                    connected = debugWaitPty();
                else
#endif
                    connected = debugWaitSocket();

                if (connected)
                    break;

                // sleep a bit more in case of infinite loop
                wxMilliSleep(10);
            }

            if (connected) {
                remotePort = gdbPort;
                emulating = 1;
                dbgMain = remoteStubMain;
                dbgSignal = remoteStubSignal;
                dbgOutput = remoteOutput;
                cmd_enable &= ~(CMDEN_NGDB_ANY | CMDEN_NGDB_GBA);
                cmd_enable |= CMDEN_GDB;
                enable_menus();
                debugger = true;
            } else {
                remoteCleanUp();
            }
        } else {
            if (armState) {
                armNextPC -= 4;
                reg[15].I -= 4;
            } else {
                armNextPC -= 2;
                reg[15].I -= 2;
            }

            debugger = true;
        }
    }
}
#endif

EVT_HANDLER_MASK(DebugGDBBreak, "Break into GDB", CMDEN_NGDB_GBA | CMDEN_GDB)
{
#ifndef NO_DEBUGGER
    GDBBreak();
#endif
}

EVT_HANDLER_MASK(DebugGDBDisconnect, "Disconnect GDB", CMDEN_GDB)
{
#ifndef NO_DEBUGGER
    debugger = false;
    dbgMain = NULL;
    dbgSignal = NULL;
    dbgOutput = NULL;
    remotePort = 0;
    remoteCleanUp();
    cmd_enable &= ~CMDEN_GDB;
    cmd_enable |= CMDEN_NGDB_GBA | CMDEN_NGDB_ANY;
    enable_menus();
#endif
}

// Options menu
EVT_HANDLER(GeneralConfigure, "General options...")
{
    int rew = gopts.rewind_interval;
    wxDialog* dlg = GetXRCDialog("GeneralConfig");

    if (ShowModal(dlg) == wxID_OK)
        update_opts();

    if (panel->game_type() != IMAGE_UNKNOWN)
        soundSetThrottle(throttle);

    if (rew != gopts.rewind_interval) {
        if (!gopts.rewind_interval) {
            if (panel->num_rewind_states) {
                cmd_enable &= ~CMDEN_REWIND;
                enable_menus();
            }

            panel->num_rewind_states = 0;
            panel->do_rewind = false;
        } else {
            if (!panel->num_rewind_states)
                panel->do_rewind = true;

            panel->rewind_time = gopts.rewind_interval * 6;
        }
    }
}

EVT_HANDLER(SpeedupConfigure, "Speedup / Turbo options...")
{
    wxDialog* dlg = GetXRCDialog("SpeedupConfig");

    unsigned save_speedup_throttle            = speedup_throttle;
    unsigned save_speedup_frame_skip          = speedup_frame_skip;
    bool     save_speedup_throttle_frame_skip = speedup_throttle_frame_skip;

    if (ShowModal(dlg) == wxID_OK)
        update_opts();
    else {
        // Restore values if cancel pressed.
        speedup_throttle            = save_speedup_throttle;
        speedup_frame_skip          = save_speedup_frame_skip;
        speedup_throttle_frame_skip = save_speedup_throttle_frame_skip;
    }
}

EVT_HANDLER(UIConfigure, "UI Settings...")
{
    wxDialog* dlg = GetXRCDialog("UIConfig");

    if (ShowModal(dlg) == wxID_OK)
        update_opts();
}

EVT_HANDLER(GameBoyConfigure, "Game Boy options...")
{
    wxDialog* dlg = GetXRCDialog("GameBoyConfig");
    wxChoice* c = XRCCTRL(*dlg, "Borders", wxChoice);
    bool borderon = gbBorderOn;

    if (!gbBorderOn && !gbBorderAutomatic)
        c->SetSelection(0);
    else if (gbBorderOn)
        c->SetSelection(1);
    else
        c->SetSelection(2);

    if (ShowModal(dlg) != wxID_OK)
        return;

    switch (c->GetSelection()) {
    case 0:
        gbBorderOn = gbBorderAutomatic = false;
        break;

    case 1:
        gbBorderOn = true;
        break;

    case 2:
        gbBorderOn = false;
        gbBorderAutomatic = true;
        break;
    }

    // this value might have been overwritten by FrameSkip
    if (panel->game_type() == IMAGE_GB) {
        if (borderon != gbBorderOn) {
            if (gbBorderOn) {
                panel->AddBorder();
                gbSgbRenderBorder();
            } else
                panel->DelBorder();
        }

        // don't want to have to reset to change colors
        memcpy(gbPalette, &systemGbPalette[gbPaletteOption * 8], 8 * sizeof(systemGbPalette[0]));
    }

    update_opts();
}

EVT_HANDLER(SetSize1x, "1x")
{
    config::OptDispScale()->SetDouble(1);
}

EVT_HANDLER(SetSize2x, "2x")
{
    config::OptDispScale()->SetDouble(2);
}

EVT_HANDLER(SetSize3x, "3x")
{
    config::OptDispScale()->SetDouble(3);
}

EVT_HANDLER(SetSize4x, "4x")
{
    config::OptDispScale()->SetDouble(4);
}

EVT_HANDLER(SetSize5x, "5x")
{
    config::OptDispScale()->SetDouble(5);
}

EVT_HANDLER(SetSize6x, "6x")
{
    config::OptDispScale()->SetDouble(6);
}

EVT_HANDLER(GameBoyAdvanceConfigure, "Game Boy Advance options...")
{
    wxDialog* dlg = GetXRCDialog("GameBoyAdvanceConfig");
    wxTextCtrl* ovcmt = XRCCTRL(*dlg, "Comment", wxTextCtrl);
    wxString cmt;
    wxChoice *ovrtc = XRCCTRL(*dlg, "OvRTC", wxChoice),
             *ovst = XRCCTRL(*dlg, "OvSaveType", wxChoice),
             *ovfs = XRCCTRL(*dlg, "OvFlashSize", wxChoice),
             *ovmir = XRCCTRL(*dlg, "OvMirroring", wxChoice);

    if (panel->game_type() == IMAGE_GBA) {
        wxString s = wxString((const char*)&rom[0xac], wxConvLibc, 4);
        XRCCTRL(*dlg, "GameCode", wxControl)
            ->SetLabel(s);
        cmt = wxString((const char*)&rom[0xa0], wxConvLibc, 12);
        wxFileConfig* cfg = wxGetApp().overrides;

        if (cfg->HasGroup(s)) {
            cfg->SetPath(s);
            cmt = cfg->Read(wxT("comment"), cmt);
            ovcmt->SetValue(cmt);
            ovrtc->SetSelection(cfg->Read(wxT("rtcEnabled"), -1) + 1);
            ovst->SetSelection(cfg->Read(wxT("saveType"), -1) + 1);
            ovfs->SetSelection((cfg->Read(wxT("flashSize"), -1) >> 17) + 1);
            ovmir->SetSelection(cfg->Read(wxT("mirroringEnabled"), -1) + 1);
            cfg->SetPath(wxT("/"));
        } else {
            ovcmt->SetValue(cmt);
            ovrtc->SetSelection(0);
            ovst->SetSelection(0);
            ovfs->SetSelection(0);
            ovmir->SetSelection(0);
        }
    } else {
        XRCCTRL(*dlg, "GameCode", wxControl)
            ->SetLabel(wxEmptyString);
        ovcmt->SetValue(wxEmptyString);
        ovrtc->SetSelection(0);
        ovst->SetSelection(0);
        ovfs->SetSelection(0);
        ovmir->SetSelection(0);
    }

    if (ShowModal(dlg) != wxID_OK)
        return;

    if (panel->game_type() == IMAGE_GBA) {
        agbPrintEnable(agbPrint);
        wxString s = wxString((const char*)&rom[0xac], wxConvLibc, 4);
        wxFileConfig* cfg = wxGetApp().overrides;
        bool chg;

        if (cfg->HasGroup(s)) {
            cfg->SetPath(s);
            chg = ovcmt->GetValue() != cmt || ovrtc->GetSelection() != cfg->Read(wxT("rtcEnabled"), -1) + 1 || ovst->GetSelection() != cfg->Read(wxT("saveType"), -1) + 1 || ovfs->GetSelection() != (cfg->Read(wxT("flashSize"), -1) >> 17) + 1 || ovmir->GetSelection() != cfg->Read(wxT("mirroringEnabled"), -1) + 1;
            cfg->SetPath(wxT("/"));
        } else
            chg = ovrtc->GetSelection() != 0 || ovst->GetSelection() != 0 || ovfs->GetSelection() != 0 || ovmir->GetSelection() != 0;

        if (chg) {
            wxString vba_over;
            wxFileName fn(wxGetApp().GetConfigurationPath(), wxT("vba-over.ini"));

            if (fn.FileExists()) {
                wxFileInputStream fis(fn.GetFullPath());
                wxStringOutputStream sos(&vba_over);
                fis.Read(sos);
            }

            if (cfg->HasGroup(s)) {
                cfg->SetPath(s);

                if (cfg->Read(wxT("path"), wxEmptyString) == fn.GetPath()) {
                    // EOL can be either \n (unix), \r\n (dos), or \r (old mac)
                    wxString res(wxT("(^|[\n\r])" // a new line
                                     L"(" // capture group as \2
                                     L"(#[^\n\r]*(\r?\n|\r))?" // an optional comment line
                                     L"\\[")); // the group header
                    res += s;
                    res += wxT("\\]"
                               L"([^[#]" // non-comment non-group-start chars
                               L"|[^\r\n \t][ \t]*[[#]" // or comment/grp start chars in middle of line
                               L"|#[^\n\r]*(\r?\n|\r)[^[]" // or comments not followed by grp start
                               L")*"
                               L")" // end of group
                        // no need to try to describe what's next
                        // as the regex should maximize match size
                        );
                    wxRegEx re(res);

                    // there may be more than one group if it was hand-edited
                    // so remove them all
                    // could use re.Replace(), but this is more reliable
                    while (re.Matches(vba_over)) {
                        size_t beg, end;
                        re.GetMatch(&beg, &end, 2);
                        vba_over.erase(beg, end - beg);
                    }
                }

                cfg->SetPath(wxT("/"));
                cfg->DeleteGroup(s);
            }

            cfg->SetPath(s);
            cfg->Write(wxT("path"), fn.GetPath());
            cfg->Write(wxT("comment"), ovcmt->GetValue());
            vba_over.append(wxT("# "));
            vba_over.append(ovcmt->GetValue());
            vba_over.append(wxTextFile::GetEOL());
            vba_over.append(wxT('['));
            vba_over.append(s);
            vba_over.append(wxT(']'));
            vba_over.append(wxTextFile::GetEOL());
            int sel;
#define appendval(n)                                   \
    do {                                               \
        vba_over.append(wxT(n));                       \
        vba_over.append(wxT('='));                     \
        vba_over.append((wxChar)(wxT('0') + sel - 1)); \
        vba_over.append(wxTextFile::GetEOL());         \
        cfg->Write(wxT(n), sel - 1);                   \
    } while (0)

            if ((sel = ovrtc->GetSelection()) > 0)
                appendval("rtcEnabled");

            if ((sel = ovst->GetSelection()) > 0)
                appendval("saveType");

            if ((sel = ovfs->GetSelection()) > 0) {
                vba_over.append(wxT("flashSize="));
                vba_over.append(sel == 1 ? wxT("65536") : wxT("131072"));
                vba_over.append(wxTextFile::GetEOL());
                cfg->Write(wxT("flashSize"), 0x10000 << (sel - 1));
            }

            if ((sel = ovmir->GetSelection()) > 0)
                appendval("mirroringEnabled");

            cfg->SetPath(wxT("/"));
            vba_over.append(wxTextFile::GetEOL());
            fn.Mkdir(0777, wxPATH_MKDIR_FULL);
            wxTempFileOutputStream fos(fn.GetFullPath());
            fos.Write(vba_over.c_str(), vba_over.size());
            fos.Commit();
        }
    }

    update_opts();
}

EVT_HANDLER_MASK(DisplayConfigure, "Display options...", CMDEN_NREC_ANY)
{
    if (gopts.max_threads != 1) {
        gopts.max_threads = wxThread::GetCPUCount();
    }

    // Just in case GetCPUCount() returns 0 or -1
    if (gopts.max_threads < 0) {
        gopts.max_threads = 1;
    }

    wxDialog* dlg = GetXRCDialog("DisplayConfig");
    if (ShowModal(dlg) != wxID_OK) {
        return;
    }

    if (frameSkip >= 0) {
        systemFrameSkip = frameSkip;
    }

    update_opts();
}

EVT_HANDLER_MASK(ChangeFilter, "Change Pixel Filter", CMDEN_NREC_ANY)
{
    config::OptDispFilter()->NextFilter();
}

EVT_HANDLER_MASK(ChangeIFB, "Change Interframe Blending", CMDEN_NREC_ANY)
{
    config::OptDispIFB()->NextInterframe();
}

EVT_HANDLER_MASK(SoundConfigure, "Sound options...", CMDEN_NREC_ANY)
{
    int oqual = gopts.sound_qual, oapi = gopts.audio_api;
    bool oupmix = gopts.upmix, ohw = gopts.dsound_hw_accel;
    wxString odev = gopts.audio_dev;
    wxDialog* dlg = GetXRCDialog("SoundConfig");

    if (ShowModal(dlg) != wxID_OK)
        return;

    switch (panel->game_type()) {
    case IMAGE_UNKNOWN:
        break;

    case IMAGE_GB:
        gb_effects_config.echo = (float)gopts.gb_echo / 100.0;
        gb_effects_config.stereo = (float)gopts.gb_stereo / 100.0;
        gbSoundSetSampleRate(!gopts.sound_qual ? 48000 : 44100 / (1 << (gopts.sound_qual - 1)));
        break;

    case IMAGE_GBA:
        soundSetSampleRate(!gopts.sound_qual ? 48000 : 44100 / (1 << (gopts.sound_qual - 1)));
        soundFiltering = (float)gopts.gba_sound_filter / 100.0f;
        break;
    }

    // changing sample rate causes driver reload, so no explicit reload needed
    if (oqual == gopts.sound_qual &&
        // otherwise reload if API changes
        (oapi != gopts.audio_api || odev != gopts.audio_dev ||
            // or init-only options
            (oapi == AUD_XAUDIO2 && oupmix != gopts.upmix) || (oapi == AUD_FAUDIO && oupmix != gopts.upmix) || (oapi == AUD_DIRECTSOUND && ohw != gopts.dsound_hw_accel))) {
        soundShutdown();

        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
    }

    soundSetVolume((float)gopts.sound_vol / 100.0);
    update_opts();
}

EVT_HANDLER(EmulatorDirectories, "Directories...")
{
    wxDialog* dlg = GetXRCDialog("DirectoriesConfig");

    if (ShowModal(dlg) == wxID_OK)
        update_opts();
}

EVT_HANDLER(JoypadConfigure, "Joypad options...")
{
    wxDialog* dlg = GetXRCDialog("JoypadConfig");
    joy.PollAllJoysticks();

    auto frame = wxGetApp().frame;
    bool joy_timer = frame->IsJoyPollTimerRunning();

    if (!joy_timer) frame->StartJoyPollTimer();

    if (ShowModal(dlg) == wxID_OK)
        update_opts();

    if (!joy_timer) frame->StopJoyPollTimer();

    SetJoystick();
}

EVT_HANDLER(Customize, "Customize UI...")
{
    wxDialog* dlg = GetXRCDialog("AccelConfig");
    joy.PollAllJoysticks();

    auto frame = wxGetApp().frame;
    bool joy_timer = frame->IsJoyPollTimerRunning();

    if (!joy_timer) frame->StartJoyPollTimer();

    if (ShowModal(dlg) == wxID_OK)
        update_opts();

    if (!joy_timer) frame->StopJoyPollTimer();

    SetJoystick();
}

#ifndef NO_ONLINEUPDATES
#include "autoupdater/autoupdater.h"
#endif // NO_ONLINEUPDATES

EVT_HANDLER(UpdateEmu, "Check for updates...")
{
#ifndef NO_ONLINEUPDATES
    checkUpdatesUi();
#endif // NO_ONLINEUPDATES
}

EVT_HANDLER(FactoryReset, "Factory Reset...")
{
    wxMessageDialog dlg(NULL, wxString(wxT(
"YOUR CONFIGURATION WILL BE DELETED!\n\n")) + wxString(wxT(
"Are you sure?")),
                        wxT("FACTORY RESET"), wxYES_NO | wxNO_DEFAULT | wxCENTRE);

    if (dlg.ShowModal() == wxID_YES) {
        wxGetApp().cfg->DeleteAll();

        wxExecute(wxStandardPaths::Get().GetExecutablePath(), wxEXEC_ASYNC);
        Close(true);
    }
}

EVT_HANDLER(BugReport, "Report bugs...")
{
    wxLaunchDefaultBrowser(wxT("https://github.com/visualboyadvance-m/visualboyadvance-m/issues"));
}

EVT_HANDLER(FAQ, "VBA-M support forum")
{
    wxLaunchDefaultBrowser(wxT("https://github.com/visualboyadvance-m/visualboyadvance-m/"));
}

EVT_HANDLER(Translate, "Translations")
{
    wxLaunchDefaultBrowser(wxT("http://www.transifex.com/projects/p/vba-m"));
}

// was About
EVT_HANDLER(wxID_ABOUT, "About...")
{
    wxAboutDialogInfo ai;
    ai.SetName(wxT("VisualBoyAdvance-M"));
    wxString version(vbam_version);
    ai.SetVersion(version);
    // setting website, icon, license uses custom aboutbox on win32 & macosx
    // but at least win32 standard about is nothing special
    ai.SetWebSite(wxT("http://www.vba-m.com/"));
    ai.SetIcon(GetIcons().GetIcon(wxSize(32, 32), wxIconBundle::FALLBACK_NEAREST_LARGER));
    ai.SetDescription(_("Nintendo GameBoy (+Color+Advance) emulator."));
    ai.SetCopyright(_("Copyright (C) 1999-2003 Forgotten\nCopyright (C) 2004-2006 VBA development team\nCopyright (C) 2007-2020 VBA-M development team"));
    ai.SetLicense(_("This program is free software: you can redistribute it and/or modify\n"
                    "it under the terms of the GNU General Public License as published by\n"
                    "the Free Software Foundation, either version 2 of the License, or\n"
                    "(at your option) any later version.\n\n"
                    "This program is distributed in the hope that it will be useful,\n"
                    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                    "GNU General Public License for more details.\n\n"
                    "You should have received a copy of the GNU General Public License\n"
                    "along with this program.  If not, see http://www.gnu.org/licenses ."));
    // from gtk
    ai.AddDeveloper(wxT("Forgotten"));
    ai.AddDeveloper(wxT("kxu"));
    ai.AddDeveloper(wxT("Pokemonhacker"));
    ai.AddDeveloper(wxT("Spacy51"));
    ai.AddDeveloper(wxT("mudlord"));
    ai.AddDeveloper(wxT("Nach"));
    ai.AddDeveloper(wxT("jbo_85"));
    ai.AddDeveloper(wxT("bgK"));
    ai.AddArtist(wxT("Matteo Drera"));
    ai.AddArtist(wxT("Jakub Steiner"));
    ai.AddArtist(wxT("Jones Lee"));
    // from win32
    ai.AddDeveloper(wxT("Jonas Quinn"));
    ai.AddDeveloper(wxT("DJRobX"));
    ai.AddDeveloper(wxT("Spacy"));
    ai.AddDeveloper(wxT("Squall Leonhart"));
    // wx
    ai.AddDeveloper(wxT("Thomas J. Moore"));
    // from win32 "thanks"
    ai.AddDeveloper(wxT("blargg"));
    ai.AddDeveloper(wxT("Costis"));
    ai.AddDeveloper(wxT("chrono"));
    ai.AddDeveloper(wxT("xKiv"));
    ai.AddDeveloper(wxT("skidau"));
    ai.AddDeveloper(wxT("TheCanadianBacon"));
    ai.AddDeveloper(wxT("rkitover"));
    ai.AddDeveloper(wxT("Mystro256"));
    ai.AddDeveloper(wxT("retro-wertz"));
    ai.AddDeveloper(wxT("denisfa"));
    ai.AddDeveloper(wxT("orbea"));
    ai.AddDeveloper(wxT("Orig. VBA team"));
    ai.AddDeveloper(wxT("... many contributors who send us patches/PRs"));
    wxAboutBox(ai);
}

EVT_HANDLER(Bilinear, "Use bilinear filter with 3d renderer")
{
    GetMenuOptionBool("Bilinear", &gopts.bilinear);
    // Force new panel with new bilinear option.
    panel->ResetPanel();
    update_opts();
}

EVT_HANDLER(RetainAspect, "Retain aspect ratio when resizing")
{
    GetMenuOptionBool("RetainAspect", &gopts.retain_aspect);
    // Force new panel with new aspect ratio options.
    panel->ResetPanel();
    update_opts();
}

EVT_HANDLER(Printer, "Enable printer emulation")
{
    GetMenuOptionInt("Printer", &winGbPrinterEnabled, 1);
#if (defined __WIN32__ || defined _WIN32)
#ifndef NO_LINK
    gbSerialFunction = gbStartLink;
#else
    gbSerialFunction = NULL;
#endif
#endif
    if (winGbPrinterEnabled)
        gbSerialFunction = gbPrinterSend;

    update_opts();
}

EVT_HANDLER(PrintGather, "Automatically gather a full page before printing")
{
    GetMenuOptionBool("PrintGather", &gopts.print_auto_page);
    update_opts();
}

EVT_HANDLER(PrintSnap, "Automatically save printouts as screen captures with -print suffix")
{
    GetMenuOptionBool("PrintSnap", &gopts.print_screen_cap);
    update_opts();
}

EVT_HANDLER(GBASoundInterpolation, "GBA sound interpolation")
{
    GetMenuOptionBool("GBASoundInterpolation", &soundInterpolation);
    update_opts();
}

EVT_HANDLER(GBDeclicking, "GB sound declicking")
{
    GetMenuOptionBool("GBDeclicking", &gopts.gb_declick);
    // note that setting declick may reset gb sound engine
    gbSoundSetDeclicking(gopts.gb_declick);
    update_opts();
}

EVT_HANDLER(GBEnhanceSound, "Enable GB sound effects")
{
    GetMenuOptionBool("GBEnhanceSound", &gopts.gb_effects_config_enabled);
    gb_effects_config.enabled = gopts.gb_effects_config_enabled;
    update_opts();
}

EVT_HANDLER(GBSurround, "GB surround sound effect (%)")
{
    GetMenuOptionBool("GBSurround",&gopts.gb_effects_config_surround);
    gb_effects_config.surround = gopts.gb_effects_config_surround;
    update_opts();
}

EVT_HANDLER(AGBPrinter, "Enable AGB printer")
{
    GetMenuOptionInt("AGBPrinter", &agbPrint, 1);
    update_opts();
}

EVT_HANDLER_MASK(GBALcdFilter, "Enable LCD filter", CMDEN_GBA)
{
    bool menuPress = false;
    GetMenuOptionBool("GBALcdFilter", &menuPress);
    toggleBooleanVar(&menuPress, &gbaLcdFilter);
    SetMenuOption("GBALcdFilter", gbaLcdFilter ? 1 : 0);
    utilUpdateSystemColorMaps(gbaLcdFilter);
    update_opts();
}

EVT_HANDLER_MASK(GBLcdFilter, "Enable LCD filter", CMDEN_GB)
{
    bool menuPress = false;
    GetMenuOptionBool("GBLcdFilter", &menuPress);
    toggleBooleanVar(&menuPress, &gbLcdFilter);
    SetMenuOption("GBLcdFilter", gbLcdFilter ? 1 : 0);
    utilUpdateSystemColorMaps(gbLcdFilter);
    update_opts();
}

EVT_HANDLER(GBColorOption, "Enable GB color option")
{
    bool menuPress = false;
    bool intVar = gbColorOption ? true : false;
    GetMenuOptionBool("GBColorOption", &menuPress);
    toggleBooleanVar(&menuPress, &intVar);
    SetMenuOption("GBColorOption", intVar ? 1 : 0);
    gbColorOption = intVar ? 1 : 0;
    update_opts();
}

EVT_HANDLER(ApplyPatches, "Apply IPS/UPS/IPF patches if found")
{
    GetMenuOptionInt("ApplyPatches", &autoPatch, 1);
    update_opts();
}

EVT_HANDLER(MMX, "Enable MMX")
{
#ifdef MMX
    GetMenuOptionInt("MMX", &enableMMX, 1);
    update_opts();
#endif
}

EVT_HANDLER(KeepOnTop, "Keep window on top")
{
    GetMenuOptionBool("KeepOnTop", &gopts.keep_on_top);
    MainFrame* mf = wxGetApp().frame;

    if (gopts.keep_on_top)
        mf->SetWindowStyle(mf->GetWindowStyle() | wxSTAY_ON_TOP);
    else
        mf->SetWindowStyle(mf->GetWindowStyle() & ~wxSTAY_ON_TOP);

    update_opts();
}

EVT_HANDLER(StatusBar, "Enable status bar")
{
    GetMenuOptionInt("StatusBar", &gopts.statusbar, 1);
    update_opts();
    MainFrame* mf = wxGetApp().frame;

    if (gopts.statusbar)
        mf->GetStatusBar()->Show();
    else
        mf->GetStatusBar()->Hide();

    mf->SendSizeEvent();
    panel->AdjustSize(false);
    mf->SendSizeEvent();
}

EVT_HANDLER(NoStatusMsg, "Disable on-screen status messages")
{
    GetMenuOptionInt("NoStatusMsg", &disableStatusMessages, 1);
    update_opts();
}

EVT_HANDLER(FrameSkipAuto, "Auto Skip frames.")
{
    GetMenuOptionInt("FrameSkipAuto", &autoFrameSkip, 1);
    update_opts();
}

EVT_HANDLER(Fullscreen, "Enter fullscreen mode at startup")
{
    GetMenuOptionConfig("Fullscreen", config::OptionID::kGeomFullScreen);
}

EVT_HANDLER(PauseWhenInactive, "Pause game when main window loses focus")
{
    GetMenuOptionInt("PauseWhenInactive", &pauseWhenInactive, 1);
    update_opts();
}

EVT_HANDLER(RTC, "Enable RTC (vba-over.ini override is rtcEnabled")
{
    GetMenuOptionInt("RTC", &rtcEnabled, 1);
    update_opts();
}

EVT_HANDLER(Transparent, "Draw on-screen messages transparently")
{
    GetMenuOptionInt("Transparent", &showSpeedTransparent, 1);
    update_opts();
}

EVT_HANDLER(SkipIntro, "Skip BIOS initialization")
{
    GetMenuOptionInt("SkipIntro", &skipBios, 1);
    update_opts();
}

EVT_HANDLER(BootRomEn, "Use the specified BIOS file for GBA")
{
    GetMenuOptionInt("BootRomEn", &useBiosFileGBA, 1);
    update_opts();
}

EVT_HANDLER(BootRomGB, "Use the specified BIOS file for GB")
{
    int val = 0;
    GetMenuOptionInt("BootRomGB", &val, 1);

    if (val == 1 && colorizerHack == 1) {
        wxLogError(_("Cannot use GB BIOS when Colorizer Hack is enabled."));
        val = 0;
        SetMenuOption("BootRomGB", 0);
    }

    useBiosFileGB = val;

    update_opts();
}

EVT_HANDLER(BootRomGBC, "Use the specified BIOS file for GBC")
{
    GetMenuOptionInt("BootRomGBC", &useBiosFileGBC, 1);
    update_opts();
}

EVT_HANDLER(VSync, "Wait for vertical sync")
{
    GetMenuOptionInt("VSync", &vsync, 1);
    update_opts();
    panel->ResetPanel();
}

void MainFrame::EnableNetworkMenu()
{
    cmd_enable &= ~CMDEN_LINK_ANY;

    if (gopts.gba_link_type != 0)
        cmd_enable |= CMDEN_LINK_ANY;

    if (gopts.link_proto)
        cmd_enable &= ~CMDEN_LINK_ANY;

    enable_menus();
}

void SetLinkTypeMenu(const char* type, int value)
{
    MainFrame* mf = wxGetApp().frame;
    mf->SetMenuOption("LinkType0Nothing", 0);
    mf->SetMenuOption("LinkType1Cable", 0);
    mf->SetMenuOption("LinkType2Wireless", 0);
    mf->SetMenuOption("LinkType3GameCube", 0);
    mf->SetMenuOption("LinkType4Gameboy", 0);
    mf->SetMenuOption(type, 1);
    gopts.gba_link_type = value;
    update_opts();
#ifndef NO_LINK
    CloseLink();
#endif
    mf->EnableNetworkMenu();
}

EVT_HANDLER_MASK(LanLink, "Start Network link", CMDEN_LINK_ANY)
{
#ifndef NO_LINK
    LinkMode mode = GetLinkMode();

    if (mode != LINK_DISCONNECTED) {
        // while we could deactivate the command when connected, it is more
        // user-friendly to display a message indidcating why
        wxLogError(_("LAN link is already active.  Disable link mode to disconnect."));
        return;
    }

    if (gopts.link_proto) {
        // see above comment
        wxLogError(_("Network is not supported in local mode."));
        return;
    }

    wxDialog* dlg = GetXRCDialog("NetLink");
    ShowModal(dlg);
    panel->SetFrameTitle();
#endif
}

EVT_HANDLER(LinkType0Nothing, "Link nothing")
{
    SetLinkTypeMenu("LinkType0Nothing", 0);
}

EVT_HANDLER(LinkType1Cable, "Link cable")
{
    SetLinkTypeMenu("LinkType1Cable", 1);
}

EVT_HANDLER(LinkType2Wireless, "Link wireless")
{
    SetLinkTypeMenu("LinkType2Wireless", 2);
}

EVT_HANDLER(LinkType3GameCube, "Link GameCube")
{
    SetLinkTypeMenu("LinkType3GameCube", 3);
}

EVT_HANDLER(LinkType4Gameboy, "Link Gameboy")
{
    SetLinkTypeMenu("LinkType4Gameboy", 4);
}

EVT_HANDLER(LinkAuto, "Enable link at boot")
{
    GetMenuOptionBool("LinkAuto", &gopts.link_auto);
    update_opts();
}

EVT_HANDLER(SpeedOn, "Enable faster network protocol by default")
{
    GetMenuOptionInt("SpeedOn", &linkHacks, 1);
    update_opts();
}

EVT_HANDLER(LinkProto, "Local host IPC")
{
    GetMenuOptionInt("LinkProto", &gopts.link_proto, 1);
    update_opts();
    enable_menus();
    EnableNetworkMenu();
}

EVT_HANDLER(LinkConfigure, "Link options...")
{
#ifndef NO_LINK
    wxDialog* dlg = GetXRCDialog("LinkConfig");

    if (ShowModal(dlg) != wxID_OK)
        return;

    SetLinkTimeout(linkTimeout);
    update_opts();
    EnableNetworkMenu();
#endif
}

// Dummy for disabling system key bindings
EVT_HANDLER_MASK(NOOP, "Do nothing", CMDEN_NEVER)
{
}

// The following have been moved to dialogs
// I will not implement as command unless there is great demand
// CheatsList
//EVT_HANDLER(CheatsLoad, "Load Cheats...")
//EVT_HANDLER(CheatsSave, "Save Cheats...")
//GeneralConfigure
//EVT_HANDLER(EmulatorRewindInterval, "EmulatorRewindInterval")
//EVT_HANDLER(EmulatorAutoApplyPatchFiles, "EmulatorAutoApplyPatchFiles")
//EVT_HANDLER(ThrottleNone, "ThrottleNone")
//EVT_HANDLER(Throttle025%, "Throttle025%")
//EVT_HANDLER(Throttle050%, "Throttle050%")
//EVT_HANDLER(Throttle100%, "Throttle100%")
//EVT_HANDLER(Throttle150%, "Throttle150%")
//EVT_HANDLER(Throttle200%, "Throttle200%")
//EVT_HANDLER(ThrottleOther, "ThrottleOther")
//GameBoyConfigure/GameBoyAdvanceConfigure
//EVT_HANDLER(FrameSkip0, "FrameSkip0")
//EVT_HANDLER(FrameSkip1, "FrameSkip1")
//EVT_HANDLER(FrameSkip2, "FrameSkip2")
//EVT_HANDLER(FrameSkip3, "FrameSkip3")
//EVT_HANDLER(FrameSkip4, "FrameSkip4")
//EVT_HANDLER(FrameSkip5, "FrameSkip5")
//EVT_HANDLER(FrameSkip6, "FrameSkip6")
//EVT_HANDLER(FrameSkip7, "FrameSkip7")
//EVT_HANDLER(FrameSkip8, "FrameSkip8")
//EVT_HANDLER(FrameSkip9, "FrameSkip9")
// GameBoyConfigure
//EVT_HANDLER(GameboyBorder, "GameboyBorder")
//EVT_HANDLER(GameboyBorderAutomatic, "GameboyBorderAutomatic")
//EVT_HANDLER(GameboyColors, "GameboyColors")
//GameBoyAdvanceConfigure
//EVT_HANDLER(EmulatorAGBPrint, "EmulatorAGBPrint")
//EVT_HANDLER(EmulatorSaveAuto, "EmulatorSaveAuto")
//EVT_HANDLER(EmulatorSaveEEPROM, "EmulatorSaveEEPROM")
//EVT_HANDLER(EmulatorSaveSRAM, "EmulatorSaveSRAM")
//EVT_HANDLER(EmulatorSaveFLASH, "EmulatorSaveFLASH")
//EVT_HANDLER(EmulatorSaveEEPROMSensor, "EmulatorSaveEEPROMSensor")
//EVT_HANDLER(EmulatorSaveFlash64K, "EmulatorSaveFlash64K")
//EVT_HANDLER(EmulatorSaveFlash128K, "EmulatorSaveFlash128K")
//EVT_HANDLER(EmulatorSaveDetectNow, "EmulatorSaveDetectNow")
//EVT_HANDLER(EmulatorRTC, "EmulatorRTC")
//DisplayConfigure
//EVT_HANDLER(EmulatorShowSpeedNone, "EmulatorShowSpeedNone")
//EVT_HANDLER(EmulatorShowSpeedPercentage, "EmulatorShowSpeedPercentage")
//EVT_HANDLER(EmulatorShowSpeedDetailed, "EmulatorShowSpeedDetailed")
//EVT_HANDLER(EmulatorShowSpeedTransparent, "EmulatorShowSpeedTransparent")
//EVT_HANDLER(VideoX1, "VideoX1")
//EVT_HANDLER(VideoX2, "VideoX2")
//EVT_HANDLER(VideoX3, "VideoX3")
//EVT_HANDLER(VideoX4, "VideoX4")
//EVT_HANDLER(VideoX5, "VideoX5")
//EVT_HANDLER(VideoX6, "VideoX6")
//EVT_HANDLER(Video320x240, "Video320x240")
//EVT_HANDLER(Video640x480, "Video640x480")
//EVT_HANDLER(Video800x600, "Video800x600")
//EVT_HANDLER(VideoFullscreen, "VideoFullscreen")
//EVT_HANDLER(VideoFullscreenMaxScale, "VideoFullscreenMaxScale")
//EVT_HANDLER(VideoRenderDDRAW, "VideoRenderDDRAW")
//EVT_HANDLER(VideoRenderD3D, "VideoRenderD3D")
//EVT_HANDLER(VideoRenderOGL, "VideoRenderOGL")
//EVT_HANDLER(VideoVsync, "VideoVsync")
//EVT_HANDLER(FilterNormal, "FilterNormal")
//EVT_HANDLER(FilterTVMode, "FilterTVMode")
//EVT_HANDLER(Filter2xSaI, "Filter2xSaI")
//EVT_HANDLER(FilterSuper2xSaI, "FilterSuper2xSaI")
//EVT_HANDLER(FilterSuperEagle, "FilterSuperEagle")
//EVT_HANDLER(FilterPixelate, "FilterPixelate")
//EVT_HANDLER(FilterMotionBlur, "FilterMotionBlur")
//EVT_HANDLER(FilterAdMameScale2x, "FilterAdMameScale2x")
//EVT_HANDLER(FilterSimple2x, "FilterSimple2x")
//EVT_HANDLER(FilterBilinear, "FilterBilinear")
//EVT_HANDLER(FilterBilinearPlus, "FilterBilinearPlus")
//EVT_HANDLER(FilterScanlines, "FilterScanlines")
//EVT_HANDLER(FilterHq2x, "FilterHq2x")
//EVT_HANDLER(FilterLq2x, "FilterLq2x")
//EVT_HANDLER(FilterIFBNone, "FilterIFBNone")
//EVT_HANDLER(FilterIFBMotionBlur, "FilterIFBMotionBlur")
//EVT_HANDLER(FilterIFBSmart, "FilterIFBSmart")
//EVT_HANDLER(FilterDisableMMX, "FilterDisableMMX")
//JoypadConfigure
//EVT_HANDLER(JoypadConfigure1, "JoypadConfigure1")
//EVT_HANDLER(JoypadConfigure2, "JoypadConfigure2")
//EVT_HANDLER(JoypadConfigure3, "JoypadConfigure3")
//EVT_HANDLER(JoypadConfigure4, "JoypadConfigure4")
//EVT_HANDLER(JoypadMotionConfigure, "JoypadMotionConfigure")

// The following functionality has been removed
// It should be done in OS, rather than in vbam
//EVT_HANDLER(EmulatorAssociate, "EmulatorAssociate")

// The following functionality has been removed
// It should be done at OS level (e.g. window manager)
//EVT_HANDLER(SystemMinimize, "SystemMinimize")

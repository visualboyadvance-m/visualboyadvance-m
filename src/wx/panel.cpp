#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef __WXGTK__
    #include <X11/Xlib.h>
    #define Status int
    #include <gdk/gdkx.h>
    #include <gtk/gtk.h>
    // For Wayland EGL.
    #ifdef HAVE_EGL
        #include <EGL/egl.h>
    #endif
#ifdef HAVE_XSS
    #include <X11/extensions/scrnsaver.h>
#endif
#endif

#include <wx/dcbuffer.h>
#include <wx/menu.h>
#include <SDL_joystick.h>

#include "../common/Patch.h"
#include "../common/version_cpp.h"
#include "../gb/gbPrinter.h"
#include "../gba/RTC.h"
#include "../gba/agbprint.h"
#include "../sdl/text.h"
#include "background-input.h"
#include "config/game-control.h"
#include "config/option.h"
#include "config/user-input.h"
#include "drawing.h"
#include "filters.h"
#include "wayland.h"
#include "widgets/render-plugin.h"
#include "wx/joyedit.h"
#include "wxutil.h"
#include "wxvbam.h"

#ifdef __WXMSW__
#include <windows.h>
#endif

namespace {

double GetFilterScale() {
    switch (config::OptDispFilter()->GetFilter()) {
        case config::Filter::kNone:
            return 1.0;
        case config::Filter::k2xsai:
        case config::Filter::kSuper2xsai:
        case config::Filter::kSupereagle:
        case config::Filter::kPixelate:
        case config::Filter::kAdvmame:
        case config::Filter::kBilinear:
        case config::Filter::kBilinearplus:
        case config::Filter::kScanlines:
        case config::Filter::kTvmode:
        case config::Filter::kSimple2x:
        case config::Filter::kLQ2x:
        case config::Filter::kHQ2x:
        case config::Filter::kXbrz2x:
            return 2.0;
        case config::Filter::kXbrz3x:
        case config::Filter::kSimple3x:
        case config::Filter::kHQ3x:
            return 3.0;
        case config::Filter::kSimple4x:
        case config::Filter::kHQ4x:
        case config::Filter::kXbrz4x:
            return 4.0;
        case config::Filter::kXbrz5x:
            return 5.0;
        case config::Filter::kXbrz6x:
            return 6.0;
        case config::Filter::kPlugin:
        case config::Filter::kLast:
            assert(false);
            return 1.0;
    }
    assert(false);
    return 1.0;
}

#define out_16 (systemColorDepth == 16)

}  // namespace

int emulating;

IMPLEMENT_DYNAMIC_CLASS(GameArea, wxPanel)

GameArea::GameArea()
    : wxPanel(),
      panel(NULL),
      emusys(NULL),
      was_paused(false),
      rewind_time(0),
      do_rewind(false),
      rewind_mem(0),
      num_rewind_states(0),
      loaded(IMAGE_UNKNOWN),
      basic_width(GBAWidth),
      basic_height(GBAHeight),
      fullscreen(false),
      paused(false),
      pointer_blanked(false),
      mouse_active_time(0),
      render_observer_(
          {config::OptionID::kDispRenderMethod, config::OptionID::kDispFilter,
           config::OptionID::kDispIFB},
          std::bind(&GameArea::ResetPanel, this)),
      scale_observer_(config::OptionID::kDispScale,
                      std::bind(&GameArea::AdjustSize, this, true)) {
    SetSizer(new wxBoxSizer(wxVERTICAL));
    // all renderers prefer 32-bit
    // well, "simple" prefers 24-bit, but that's not available for filters
    systemColorDepth = 32;
    hq2x_init(32);
    Init_2xSaI(32);
}

void GameArea::LoadGame(const wxString& name)
{
    rom_scene_rls = wxT("-");
    rom_scene_rls_name = wxT("-");
    rom_name = wxT("");
    // fex just crashes if file does not exist and it's compressed,
    // so check first
    wxFileName fnfn(name);
    bool badfile = !fnfn.IsFileReadable();

    // if path was relative, look for it before giving up
    if (badfile && !fnfn.IsAbsolute()) {
        wxString rp = fnfn.GetPath();

        // can't really decide which dir to use, so try GBA first, then GB
        if (!wxGetApp().GetAbsolutePath(gopts.gba_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gopts.gba_rom_dir) + wxT('/') + rp);
            badfile = !fnfn.IsFileReadable();
        }

        if (badfile && !wxGetApp().GetAbsolutePath(gopts.gb_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gopts.gb_rom_dir) + wxT('/') + rp);
            badfile = !fnfn.IsFileReadable();
        }

        if (badfile && !wxGetApp().GetAbsolutePath(gopts.gbc_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gopts.gbc_rom_dir) + wxT('/') + rp);
            badfile = !fnfn.IsFileReadable();
        }
    }

    // auto-conversion of wxCharBuffer to const char * seems broken
    // so save underlying wxCharBuffer (or create one of none is used)
    wxCharBuffer fnb(UTF8(fnfn.GetFullPath()));
    const char* fn = fnb.data();
    IMAGE_TYPE t = badfile ? IMAGE_UNKNOWN : utilFindType(fn);

    if (t == IMAGE_UNKNOWN) {
        wxString s;
        s.Printf(_("%s is not a valid ROM file"), name.mb_str());
        wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        return;
    }

    {
        wxFileConfig* cfg = wxGetApp().cfg;

        if (!gopts.recent_freeze) {
            gopts.recent->AddFileToHistory(name);
            wxGetApp().frame->SetRecentAccels();
            cfg->SetPath(wxT("/Recent"));
            gopts.recent->Save(*cfg);
            cfg->SetPath(wxT("/"));
            cfg->Flush();
        }
    }

    UnloadGame();
    // strip extension from actual game file name
    // FIXME: save actual file name of archive so patches & cheats can
    // be loaded from archive
    // FIXME: if archive name does not match game name, prepend archive
    // name to uniquify game name
    loaded_game = fnfn;
    loaded_game.ClearExt();
    loaded_game.MakeAbsolute();
    // load patch, if enabled
    // note that it is difficult to load from archive due to
    // ../common/Patch.cpp depending on opening the file itself and doing
    // lots of seeking & such.  The only way would be to copy the patch
    // out to a temporary file and load it (and can't just use
    // AssignTempFileName because it needs correct extension)
    // too much trouble for now, though
    bool loadpatch = autoPatch;
    wxFileName pfn = loaded_game;
    int ovSaveType = 0;

    if (loadpatch) {
        // SetExt may strip something off by accident, so append to text instead
        // pfn.SetExt(wxT("ips")
        pfn.SetFullName(pfn.GetFullName() + wxT(".ips"));

        if (!pfn.IsFileReadable()) {
            pfn.SetExt(wxT("ups"));

			if (!pfn.IsFileReadable()) {
				pfn.SetExt(wxT("bps"));

				if (!pfn.IsFileReadable()) {
					pfn.SetExt(wxT("ppf"));
					loadpatch = pfn.IsFileReadable();
				}
			}
		}
    }

    if (t == IMAGE_GB) {
        if (!gbLoadRom(fn)) {
            wxString s;
            s.Printf(_("Unable to load Game Boy ROM %s"), name.mb_str());
            wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
            dlg.ShowModal();
            return;
        }

        rom_size = gbRomSize;

        if (loadpatch) {
            int size = rom_size;
            applyPatch(UTF8(pfn.GetFullPath()), &gbRom, &size);

            if (size != (int)rom_size)
                gbUpdateSizes();

            rom_size = size;
        }

        // start sound; this must happen before CPU stuff
        gb_effects_config.enabled = gopts.gb_effects_config_enabled;
        gb_effects_config.surround = gopts.gb_effects_config_surround;
        gb_effects_config.echo = (float)gopts.gb_echo / 100.0;
        gb_effects_config.stereo = (float)gopts.gb_stereo / 100.0;
        gbSoundSetDeclicking(gopts.gb_declick);
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        soundSetEnable(gopts.sound_en);
        gbSoundSetSampleRate(!gopts.sound_qual ? 48000 : 44100 / (1 << (gopts.sound_qual - 1)));
        soundSetVolume((float)gopts.sound_vol / 100.0);
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(throttle);
        gbGetHardwareType();


        // Disable bios loading when using colorizer hack.
        if (useBiosFileGB && colorizerHack) {
            wxLogError(_("Cannot use GB BIOS file when Colorizer Hack is enabled, disabling GB BIOS file."));
            useBiosFileGB = 0;
            update_opts();
        }

        // Set up the core for the colorizer hack.
        setColorizerHack(colorizerHack);

        bool use_bios = gbCgbMode ? useBiosFileGBC : useBiosFileGB;

        wxCharBuffer fnb(UTF8((gbCgbMode ? gopts.gbc_bios : gopts.gb_bios)));
        const char* fn = fnb.data();

        gbCPUInit(fn, use_bios);

        if (use_bios && !useBios) {
            wxLogError(_("Could not load BIOS %s"), (gbCgbMode ? gopts.gbc_bios : gopts.gb_bios).mb_str());
            // could clear use flag & file name now, but better to force
            // user to do it
        }

        gbReset();

        if (gbBorderOn) {
            basic_width = gbBorderLineSkip = SGBWidth;
            basic_height = SGBHeight;
            gbBorderColumnSkip = (SGBWidth - GBWidth) / 2;
            gbBorderRowSkip = (SGBHeight - GBHeight) / 2;
        } else {
            basic_width = gbBorderLineSkip = GBWidth;
            basic_height = GBHeight;
            gbBorderColumnSkip = gbBorderRowSkip = 0;
        }

        emusys = &GBSystem;
    } else /* if(t == IMAGE_GBA) */
    {
        if (!(rom_size = CPULoadRom(fn))) {
            wxString s;
            s.Printf(_("Unable to load Game Boy Advance ROM %s"), name.mb_str());
            wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
            dlg.ShowModal();
            return;
        }

        rom_crc32 = crc32(0L, rom, rom_size);

        if (loadpatch) {
            // don't use real rom size or it might try to resize rom[]
            // instead, use known size of rom[]
            int size = 0x2000000 < rom_size ? 0x2000000 : rom_size;
            applyPatch(UTF8(pfn.GetFullPath()), &rom, &size);
            // that means we no longer really know rom_size either <sigh>

            gbaUpdateRomSize(size);
        }

        wxFileConfig* cfg = wxGetApp().overrides;
        wxString id = wxString((const char*)&rom[0xac], wxConvLibc, 4);

        if (cfg->HasGroup(id)) {
            cfg->SetPath(id);
            bool enable_rtc = cfg->Read(wxT("rtcEnabled"), rtcEnabled);

            rtcEnable(enable_rtc);

            int fsz = cfg->Read(wxT("flashSize"), (long)0);

            if (fsz != 0x10000 && fsz != 0x20000)
                fsz = 0x10000 << optFlashSize;

            flashSetSize(fsz);
            ovSaveType = cfg->Read(wxT("saveType"), cpuSaveType);

            if (ovSaveType < 0 || ovSaveType > 5)
                ovSaveType = 0;

            if (ovSaveType == 0)
                utilGBAFindSave(rom_size);
            else
                saveType = ovSaveType;

            mirroringEnable = cfg->Read(wxT("mirroringEnabled"), (long)1);
            cfg->SetPath(wxT("/"));
        } else {
            rtcEnable(rtcEnabled);
            flashSetSize(0x10000 << optFlashSize);

            if (cpuSaveType < 0 || cpuSaveType > 5)
                cpuSaveType = 0;

            if (cpuSaveType == 0)
                utilGBAFindSave(rom_size);
            else
                saveType = cpuSaveType;

            mirroringEnable = false;
        }

        doMirroring(mirroringEnable);
        // start sound; this must happen before CPU stuff
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        soundSetEnable(gopts.sound_en);
        soundSetSampleRate(!gopts.sound_qual ? 48000 : 44100 / (1 << (gopts.sound_qual - 1)));
        soundSetVolume((float)gopts.sound_vol / 100.0);
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(throttle);
        soundFiltering = (float)gopts.gba_sound_filter / 100.0f;

        rtcEnableRumble(true);

        CPUInit(UTF8(gopts.gba_bios), useBiosFileGBA);

        if (useBiosFileGBA && !useBios) {
            wxLogError(_("Could not load BIOS %s"), gopts.gba_bios.mb_str());
            // could clear use flag & file name now, but better to force
            // user to do it
        }

        CPUReset();
        basic_width = GBAWidth;
        basic_height = GBAHeight;
        emusys = &GBASystem;
    }

    if (config::OptGeomFullScreen()->GetInt()) {
        GameArea::ShowFullScreen(true);
    }

    loaded = t;
    SetFrameTitle();
    SetFocus();
    // Use custom geometry
    AdjustSize(false);
    emulating = true;
    was_paused = true;
    MainFrame* mf = wxGetApp().frame;
    mf->StopJoyPollTimer();
    mf->SetJoystick();
    mf->cmd_enable &= ~(CMDEN_GB | CMDEN_GBA);
    mf->cmd_enable |= ONLOAD_CMDEN;
    mf->cmd_enable |= loaded == IMAGE_GB ? CMDEN_GB : (CMDEN_GBA | CMDEN_NGDB_GBA);
    mf->enable_menus();
#if (defined __WIN32__ || defined _WIN32)
#ifndef NO_LINK
    gbSerialFunction = gbStartLink;
#else
    gbSerialFunction = NULL;
#endif
#endif

    // probably only need to do this for GB carts
    if (winGbPrinterEnabled)
        gbSerialFunction = gbPrinterSend;

    // probably only need to do this for GBA carts
    agbPrintEnable(agbPrint);
    // set frame skip based on ROM type
    systemFrameSkip = frameSkip;

    if (systemFrameSkip < 0)
        systemFrameSkip = 0;

    // load battery and/or saved state
    recompute_dirs();
    mf->update_state_ts(true);
    bool did_autoload = gopts.autoload_state ? LoadState() : false;

    if (!did_autoload || skipSaveGameBattery) {
        wxString bname = loaded_game.GetFullName();
#ifndef NO_LINK
        // MakeInstanceFilename doesn't do wxString, so just add slave ID here
        int playerId = GetLinkPlayerId();

        if (playerId >= 0) {
            bname.append(wxT('-'));
            bname.append(wxChar(wxT('1') + playerId));
        }

#endif
        bname.append(wxT(".sav"));
        wxFileName bat(batdir, bname);

        if (emusys->emuReadBattery(UTF8(bat.GetFullPath()))) {
            wxString msg;
            msg.Printf(_("Loaded battery %s"), bat.GetFullPath().wc_str());
            systemScreenMessage(msg);

            if (cpuSaveType == 0 && ovSaveType == 0 && t == IMAGE_GBA) {
                switch (bat.GetSize().GetValue()) {
                case 0x200:
                case 0x2000:
                    saveType = GBA_SAVE_EEPROM;
                    break;

                case 0x8000:
                    saveType = GBA_SAVE_SRAM;
                    break;

                case 0x10000:
                    if (saveType == GBA_SAVE_EEPROM || saveType == GBA_SAVE_SRAM)
                        break;
                    break;

                case 0x20000:
                    saveType = GBA_SAVE_FLASH;
                    flashSetSize(bat.GetSize().GetValue());
                    break;

                default:
                    break;
                }

                SetSaveType(saveType);
            }
        }

        // forget old save writes
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
    }

    // do an immediate rewind save
    // even if loaded from state file: not smart enough yet to just
    // do a reset or load from state file when # rewinds == 0
    do_rewind = gopts.rewind_interval > 0;
    // FIXME: backup battery file (useful if game name conflict)
    cheats_dirty = (did_autoload && !skipSaveGameCheats) || (loaded == IMAGE_GB ? gbCheatNumber > 0 : cheatsNumber > 0);

    if (gopts.autoload_cheats && (!did_autoload || skipSaveGameCheats)) {
        wxFileName cfn = loaded_game;
        // SetExt may strip something off by accident, so append to text instead
        cfn.SetFullName(cfn.GetFullName() + wxT(".clt"));

        if (cfn.IsFileReadable()) {
            bool cld;

            if (loaded == IMAGE_GB)
                cld = gbCheatsLoadCheatList(UTF8(cfn.GetFullPath()));
            else
                cld = cheatsLoadCheatList(UTF8(cfn.GetFullPath()));

            if (cld) {
                systemScreenMessage(_("Loaded cheats"));
                cheats_dirty = false;
            }
        }
    }

#ifndef NO_LINK

    if (gopts.link_auto) {
        linkMode = mf->GetConfiguredLinkMode();
        BootLink(linkMode, UTF8(gopts.link_host), linkTimeout, linkHacks, linkNumPlayers);
    }

#endif
}

void GameArea::SetFrameTitle()
{
    wxString tit = wxT("");

    if (loaded != IMAGE_UNKNOWN) {
        tit.append(loaded_game.GetFullName());
        tit.append(wxT(" - "));
    }

    tit.append(wxT("VisualBoyAdvance-M "));
    tit.append(vbam_version);

#ifndef NO_LINK
    int playerId = GetLinkPlayerId();

    if (playerId >= 0) {
        tit.append(_(" player "));
        tit.append(wxChar(wxT('1') + playerId));
    }

#endif
    wxGetApp().frame->SetTitle(tit);
}

void GameArea::recompute_dirs()
{
    batdir = gopts.battery_dir;

    if (!batdir.size()) {
        batdir = loaded_game.GetPathWithSep();
    } else {
        batdir = wxGetApp().GetAbsolutePath(gopts.battery_dir);
    }

    if (!wxIsWritable(batdir)) {
        batdir = wxGetApp().GetDataDir();
    }

    statedir = gopts.state_dir;

    if (!statedir.size()) {
        statedir = loaded_game.GetPathWithSep();
    } else {
        statedir = wxGetApp().GetAbsolutePath(gopts.state_dir);
    }

    if (!wxIsWritable(statedir)) {
        statedir = wxGetApp().GetDataDir();
    }
}

void GameArea::UnloadGame(bool destruct)
{
    if (!emulating)
        return;

    // last opportunity to autosave cheats
    if (gopts.autoload_cheats && cheats_dirty) {
        wxFileName cfn = loaded_game;
        // SetExt may strip something off by accident, so append to text instead
        cfn.SetFullName(cfn.GetFullName() + wxT(".clt"));

        if (loaded == IMAGE_GB) {
            if (!gbCheatNumber)
                wxRemoveFile(cfn.GetFullPath());
            else
                gbCheatsSaveCheatList(UTF8(cfn.GetFullPath()));
        } else {
            if (!cheatsNumber)
                wxRemoveFile(cfn.GetFullPath());
            else
                cheatsSaveCheatList(UTF8(cfn.GetFullPath()));
        }
    }

    // if timer was counting down for save, go ahead and save
    // this might not be safe, though..
    if (systemSaveUpdateCounter > SYSTEM_SAVE_NOT_UPDATED) {
        SaveBattery();
    }

    MainFrame* mf = wxGetApp().frame;
#ifndef NO_FFMPEG
    snd_rec.Stop();
    vid_rec.Stop();
#endif
    systemStopGameRecording();
    systemStopGamePlayback();

#ifndef NO_DEBUGGER
    debugger = false;
    remoteCleanUp();
    mf->cmd_enable |= CMDEN_NGDB_ANY;
#endif

    if (loaded == IMAGE_GB) {
        gbCleanUp();
        gbCheatRemoveAll();
    } else if (loaded == IMAGE_GBA) {
        CPUCleanUp();
        cheatsDeleteAll(false);
    }

    emulating = false;
    loaded = IMAGE_UNKNOWN;
    emusys = NULL;
    soundShutdown();

    disableKeyboardBackgroundInput();

    if (destruct)
        return;

    // in destructor, panel should be auto-deleted by wx since all panels
    // are derived from a window attached as child to GameArea
    ResetPanel();

    // close any game-related viewer windows
    // in destructor, viewer windows are in process of being deleted anyway
    while (!mf->popups.empty())
        mf->popups.front()->Close(true);

    // remaining items are GUI updates that should not be needed in destructor
    SetFrameTitle();
    mf->cmd_enable &= UNLOAD_CMDEN_KEEP;
    mf->update_state_ts(true);
    mf->enable_menus();
    mf->StartJoyPollTimer();
    mf->SetJoystick();
    mf->ResetCheatSearch();

    if (rewind_mem)
        num_rewind_states = 0;
}

bool GameArea::LoadState()
{
    int slot = wxGetApp().frame->newest_state_slot();

    if (slot < 1)
        return false;

    return LoadState(slot);
}

bool GameArea::LoadState(int slot)
{
    wxString fname;
    fname.Printf(SAVESLOT_FMT, game_name().wc_str(), slot);
    return LoadState(wxFileName(statedir, fname));
}

bool GameArea::LoadState(const wxFileName& fname)
{
    // FIXME: first save to backup state if not backup state
    bool ret = emusys->emuReadState(UTF8(fname.GetFullPath()));

    if (ret && num_rewind_states) {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~CMDEN_REWIND;
        mf->enable_menus();
        num_rewind_states = 0;
        // do an immediate rewind save
        // even if loaded from state file: not smart enough yet to just
        // do a reset or load from state file when # rewinds == 0
        do_rewind = true;
        rewind_time = gopts.rewind_interval * 6;
    }

    if (ret) {
        // forget old save writes
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
        // no point in blending after abrupt change
        InterframeCleanup();
        // frame rate calc should probably reset as well
        was_paused = true;
        // save state had a screen frame, so draw it
        systemDrawScreen();
    }

    wxString msg;
    msg.Printf(ret ? _("Loaded state %s") : _("Error loading state %s"),
        fname.GetFullPath().wc_str());
    systemScreenMessage(msg);
    return ret;
}

bool GameArea::SaveState()
{
    return SaveState(wxGetApp().frame->oldest_state_slot());
}

bool GameArea::SaveState(int slot)
{
    wxString fname;
    fname.Printf(SAVESLOT_FMT, game_name().wc_str(), slot);
    return SaveState(wxFileName(statedir, fname));
}

bool GameArea::SaveState(const wxFileName& fname)
{
    // FIXME: first copy to backup state if not backup state
    bool ret = emusys->emuWriteState(UTF8(fname.GetFullPath()));
    wxGetApp().frame->update_state_ts(true);
    wxString msg;
    msg.Printf(ret ? _("Saved state %s") : _("Error saving state %s"),
        fname.GetFullPath().wc_str());
    systemScreenMessage(msg);
    return ret;
}

void GameArea::SaveBattery()
{
    // MakeInstanceFilename doesn't do wxString, so just add slave ID here
    wxString bname = game_name();
#ifndef NO_LINK
    int playerId = GetLinkPlayerId();

    if (playerId >= 0) {
        bname.append(wxT('-'));
        bname.append(wxChar(wxT('1') + playerId));
    }

#endif
    bname.append(wxT(".sav"));
    wxFileName bat(batdir, bname);
    bat.Mkdir(0777, wxPATH_MKDIR_FULL);
    wxString fn = bat.GetFullPath();

    // FIXME: add option to support ring of backups
    // of course some games just write battery way too often for such
    // a thing to be useful
    if (!emusys->emuWriteBattery(UTF8(fn)))
        wxLogError(_("Error writing battery %s"), fn.mb_str());

    systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
}

void GameArea::AddBorder()
{
    if (basic_width != GBWidth)
        return;

    basic_width = SGBWidth;
    basic_height = SGBHeight;
    gbBorderLineSkip = SGBWidth;
    gbBorderColumnSkip = (SGBWidth - GBWidth) / 2;
    gbBorderRowSkip = (SGBHeight - GBHeight) / 2;
    AdjustSize(false);
    wxGetApp().frame->Fit();
    GetSizer()->Detach(panel->GetWindow());
    ResetPanel();
}

void GameArea::DelBorder()
{
    if (basic_width != SGBWidth)
        return;

    basic_width = GBWidth;
    basic_height = GBHeight;
    gbBorderLineSkip = GBWidth;
    gbBorderColumnSkip = gbBorderRowSkip = 0;
    AdjustSize(false);
    wxGetApp().frame->Fit();
    GetSizer()->Detach(panel->GetWindow());
    ResetPanel();
}

void GameArea::AdjustMinSize()
{
    wxWindow* frame           = wxGetApp().frame;
    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);
    double display_scale = config::OptDispScale()->GetDouble();

    // note: could safely set min size to 1x or less regardless of video_scale
    // but setting it to scaled size makes resizing to default easier
    wxSize sz((std::ceil(basic_width * display_scale) * dpi_scale_factor),
              (std::ceil(basic_height * display_scale) * dpi_scale_factor));
    SetMinSize(sz);
#if wxCHECK_VERSION(2, 8, 8)
    sz = frame->ClientToWindowSize(sz);
#else
    sz += frame->GetSize() - frame->GetClientSize();
#endif
    frame->SetMinSize(sz);
}

void GameArea::LowerMinSize()
{
    wxWindow* frame           = wxGetApp().frame;
    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);

    wxSize sz(std::ceil(basic_width * dpi_scale_factor),
              std::ceil(basic_height * dpi_scale_factor));

    SetMinSize(sz);
    // do not take decorations into account
    frame->SetMinSize(sz);
}

void GameArea::AdjustSize(bool force)
{
    AdjustMinSize();

    if (fullscreen)
        return;

    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);
    double display_scale = config::OptDispScale()->GetDouble();

    const wxSize newsz(
        (std::ceil(basic_width * display_scale) * dpi_scale_factor),
        (std::ceil(basic_height * display_scale) * dpi_scale_factor));

    if (!force) {
        wxSize sz = GetClientSize();

        if (sz.GetWidth() >= newsz.GetWidth() && sz.GetHeight() >= newsz.GetHeight())
            return;
    }

    SetSize(newsz);
    GetParent()->SetClientSize(newsz);
    wxGetApp().frame->Fit();
}

void GameArea::ResetPanel() {
    if (panel) {
        panel->Destroy();
        panel = nullptr;
    }
}

void GameArea::ShowFullScreen(bool full)
{
    if (full == fullscreen) {
        // in case the tlw somehow lost its mind, force it to proper mode
        if (wxGetApp().frame->IsFullScreen() != fullscreen)
            wxGetApp().frame->ShowFullScreen(full);

        return;
    }

// on Mac maximize is native fullscreen, so ignore fullscreen requests
#ifdef __WXMAC__
    if (full && main_frame->IsMaximized()) return;
#endif

    fullscreen = full;

    // just in case screen mode is going to change, go ahead and preemptively
    // delete panel to be recreated immediately after resize
    ResetPanel();

    // Windows does not restore old window size/pos
    // at least under Wine
    // so store them before entering fullscreen
    static bool cursz_valid = false;
    static wxSize cursz;
    static wxPoint curpos;
    MainFrame* tlw = wxGetApp().frame;
    int dno = wxDisplay::GetFromWindow(tlw);

    if (!full) {
        if (gopts.fs_mode.w && gopts.fs_mode.h) {
            wxDisplay d(dno);
            d.ChangeMode(wxDefaultVideoMode);
        }

        tlw->ShowFullScreen(false);

        if (!cursz_valid) {
            curpos = wxDefaultPosition;
            cursz = tlw->GetMinSize();
        }

        tlw->SetSize(cursz);
        tlw->SetPosition(curpos);
        AdjustMinSize();
    } else {
        // close all non-modal dialogs
        while (!tlw->popups.empty())
            tlw->popups.front()->Close();

        // Some kbd accels can send a menu open event without a close event,
        // this happens on Mac in HiDPI mode for the fullscreen toggle accel.
        main_frame->SetMenusOpened(false);

        // mouse stays blank whenever full-screen
        HidePointer();
        cursz_valid = true;
        cursz = tlw->GetSize();
        curpos = tlw->GetPosition();
        LowerMinSize();

        if (gopts.fs_mode.w && gopts.fs_mode.h) {
            // grglflargm
            // stupid wx does not do fullscreen properly when video mode is
            // changed under X11.   Since it does not use xrandr to switch, it
            // just changes the video mode without resizing the desktop.  It
            // still uses the original desktop size for fullscreen, though.
            // It also seems to stick the top-left corner wherever it pleases,
            // so I can't just resize the panel and expect it to show up.
            // ^(*&^^&!!!!  maybe just disable this code altogether on UNIX
            // most 3d cards are fine with full screen size, anyway.
            wxDisplay d(dno);

            if (!d.ChangeMode(gopts.fs_mode)) {
                wxLogInfo(_("Fullscreen mode %dx%d-%d@%d not supported; looking for another"),
                    gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                // specifying a mode may not work with bpp/rate of 0
                // in particular, unix does Matches() in wrong direction
                wxArrayVideoModes vm = d.GetModes();
                int best_mode = -1;
                size_t i;

                for (i = 0; i < vm.size(); i++) {
                    if (vm[i].w != gopts.fs_mode.w || vm[i].h != gopts.fs_mode.h)
                        continue;

                    int bpp = vm[i].bpp;

                    if (gopts.fs_mode.bpp && bpp == gopts.fs_mode.bpp)
                        break;

                    if (!gopts.fs_mode.bpp && bpp == 32) {
                        gopts.fs_mode.bpp = 32;
                        break;
                    }

                    int bm_bpp = best_mode < 0 ? 0 : vm[i].bpp;

                    if (bpp == 32 && bm_bpp != 32)
                        best_mode = i;
                    else if (bpp == 24 && bm_bpp < 24)
                        best_mode = i;
                    else if (bpp == 16 && bm_bpp < 24 && bm_bpp != 16)
                        best_mode = i;
                    else if (!best_mode)
                        best_mode = i;
                }

                if (i == vm.size() && best_mode >= 0)
                    i = best_mode;

                if (i == vm.size()) {
                    wxLogWarning(_("Fullscreen mode %dx%d-%d@%d not supported"),
                        gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                    gopts.fs_mode = wxVideoMode();

                    for (i = 0; i < vm.size(); i++)
                        wxLogInfo(_("Valid mode: %dx%d-%d@%d"), vm[i].w,
                            vm[i].h, vm[i].bpp,
                            vm[i].refresh);
                } else {
                    gopts.fs_mode.bpp = vm[i].bpp;
                    gopts.fs_mode.refresh = vm[i].refresh;
                }

                wxLogInfo(_("Chose mode %dx%d-%d@%d"),
                    gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);

                if (!d.ChangeMode(gopts.fs_mode)) {
                    wxLogWarning(_("Failed to change mode to %dx%d-%d@%d"),
                        gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                    gopts.fs_mode.w = 0;
                }
            }
        }

        tlw->ShowFullScreen(true);
    }
}

GameArea::~GameArea()
{
    UnloadGame(true);

    if (rewind_mem)
        free(rewind_mem);

    if (gopts.fs_mode.w && gopts.fs_mode.h && fullscreen) {
        MainFrame* tlw = wxGetApp().frame;
        int dno = wxDisplay::GetFromWindow(tlw);
        wxDisplay d(dno);
        d.ChangeMode(wxDefaultVideoMode);
    }
}

void GameArea::OnKillFocus(wxFocusEvent& ev)
{
    config::GameControlState::Instance().Reset();
    ev.Skip();
}

void GameArea::Pause()
{
    if (paused)
        return;

    // don't pause when linked
#ifndef NO_LINK
    if (GetLinkMode() != LINK_DISCONNECTED)
        return;
#endif

    paused = was_paused = true;

    // when the game is paused like this, we should not allow any
    // input to remain pressed, because they could be released
    // outside of the game zone and we would not know about it.
    config::GameControlState::Instance().Reset();

    if (loaded != IMAGE_UNKNOWN)
        soundPause();

    wxGetApp().frame->StartJoyPollTimer();
}

void GameArea::Resume()
{
    if (!paused)
        return;

    paused = false;
    SetExtraStyle(GetExtraStyle() | wxWS_EX_PROCESS_IDLE);

    if (loaded != IMAGE_UNKNOWN)
        soundResume();

    wxGetApp().frame->StopJoyPollTimer();

    SetFocus();
}

void GameArea::OnIdle(wxIdleEvent& event)
{
    wxString pl = wxGetApp().pending_load;
    MainFrame* mf = wxGetApp().frame;

    if (pl.size()) {
        // sometimes this gets into a loop if LoadGame() called before
        // clearing pending_load.  weird.
        wxGetApp().pending_load = wxEmptyString;
        LoadGame(pl);

#ifndef NO_DEBUGGER
        if (gdbBreakOnLoad)
            mf->GDBBreak();

        if (debugger && loaded != IMAGE_GBA) {
            wxLogError(_("Not a valid GBA cartridge"));
            UnloadGame();
        }
#endif
    }

    // stupid wx doesn't resize to screen size
    // forcing it this way just puts it in an infinite loop, though
    // with wx trying to resize/reposition to what it thinks is full screen
    // every time it detects a manual resize like this
    if (gopts.fs_mode.w && gopts.fs_mode.h && fullscreen) {
        wxSize sz = GetSize();

        if (sz.GetWidth() != gopts.fs_mode.w || sz.GetHeight() != gopts.fs_mode.h) {
            wxGetApp().frame->Move(0, 84);
            SetSize(gopts.fs_mode.w, gopts.fs_mode.h);
        }
    }

    if (!emusys)
        return;

    if (!panel) {
        switch (config::OptDispRenderMethod()->GetRenderMethod()) {
            case config::RenderMethod::kSimple:
                panel = new BasicDrawingPanel(this, basic_width, basic_height);
                break;
#ifdef __WXMAC__
            case config::RenderMethod::kQuartz2d:
                panel =
                    new Quartz2DDrawingPanel(this, basic_width, basic_height);
                break;
#endif
#ifndef NO_OGL
            case config::RenderMethod::kOpenGL:
                panel = new GLDrawingPanel(this, basic_width, basic_height);
                break;
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
            case config::RenderMethod::kDirect3d:
                panel = new DXDrawingPanel(this, basic_width, basic_height);
                break;
#endif
            case config::RenderMethod::kLast:
                assert(false);
                return;
        }

        wxWindow* w = panel->GetWindow();

        // set up event handlers
        w->Connect(wxEVT_KEY_DOWN,         wxKeyEventHandler(GameArea::OnKeyDown),         NULL, this);
        w->Connect(wxEVT_KEY_UP,           wxKeyEventHandler(GameArea::OnKeyUp),           NULL, this);
        w->Connect(wxEVT_PAINT,            wxPaintEventHandler(GameArea::PaintEv),         NULL, this);
        w->Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(GameArea::EraseBackground), NULL, this);

        // set userdata so we know it's the panel and not the frame being resized
        // the userdata is freed on disconnect/destruction
        this->Connect(wxEVT_SIZE,          wxSizeEventHandler(GameArea::OnSize),           NULL, this);

        // We need to check if the buttons stayed pressed when focus the panel.
        w->Connect(wxEVT_KILL_FOCUS,       wxFocusEventHandler(GameArea::OnKillFocus),     NULL, this);

        // Update mouse last-used timers on mouse events etc..
        w->Connect(wxEVT_MOTION,           wxMouseEventHandler(GameArea::MouseEvent),      NULL, this);
        w->Connect(wxEVT_LEFT_DOWN,        wxMouseEventHandler(GameArea::MouseEvent),      NULL, this);
        w->Connect(wxEVT_RIGHT_DOWN,       wxMouseEventHandler(GameArea::MouseEvent),      NULL, this);
        w->Connect(wxEVT_MIDDLE_DOWN,      wxMouseEventHandler(GameArea::MouseEvent),      NULL, this);
        w->Connect(wxEVT_MOUSEWHEEL,       wxMouseEventHandler(GameArea::MouseEvent),      NULL, this);

        w->SetBackgroundStyle(wxBG_STYLE_CUSTOM);
        w->SetSize(wxSize(basic_width, basic_height));

        // Allow input while on background
        if (allowKeyboardBackgroundInput)
            enableKeyboardBackgroundInput(w);

        if (maxScale)
            w->SetMaxSize(wxSize(basic_width * maxScale,
                basic_height * maxScale));

        // if user changed Display/Scale config, this needs to run
        AdjustMinSize();
        AdjustSize(false);

        unsigned frame_priority = gopts.retain_aspect ? 0 : 1;

        GetSizer()->Clear();

        // add spacers on top and bottom to center panel vertically
        // but not on 2.8 which does not handle this correctly
        if (gopts.retain_aspect)
#if wxCHECK_VERSION(2, 9, 0)
            GetSizer()->Add(0, 0, wxEXPAND);
#else
            frame_priority = 1;
#endif

        // this triggers an assertion dialog in <= 3.1.2 in debug mode
        GetSizer()->Add(w, frame_priority, gopts.retain_aspect ? (wxSHAPED | wxALIGN_CENTER | wxEXPAND) : wxEXPAND);

#if wxCHECK_VERSION(2, 9, 0)
        if (gopts.retain_aspect)
            GetSizer()->Add(0, 0, wxEXPAND);
#endif

        Layout();

#if wxCHECK_VERSION(2, 9, 0)
        SendSizeEvent();
#endif

        if (pointer_blanked)
            w->SetCursor(wxCursor(wxCURSOR_BLANK));

        // set focus to panel
        w->SetFocus();

        // generate system color maps (after output module init)
        if (loaded == IMAGE_GBA) utilUpdateSystemColorMaps(gbaLcdFilter);
        else if (loaded == IMAGE_GB) utilUpdateSystemColorMaps(gbLcdFilter);
        else utilUpdateSystemColorMaps(false);
    }

    mf->PollJoysticks();

    if (!paused) {
        HidePointer();
        HideMenuBar();
        event.RequestMore();

#ifndef NO_DEBUGGER
        if (debugger) {
            was_paused = true;
            dbgMain();

            if (!emulating) {
                emulating = true;
                UnloadGame();
            }

            return;
        }
#endif

        emusys->emuMain(emusys->emuCount);
#ifndef NO_LINK

        if (loaded == IMAGE_GBA && GetLinkMode() != LINK_DISCONNECTED)
            CheckLinkConnection();

#endif
    } else {
        was_paused = true;

        if (paused)
            SetExtraStyle(GetExtraStyle() & ~wxWS_EX_PROCESS_IDLE);

        ShowMenuBar();
    }

    if (do_rewind && emusys->emuWriteMemState) {
        if (!rewind_mem) {
            rewind_mem = (char*)malloc(NUM_REWINDS * REWIND_SIZE);
            num_rewind_states = next_rewind_state = 0;
        }

        if (!rewind_mem) {
            wxLogError(_("No memory for rewinding"));
            wxGetApp().frame->Close(true);
            return;
        }

        long resize;

        if (!emusys->emuWriteMemState(&rewind_mem[REWIND_SIZE * next_rewind_state],
                REWIND_SIZE, resize /* actual size */))
            // if you see a lot of these, maybe increase REWIND_SIZE
            wxLogInfo(_("Error writing rewind state"));
        else {
            if (!num_rewind_states) {
                mf->cmd_enable |= CMDEN_REWIND;
                mf->enable_menus();
            }

            if (num_rewind_states < NUM_REWINDS)
                ++num_rewind_states;

            next_rewind_state = (next_rewind_state + 1) % NUM_REWINDS;
        }

        do_rewind = false;
    }
}

static bool process_user_input(bool down, const config::UserInput& user_input)
{
    if (down)
        return config::GameControlState::Instance().OnInputPressed(user_input);
    else
        return config::GameControlState::Instance().OnInputReleased(user_input);
}

static void draw_black_background(wxWindow* win) {
    wxClientDC dc(win);
    wxCoord w, h;
    dc.GetSize(&w, &h);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, w, h);
}

static void process_keyboard_event(const wxKeyEvent& ev, bool down)
{
    int kc = ev.GetKeyCode();

    // Under Wayland or if the key is unicode, we can't use wxGetKeyState().
    if (!IsWayland() && kc != WXK_NONE) {
        // Check if the key state corresponds to the event.
        if (down != wxGetKeyState(static_cast<wxKeyCode>(kc))) {
            return;
        }
    }

    int key = getKeyboardKeyCode(ev);
    int mod = ev.GetModifiers();

    if (key == WXK_NONE) {
        return;
    }

    // modifier-only key releases do not set the modifier flag
    // so we set it here to match key release events to key press events
    switch (key) {
        case WXK_SHIFT:
            mod |= wxMOD_SHIFT;
            break;
        case WXK_ALT:
            mod |= wxMOD_ALT;
            break;
        case WXK_CONTROL:
            mod |= wxMOD_CONTROL;
            break;
#ifdef __WXMAC__
        case WXK_RAW_CONTROL:
            mod |= wxMOD_RAW_CONTROL;
            break;
#endif
        default:
            // This resets mod for any non-modifier key. This has the side
            // effect of not being able to set up a modifier+key for a game
            // control.
            mod = 0;
            break;
    }

    if (process_user_input(down, config::UserInput(key, mod, 0))) {
        wxWakeUpIdle();
    };
}

#ifdef __WXGTK__
static Display* GetX11Display()
{
    return GDK_WINDOW_XDISPLAY(gtk_widget_get_window(wxGetApp().frame->GetHandle()));
}
#endif

void GameArea::OnKeyDown(wxKeyEvent& ev)
{
    process_keyboard_event(ev, true);
}

void GameArea::OnKeyUp(wxKeyEvent& ev)
{
    process_keyboard_event(ev, false);
}

// these three are forwarded to the DrawingPanel instance
void GameArea::PaintEv(wxPaintEvent& ev)
{
    panel->PaintEv(ev);
}

void GameArea::EraseBackground(wxEraseEvent& ev)
{
    panel->EraseBackground(ev);
}

void GameArea::OnSize(wxSizeEvent& ev)
{
    draw_black_background(this);
    Layout();

    // panel may resize
    if (panel)
        panel->OnSize(ev);
    Layout();

    ev.Skip();
}

void GameArea::OnSDLJoy(wxJoyEvent& ev)
{
    process_user_input(ev.pressed(), config::UserInput(ev));

    // tell Linux to turn off the screensaver/screen-blank if joystick button was pressed
    // this shouldn't be necessary of course
#if defined(__WXGTK__) && defined(HAVE_XSS)
    if (!wxGetApp().UsingWayland()) {
        auto display = GetX11Display();
        XResetScreenSaver(display);
        XFlush(display);
    }
#endif
}

BEGIN_EVENT_TABLE(GameArea, wxPanel)
EVT_IDLE(GameArea::OnIdle)
EVT_SDLJOY(GameArea::OnSDLJoy)
// FIXME: wxGTK does not generate motion events in MainFrame (not sure
// what to do about it)
EVT_MOUSE_EVENTS(GameArea::MouseEvent)
END_EVENT_TABLE()

DrawingPanelBase::DrawingPanelBase(int _width, int _height)
    : width(_width),
      height(_height),
      scale(1),
      did_init(false),
      todraw(0),
      pixbuf1(0),
      pixbuf2(0),
      nthreads(0),
      rpi_(nullptr) {
    memset(delta, 0xff, sizeof(delta));

    if (config::OptDispFilter()->GetFilter() == config::Filter::kPlugin) {
        rpi_ = widgets::MaybeLoadFilterPlugin(
            config::OptDispFilterPlugin()->GetString(), &filter_plugin_);
        if (rpi_) {
            rpi_->Flags &= ~RPI_565_SUPP;

            if (rpi_->Flags & RPI_888_SUPP) {
                rpi_->Flags &= ~RPI_555_SUPP;
                // FIXME: should this be 32 or 24?  No docs or sample source
                systemColorDepth = 32;
            } else
                systemColorDepth = 16;

            if (!rpi_->Output) {
                // note that in actual kega fusion plugins, rpi_->Output is
                // unused (as is rpi_->Handle)
                rpi_->Output =
                    (RENDPLUG_Output)filter_plugin_.GetSymbol("RenderPluginOutput");
            }
            scale *= (rpi_->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
        } else {
            // This is going to delete the object. Do nothing more here.
            config::OptDispFilterPlugin()->SetString(wxEmptyString);
            config::OptDispFilter()->SetFilter(config::Filter::kNone);
            return;
        }
    }

    if (config::OptDispFilter()->GetFilter() != config::Filter::kPlugin) {
        scale *= GetFilterScale();
        systemColorDepth = 32;
    }

    // Intialize color tables
    if (systemColorDepth == 32) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
        systemRedShift = 3;
        systemGreenShift = 11;
        systemBlueShift = 19;
        RGB_LOW_BITS_MASK = 0x00010101;
#else
        systemRedShift = 27;
        systemGreenShift = 19;
        systemBlueShift = 11;
        RGB_LOW_BITS_MASK = 0x01010100;
#endif
    } else {
        // plugins expect RGB in native byte order
        systemRedShift = 10;
        systemGreenShift = 5;
        systemBlueShift = 0;
        RGB_LOW_BITS_MASK = 0x0421;
    }
}

DrawingPanel::DrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanelBase(_width, _height)
    , wxPanel(parent, wxID_ANY, wxPoint(0, 0), parent->GetClientSize(),
          wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
}

void DrawingPanelBase::DrawingPanelInit()
{
    did_init = true;
}

void DrawingPanelBase::PaintEv(wxPaintEvent& ev)
{
    (void)ev; // unused params
    wxPaintDC dc(GetWindow());

    if (!todraw) {
        // since this is set for custom background, not drawing anything
        // will cause garbage to be displayed, so draw a black area
        draw_black_background(GetWindow());
        return;
    }

    DrawArea(dc);

    // currently we draw the OSD directly on the framebuffer to reduce flickering
    //DrawOSD(dc);
}

void DrawingPanelBase::EraseBackground(wxEraseEvent& ev)
{
    (void)ev; // unused params
    // do nothing, do not allow propagation
}

// In order to run filters in parallel, they have to run from the method of
// a wxThread-derived class
//   threads are only created once, if possible, to avoid thread creation
//   overhead every frame; mutex+cond used to signal start and sem for end

//   when threading, there _will_ be bands for any nontrivial filter
//   usually the top and bottom line(s) of each region will look a little
//   different.  To ease this a tiny bit, 2 extra lines are generated at
//   the top of each region, so hopefully only the bottom of the previous
//   region will look screwed up.  The only correct way to handle this
//   is to draw an increasingly larger band around the seam until the
//   seam cover matches a line in both top & bottom regions, and then apply
//   cover to seam.   Way too much trouble for this, though.

//   another issue to consider is whether or not these filters are thread-
//   safe to begin with.  The built-in ones are verifyable (I didn't verify
//   them, though).  The plugins cannot be verified.  However, unlike the MFC
//   interface, I will allow them to be threaded at user's discretion.
class FilterThread : public wxThread {
public:
    FilterThread() : wxThread(wxTHREAD_JOINABLE), lock_(), sig_(lock_) {}

    wxMutex lock_;
    wxCondition sig_;
    wxSemaphore* done_;

    // Set these params before running
    int nthreads_;
    int threadno_;
    int width_;
    int height_;
    double scale_;
    const RENDER_PLUGIN_INFO* rpi_;
    uint8_t* dst_;
    uint8_t* delta_;

    // set this param every round
    // if NULL, end thread
    uint8_t* src_;

    ExitCode Entry() override {
        // This is the band this thread will process
        // threadno == -1 means just do a dummy round on the border line
        const int procy = height_ * threadno_ / nthreads_;
        height_ = height_ * (threadno_ + 1) / nthreads_ - procy;
        const int inbpp = systemColorDepth >> 3;
        const int inrb = systemColorDepth == 16   ? 2
                         : systemColorDepth == 24 ? 0
                                                  : 1;
        const int instride = (width_ + inrb) * inbpp;
        const int outbpp = out_16 ? 2 : systemColorDepth == 24 ? 3 : 4;
        const int outrb = systemColorDepth == 24 ? 0 : 4;
        const int outstride = std::ceil(width_ * outbpp * scale_) + outrb;
        delta_ += instride * procy;

        // FIXME: fugly hack
        if (config::OptDispRenderMethod()->GetRenderMethod() ==
            config::RenderMethod::kOpenGL) {
            dst_ += (int)std::ceil(outstride * (procy + 1) * scale_);
        } else {
            dst_ += (int)std::ceil(outstride * (procy + (1 / scale_)) * scale_);
        }

        while (nthreads_ == 1 || sig_.Wait() == wxCOND_NO_ERROR) {
            if (!src_ /* && nthreads > 1 */) {
                lock_.Unlock();
                return 0;
            }

            src_ += instride;

            // interframe blending filter
            // definitely not thread safe by default
            // added procy param to provide offset into accum buffers
            ApplyInterframe(instride, procy);

            if (config::OptDispFilter()->GetFilter() == config::Filter::kNone) {
                if (nthreads_ == 1)
                    return 0;

                done_->Post();
                continue;
            }

            // src += instride * procy;

            // naturally, any of these with accumulation buffers like those
            // of the IFB filters will screw up royally as well
            ApplyFilter(instride, outstride);
            if (nthreads_ == 1) {
                return 0;
            }

            done_->Post();
            continue;
        }

        return 0;
    }

private:
    // interframe blending filter
    // definitely not thread safe by default
    // added procy param to provide offset into accum buffers
    void ApplyInterframe(int instride, int procy) {
        switch (config::OptDispIFB()->GetInterframe()) {
            case config::Interframe::kNone:
                break;

            case config::Interframe::kSmart:
                if (systemColorDepth == 16)
                    SmartIB(src_, instride, width_, procy, height_);
                else
                    SmartIB32(src_, instride, width_, procy, height_);
                break;

            case config::Interframe::kMotionBlur:
                // FIXME: if(renderer == d3d/gl && filter == NONE) break;
                if (systemColorDepth == 16)
                    MotionBlurIB(src_, instride, width_, procy, height_);
                else
                    MotionBlurIB32(src_, instride, width_, procy, height_);
                break;

            case config::Interframe::kLast:
                assert(false);
                break;
        }
    }

    // naturally, any of these with accumulation buffers like those
    // of the IFB filters will screw up royally as well
    void ApplyFilter(int instride, int outstride) {
        switch (config::OptDispFilter()->GetFilter()) {
            case config::Filter::k2xsai:
                _2xSaI32(src_, instride, delta_, dst_, outstride, width_,
                         height_);
                break;

            case config::Filter::kSuper2xsai:
                Super2xSaI32(src_, instride, delta_, dst_, outstride, width_,
                             height_);
                break;

            case config::Filter::kSupereagle:
                SuperEagle32(src_, instride, delta_, dst_, outstride, width_,
                             height_);
                break;

            case config::Filter::kPixelate:
                Pixelate32(src_, instride, delta_, dst_, outstride, width_,
                           height_);
                break;

            case config::Filter::kAdvmame:
                AdMame2x32(src_, instride, delta_, dst_, outstride, width_,
                           height_);
                break;

            case config::Filter::kBilinear:
                Bilinear32(src_, instride, delta_, dst_, outstride, width_,
                           height_);
                break;

            case config::Filter::kBilinearplus:
                BilinearPlus32(src_, instride, delta_, dst_, outstride, width_,
                               height_);
                break;

            case config::Filter::kScanlines:
                Scanlines32(src_, instride, delta_, dst_, outstride, width_,
                            height_);
                break;

            case config::Filter::kTvmode:
                ScanlinesTV32(src_, instride, delta_, dst_, outstride, width_,
                              height_);
                break;

            case config::Filter::kLQ2x:
                lq2x32(src_, instride, delta_, dst_, outstride, width_,
                       height_);
                break;

            case config::Filter::kSimple2x:
                Simple2x32(src_, instride, delta_, dst_, outstride, width_,
                           height_);
                break;

            case config::Filter::kSimple3x:
                Simple3x32(src_, instride, delta_, dst_, outstride, width_,
                           height_);
                break;

            case config::Filter::kSimple4x:
                Simple4x32(src_, instride, delta_, dst_, outstride, width_,
                           height_);
                break;

            case config::Filter::kHQ2x:
                hq2x32(src_, instride, delta_, dst_, outstride, width_,
                       height_);
                break;

            case config::Filter::kHQ3x:
                hq3x32_32(src_, instride, delta_, dst_, outstride, width_,
                          height_);
                break;

            case config::Filter::kHQ4x:
                hq4x32_32(src_, instride, delta_, dst_, outstride, width_,
                          height_);
                break;

            case config::Filter::kXbrz2x:
                xbrz2x32(src_, instride, delta_, dst_, outstride, width_,
                         height_);
                break;

            case config::Filter::kXbrz3x:
                xbrz3x32(src_, instride, delta_, dst_, outstride, width_,
                         height_);
                break;

            case config::Filter::kXbrz4x:
                xbrz4x32(src_, instride, delta_, dst_, outstride, width_,
                         height_);
                break;

            case config::Filter::kXbrz5x:
                xbrz5x32(src_, instride, delta_, dst_, outstride, width_,
                         height_);
                break;

            case config::Filter::kXbrz6x:
                xbrz6x32(src_, instride, delta_, dst_, outstride, width_,
                         height_);
                break;

            case config::Filter::kPlugin:
                // MFC interface did not do plugins in parallel
                // Probably because it's almost certain they carry state
                // or do other non-thread-safe things But the user can
                // always turn mt off of it's not working..
                RENDER_PLUGIN_OUTP outdesc;
                outdesc.Size = sizeof(outdesc);
                outdesc.Flags = rpi_->Flags;
                outdesc.SrcPtr = src_;
                outdesc.SrcPitch = instride;
                outdesc.SrcW = width_;
                // FIXME: win32 code adds to H, saying that frame isn't
                // fully rendered otherwise I need to verify that
                // statement before I go adding stuff that may make it
                // crash.
                outdesc.SrcH = height_;  // + scale / 2
                outdesc.DstPtr = dst_;
                outdesc.DstPitch = outstride;
                outdesc.DstW = std::ceil(width_ * scale_);
                // on the other hand, there is at least 1 line below, so
                // I'll add that to dest in case safety checks in plugin
                // use < instead of <=
                outdesc.DstH =
                    std::ceil(height_ * scale_);  // + scale * (scale / 2)
                rpi_->Output(&outdesc);
                break;

            case config::Filter::kNone:
            case config::Filter::kLast:
                assert(false);
                break;
        }
    }
};

void DrawingPanelBase::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = out_16 ? 2 : systemColorDepth == 24 ? 3 : 4;
    int outrb = systemColorDepth == 24 ? 0 : 4;
    int outstride = std::ceil(width * outbpp * scale) + outrb;

    if (!pixbuf2) {
        int allocstride = outstride, alloch = height;

        // gb may write borders, so allocate enough for them
        if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
            allocstride = std::ceil(GameArea::SGBWidth * outbpp * scale) + outrb;
            alloch = GameArea::SGBHeight;
        }

        pixbuf2 = (uint8_t*)calloc(allocstride, std::ceil((alloch + 2) * scale));
    }

    if (config::OptDispFilter()->GetFilter() == config::Filter::kNone) {
        todraw = *data;
        // *data is assigned below, after old buf has been processed
        pixbuf1 = pixbuf2;
        pixbuf2 = todraw;
    } else
        todraw = pixbuf2;

    // FIXME: filters race condition?
    gopts.max_threads = 1;

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (config::OptDispFilter()->GetFilter() != config::Filter::kNone ||
        config::OptDispIFB()->GetInterframe() != config::Interframe::kNone) {
        if (nthreads != gopts.max_threads) {
            if (nthreads) {
                if (nthreads > 1)
                    for (int i = 0; i < nthreads; i++) {
                        threads[i].lock_.Lock();
                        threads[i].src_ = NULL;
                        threads[i].sig_.Signal();
                        threads[i].lock_.Unlock();
                        threads[i].Wait();
                    }

                delete[] threads;
            }

            nthreads = gopts.max_threads;
            threads = new FilterThread[nthreads];
            // first time around, no threading in order to avoid
            // static initializer conflicts
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].Entry();

            // go ahead and start the threads up, though
            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].threadno_ = i;
                    threads[i].nthreads_ = nthreads;
                    threads[i].width_ = width;
                    threads[i].height_ = height;
                    threads[i].scale_ = scale;
                    threads[i].dst_ = todraw;
                    threads[i].delta_ = delta;
                    threads[i].rpi_ = rpi_;
                    threads[i].done_ = &filt_done;
                    threads[i].lock_.Lock();
                    threads[i].Create();
                    threads[i].Run();
                }
            }
        } else if (nthreads == 1) {
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].Entry();
        } else {
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock_.Lock();
                threads[i].src_ = *data;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
            }

            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();
        }
    }

    // swap buffers now that src has been processed
    if (config::OptDispFilter()->GetFilter() == config::Filter::kNone) {
        *data = pixbuf1;
    }

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !gopts.statusbar) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        if (panel->osdstat.size())
            drawText(todraw + outstride * (systemColorDepth != 24), outstride,
                10, 20, UTF8(panel->osdstat), showSpeedTransparent);

        if (!disableStatusMessages && !panel->osdtext.empty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                wxString message = panel->osdtext;
                int linelen = std::ceil(width * scale - 20) / 8;
                int nlines = (message.size() + linelen - 1) / linelen;
                int cury = height - 14 - nlines * 10;
                char* buf = strdup(UTF8(message));
                char* ptr = buf;

                while (nlines > 1) {
                    char lchar = ptr[linelen];
                    ptr[linelen] = 0;
                    drawText(todraw + outstride * (systemColorDepth != 24),
                        outstride, 10, cury, ptr,
                        showSpeedTransparent);
                    cury += 10;
                    nlines--;
                    ptr += linelen;
                    *ptr = lchar;
                }

                drawText(todraw + outstride * (systemColorDepth != 24),
                    outstride, 10, cury, ptr,
                    showSpeedTransparent);

                free(buf);
                buf = NULL;
            } else
                panel->osdtext.clear();
        }
    }

    // next, draw the frame (queue a PaintEv) Refresh must be used under
    // Wayland or nothing is drawn.
    if (wxGetApp().UsingWayland())
        GetWindow()->Refresh();
    else {
        DrawingPanelBase* panel = wxGetApp().frame->GetPanel()->panel;
        if (panel) {
            wxClientDC dc(panel->GetWindow());
            panel->DrawArea(dc);
        }
    }

    // finally, draw on-screen text using wx method, if possible
    // this method flickers too much right now
    //DrawOSD(dc);
}

void DrawingPanelBase::DrawOSD(wxWindowDC& dc)
{
    // draw OSD message, if available
    // doing this here rather than using drawText() directly into the screen
    // image gives us 2 advantages:
    //   - non-ASCII text in messages
    //   - message is always default font size, regardless of screen stretch
    // and one known disadvantage:
    //   - 3d renderers may overwrite text asynchronously
    // Until I go through the trouble of making this render to a bitmap, and
    // then using the bitmap as a texture for the 3d renderers or rendering
    // directly into the output like DrawText, this is only enabled for
    // non-3d renderers.
    GameArea* panel = wxGetApp().frame->GetPanel();
    dc.SetTextForeground(wxColour(255, 0, 0, showSpeedTransparent ? 128 : 255));
    dc.SetTextBackground(wxColour(0, 0, 0, 0));
    dc.SetUserScale(1.0, 1.0);

    if (panel->osdstat.size())
        dc.DrawText(panel->osdstat, 10, 20);

    if (!panel->osdtext.empty()) {
        if (systemGetClock() - panel->osdtime >= OSD_TIME) {
            panel->osdtext.clear();
            wxGetApp().frame->PopStatusText();
        }
    }

    if (!disableStatusMessages && !panel->osdtext.empty()) {
        wxSize asz = dc.GetSize();
        wxString msg = panel->osdtext;
        int lw, lh;
        int mlw = asz.GetWidth() - 20;
        dc.GetTextExtent(msg, &lw, &lh);

        if (lw <= mlw)
            dc.DrawText(msg, 10, asz.GetHeight() - 16 - lh);
        else {
            // Since font is likely proportional, the only way to
            // find amt of text that will fit on a line is to search
            wxArrayInt llen; // length of each line, in chars

            for (size_t off = 0; off < msg.size();) {
// One way would be to bsearch on GetTextExtent() looking for
// best fit.
// Another would be to use GetPartialTextExtents and search
// the resulting array.
// GetPartialTextExtents may be inaccurate, but calling it
// once per line may be more efficient than calling
// GetTextExtent log2(remaining_len) times per line.
#if 1
                wxArrayInt clen;
                dc.GetPartialTextExtents(msg.substr(off), clen);
#endif
                int lenl = 0, lenh = msg.size() - off - 1, len;

                do {
                    len = (lenl + lenh) / 2;
#if 1
                    lw = clen[len];
#else
                    dc.GetTextExtent(msg.substr(off, len), &lw, NULL);
#endif

                    if (lw > mlw)
                        lenh = len - 1;
                    else if (lw < mlw)
                        lenl = len + 1;
                    else
                        break;
                } while (lenh >= lenl);

                if (lw <= mlw)
                    len++;

                off += len;
                llen.push_back(len);
            }

            int nlines = llen.size();
            int cury = asz.GetHeight() - 14 - nlines * (lh + 2);

            for (int off = 0, line = 0; line < nlines; off += llen[line], line++, cury += lh + 2)
                dc.DrawText(msg.substr(off, llen[line]), 10, cury);
        }
    }
}

void DrawingPanelBase::OnSize(wxSizeEvent& ev)
{
    ev.Skip();
}

DrawingPanelBase::~DrawingPanelBase()
{
    // pixbuf1 freed by emulator
    if (pixbuf1 != pixbuf2 && pixbuf2)
    {
        free(pixbuf2);
        pixbuf2 = NULL;
    }
    InterframeCleanup();

    if (nthreads) {
        if (nthreads > 1)
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock_.Lock();
                threads[i].src_ = NULL;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
                threads[i].Wait();
            }

        delete[] threads;
    }

    disableKeyboardBackgroundInput();
}

BasicDrawingPanel::BasicDrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanel(parent, _width, _height)
{
    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (config::OptDispFilter()->GetFilter() == config::Filter::kNone &&
        config::OptDispIFB()->GetInterframe() == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = 32;
    }
    if (!did_init) {
        DrawingPanelInit();
    }
}

void BasicDrawingPanel::DrawArea(wxWindowDC& dc)
{
    wxImage* im;

    if (systemColorDepth == 24) {
        // never scaled, no borders, no transformations needed
        im = new wxImage(width, height, todraw, true);
    } else if (out_16) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        uint16_t* src = (uint16_t*)todraw + (int)std::ceil((width + 2) * scale); // skip top border
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = ((*src >> systemRedShift) & 0x1f) << 3;
                *dst++ = ((*src >> systemGreenShift) & 0x1f) << 3;
                *dst++ = ((*src >> systemBlueShift) & 0x1f) << 3;
            }

            src += 2; // skip rhs border
        }
    } else if (config::OptDispFilter()->GetFilter() != config::Filter::kNone) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        uint32_t* src = (uint32_t*)todraw + (int)std::ceil(width * scale) + 1; // skip top border
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = *src >> (systemRedShift - 3);
                *dst++ = *src >> (systemGreenShift - 3);
                *dst++ = *src >> (systemBlueShift - 3);
            }

            ++src; // skip rhs border
        }
    } else {  // 32 bit
        // not scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        uint32_t* src = (uint32_t*)todraw + (int)std::ceil((width + 1) * scale); // skip top border
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = *src >> (systemRedShift - 3);
                *dst++ = *src >> (systemGreenShift - 3);
                *dst++ = *src >> (systemBlueShift - 3);
            }

            ++src; // skip rhs border
        }
    }

    DrawImage(dc, im);

    delete im;
}

void BasicDrawingPanel::DrawImage(wxWindowDC& dc, wxImage* im)
{
    double sx, sy;
    int w, h;
    GetClientSize(&w, &h);
    sx = w / (width * scale);
    sy = h / (height * scale);
    dc.SetUserScale(sx, sy);
    wxBitmap bm(*im);
    dc.DrawBitmap(bm, 0, 0);
}

#ifndef NO_OGL
// following 3 for vsync
#ifdef __WXMAC__
#include <OpenGL/OpenGL.h>
#endif
#ifdef __WXGTK__ // should actually check for X11, but GTK implies X11
#ifndef Status
#define Status int
#endif
#include <GL/glx.h>
#endif
#ifdef __WXMSW__
#include <GL/gl.h>
#include <GL/glext.h>
#endif

// This is supposed to be the default, but DOUBLEBUFFER doesn't seem to be
// turned on by default for wxGTK.
static int glopts[] = {
    WX_GL_RGBA, WX_GL_DOUBLEBUFFER, 0
};

GLDrawingPanel::GLDrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanelBase(_width, _height)
    , wxglc(parent, wxID_ANY, glopts, wxPoint(0, 0), parent->GetClientSize(),
          wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
    widgets::RequestHighResolutionOpenGlSurfaceForWindow(this);
#ifndef wxGL_IMPLICIT_CONTEXT
    ctx = new wxGLContext(this);
    SetCurrent(*ctx);
#endif
    if (!did_init) DrawingPanelInit();
}

GLDrawingPanel::~GLDrawingPanel()
{
    // this should be automatically deleted w/ context
    // it's also unsafe if panel no longer displayed
    if (did_init)
    {
#ifndef wxGL_IMPLICIT_CONTEXT
        SetCurrent(*ctx);
#else
        SetCurrent();
#endif
        glDeleteLists(vlist, 1);
        glDeleteTextures(1, &texid);
    }

#ifndef wxGL_IMPLICIT_CONTEXT
    delete ctx;
#endif
}

void GLDrawingPanel::DrawingPanelInit()
{
#ifndef wxGL_IMPLICIT_CONTEXT
    SetCurrent(*ctx);
#else
    SetCurrent();
#endif

    DrawingPanelBase::DrawingPanelInit();

    AdjustViewport();

    // taken from GTK front end almost verbatim
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 1.0, 0.0, 0.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    vlist = glGenLists(1);
    glNewList(vlist, GL_COMPILE);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0, 0.0);
    glVertex3i(0, 0, 0);
    glTexCoord2f(1.0, 0.0);
    glVertex3i(1, 0, 0);
    glTexCoord2f(0.0, 1.0);
    glVertex3i(0, 1, 0);
    glTexCoord2f(1.0, 1.0);
    glVertex3i(1, 1, 0);
    glEnd();
    glEndList();
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
        gopts.bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        gopts.bilinear ? GL_LINEAR : GL_NEAREST);

#define int_fmt out_16 ? GL_RGB5 : GL_RGB
#define tex_fmt out_16 ? GL_BGRA : GL_RGBA, \
                out_16 ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_BYTE
#if 0
        texsize = width > height ? width : height;
        texsize = std::ceil(texsize * scale);
        // texsize = 1 << ffs(texsize);
        texsize = texsize | (texsize >> 1);
        texsize = texsize | (texsize >> 2);
        texsize = texsize | (texsize >> 4);
        texsize = texsize | (texsize >> 8);
        texsize = (texsize >> 1) + 1;
        glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, texsize, texsize, 0, tex_fmt, NULL);
#else
    // but really, most cards support non-p2 and rect
    // if not, use cairo or wx renderer
    glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, std::ceil(width * scale), std::ceil(height * scale), 0, tex_fmt, NULL);
#endif
    glClearColor(0.0, 0.0, 0.0, 1.0);
// non-portable vsync code
#if defined(__WXGTK__)
    if (IsWayland()) {
#ifdef HAVE_EGL
        if (vsync)
            wxLogDebug(_("Enabling EGL VSync."));
        else
            wxLogDebug(_("Disabling EGL VSync."));

        eglSwapInterval(0, vsync);
#endif
    }
    else {
        if (vsync)
            wxLogDebug(_("Enabling GLX VSync."));
        else
            wxLogDebug(_("Disabling GLX VSync."));

        static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;
        static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = NULL;
        static PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = NULL;

        auto display        = GetX11Display();
        auto default_screen = DefaultScreen(display);

        char* glxQuery = (char*)glXQueryExtensionsString(display, default_screen);

        if (strstr(glxQuery, "GLX_EXT_swap_control") != NULL)
        {
            glXSwapIntervalEXT = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT"));
            if (glXSwapIntervalEXT)
                glXSwapIntervalEXT(glXGetCurrentDisplay(), glXGetCurrentDrawable(), vsync);
            else
                systemScreenMessage(_("Failed to set glXSwapIntervalEXT"));
        }
        if (strstr(glxQuery, "GLX_SGI_swap_control") != NULL)
        {
            glXSwapIntervalSGI = reinterpret_cast<PFNGLXSWAPINTERVALSGIPROC>(glXGetProcAddress((const GLubyte*)("glXSwapIntervalSGI")));

            if (glXSwapIntervalSGI)
                glXSwapIntervalSGI(vsync);
            else
                systemScreenMessage(_("Failed to set glXSwapIntervalSGI"));
        }
        if (strstr(glxQuery, "GLX_MESA_swap_control") != NULL)
        {
            glXSwapIntervalMESA = reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(glXGetProcAddress((const GLubyte*)("glXSwapIntervalMESA")));

            if (glXSwapIntervalMESA)
                glXSwapIntervalMESA(vsync);
            else
                systemScreenMessage(_("Failed to set glXSwapIntervalMESA"));
        }
    }
#elif defined(__WXMSW__)
    typedef const char* (*wglext)();
    wglext wglGetExtensionsStringEXT = (wglext)wglGetProcAddress("wglGetExtensionsStringEXT");
    if (wglGetExtensionsStringEXT == NULL) {
        systemScreenMessage(_("No support for wglGetExtensionsStringEXT"));
    }
    else if (strstr(wglGetExtensionsStringEXT(), "WGL_EXT_swap_control") == 0) {
        systemScreenMessage(_("No support for WGL_EXT_swap_control"));
    }

    typedef BOOL (__stdcall *PFNWGLSWAPINTERVALEXTPROC)(BOOL);
    static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(vsync);
    else
        systemScreenMessage(_("Failed to set wglSwapIntervalEXT"));
#elif defined(__WXMAC__)
    int swap_interval = vsync ? 1 : 0;
    CGLContextObj cgl_context = CGLGetCurrentContext();
    CGLSetParameter(cgl_context, kCGLCPSwapInterval, &swap_interval);
#else
    systemScreenMessage(_("No VSYNC available on this platform"));
#endif
}

void GLDrawingPanel::OnSize(wxSizeEvent& ev)
{
    AdjustViewport();

    // Temporary hack to backport 800d6ed69b from wxWidgets until 3.2.2 is released.
    if (IsWayland())
        MoveWaylandSubsurface(this);

    ev.Skip();
}

void GLDrawingPanel::AdjustViewport()
{
#ifndef wxGL_IMPLICIT_CONTEXT
    SetCurrent(*ctx);
#else
    SetCurrent();
#endif

    int x, y;
    widgets::GetRealPixelClientSize(this, &x, &y);
    glViewport(0, 0, x, y);

#ifdef DEBUG
    // you can use this to check that the gl surface is indeed high res
    GLint m_viewport[4];
    glGetIntegerv(GL_VIEWPORT, m_viewport);
    wxLogDebug(wxT("GL VIEWPORT: %d, %d, %d, %d"), m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
#endif
}

void GLDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc; // unused params
#ifndef wxGL_IMPLICIT_CONTEXT
    SetCurrent(*ctx);
#else
    SetCurrent();
#endif

    if (!did_init)
        DrawingPanelInit();

    if (todraw) {
        int rowlen = std::ceil(width * scale) + (out_16 ? 2 : 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlen);
#if wxBYTE_ORDER == wxBIG_ENDIAN

        // FIXME: is this necessary?
        if (out_16)
            glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);

#endif
        glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, std::ceil(width * scale), (int)std::ceil(height * scale),
            0, tex_fmt, todraw + (int)std::ceil(rowlen * (out_16 ? 2 : 4) * scale));
        glCallList(vlist);
    } else
        glClear(GL_COLOR_BUFFER_BIT);

    SwapBuffers();
}

#endif // GL support

#if defined(__WXMSW__) && !defined(NO_D3D)
#define DIRECT3D_VERSION 0x0900
#include <d3d9.h> // main include file
//#include <d3dx9core.h> // required for font rendering
#include <dxerr9.h> // contains debug functions

DXDrawingPanel::DXDrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanel(parent, _width, _height)
{
    // FIXME: implement
}

void DXDrawingPanel::DrawArea(wxWindowDC& dc)
{
    // FIXME: implement
    if (!did_init) {
      DrawingPanelInit();
    }

    if (todraw) {
    }
}
#endif

#ifndef NO_FFMPEG
static const wxString media_err(recording::MediaRet ret)
{
    switch (ret) {
    case recording::MRET_OK:
        return wxT("");

    case recording::MRET_ERR_NOMEM:
        return _("memory allocation error");

    case recording::MRET_ERR_NOCODEC:
        return _("error initializing codec");

    case recording::MRET_ERR_FERR:
        return _("error writing to output file");

    case recording::MRET_ERR_FMTGUESS:
        return _("can't guess output format from file name");

    default:
        //    case MRET_ERR_RECORDING:
        //    case MRET_ERR_BUFSIZE:
        return _("programming error; aborting!");
    }
}

void GameArea::StartVidRecording(const wxString& fname)
{
    recording::MediaRet ret;

    vid_rec.SetSampleRate(soundGetSampleRate());
    if ((ret = vid_rec.Record(UTF8(fname), basic_width, basic_height,
             systemColorDepth))
        != recording::MRET_OK)
        wxLogError(_("Unable to begin recording to %s (%s)"), fname.mb_str(),
            media_err(ret));
    else {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~(CMDEN_NVREC | CMDEN_NREC_ANY);
        mf->cmd_enable |= CMDEN_VREC;
        mf->enable_menus();
    }
}

void GameArea::StopVidRecording()
{
    vid_rec.Stop();
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~CMDEN_VREC;
    mf->cmd_enable |= CMDEN_NVREC;

    if (!(mf->cmd_enable & (CMDEN_VREC | CMDEN_SREC)))
        mf->cmd_enable |= CMDEN_NREC_ANY;

    mf->enable_menus();
}

void GameArea::StartSoundRecording(const wxString& fname)
{
    recording::MediaRet ret;

    snd_rec.SetSampleRate(soundGetSampleRate());
    if ((ret = snd_rec.Record(UTF8(fname))) != recording::MRET_OK)
        wxLogError(_("Unable to begin recording to %s (%s)"), fname.mb_str(),
            media_err(ret));
    else {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~(CMDEN_NSREC | CMDEN_NREC_ANY);
        mf->cmd_enable |= CMDEN_SREC;
        mf->enable_menus();
    }
}

void GameArea::StopSoundRecording()
{
    snd_rec.Stop();
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~CMDEN_SREC;
    mf->cmd_enable |= CMDEN_NSREC;

    if (!(mf->cmd_enable & (CMDEN_VREC | CMDEN_SREC)))
        mf->cmd_enable |= CMDEN_NREC_ANY;

    mf->enable_menus();
}

void GameArea::AddFrame(const uint16_t* data, int length)
{
    recording::MediaRet ret;

    if ((ret = vid_rec.AddFrame(data, length)) != recording::MRET_OK) {
        wxLogError(_("Error in audio/video recording (%s); aborting"),
            media_err(ret));
        vid_rec.Stop();
    }

    if ((ret = snd_rec.AddFrame(data, length)) != recording::MRET_OK) {
        wxLogError(_("Error in audio recording (%s); aborting"), media_err(ret));
        snd_rec.Stop();
    }
}

void GameArea::AddFrame(const uint8_t* data)
{
    recording::MediaRet ret;

    if ((ret = vid_rec.AddFrame(data)) != recording::MRET_OK) {
        wxLogError(_("Error in video recording (%s); aborting"), media_err(ret));
        vid_rec.Stop();
    }
}
#endif

void GameArea::MouseEvent(wxMouseEvent& ev)
{
    mouse_active_time = systemGetClock();

    wxPoint cur_pos = wxGetMousePosition();

    // Ignore small movements.
    if (!ev.Moving() || (std::abs(cur_pos.x - mouse_last_pos.x) >= 11 && std::abs(cur_pos.y - mouse_last_pos.y) >= 11)) {
        ShowPointer();
        ShowMenuBar();
    }

    mouse_last_pos = cur_pos;

    ev.Skip();
}

void GameArea::ShowPointer()
{
    if (!pointer_blanked || fullscreen) return;

    pointer_blanked = false;

    SetCursor(wxNullCursor);

    if (panel)
        panel->GetWindow()->SetCursor(wxNullCursor);
}

void GameArea::HidePointer()
{
    if (pointer_blanked || !main_frame) return;

    // FIXME: make time configurable
    if ((fullscreen || (systemGetClock() - mouse_active_time) > 3000) &&
        !(main_frame->MenusOpened() || main_frame->DialogOpened())) {
        pointer_blanked = true;
        SetCursor(wxCursor(wxCURSOR_BLANK));

        // wxGTK requires that subwindows get the cursor as well
        if (panel)
            panel->GetWindow()->SetCursor(wxCursor(wxCURSOR_BLANK));
    }
}

// We do not hide the menubar on mac, on mac it is not part of the main frame
// and the user can adjust hiding behavior herself.
void GameArea::HideMenuBar()
{
#ifndef __WXMAC__
    if (!main_frame || menu_bar_hidden || !gopts.hide_menu_bar) return;

    if (((systemGetClock() - mouse_active_time) > 3000) && !main_frame->MenusOpened()) {
#ifdef __WXMSW__
        current_hmenu = static_cast<HMENU>(main_frame->GetMenuBar()->GetHMenu());
        ::SetMenu(main_frame->GetHandle(), nullptr);
#else
        main_frame->GetMenuBar()->Hide();
#endif
        SendSizeEvent();
        menu_bar_hidden = true;
    }
#endif
}

void GameArea::ShowMenuBar()
{
#ifndef __WXMAC__
    if (!main_frame || !menu_bar_hidden) return;

#ifdef __WXMSW__
    if (current_hmenu != nullptr) {
        ::SetMenu(main_frame->GetHandle(), current_hmenu);
        current_hmenu = nullptr;
    }
#else
    main_frame->GetMenuBar()->Show();
#endif
    SendSizeEvent();
    menu_bar_hidden = false;
#endif
}

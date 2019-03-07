#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <wx/dcbuffer.h>
#include <SDL_joystick.h>

#include "version.h"
#include "../common/ConfigManager.h"
#include "../common/Patch.h"
#include "../gb/gbPrinter.h"
#include "../gba/RTC.h"
#include "../gba/agbprint.h"
#include "../sdl/text.h"
#include "drawing.h"
#include "filters.h"
#include "wxvbam.h"

int emulating;

IMPLEMENT_DYNAMIC_CLASS(GameArea, wxPanel)

GameArea::GameArea()
    : wxPanel()
    , loaded(IMAGE_UNKNOWN)
    , panel(NULL)
    , emusys(NULL)
    , basic_width(GBAWidth)
    , basic_height(GBAHeight)
    , fullscreen(false)
    , paused(false)
    , was_paused(false)
    , rewind_time(0)
    , do_rewind(false)
    , rewind_mem(0)
    , pointer_blanked(false)
    , mouse_active_time(0)
{
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
    wxCharBuffer fnb(fnfn.GetFullPath().mb_fn_str());
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
                pfn.SetExt(wxT("ppf"));
                loadpatch = pfn.IsFileReadable();
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
            // auto-conversion of wxCharBuffer to const char * seems broken
            // so save underlying wxCharBuffer (or create one of none is used)
            wxCharBuffer pfnb(pfn.GetFullPath().mb_fn_str());
            applyPatch(pfnb.data(), &gbRom, &size);

            if (size != rom_size)
                gbUpdateSizes();

            rom_size = size;
        }

        // start sound; this must happen before CPU stuff
        gb_effects_config.enabled = gopts.gb_effects_config_enabled;
        gb_effects_config.surround = gopts.gb_effects_config_surround;
        gb_effects_config.echo = (float)gopts.gb_echo / 100.0;
        gb_effects_config.stereo = (float)gopts.gb_stereo / 100.0;
        gbSoundSetDeclicking(gopts.gb_declick);
        soundInit();
        soundSetEnable(gopts.sound_en);
        gbSoundSetSampleRate(!gopts.sound_qual ? 48000 : 44100 / (1 << (gopts.sound_qual - 1)));
        soundSetVolume((float)gopts.sound_vol / 100.0);
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(throttle);
        gbGetHardwareType();
        bool use_bios = false;
        // auto-conversion of wxCharBuffer to const char * seems broken
        // so save underlying wxCharBuffer (or create one of none is used)
        const char* fn = NULL;
        wxCharBuffer fnb;

        if (gbCgbMode) {
            use_bios = useBiosFileGBC;
            fnb = gopts.gbc_bios.mb_fn_str();
        } else {
            use_bios = useBiosFileGB;
            fnb = gopts.gb_bios.mb_fn_str();
        }

        fn = fnb.data();
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
            int size = 0x2000000;
            // auto-conversion of wxCharBuffer to const char * seems broken
            // so save underlying wxCharBuffer (or create one of none is used)
            wxCharBuffer pfnb(pfn.GetFullPath().mb_fn_str());
            applyPatch(pfnb.data(), &rom, &size);
            // that means we no longer really know rom_size either <sigh>
        }

        wxFileConfig* cfg = wxGetApp().overrides;
        wxString id = wxString((const char*)&rom[0xac], wxConvLibc, 4);

        if (cfg->HasGroup(id)) {
            cfg->SetPath(id);
            rtcEnable(cfg->Read(wxT("rtcEnabled"), rtcEnabled));
            int fsz = cfg->Read(wxT("flashSize"), (long)0);

            if (fsz != 0x10000 && fsz != 0x20000)
                fsz = 0x10000 << winFlashSize;

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
            flashSetSize(0x10000 << winFlashSize);

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
        soundInit();
        soundSetEnable(gopts.sound_en);
        soundSetSampleRate(!gopts.sound_qual ? 48000 : 44100 / (1 << (gopts.sound_qual - 1)));
        soundSetVolume((float)gopts.sound_vol / 100.0);
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(throttle);
        soundFiltering = (float)gopts.gba_sound_filter / 100.0f;
        CPUInit(gopts.gba_bios.mb_fn_str(), useBiosFileGBA);

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

    if (fullScreen)
        GameArea::ShowFullScreen(true);

    loaded = t;
    SetFrameTitle();
    SetFocus();
    AdjustSize(true);
    emulating = true;
    was_paused = true;
    MainFrame* mf = wxGetApp().frame;
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
        fnb = bat.GetFullPath().mb_fn_str();

        if (emusys->emuReadBattery(fnb.data())) {
            wxString msg;
            msg.Printf(_("Loaded battery %s"), bat.GetFullPath().mb_str());
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
                cld = gbCheatsLoadCheatList(cfn.GetFullPath().mb_fn_str());
            else
                cld = cheatsLoadCheatList(cfn.GetFullPath().mb_fn_str());

            if (cld) {
                systemScreenMessage(_("Loaded cheats"));
                cheats_dirty = false;
            }
        }
    }

#ifndef NO_LINK

    if (gopts.link_auto) {
        linkMode = mf->GetConfiguredLinkMode();
        BootLink(linkMode, gopts.link_host.mb_str(wxConvUTF8), linkTimeout, linkHacks, linkNumPlayers);
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
#ifndef FINAL_BUILD
    tit.append(_(VERSION));
#endif
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

    statedir = gopts.state_dir;

    if (!statedir.size()) {
        statedir = loaded_game.GetPathWithSep();
    } else {
        statedir = wxGetApp().GetAbsolutePath(gopts.state_dir);
    }

    if (!wxIsWritable(batdir))
        batdir = wxGetApp().GetConfigurationPath();

    if (!wxIsWritable(statedir))
        statedir = wxGetApp().GetConfigurationPath();
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
                gbCheatsSaveCheatList(cfn.GetFullPath().mb_fn_str());
        } else {
            if (!cheatsNumber)
                wxRemoveFile(cfn.GetFullPath());
            else
                cheatsSaveCheatList(cfn.GetFullPath().mb_fn_str());
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
    debugger = false;
    remoteCleanUp();
    mf->cmd_enable |= CMDEN_NGDB_ANY;

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

    if (destruct)
        return;

    // in destructor, panel should be auto-deleted by wx since all panels
    // are derived from a window attached as child to GameArea
    if (panel)
        panel->Destroy();

    panel = NULL;

    // close any game-related viewer windows
    // in destructor, viewer windows are in process of being deleted anyway
    while (!mf->popups.empty())
        mf->popups.front()->Close(true);

    // remaining items are GUI updates that should not be needed in destructor
    SetFrameTitle();
    mf->cmd_enable &= UNLOAD_CMDEN_KEEP;
    mf->update_state_ts(true);
    mf->enable_menus();
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
    fname.Printf(SAVESLOT_FMT, game_name().mb_str(), slot);
    return LoadState(wxFileName(statedir, fname));
}

bool GameArea::LoadState(const wxFileName& fname)
{
    // FIXME: first save to backup state if not backup state
    bool ret = emusys->emuReadState(fname.GetFullPath().mb_fn_str());

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
        fname.GetFullPath().mb_str());
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
    fname.Printf(SAVESLOT_FMT, game_name().mb_str(), slot);
    return SaveState(wxFileName(statedir, fname));
}

bool GameArea::SaveState(const wxFileName& fname)
{
    // FIXME: first copy to backup state if not backup state
    bool ret = emusys->emuWriteState(fname.GetFullPath().mb_fn_str());
    wxGetApp().frame->update_state_ts(true);
    wxString msg;
    msg.Printf(ret ? _("Saved state %s") : _("Error saving state %s"),
        fname.GetFullPath().mb_str());
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
    // auto-conversion of wxCharBuffer to const char * seems broken
    // so save underlying wxCharBuffer (or create one of none is used)
    wxCharBuffer fnb = fn.mb_fn_str();

    // FIXME: add option to support ring of backups
    // of course some games just write battery way too often for such
    // a thing to be useful
    if (!emusys->emuWriteBattery(fnb.data()))
        wxLogError(_("Error writing battery %s"), fn);

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

    if (panel)
        panel->Destroy();

    panel = NULL;
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

    if (panel)
        panel->Destroy();

    panel = NULL;
}

void GameArea::AdjustMinSize()
{
    wxWindow* frame           = wxGetApp().frame;
    double hidpi_scale_factor = HiDPIScaleFactor();

    // note: could safely set min size to 1x or less regardless of video_scale
    // but setting it to scaled size makes resizing to default easier
    wxSize sz((std::ceil(basic_width  * gopts.video_scale) / hidpi_scale_factor),
              (std::ceil(basic_height * gopts.video_scale) / hidpi_scale_factor));
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
    double hidpi_scale_factor = HiDPIScaleFactor();

    wxSize sz(std::ceil(basic_width  / hidpi_scale_factor),
              std::ceil(basic_height / hidpi_scale_factor));

    SetMinSize(sz);
    // do not take decorations into account
    frame->SetMinSize(sz);
}

void GameArea::AdjustSize(bool force)
{
    AdjustMinSize();

    if (fullscreen)
        return;

    double hidpi_scale_factor = HiDPIScaleFactor();
    const wxSize newsz((std::ceil(basic_width  * gopts.video_scale) / hidpi_scale_factor),
                       (std::ceil(basic_height * gopts.video_scale) / hidpi_scale_factor));

    if (!force) {
        wxSize sz = GetClientSize();

        if (sz.GetWidth() >= newsz.GetWidth() && sz.GetHeight() >= newsz.GetHeight())
            return;
    }

    SetSize(newsz);
    GetParent()->SetClientSize(newsz);
    wxGetApp().frame->Fit();
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
    if (panel) {
        panel->Destroy();
        panel = NULL;
    }

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
                int i;

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

    if (loaded != IMAGE_UNKNOWN)
        soundPause();
}

void GameArea::Resume()
{
    if (!paused)
        return;

    paused = false;
    SetExtraStyle(GetExtraStyle() | wxWS_EX_PROCESS_IDLE);

    if (loaded != IMAGE_UNKNOWN)
        soundResume();

    SetFocus();
}

void GameArea::OnIdle(wxIdleEvent& event)
{
    wxString pl = wxGetApp().pending_load;

    if (pl.size()) {
        // sometimes this gets into a loop if LoadGame() called before
        // clearing pending_load.  weird.
        wxGetApp().pending_load = wxEmptyString;
        LoadGame(pl);
        MainFrame* mf = wxGetApp().frame;

        if (gdbBreakOnLoad)
            mf->GDBBreak();

        if (debugger && loaded != IMAGE_GBA) {
            wxLogError(_("Not a valid GBA cartridge"));
            UnloadGame();
        }
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
        switch (gopts.render_method) {
        case RND_SIMPLE:
            panel = new BasicDrawingPanel(this, basic_width, basic_height);
            break;
#ifdef __WXMAC__
        case RND_QUARTZ2D:
            panel = new Quartz2DDrawingPanel(this, basic_width, basic_height);
            break;
#endif
#ifndef NO_OGL
        case RND_OPENGL:
            panel = new GLDrawingPanel(this, basic_width, basic_height);
            break;
#endif
#ifdef __WXMSW__
        case RND_DIRECT3D:
            panel = new DXDrawingPanel(this, basic_width, basic_height);
            break;
#endif
        }

        wxWindow* w = panel->GetWindow();

        // set up event handlers
        // use both CHAR_HOOK and KEY_DOWN in case CHAR_HOOK does not work for whatever reason
        w->Connect(wxEVT_CHAR_HOOK,        wxKeyEventHandler(GameArea::OnKeyDown),         NULL, this);
        w->Connect(wxEVT_KEY_DOWN,         wxKeyEventHandler(GameArea::OnKeyDown),         NULL, this);
        w->Connect(wxEVT_KEY_UP,           wxKeyEventHandler(GameArea::OnKeyUp),           NULL, this);
        w->Connect(wxEVT_PAINT,            wxPaintEventHandler(GameArea::PaintEv),         NULL, this);
        w->Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(GameArea::EraseBackground), NULL, this);

        // set userdata so we know it's the panel and not the frame being resized
        // the userdata is freed on disconnect/destruction
        w->Connect(wxEVT_SIZE,             wxSizeEventHandler(GameArea::OnSize),           new wxObject, this);
        this->Connect(wxEVT_SIZE,          wxSizeEventHandler(GameArea::OnSize),           NULL, this);

        w->SetBackgroundStyle(wxBG_STYLE_CUSTOM);
        w->SetSize(wxSize(basic_width, basic_height));

        if (maxScale)
            w->SetMaxSize(wxSize(basic_width * maxScale,
                basic_height * maxScale));

        // if user changed Display/Scale config, this needs to run
        AdjustMinSize();
        AdjustSize(false);

        // add spacers on top and bottom to center panel vertically
        GetSizer()->Add(0, 0, 1, wxEXPAND);
        GetSizer()->Add(w,    0, gopts.retain_aspect ? (wxSHAPED | wxALIGN_CENTER | wxEXPAND) : wxEXPAND);
        GetSizer()->Add(0, 0, 1, wxEXPAND);
        Layout();

        if (pointer_blanked)
            w->SetCursor(wxCursor(wxCURSOR_BLANK));

        // set focus to panel
        w->SetFocus();
    }

    if (!paused) {
        HidePointer();
        event.RequestMore();

        if (debugger) {
            was_paused = true;
            dbgMain();

            if (!emulating) {
                emulating = true;
                UnloadGame();
            }

            return;
        }

        emusys->emuMain(emusys->emuCount);
#ifndef NO_LINK

        if (loaded == IMAGE_GBA && GetLinkMode() != LINK_DISCONNECTED)
            CheckLinkConnection();

#endif
    } else {
        was_paused = true;

        if (paused)
            SetExtraStyle(GetExtraStyle() & ~wxWS_EX_PROCESS_IDLE);
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
                MainFrame* mf = wxGetApp().frame;
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

// Note: keys will get stuck if they are released while window has no focus
// can't really do anything about it, except scan for pressed keys on
// activate events.  Maybe later.

static uint32_t bmask[NUM_KEYS] = {
    KEYM_UP, KEYM_DOWN, KEYM_LEFT, KEYM_RIGHT, KEYM_A, KEYM_B, KEYM_L, KEYM_R,
    KEYM_SELECT, KEYM_START, KEYM_MOTION_UP, KEYM_MOTION_DOWN, KEYM_MOTION_LEFT,
    KEYM_MOTION_RIGHT, KEYM_MOTION_IN, KEYM_MOTION_OUT, KEYM_AUTO_A, KEYM_AUTO_B, KEYM_SPEED, KEYM_CAPTURE,
    KEYM_GS
};

static wxJoyKeyBinding_v keys_pressed;

struct game_key {
    int player;
    int key_num;
    int bind_num;
    wxJoyKeyBinding_v& b;
};

static std::vector<game_key>* game_keys_pressed(int key, int mod, int joy)
{
    auto vec = new std::vector<game_key>;

    for (int player = 0; player < 4; player++)
        for (int key_num = 0; key_num < NUM_KEYS; key_num++) {
            wxJoyKeyBinding_v& b = gopts.joykey_bindings[player][key_num];

            for (int bind_num = 0; bind_num < b.size(); bind_num++)
                if (b[bind_num].key == key && b[bind_num].mod == mod && b[bind_num].joy == joy)
                    vec->push_back({player, key_num, bind_num, b});
        }

    return vec;
}

static bool process_key_press(bool down, int key, int mod, int joy = 0)
{
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
    }

    // check if key is already pressed
    int kpno;

    for (kpno = 0; kpno < keys_pressed.size(); kpno++)
        if (keys_pressed[kpno].key == key && keys_pressed[kpno].mod == mod && keys_pressed[kpno].joy == joy)
            break;

    auto game_keys = game_keys_pressed(key, mod, joy);

    const bool is_game_key = game_keys->size();

    if (kpno < keys_pressed.size()) {
        // double press is noop
        if (down)
            return is_game_key;

        // if released, forget it
        keys_pressed.erase(keys_pressed.begin() + kpno);
    } else {
        // double release is noop
        if (!down)
            return is_game_key;

        // otherwise remember it
        keys_pressed.push_back({key, mod, joy});
    }

    for (auto&& game_key : *game_keys) {
        if (down) {
            // press button
            joypress[game_key.player] |= bmask[game_key.key_num];
        }
        else {
            // only release if no others pressed
            int bind2;
            auto b = game_key.b;

            for (bind2 = 0; bind2 < game_key.b.size(); bind2++) {
                if (game_key.bind_num == bind2 || (b[bind2].key == key && b[bind2].mod == mod && b[bind2].joy == joy))
                    continue;

                for (kpno = 0; kpno < keys_pressed.size(); kpno++)
                    if (keys_pressed[kpno].key == b[bind2].key && keys_pressed[kpno].mod == b[bind2].mod && keys_pressed[kpno].joy == b[bind2].joy)
                        break;
            }

            if (bind2 == b.size()) {
                // release button
                joypress[game_key.player] &= ~bmask[game_key.key_num];
            }
        }
    }

    delete game_keys;

    return is_game_key;
}

static void draw_black_background(wxWindow* win) {
    wxClientDC dc(win);
    wxCoord w, h;
    dc.GetSize(&w, &h);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, w, h);
}

void GameArea::OnKeyDown(wxKeyEvent& ev)
{
    if (process_key_press(true, ev.GetKeyCode(), ev.GetModifiers())) {
        ev.Skip(false);
        ev.StopPropagation();
        wxWakeUpIdle();
    }
    else {
        ev.Skip(true);
    }
}

void GameArea::OnKeyUp(wxKeyEvent& ev)
{
    if (process_key_press(false, ev.GetKeyCode(), ev.GetModifiers())) {
        ev.Skip(false);
        ev.StopPropagation();
        wxWakeUpIdle();
    }
    else {
        ev.Skip(true);
    }
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
    if (!ev.GetEventUserData()) { // is frame
        draw_black_background(this);
        Layout();
    }

    // panel may resize
    if (panel)
        panel->OnSize(ev);

    ev.Skip(true);
}

void GameArea::OnSDLJoy(wxSDLJoyEvent& ev)
{
    int key = ev.GetControlIndex();
    int mod = wxJoyKeyTextCtrl::DigitalButton(ev);
    int joy = ev.GetJoy() + 1;

    // mutually exclusive key types unpress their opposite
    if (mod == WXJB_AXIS_PLUS) {
        process_key_press(false, key, WXJB_AXIS_MINUS, joy);
        process_key_press(ev.GetControlValue() != 0, key, mod, joy);
    } else if (mod == WXJB_AXIS_MINUS) {
        process_key_press(false, key, WXJB_AXIS_PLUS, joy);
        process_key_press(ev.GetControlValue() != 0, key, mod, joy);
    } else if (mod >= WXJB_HAT_FIRST && mod <= WXJB_HAT_LAST) {
        int value = ev.GetControlValue();
        process_key_press(value & SDL_HAT_UP, key, WXJB_HAT_N, joy);
        process_key_press(value & SDL_HAT_DOWN, key, WXJB_HAT_S, joy);
        process_key_press(value & SDL_HAT_RIGHT, key, WXJB_HAT_E, joy);
        process_key_press(value & SDL_HAT_LEFT, key, WXJB_HAT_W, joy);
    } else
        process_key_press(ev.GetControlValue() != 0, key, mod, joy);
}

BEGIN_EVENT_TABLE(GameArea, wxPanel)
EVT_IDLE(GameArea::OnIdle)
EVT_SDLJOY(GameArea::OnSDLJoy)
// FIXME: wxGTK does not generate motion events in MainFrame (not sure
// what to do about it)
EVT_MOUSE_EVENTS(GameArea::MouseEvent)
END_EVENT_TABLE()

DrawingPanelBase::DrawingPanelBase(int _width, int _height)
    : width(_width)
    , height(_height)
    , scale(1)
    , did_init(false)
    , todraw(0)
    , pixbuf1(0)
    , pixbuf2(0)
    , rpi(0)
    , nthreads(0)
{
    memset(delta, 0xff, sizeof(delta));

    if (gopts.filter == FF_PLUGIN) {
        do // do { } while(0) so break; exits entire block
        {
            // could've also just used goto & a label, I guess
            gopts.filter = FF_NONE; // preemptive in case of errors
            systemColorDepth = 32;

            if (gopts.filter_plugin.empty())
                break;

            wxFileName fpn(gopts.filter_plugin);
            fpn.MakeAbsolute(wxGetApp().GetPluginsDir());

            if (!filt_plugin.Load(fpn.GetFullPath(), wxDL_VERBATIM | wxDL_NOW))
                break;

            RENDPLUG_GetInfo gi = (RENDPLUG_GetInfo)filt_plugin.GetSymbol(wxT("RenderPluginGetInfo"));

            if (!gi)
                break;

            // need to be able to write to _rpi to set Output() and Flags
            RENDER_PLUGIN_INFO* _rpi = gi();

            // FIXME: maybe < RPI_VERSION, assuming future vers. back compat?
            if (!_rpi || (_rpi->Flags & 0xff) != RPI_VERSION || !(_rpi->Flags & (RPI_555_SUPP | RPI_888_SUPP)))
                break;

            _rpi->Flags &= ~RPI_565_SUPP;

            if (_rpi->Flags & RPI_888_SUPP) {
                _rpi->Flags &= ~RPI_555_SUPP;
                // FIXME: should this be 32 or 24?  No docs or sample source
                systemColorDepth = 32;
            } else
                systemColorDepth = 16;

            if (!_rpi->Output)
                // note that in actual kega fusion plugins, rpi->Output is
                // unused (as is rpi->Handle)
                _rpi->Output = (RENDPLUG_Output)filt_plugin.GetSymbol(wxT("RenderPluginOutput"));

            scale *= (_rpi->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
            rpi = _rpi;
            gopts.filter = FF_PLUGIN; // now that there is a valid plugin
        } while (0);
    } else {
        scale *= builtin_ff_scale(gopts.filter);
#define out_16 (systemColorDepth == 16)
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

    // FIXME: should be "true" for GBA carts if lcd mode selected
    // which means this needs to be re-run at pref change time
    utilUpdateSystemColorMaps(false);
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
    FilterThread()
        : wxThread(wxTHREAD_JOINABLE)
        , lock()
        , sig(lock)
    {
    }

    wxMutex lock;
    wxCondition sig;
    wxSemaphore* done;

    // Set these params before running
    int nthreads, threadno;
    int width, height;
    double scale;
    const RENDER_PLUGIN_INFO* rpi;
    uint8_t *dst, *delta;

    // set this param every round
    // if NULL, end thread
    uint8_t* src;

    ExitCode Entry()
    {
        // This is the band this thread will process
        // threadno == -1 means just do a dummy round on the border line
        int procy = height * threadno / nthreads;
        height = height * (threadno + 1) / nthreads - procy;
        int inbpp = systemColorDepth >> 3;
        int inrb = systemColorDepth == 16 ? 2 : systemColorDepth == 24 ? 0 : 1;
        int instride = (width + inrb) * inbpp;
        int outbpp = out_16 ? 2 : systemColorDepth == 24 ? 3 : 4;
        int outrb = systemColorDepth == 24 ? 0 : 4;
        int outstride = std::ceil(width * outbpp * scale) + outrb;
        delta += instride * procy;
        // FIXME: fugly hack
        if(gopts.render_method == RND_OPENGL)
            dst += (int)std::ceil(outstride * (procy + 1) * scale);
        else
            dst += (int)std::ceil(outstride * (procy + (1 / scale)) * scale);

        while (nthreads == 1 || sig.Wait() == wxCOND_NO_ERROR) {
            if (!src /* && nthreads > 1 */) {
                lock.Unlock();
                return 0;
            }

            src += instride;

            // interframe blending filter
            // definitely not thread safe by default
            // added procy param to provide offset into accum buffers
            if (gopts.ifb != IFB_NONE) {
                switch (gopts.ifb) {
                case IFB_SMART:
                    if (systemColorDepth == 16)
                        SmartIB(src, instride, width, procy, height);
                    else
                        SmartIB32(src, instride, width, procy, height);

                    break;

                case IFB_MOTION_BLUR:

                    // FIXME: if(renderer == d3d/gl && filter == NONE) break;
                    if (systemColorDepth == 16)
                        MotionBlurIB(src, instride, width, procy, height);
                    else
                        MotionBlurIB32(src, instride, width, procy, height);

                    break;
                }
            }

            if (gopts.filter == FF_NONE) {
                if (nthreads == 1)
                    return 0;

                done->Post();
                continue;
            }

            src += instride * procy;

            // naturally, any of these with accumulation buffers like those of
            // the IFB filters will screw up royally as well
            switch (gopts.filter) {
            case FF_2XSAI:
                _2xSaI32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_SUPER2XSAI:
                Super2xSaI32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_SUPEREAGLE:
                SuperEagle32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_PIXELATE:
                Pixelate32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_ADVMAME:
                AdMame2x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_BILINEAR:
                Bilinear32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_BILINEARPLUS:
                BilinearPlus32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_SCANLINES:
                Scanlines32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_TV:
                ScanlinesTV32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_LQ2X:
                lq2x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_SIMPLE2X:
                Simple2x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_SIMPLE3X:
                Simple3x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_SIMPLE4X:
                Simple4x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_HQ2X:
                hq2x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_HQ3X:
                hq3x32_32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_HQ4X:
                hq4x32_32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_XBRZ2X:
                xbrz2x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_XBRZ3X:
                xbrz3x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_XBRZ4X:
                xbrz4x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_XBRZ5X:
                xbrz5x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_XBRZ6X:
                xbrz6x32(src, instride, delta, dst, outstride, width, height);
                break;

            case FF_PLUGIN:
                // MFC interface did not do plugins in parallel
                // Probably because it's almost certain they carry state or do
                // other non-thread-safe things
                // But the user can always turn mt off of it's not working..
                RENDER_PLUGIN_OUTP outdesc;
                outdesc.Size = sizeof(outdesc);
                outdesc.Flags = rpi->Flags;
                outdesc.SrcPtr = src;
                outdesc.SrcPitch = instride;
                outdesc.SrcW = width;
                // FIXME: win32 code adds to H, saying that frame isn't fully
                // rendered otherwise
                // I need to verify that statement before I go adding stuff that
                // may make it crash.
                outdesc.SrcH = height; // + scale / 2
                outdesc.DstPtr = dst;
                outdesc.DstPitch = outstride;
                outdesc.DstW = std::ceil(width * scale);
                // on the other hand, there is at least 1 line below, so I'll add
                // that to dest in case safety checks in plugin use < instead of <=
                outdesc.DstH = std::ceil(height * scale); // + scale * (scale / 2)
                rpi->Output(&outdesc);
                break;

            default:
                break;
            }

            if (nthreads == 1)
                return 0;

            done->Post();
            continue;
        }

        return 0;
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

    if (gopts.filter == FF_NONE) {
        todraw = *data;
        // *data is assigned below, after old buf has been processed
        pixbuf1 = pixbuf2;
        pixbuf2 = todraw;
    } else
        todraw = pixbuf2;

    // FIXME: filters race condition?
    gopts.max_threads = 1;

    // First, apply filters, if applicable, in parallel, if enabled
    if (gopts.filter != FF_NONE || gopts.ifb != FF_NONE /* FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur) */) {
        if (nthreads != gopts.max_threads) {
            if (nthreads) {
                if (nthreads > 1)
                    for (int i = 0; i < nthreads; i++) {
                        threads[i].lock.Lock();
                        threads[i].src = NULL;
                        threads[i].sig.Signal();
                        threads[i].lock.Unlock();
                        threads[i].Wait();
                    }

                delete[] threads;
            }

            nthreads = gopts.max_threads;
            threads = new FilterThread[nthreads];
            // first time around, no threading in order to avoid
            // static initializer conflicts
            threads[0].threadno = 0;
            threads[0].nthreads = 1;
            threads[0].width = width;
            threads[0].height = height;
            threads[0].scale = scale;
            threads[0].src = *data;
            threads[0].dst = todraw;
            threads[0].delta = delta;
            threads[0].rpi = rpi;
            threads[0].Entry();

            // go ahead and start the threads up, though
            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].threadno = i;
                    threads[i].nthreads = nthreads;
                    threads[i].width = width;
                    threads[i].height = height;
                    threads[i].scale = scale;
                    threads[i].dst = todraw;
                    threads[i].delta = delta;
                    threads[i].rpi = rpi;
                    threads[i].done = &filt_done;
                    threads[i].lock.Lock();
                    threads[i].Create();
                    threads[i].Run();
                }
            }
        } else if (nthreads == 1) {
            threads[0].threadno = 0;
            threads[0].nthreads = 1;
            threads[0].width = width;
            threads[0].height = height;
            threads[0].scale = scale;
            threads[0].src = *data;
            threads[0].dst = todraw;
            threads[0].delta = delta;
            threads[0].rpi = rpi;
            threads[0].Entry();
        } else {
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock.Lock();
                threads[i].src = *data;
                threads[i].sig.Signal();
                threads[i].lock.Unlock();
            }

            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();
        }
    }

    // swap buffers now that src has been processed
    if (gopts.filter == FF_NONE)
        *data = pixbuf1;

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !gopts.statusbar) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        if (panel->osdstat.size())
            drawText(todraw + outstride * (systemColorDepth != 24), outstride,
                10, 20, panel->osdstat.mb_str(), showSpeedTransparent);

        if (!disableStatusMessages && !panel->osdtext.empty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                wxString message = panel->osdtext;
                int linelen = std::ceil(width * scale - 20) / 8;
                int nlines = (message.size() + linelen - 1) / linelen;
                int cury = height - 14 - nlines * 10;
                char* buf = strdup(message.mb_str());
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

            for (int off = 0; off < msg.size();) {
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
    ev.Skip(true);
}

DrawingPanelBase::~DrawingPanelBase()
{
    // pixbuf1 freed by emulator
    if (pixbuf2)
        free(pixbuf2);

    InterframeCleanup();

    if (nthreads) {
        if (nthreads > 1)
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock.Lock();
                threads[i].src = NULL;
                threads[i].sig.Signal();
                threads[i].lock.Unlock();
                threads[i].Wait();
            }

        delete[] threads;
    }
}

BasicDrawingPanel::BasicDrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanel(parent, _width, _height)
{
    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (gopts.filter == FF_NONE && gopts.ifb == IFB_NONE)
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = 24;
    if (!did_init) DrawingPanelInit();
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
    } else if (gopts.filter != FF_NONE) {
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
    } else { // 32 bit
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
    RequestHighResolutionOpenGLSurface();
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
#if defined(__WXGTK__) && defined(GLX_SGI_swap_control)
    static PFNGLXSWAPINTERVALSGIPROC si = NULL;

    if (!si)
        si = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddress((const GLubyte*)"glxSwapIntervalSGI");

    if (si)
        si(vsync);

#else
#if defined(__WXMSW__) && defined(WGL_EXT_swap_control)
    static PFNWGLSWAPINTERVALEXTPROC si = NULL;

    if (!si)
        si = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

    if (si)
        si(vsync);

#else
#ifdef __WXMAC__
    int swap_interval = vsync ? 1 : 0;
    CGLContextObj cgl_context = CGLGetCurrentContext();
    CGLSetParameter(cgl_context, kCGLCPSwapInterval, &swap_interval);
#else
//#warning no vsync support on this platform
#endif
#endif
#endif
}

void GLDrawingPanel::OnSize(wxSizeEvent& ev)
{
    AdjustViewport();

    ev.Skip(true);
}

void GLDrawingPanel::AdjustViewport()
{
#ifndef wxGL_IMPLICIT_CONTEXT
    SetCurrent(*ctx);
#else
    SetCurrent();
#endif

    int x, y;
    GetRealPixelClientSize(&x, &y);
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
static const wxString media_err(MediaRet ret)
{
    switch (ret) {
    case MRET_OK:
        return wxT("");

    case MRET_ERR_NOMEM:
        return _("memory allocation error");

    case MRET_ERR_NOCODEC:
        return _("error initializing codec");

    case MRET_ERR_FERR:
        return _("error writing to output file");

    case MRET_ERR_FMTGUESS:
        return _("can't guess output format from file name");

    default:
        //    case MRET_ERR_RECORDING:
        //    case MRET_ERR_BUFSIZE:
        return _("programming error; aborting!");
    }
}

void GameArea::StartVidRecording(const wxString& fname)
{
    // auto-conversion of wxCharBuffer to const char * seems broken
    // so save underlying wxCharBuffer (or create one of none is used)
    wxCharBuffer fnb(fname.mb_fn_str());
    MediaRet ret;

    if ((ret = vid_rec.Record(fnb.data(), basic_width, basic_height,
             systemColorDepth))
        != MRET_OK)
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
    // auto-conversion of wxCharBuffer to const char * seems broken
    // so save underlying wxCharBuffer (or create one of none is used)
    wxCharBuffer fnb(fname.mb_fn_str());
    MediaRet ret;

    if ((ret = snd_rec.Record(fnb.data())) != MRET_OK)
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
    MediaRet ret;

    if ((ret = vid_rec.AddFrame(data)) != MRET_OK) {
        wxLogError(_("Error in audio/video recording (%s); aborting"),
            media_err(ret));
        vid_rec.Stop();
    }

    if ((ret = snd_rec.AddFrame(data)) != MRET_OK) {
        wxLogError(_("Error in audio recording (%s); aborting"), media_err(ret));
        snd_rec.Stop();
    }
}

void GameArea::AddFrame(const uint8_t* data)
{
    MediaRet ret;

    if ((ret = vid_rec.AddFrame(data)) != MRET_OK) {
        wxLogError(_("Error in video recording (%s); aborting"), media_err(ret));
        vid_rec.Stop();
    }
}
#endif

void GameArea::ShowPointer()
{
    if (fullscreen)
        return;

    mouse_active_time = systemGetClock();

    if (!pointer_blanked)
        return;

    pointer_blanked = false;
    SetCursor(wxNullCursor);

    if (panel)
        panel->GetWindow()->SetCursor(wxNullCursor);
}

void GameArea::HidePointer()
{
    if (pointer_blanked)
        return;

    // FIXME: make time configurable
    if ((fullscreen || (systemGetClock() - mouse_active_time) > 3000) &&
        !(main_frame && (main_frame->MenusOpened() || main_frame->DialogOpened()))) {
        pointer_blanked = true;
        SetCursor(wxCursor(wxCURSOR_BLANK));

        // wxGTK requires that subwindows get the cursor as well
        if (panel)
            panel->GetWindow()->SetCursor(wxCursor(wxCURSOR_BLANK));
    }
}

// stub HiDPI methods, see macsupport.mm for the Mac support
#ifndef __WXMAC__
double HiDPIAware::HiDPIScaleFactor()
{
    if (hidpi_scale_factor == 0) {
        hidpi_scale_factor = 1.0;
    }

    return hidpi_scale_factor;
}

void HiDPIAware::RequestHighResolutionOpenGLSurface()
{
}

void HiDPIAware::GetRealPixelClientSize(int* x, int* y)
{
    GetWindow()->GetClientSize(x, y);
}
#endif // HiDPI stubs

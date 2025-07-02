#if defined(__APPLE__) && defined(__MACH__)
#define GL_SILENCE_DEPRECATION 1
#endif

#include "wx/wxvbam.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#ifdef __WXGTK__
    #include <X11/Xlib.h>
    #define Status int
    #include <gdk/gdkx.h>
    #include <gdk/gdkwayland.h>
    #include <gtk/gtk.h>
    // For Wayland EGL.
    #ifdef HAVE_EGL
        #include <EGL/egl.h>
    #endif
#ifdef HAVE_XSS
    #include <X11/extensions/scrnsaver.h>
#endif
#endif

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include <wx/dcbuffer.h>
#include <wx/menu.h>

#include "components/draw_text/draw_text.h"
#include "components/filters/filters.h"
#include "components/filters_agb/filters_agb.h"
#include "components/filters_interframe/interframe.h"
#include "core/base/check.h"
#include "core/base/file_util.h"
#include "core/base/image_util.h"
#include "core/base/patch.h"
#include "core/base/system.h"
#include "core/base/version.h"
#include "core/gb/gb.h"
#include "core/gb/gbCheats.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbPrinter.h"
#include "core/gb/gbSound.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaPrint.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"
#include "wx/background-input.h"
#include "wx/config/cmdtab.h"
#include "wx/config/emulated-gamepad.h"
#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/drawing.h"
#include "wx/wayland.h"
#include "wx/widgets/render-plugin.h"
#include "wx/widgets/user-input-event.h"

#ifdef __WXMSW__
#include <windows.h>
#endif

#ifdef _MSC_VER
#define strdup _strdup
#endif

namespace {

double GetFilterScale() {
    switch (OPTION(kDispFilter)) {
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
            VBAM_NOTREACHED();
            return 1.0;
    }
    VBAM_NOTREACHED();
    return 1.0;
}

long GetSampleRate() {
    switch (OPTION(kSoundAudioRate)) {
        case config::AudioRate::k48kHz:
            return 48000;
            break;
        case config::AudioRate::k44kHz:
            return 44100;
            break;
        case config::AudioRate::k22kHz:
            return 22050;
            break;
        case config::AudioRate::k11kHz:
            return 11025;
            break;
        case config::AudioRate::kLast:
            VBAM_NOTREACHED();
            return 44100;
    }
    VBAM_NOTREACHED();
    return 44100;
}

#define out_8  (systemColorDepth ==  8)
#define out_16 (systemColorDepth == 16)
#define out_24 (systemColorDepth == 24)

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
      render_observer_({config::OptionID::kDispBilinear, config::OptionID::kDispFilter,
                        config::OptionID::kDispRenderMethod, config::OptionID::kDispIFB,
                        config::OptionID::kDispStretch, config::OptionID::kPrefVsync,
                        config::OptionID::kSDLRenderer, config::OptionID::kBitDepth},
                       std::bind(&GameArea::ResetPanel, this)),
      scale_observer_(config::OptionID::kDispScale, std::bind(&GameArea::AdjustSize, this, true)),
      gb_border_observer_(config::OptionID::kPrefBorderOn,
                          std::bind(&GameArea::OnGBBorderChanged, this, std::placeholders::_1)),
      gb_palette_observer_({config::OptionID::kGBPalette0, config::OptionID::kGBPalette1,
                            config::OptionID::kGBPalette2, config::OptionID::kPrefGBPaletteOption},
                           std::bind(&gbResetPalette)),
      gb_declick_observer_(
          config::OptionID::kSoundGBDeclicking,
          [&](config::Option* option) { gbSoundSetDeclicking(option->GetBool()); }),
      lcd_filters_observer_({config::OptionID::kGBLCDFilter, config::OptionID::kGBALCDFilter},
                            std::bind(&GameArea::UpdateLcdFilter, this)),
      audio_rate_observer_(config::OptionID::kSoundAudioRate,
                           std::bind(&GameArea::OnAudioRateChanged, this)),
      audio_volume_observer_(config::OptionID::kSoundVolume,
                             std::bind(&GameArea::OnVolumeChanged, this, std::placeholders::_1)),
      audio_observer_({config::OptionID::kSoundAudioAPI, config::OptionID::kSoundAudioDevice,
                       config::OptionID::kSoundBuffers, config::OptionID::kSoundDSoundHWAccel,
                       config::OptionID::kSoundUpmix},
                      [&](config::Option*) { schedule_audio_restart_ = true; }) {
    SetSizer(new wxBoxSizer(wxVERTICAL));
    // all renderers prefer 32-bit
    // well, "simple" prefers 24-bit, but that's not available for filters
    systemColorDepth = (OPTION(kBitDepth) + 1) << 3;

    hq2x_init(32);
    Init_2xSaI(32);
}

void GameArea::LoadGame(const wxString& name)
{
    rom_scene_rls = "-";
    rom_scene_rls_name = "-";
    rom_name = "";
    // fex just crashes if file does not exist and it's compressed,
    // so check first
    wxFileName fnfn(name);
    bool badfile = !fnfn.IsFileReadable();

    // if path was relative, look for it before giving up
    if (badfile && !fnfn.IsAbsolute()) {
        wxString rp = fnfn.GetPath();

        // can't really decide which dir to use, so try GBA first, then GB
        const wxString gba_rom_dir = OPTION(kGBAROMDir);
        if (!wxGetApp().GetAbsolutePath(gba_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gba_rom_dir) + '/' + rp);
            badfile = !fnfn.IsFileReadable();
        }

        const wxString gb_rom_dir = OPTION(kGBROMDir);
        if (badfile && !wxGetApp().GetAbsolutePath(gb_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gb_rom_dir) + '/' + rp);
            badfile = !fnfn.IsFileReadable();
        }

        const wxString gbc_rom_dir = OPTION(kGBGBCROMDir);
        if (badfile && !wxGetApp().GetAbsolutePath(gbc_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gbc_rom_dir) + '/' + rp);
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
        wxConfigBase* cfg = wxConfigBase::Get();

        if (!OPTION(kGenFreezeRecent)) {
            gopts.recent->AddFileToHistory(name);
            wxGetApp().frame->ResetRecentAccelerators();
            cfg->SetPath("/Recent");
            gopts.recent->Save(*cfg);
            cfg->SetPath("/");
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
    bool loadpatch = OPTION(kPrefAutoPatch);
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

        if (loadpatch) {
            gbApplyPatch(UTF8(pfn.GetFullPath()));
        }

        // Apply overrides.
        wxFileConfig* cfg = wxGetApp().gb_overrides_.get();
        const std::string& title = g_gbCartData.title();
        if (cfg->HasGroup(title)) {
            cfg->SetPath(title);
            coreOptions.gbPrinterEnabled = cfg->Read("gbPrinter", coreOptions.gbPrinterEnabled);
            cfg->SetPath("/");
        }

        // start sound; this must happen before CPU stuff
        gb_effects_config.enabled = OPTION(kSoundGBEnableEffects);
        gb_effects_config.surround = OPTION(kSoundGBSurround);
        gb_effects_config.echo = (float)OPTION(kSoundGBEcho) / 100.0;
        gb_effects_config.stereo = (float)OPTION(kSoundGBStereo) / 100.0;
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        soundSetEnable(gopts.sound_en);
        gbSoundSetSampleRate(GetSampleRate());
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(coreOptions.throttle);
        gbGetHardwareType();


        // Disable bios loading when using colorizer hack.
        if (OPTION(kPrefUseBiosGB) && OPTION(kGBColorizerHack)) {
            wxLogError(_("Cannot use Game Boy BIOS file when Colorizer Hack is enabled, disabling Game Boy BIOS file."));
            OPTION(kPrefUseBiosGB) = false;
        }

        // Set up the core for the colorizer hack.
        setColorizerHack(OPTION(kGBColorizerHack));

        const bool use_bios = gbCgbMode ? OPTION(kPrefUseBiosGBC).Get()
                                        : OPTION(kPrefUseBiosGB).Get();

        const wxString bios_file = gbCgbMode ? OPTION(kGBGBCBiosFile).Get() : OPTION(kGBBiosFile).Get();
        gbCPUInit(bios_file.To8BitData().data(), use_bios);

        if (use_bios && !coreOptions.useBios) {
            wxLogError(_("Could not load BIOS %s"), bios_file);
            // could clear use flag & file name now, but better to force
            // user to do it
        }

        gbReset();

        if (OPTION(kPrefBorderOn)) {
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

        rom_crc32 = crc32(0L, g_rom, rom_size);

        if (loadpatch) {
            // don't use real rom size or it might try to resize rom[]
            // instead, use known size of rom[]
            int size = 0x2000000 < rom_size ? 0x2000000 : rom_size;
            applyPatch(UTF8(pfn.GetFullPath()), &g_rom, &size);
            // that means we no longer really know rom_size either <sigh>

            gbaUpdateRomSize(size);
        }

        wxFileConfig* cfg = wxGetApp().overrides_.get();
        wxString id = wxString((const char*)&g_rom[0xac], wxConvLibc, 4);

        if (cfg->HasGroup(id)) {
            cfg->SetPath(id);
            bool enable_rtc = cfg->Read(wxT("rtcEnabled"), coreOptions.rtcEnabled);

            rtcEnable(enable_rtc);

            int fsz = cfg->Read(wxT("flashSize"), (long)0);

            if (fsz != 0x10000 && fsz != 0x20000)
                fsz = 0x10000 << OPTION(kPrefFlashSize);

            flashSetSize(fsz);
            ovSaveType = cfg->Read(wxT("coreOptions.saveType"), coreOptions.cpuSaveType);

            if (ovSaveType < 0 || ovSaveType > 5)
                ovSaveType = 0;

            if (ovSaveType == 0)
                flashDetectSaveType(rom_size);
            else
                coreOptions.saveType = ovSaveType;

            coreOptions.mirroringEnable = cfg->Read(wxT("mirroringEnabled"), (long)1);
            cfg->SetPath(wxT("/"));
        } else {
            rtcEnable(coreOptions.rtcEnabled);
            flashSetSize(0x10000 << OPTION(kPrefFlashSize));

            if (coreOptions.cpuSaveType < 0 || coreOptions.cpuSaveType > 5)
                coreOptions.cpuSaveType = 0;

            if (coreOptions.cpuSaveType == 0)
                flashDetectSaveType(rom_size);
            else
                coreOptions.saveType = coreOptions.cpuSaveType;

            coreOptions.mirroringEnable = false;
        }

        doMirroring(coreOptions.mirroringEnable);
        // start sound; this must happen before CPU stuff
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        soundSetEnable(gopts.sound_en);
        soundSetSampleRate(GetSampleRate());
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(coreOptions.throttle);
        soundFiltering = (float)OPTION(kSoundGBAFiltering) / 100.0f;

        rtcEnableRumble(true);

        CPUInit(UTF8(gopts.gba_bios), OPTION(kPrefUseBiosGBA));

        if (OPTION(kPrefUseBiosGBA) && !coreOptions.useBios) {
            wxLogError(_("Could not load BIOS %s"), gopts.gba_bios.mb_str());
            // could clear use flag & file name now, but better to force
            // user to do it
        }

        CPUReset();
        basic_width = GBAWidth;
        basic_height = GBAHeight;
        emusys = &GBASystem;
    }

    // Set sound volume.
    soundSetVolume((float)OPTION(kSoundVolume) / 100.0);

    if (OPTION(kGeomFullScreen)) {
        GameArea::ShowFullScreen(true);
    }

    loaded = t;
    SetFrameTitle();
    SetFocus();
    // Use custom geometry
    AdjustSize(false);
    emulating = true;
    was_paused = true;
    schedule_audio_restart_ = false;
    MainFrame* mf = wxGetApp().frame;
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

    SuspendScreenSaver();

    // probably only need to do this for GB carts
    if (coreOptions.gbPrinterEnabled)
        gbSerialFunction = gbPrinterSend;

    // probably only need to do this for GBA carts
    agbPrintEnable(OPTION(kPrefAgbPrint));

    // set frame skip based on ROM type
    const int frame_skip = OPTION(kPrefFrameSkip);
    if (frame_skip != -1) {
        systemFrameSkip = frame_skip;
    }

    // load battery and/or saved state
    recompute_dirs();
    mf->update_state_ts(true);
    bool did_autoload = OPTION(kGenAutoLoadLastState) ? LoadState() : false;

    if (!did_autoload || coreOptions.skipSaveGameBattery) {
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

            if (coreOptions.cpuSaveType == 0 && ovSaveType == 0 && t == IMAGE_GBA) {
                switch (bat.GetSize().GetValue()) {
                case 0x200:
                case 0x2000:
                    coreOptions.saveType = GBA_SAVE_EEPROM;
                    break;

                case 0x8000:
                    coreOptions.saveType = GBA_SAVE_SRAM;
                    break;

                case 0x10000:
                    if (coreOptions.saveType == GBA_SAVE_EEPROM || coreOptions.saveType == GBA_SAVE_SRAM)
                        break;
                    break;

                case 0x20000:
                    coreOptions.saveType = GBA_SAVE_FLASH;
                    flashSetSize(bat.GetSize().GetValue());
                    break;

                default:
                    break;
                }

                SetSaveType(coreOptions.saveType);
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
    cheats_dirty = (did_autoload && !coreOptions.skipSaveGameCheats) || (loaded == IMAGE_GB ? gbCheatNumber > 0 : cheatsNumber > 0);

    if (OPTION(kPrefAutoSaveLoadCheatList) && (!did_autoload || coreOptions.skipSaveGameCheats)) {
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
    if (OPTION(kGBALinkAuto)) {
        BootLink(mf->GetConfiguredLinkMode(), UTF8(gopts.link_host),
                 gopts.link_timeout, OPTION(kGBALinkFast),
                 gopts.link_num_players);
    }
#endif

#if defined(VBAM_ENABLE_DEBUGGER)
    if (OPTION(kPrefGDBBreakOnLoad)) {
        mf->GDBBreak();
    }
#endif  // defined(VBAM_ENABLE_DEBUGGER)
}

void GameArea::SetFrameTitle()
{
    wxString tit = wxT("");

    if (loaded != IMAGE_UNKNOWN) {
        tit.append(loaded_game.GetFullName());
        tit.append(wxT(" - "));
    }

    tit.append(wxT("VisualBoyAdvance-M "));
    tit.append(kVbamVersion);

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
    batdir = OPTION(kGenBatteryDir);

    if (batdir.empty()) {
        batdir = loaded_game.GetPathWithSep();
    } else {
        batdir = wxGetApp().GetAbsolutePath(batdir);
    }

    if (!wxIsWritable(batdir)) {
        batdir = wxGetApp().GetDataDir();
    }

    statedir = OPTION(kGenStateDir);

    if (statedir.empty()) {
        statedir = loaded_game.GetPathWithSep();
    } else {
        statedir = wxGetApp().GetAbsolutePath(statedir);
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
    if (OPTION(kPrefAutoSaveLoadCheatList) && cheats_dirty) {
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

#if defined(VBAM_ENABLE_DEBUGGER)
    debugger = false;
    remoteCleanUp();
    mf->cmd_enable |= CMDEN_NGDB_ANY;
#endif  // VBAM_ENABLE_DEBUGGER

    if (loaded == IMAGE_GB) {
        gbCleanUp();
        gbCheatRemoveAll();

        // Reset overrides.
        coreOptions.gbPrinterEnabled = OPTION(kPrefGBPrinter);
    } else if (loaded == IMAGE_GBA) {
        CPUCleanUp();
        cheatsDeleteAll(false);
    }

    UnsuspendScreenSaver();
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
    const double display_scale = OPTION(kDispScale);

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
    const double display_scale = OPTION(kDispScale);

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
                wxLogInfo(_("Fullscreen mode %d x %d - %d @ %d not supported; looking for another"),
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
                    wxLogWarning(_("Fullscreen mode %d x %d - %d @ %d not supported"),
                        gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                    gopts.fs_mode = wxVideoMode();

                    for (i = 0; i < vm.size(); i++)
                        wxLogInfo(_("Valid mode: %d x %d - %d @ %d"), vm[i].w,
                            vm[i].h, vm[i].bpp,
                            vm[i].refresh);
                } else {
                    gopts.fs_mode.bpp = vm[i].bpp;
                    gopts.fs_mode.refresh = vm[i].refresh;
                }

                wxLogInfo(_("Chose mode %d x %d - %d @ %d"),
                    gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);

                if (!d.ChangeMode(gopts.fs_mode)) {
                    wxLogWarning(_("Failed to change mode to %d x %d - %d @ %d"),
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
    wxGetApp().emulated_gamepad()->Reset();
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
    UnsuspendScreenSaver();

    // when the game is paused like this, we should not allow any
    // input to remain pressed, because they could be released
    // outside of the game zone and we would not know about it.
    wxGetApp().emulated_gamepad()->Reset();

    if (loaded != IMAGE_UNKNOWN)
        soundPause();
}

void GameArea::Resume()
{
    if (!paused)
        return;

    paused = false;
    SuspendScreenSaver();
    SetExtraStyle(GetExtraStyle() | wxWS_EX_PROCESS_IDLE);

    if (loaded != IMAGE_UNKNOWN)
        soundResume();

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

#if defined(VBAM_ENABLE_DEBUGGER)
        if (OPTION(kPrefGDBBreakOnLoad)) {
            mf->GDBBreak();
        }

        if (debugger && loaded != IMAGE_GBA) {
            wxLogError(_("Not a valid Game Boy Advance cartridge"));
            UnloadGame();
        }
#endif  // defined(VBAM_ENABLE_DEBUGGER)
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

    if (schedule_audio_restart_) {
        soundShutdown();
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        schedule_audio_restart_ = false;
    }

    if (!panel) {
        switch (OPTION(kDispRenderMethod)) {
            case config::RenderMethod::kSimple:
                no_border = false;

                panel = new BasicDrawingPanel(this, basic_width, basic_height);
                break;
            case config::RenderMethod::kSDL:
                no_border = false;

                panel = new SDLDrawingPanel(this, basic_width, basic_height);
                break;
#ifdef __WXMAC__
#ifndef NO_METAL
            case config::RenderMethod::kMetal:
                no_border = false;

                if (is_macosx_1012_or_newer()) {
                    panel =
                    new MetalDrawingPanel(this, basic_width, basic_height);
                } else {
                    panel = new GLDrawingPanel(this, basic_width, basic_height);
                    log("Metal is unavailable, defaulting to OpenGL");
                }
                break;
#endif
            case config::RenderMethod::kQuartz2d:
                no_border = false;

                panel =
                    new Quartz2DDrawingPanel(this, basic_width, basic_height);
                break;
#endif
#ifndef NO_OGL
            case config::RenderMethod::kOpenGL:
                if (out_8) {
                    no_border = true;
                } else {
                    no_border = false;
                }

                panel = new GLDrawingPanel(this, basic_width, basic_height);
                break;
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
            case config::RenderMethod::kDirect3d:
                no_border = false;

                panel = new DXDrawingPanel(this, basic_width, basic_height);
                break;
#endif
            case config::RenderMethod::kLast:
                VBAM_NOTREACHED();
                return;
        }

        wxWindow* w = panel->GetWindow();

        // set up event handlers
        w->Bind(VBAM_EVT_USER_INPUT, &GameArea::OnUserInput, this);
        w->Bind(wxEVT_PAINT, &GameArea::PaintEv, this);
        w->Bind(wxEVT_ERASE_BACKGROUND, &GameArea::EraseBackground, this);

        // set userdata so we know it's the panel and not the frame being resized
        // the userdata is freed on disconnect/destruction
        this->Bind(wxEVT_SIZE, &GameArea::OnSize, this);

        // We need to check if the buttons stayed pressed when focus the panel.
        w->Bind(wxEVT_KILL_FOCUS, &GameArea::OnKillFocus, this);

        // Update mouse last-used timers on mouse events etc..
        w->Bind(wxEVT_MOTION, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_LEFT_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_RIGHT_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_MIDDLE_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_MOUSEWHEEL, &GameArea::MouseEvent, this);

        w->SetBackgroundStyle(wxBG_STYLE_CUSTOM);
        w->SetSize(wxSize(basic_width, basic_height));

        // Allow input while on background
        if (OPTION(kUIAllowKeyboardBackgroundInput)) {
            enableKeyboardBackgroundInput(w->GetEventHandler());
        }

        if (gopts.max_scale)
            w->SetMaxSize(wxSize(basic_width * gopts.max_scale,
                                 basic_height * gopts.max_scale));

        // if user changed Display/Scale config, this needs to run
        AdjustMinSize();
        AdjustSize(false);

        const bool retain_aspect = OPTION(kDispStretch);
        const unsigned frame_priority = retain_aspect ? 0 : 1;

        GetSizer()->Clear();

        // add spacers on top and bottom to center panel vertically
        // but not on 2.8 which does not handle this correctly
        if (retain_aspect) {
            GetSizer()->Add(0, 0, wxEXPAND);
        }

        // this triggers an assertion dialog in <= 3.1.2 in debug mode
        GetSizer()->Add(
            w, frame_priority,
            retain_aspect ? (wxSHAPED | wxALIGN_CENTER | wxEXPAND) : wxEXPAND);

        if (retain_aspect) {
            GetSizer()->Add(0, 0, wxEXPAND);
        }

        Layout();
        SendSizeEvent();

        if (pointer_blanked)
            w->SetCursor(wxCursor(wxCURSOR_BLANK));

        // set focus to panel
        w->SetFocus();

        // generate system color maps (after output module init)
        UpdateLcdFilter();
    }

    if (!paused) {
        HidePointer();
        HideMenuBar();
        event.RequestMore();

#if defined(VBAM_ENABLE_DEBUGGER)
        if (debugger) {
            was_paused = true;
            dbgMain();

            if (!emulating) {
                emulating = true;
                SuspendScreenSaver();
                UnloadGame();
            }

            return;
        }
#endif  // defined(VBAM_ENABLE_DEBUGGER)

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

static void draw_black_background(wxWindow* win) {
    wxClientDC dc(win);
    wxCoord w, h;
    dc.GetSize(&w, &h);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, w, h);
}

#ifdef __WXGTK__
static Display* GetX11Display() {
    return GDK_WINDOW_XDISPLAY(gtk_widget_get_window(wxGetApp().frame->GetHandle()));
}
#endif  // __WXGTK__

void GameArea::OnUserInput(widgets::UserInputEvent& event) {
    bool emulated_key_pressed = false;
    for (const auto& event_data : event.data()) {
        if (event_data.pressed) {
            if (wxGetApp().emulated_gamepad()->OnInputPressed(event_data.input)) {
                emulated_key_pressed = true;
            }
        } else {
            if (wxGetApp().emulated_gamepad()->OnInputReleased(event_data.input)) {
                emulated_key_pressed = true;
            }
        }
    }

    if (emulated_key_pressed) {
        wxWakeUpIdle();

#if defined(__WXGTK__) && defined(HAVE_X11) && !defined(HAVE_XSS)
        // Tell X11 to turn off the screensaver/screen-blank if a button was
        // was pressed. This shouldn't be necessary.
        if (!wxGetApp().UsingWayland()) {
            auto display = GetX11Display();
            XResetScreenSaver(display);
            XFlush(display);
        }
#endif
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
    draw_black_background(this);
    Layout();

    // panel may resize
    if (panel)
        panel->OnSize(ev);
    Layout();

    ev.Skip();
}

BEGIN_EVENT_TABLE(GameArea, wxPanel)
EVT_IDLE(GameArea::OnIdle)
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

    if (OPTION(kDispFilter) == config::Filter::kPlugin) {
        rpi_ = widgets::MaybeLoadFilterPlugin(OPTION(kDispFilterPlugin),
                                              &filter_plugin_);
        if (rpi_) {
            rpi_->Flags &= ~RPI_565_SUPP;

            if (rpi_->Flags & RPI_888_SUPP) {
                rpi_->Flags &= ~RPI_555_SUPP;
                // FIXME: should this be 32 or 24?  No docs or sample source
                systemColorDepth = 32;
            } else if (rpi_->Flags & RPI_555_SUPP) {
                rpi_->Flags &= ~RPI_888_SUPP;
                systemColorDepth = 16;
            } else {
                rpi_->Flags &= ~RPI_888_SUPP;
                rpi_->Flags &= ~RPI_555_SUPP;
                systemColorDepth = 8;
            }

            if (!rpi_->Output) {
                // note that in actual kega fusion plugins, rpi_->Output is
                // unused (as is rpi_->Handle)
                rpi_->Output =
                    (RENDPLUG_Output)filter_plugin_.GetSymbol("RenderPluginOutput");
            }
            scale *= (rpi_->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
        } else {
            // This is going to delete the object. Do nothing more here.
            OPTION(kDispFilterPlugin) = wxEmptyString;
            OPTION(kDispFilter) = config::Filter::kNone;
            return;
        }
    }

    if (OPTION(kDispFilter) != config::Filter::kPlugin) {
        scale *= GetFilterScale();

        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    // Intialize color tables
    if (systemColorDepth == 24) {
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
    } else if (systemColorDepth == 32) {
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
        const int height_real = height_;
        const int procy = height_ * threadno_ / nthreads_;
        height_ = height_ * (threadno_ + 1) / nthreads_ - procy;
        const int inbpp = systemColorDepth >> 3;
        const int inrb = out_8 ? 2 : out_16  ? 2
                         : out_24 ? 0 : 1;
        const int instride = (width_ + inrb) * inbpp;
        const int instride32 = width_ * 4;
        const int outbpp = systemColorDepth >> 3;
        const int outrb = out_8 ? 2 : out_24 ? 0 : 4;
        const int outstride = std::ceil(width_ * outbpp * scale_) + outrb;
        const int outstride32 = std::ceil(width_ * 4 * scale_);
        uint8_t *dest = NULL;
        int pos = 0;
        uint32_t *src2_ = NULL;
        uint32_t *dst2_ = NULL;
        delta_ += instride * procy;

        // FIXME: fugly hack
        if (OPTION(kDispRenderMethod) == config::RenderMethod::kOpenGL) {
            dst_ += (int)std::ceil(outstride * (procy + 1) * scale_);
        } else if (OPTION(kDispRenderMethod) == config::RenderMethod::kSDL) {
            dst_ += (int)std::ceil(outstride * (procy + 1) * scale_);
        } else {
            dst_ += (int)std::ceil(outstride * (procy + (1 / scale_)) * scale_);
        }

        dest = dst_;

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

            if (OPTION(kDispFilter) == config::Filter::kNone) {
                if (nthreads_ == 1)
                    return 0;

                done_->Post();
                continue;
            }

            // src += instride * procy;

            // naturally, any of these with accumulation buffers like those
            // of the IFB filters will screw up royally as well
            if (systemColorDepth == 32) {
                ApplyFilter(instride, outstride);
            } else {
                src2_ = (uint32_t *)calloc(4, std::ceil(width_ * height_real));
                dst2_ = (uint32_t *)calloc(4, std::ceil((width_ * scale_) * (height_real * scale_)));

                if (out_8) {
                    int src_pos = 0;
                    for (int y = 0; y < height_real; y++) {
                        for (int x = 0; x < width_; x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            src2_[pos] = (src_[src_pos] & 0xe0) + ((src_[src_pos] & 0x1c) << 11) + ((src_[src_pos] & 0x3) << 22);
#else
                            src2_[pos] = ((src_[src_pos] & 0xe0) << 24) + ((src_[src_pos] & 0x1c) << 19) + ((src_[src_pos] & 0x3) << 14);
#endif
                            pos++;
                            src_pos++;
                        }
                        src_pos += 2;
                    }
                } else if (out_16) {
                    uint16_t *src16_ = (uint16_t *)src_;
                    int src_pos = 0;
                    for (int y = 0; y < height_real; y++) {
                        for (int x = 0; x < width_; x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            src2_[pos] = ((src16_[src_pos] & 0x7c00) >> 7) + ((src16_[src_pos] & 0x03e0) << 6) + ((src16_[src_pos] & 0x1f) << 19);
#else
                            src2_[pos] = ((src16_[src_pos] & 0x7c00) << 17) + ((src16_[src_pos] & 0x03e0) << 14) + ((src16_[src_pos] & 0x1f) << 11);
#endif
                            pos++;
                            src_pos++;
                        }
                        src_pos += 2;
                    }
                } else if (out_24) {
                    int src_pos = 0;
                    uint8_t *src8_ = (uint8_t *)src2_;
                    for (int y = 0; y < height_real; y++) {
                        for (int x = 0; x < width_; x++) {
                            src8_[pos] = src_[src_pos];
                            src8_[pos+1] = src_[src_pos+1];
                            src8_[pos+2] = src_[src_pos+2];
                            src8_[pos+3] = 0;

                            pos += 4;
                            src_pos += 3;
                        }
                    }
                }

                src_ = (uint8_t *)src2_;
                dst_ = (uint8_t *)dst2_;

                ApplyFilter(instride32, outstride32);

                dst_ = (uint8_t *)dst2_;

                if (out_8) {
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < (height_real * scale_); y++) {
                        for (int x = 0; x < (width_ * scale_); x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            dest[pos] = (uint8_t)(((dst_[dst_pos] >> 5) << 5) + ((dst_[dst_pos+1] >> 5) << 2) + ((dst_[dst_pos+2] >> 6) & 0x3));
#else
                            dest[pos] = (uint8_t)(((dst_[dst_pos+3] >> 5) << 5) + ((dst_[dst_pos+2] >> 5) << 2) + ((dst_[dst_pos+1] >> 6) & 0x3));
#endif
                            pos++;
                            dst_pos += 4;
                        }
                        pos += 2;
                    }
                } else if (out_16) {
                    uint16_t *dest16_ = (uint16_t *)dest;
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < (height_real * scale_); y++) {
                        for (int x = 0; x < (width_ * scale_); x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            dest16_[pos] = (uint16_t)(((dst_[dst_pos] >> 3) << 10) + ((dst_[dst_pos+1] >> 3) << 5) + (dst_[dst_pos+2] >> 3));
#else
                            dest16_[pos] = (uint16_t)(((dst_[dst_pos+3] >> 3) << 10) + ((dst_[dst_pos+2] >> 3) << 5) + (dst_[dst_pos+1] >> 3));
#endif
                            pos++;
                            dst_pos += 4;
                        }
                        pos += 2;
                    }
                } else if (out_24) {
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < (height_real * scale_); y++) {
                        for (int x = 0; x < (width_ * scale_); x++) {
                            dest[pos] = dst_[dst_pos];
                            dest[pos+1] = dst_[dst_pos+1];
                            dest[pos+2] = dst_[dst_pos+2];
                            pos += 3;
                            dst_pos += 4;
                        }
                    }
                }

                if (src2_ != NULL) {
                    free(src2_);
                }

                if (dst2_ != NULL) {
                    free(dst2_);
                }
            }

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
        switch (OPTION(kDispIFB)) {
            case config::Interframe::kNone:
                break;

            case config::Interframe::kSmart:
                if (out_8) {
                    SmartIB8(src_, instride, width_, procy, height_);
                } else if (out_16) {
                    SmartIB(src_, instride, width_, procy, height_);
                } else if (out_24) {
                    SmartIB24(src_, instride, width_, procy, height_);
                } else {
                    SmartIB32(src_, instride, width_, procy, height_);
                }
                break;

            case config::Interframe::kMotionBlur:
                // FIXME: if(renderer == d3d/gl && filter == NONE) break;
                if (out_8) {
                    MotionBlurIB8(src_, instride, width_, procy, height_);
                } else if (out_16) {
                    MotionBlurIB(src_, instride, width_, procy, height_);
                } else if (out_24) {
                    MotionBlurIB24(src_, instride, width_, procy, height_);
                } else {
                    MotionBlurIB32(src_, instride, width_, procy, height_);
                }
                break;

            case config::Interframe::kLast:
                VBAM_NOTREACHED();
                break;
        }
    }

    // naturally, any of these with accumulation buffers like those
    // of the IFB filters will screw up royally as well
    void ApplyFilter(int instride, int outstride) {
        switch (OPTION(kDispFilter)) {
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
                VBAM_NOTREACHED();
                break;
        }
    }
};

void DrawingPanelBase::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = systemColorDepth >> 3;
    int outrb = out_8 ? 2 : out_24 ? 0 : 4;
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

    if (OPTION(kDispFilter) == config::Filter::kNone) {
        todraw = *data;
        // *data is assigned below, after old buf has been processed
        pixbuf1 = pixbuf2;
        pixbuf2 = todraw;
    } else
        todraw = pixbuf2;

    // FIXME: filters race condition?
    const int max_threads = 1;

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
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

            nthreads = max_threads;
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
    if (OPTION(kDispFilter) == config::Filter::kNone) {
        *data = pixbuf1;
    }

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        if (panel->osdstat.size())
            drawText(todraw + outstride * (systemColorDepth != 24), outstride,
                10, 20, UTF8(panel->osdstat), OPTION(kPrefShowSpeedTransparent));

        if (!panel->osdtext.empty()) {
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
                        OPTION(kPrefShowSpeedTransparent));
                    cury += 10;
                    nlines--;
                    ptr += linelen;
                    *ptr = lchar;
                }

                drawText(todraw + outstride * (systemColorDepth != 24),
                    outstride, 10, cury, ptr,
                    OPTION(kPrefShowSpeedTransparent));

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
    dc.SetTextForeground(wxColour(255, 0, 0, OPTION(kPrefShowSpeedTransparent) ? 128 : 255));
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

    if (!OPTION(kPrefDisableStatus) && !panel->osdtext.empty()) {
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

SDLDrawingPanel::SDLDrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanel(parent, _width, _height)
{
    memset(delta, 0xff, sizeof(delta));

    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    DrawingPanelInit();
}

SDLDrawingPanel::~SDLDrawingPanel()
{
    if (did_init)
    {
        if (sdlwindow != NULL)
             SDL_DestroyWindow(sdlwindow);

        if (texture != NULL)
             SDL_DestroyTexture(texture);

        if (renderer != NULL)
             SDL_DestroyRenderer(renderer);

        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        did_init = false;
    }
}

void SDLDrawingPanel::EraseBackground(wxEraseEvent& ev)
{
    (void)ev; // unused params
    // do nothing, do not allow propagation
}

void SDLDrawingPanel::PaintEv(wxPaintEvent& ev)
{
    // FIXME: implement
    if (!did_init) {
        DrawingPanelInit();
    }

    (void)ev; // unused params

    if (!todraw) {
        // since this is set for custom background, not drawing anything
        // will cause garbage to be displayed, so draw a black area
        draw_black_background(GetWindow());
        return;
    }

    if (todraw) {
        DrawArea();
    }
}

void SDLDrawingPanel::DrawingPanelInit()
{
    wxString renderer_name = OPTION(kSDLRenderer);

#ifdef ENABLE_SDL3
    SDL_PropertiesID props = SDL_CreateProperties();
#endif
#ifdef __WXGTK__
    GtkWidget *widget = wxGetApp().frame->GetPanel()->GetHandle();
    gtk_widget_realize(widget);
    XID xid = 0;
    struct wl_surface *wayland_surface = NULL;
    struct wl_display *wayland_display = NULL;

#ifdef ENABLE_SDL3
    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        wayland_display = gdk_wayland_display_get_wl_display(gtk_widget_get_display(widget));
        wayland_surface = gdk_wayland_window_get_wl_surface(gtk_widget_get_window(widget));

        if (SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, wayland_display) == false) {
            systemScreenMessage(_("Failed to set wayland display"));
            return;
        }
    } else {
#endif
        xid = GDK_WINDOW_XID(gtk_widget_get_window(widget));
#ifdef ENABLE_SDL3
    }
#endif

#ifdef ENABLE_SDL3
    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    } else {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    }
#else
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");
#endif
#endif

    DrawingPanel::DrawingPanelInit();

#ifdef ENABLE_SDL3
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false) {
#else
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
#endif
        systemScreenMessage(_("Failed to initialize SDL video subsystem"));
        return;
    }

#ifdef ENABLE_SDL3
    if (SDL_WasInit(SDL_INIT_VIDEO) == false) {
        if (SDL_Init(SDL_INIT_VIDEO) == false) {
#else
    if (SDL_WasInit(SDL_INIT_VIDEO) < 0) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
#endif
            systemScreenMessage(_("Failed to initialize SDL video"));
            return;
        }
    }

#ifdef ENABLE_SDL3
#ifdef __WXGTK__
    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, wayland_surface) == false) {
            systemScreenMessage(_("Failed to set wayland surface"));
            return;
        }
    } else {
        if (SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, xid) == false)
#elif defined(__WXMAC__)
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER, wxGetApp().frame->GetPanel()->GetHandle()) == false)
#elif defined(__WXMSW__)
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, GetHWND()) == false)
#else
        if (SDL_SetPointerProperty(props, "sdl2-compat.external_window", GetWindow()->GetHandle()) == false)
#endif
        {
            systemScreenMessage(_("Failed to set parent window"));
            return;
        }

#ifdef __WXGTK__
    }
#endif

    if (SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true) == false) {
        systemScreenMessage(_("Failed to set OpenGL properties"));
    }

#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_USE_D3D9EX, false);
#endif

    sdlwindow = SDL_CreateWindowWithProperties(props);

    if (sdlwindow == NULL) {
        systemScreenMessage(_("Failed to create SDL window"));
        return;
    }

    if (props != NULL)
        SDL_DestroyProperties(props);
            
    if (OPTION(kSDLRenderer) == wxString("default")) {
        renderer = SDL_CreateRenderer(sdlwindow, NULL);
        log("SDL renderer: default");
    } else {
        renderer = SDL_CreateRenderer(sdlwindow, renderer_name.mb_str());
        log("SDL renderer: %s", (const char *)renderer_name.mb_str());

        if (renderer == NULL) {
            log("ERROR: Renderer creating failed, using default renderer");
            printf("SDL Error: %s\n", SDL_GetError());

            renderer = SDL_CreateRenderer(sdlwindow, NULL);
        }
    }
            
    if (renderer == NULL) {
        systemScreenMessage(_("Failed to create SDL renderer"));
        return;
    }
#else
#ifdef __WXGTK__
    sdlwindow = SDL_CreateWindowFrom((void *)xid);
#elif defined(__WXMAC__)
    sdlwindow = SDL_CreateWindowFrom(wxGetApp().frame->GetPanel()->GetHandle());
#else
    sdlwindow = SDL_CreateWindowFrom(GetWindow()->GetHandle());
#endif
            
    if (sdlwindow == NULL) {
        systemScreenMessage(_("Failed to create SDL window"));
        return;
    }
            
    if (OPTION(kSDLRenderer) == wxString("default")) {
        renderer = SDL_CreateRenderer(sdlwindow, -1, 0);
        log("SDL renderer: default");
    } else {
        for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
            wxString renderer_name = OPTION(kSDLRenderer);
            SDL_RendererInfo render_info;
                    
            SDL_GetRenderDriverInfo(i, &render_info);
                    
            if (!strcmp(renderer_name.mb_str(), render_info.name)) {
                if (!strcmp(render_info.name, "software")) {
                    renderer = SDL_CreateRenderer(sdlwindow, i, SDL_RENDERER_SOFTWARE);
                } else {
                    renderer = SDL_CreateRenderer(sdlwindow, i, SDL_RENDERER_ACCELERATED);
                }
                        
                log("SDL renderer: %s", render_info.name);
            }
        }
                
        if (renderer == NULL) {
            log("ERROR: Renderer creating failed, using default renderer");
            renderer = SDL_CreateRenderer(sdlwindow, -1, 0);
        }
    }
            
    if (renderer == NULL) {
        systemScreenMessage(_("Failed to create SDL renderer"));
        return;
    }
#endif

    if (out_8) {
#ifdef ENABLE_SDL3
        texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(8, 0xE0, 0x1C, 0x03, 0x00), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
#else
        texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(8, 0xE0, 0x1C, 0x03, 0x00), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
#endif
    } else if (out_16) {
#ifdef ENABLE_SDL3
        if (OPTION(kSDLRenderer) == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(16, 0x7C00, 0x03E0, 0x001F, 0x0000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (OPTION(kSDLRenderer) == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(16, 0x7C00, 0x03E0, 0x001F, 0x0000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    } else if (out_24) {
#ifdef ENABLE_SDL3
        texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
#else
        texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
#endif
    } else {
#ifdef ENABLE_SDL3
        if (OPTION(kSDLRenderer) == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (OPTION(kSDLRenderer) == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    }
            
    did_init = true;
}
        
void SDLDrawingPanel::OnSize(wxSizeEvent& ev)
{
    if (todraw) {
        DrawArea();
    }
            
    ev.Skip();
}
        
void SDLDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc;
    DrawArea();
}

void SDLDrawingPanel::DrawArea()
{
    uint32_t srcPitch = 0;
    uint32_t *todraw_argb = NULL;
    uint16_t *todraw_rgb565 = NULL;

    if (!did_init)
        DrawingPanelInit();
            
    if (out_8) {
        srcPitch = std::ceil(width * scale) + 2;
    } else if (out_16) {
        srcPitch = std::ceil(width * scale * 2) + 4;
    } else if (out_24) {
        srcPitch = std::ceil(width * scale * 3);
    } else {
        srcPitch = std::ceil(width * scale * 4) + 4;
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);

    if ((OPTION(kSDLRenderer) == wxString("direct3d")) && (systemColorDepth == 32)) {
        todraw_argb = (uint32_t *)(todraw + srcPitch);
            
        for (int i = 0; i < (height * scale); i++) {
            for (int j = 0; j < (width * scale); j++) {
                todraw_argb[j + (i * (srcPitch / 4))] = 0xFF000000 | ((todraw_argb[j + (i * (srcPitch / 4))] & 0xFF) << 16) | (todraw_argb[j + (i * (srcPitch / 4))] & 0xFF00) | ((todraw_argb[j + (i * (srcPitch / 4))] & 0xFF0000) >> 16);
            }
        }

        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw_argb, srcPitch);
    } else if ((OPTION(kSDLRenderer) == wxString("direct3d")) && (systemColorDepth == 16)) {
        todraw_rgb565 = (uint16_t *)(todraw + srcPitch);

        for (int i = 0; i < (height * scale); i++) {
            for (int j = 0; j < (width * scale); j++) {
                todraw_rgb565[j + (i * (srcPitch / 2))] = ((todraw_rgb565[j + (i * (srcPitch / 2))] & 0x7FE0) << 1) | (todraw_rgb565[j + (i * (srcPitch / 2))] & 0x1F);
            }
        }

        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw_rgb565, srcPitch);
    } else {
        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw + srcPitch, srcPitch);
    }

    if (texture != NULL)
#ifdef ENABLE_SDL3
        SDL_RenderTexture(renderer, texture, NULL, NULL);
#else
        SDL_RenderCopy(renderer, texture, NULL, NULL);
#endif
    
    SDL_RenderPresent(renderer);
}
        
void SDLDrawingPanel::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = systemColorDepth >> 3;
    int outrb = out_8 ? 2 : out_24 ? 0 : 4;
    int outstride = std::ceil(width * outbpp * scale) + outrb;
            
    // FIXME: filters race condition?
    const int max_threads = 1;
            
    if (!pixbuf2) {
        int allocstride = outstride, alloch = height;
                
        // gb may write borders, so allocate enough for them
        if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
            allocstride = std::ceil(GameArea::SGBWidth * outbpp * scale) + outrb;
            alloch = GameArea::SGBHeight;
        }

        pixbuf2 = (uint8_t*)calloc(allocstride, std::ceil((alloch + 2) * scale));
    }
            
    if (OPTION(kDispFilter) == config::Filter::kNone) {
        todraw = *data;
        // *data is assigned below, after old buf has been processed
        pixbuf1 = pixbuf2;
        pixbuf2 = todraw;
    } else
        todraw = pixbuf2;

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
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
                    
            nthreads = max_threads;
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
    if (OPTION(kDispFilter) == config::Filter::kNone) {
        *data = pixbuf1;
    }
            
    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();
                
        if (panel->osdstat.size())
            drawText(todraw + outstride * (systemColorDepth != 24), outstride,
                        10, 20, UTF8(panel->osdstat), OPTION(kPrefShowSpeedTransparent));
                
        if (!panel->osdtext.empty()) {
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
                                OPTION(kPrefShowSpeedTransparent));
                    cury += 10;
                    nlines--;
                    ptr += linelen;
                    *ptr = lchar;
                }
                        
                drawText(todraw + outstride * (systemColorDepth != 24),
                            outstride, 10, cury, ptr,
                            OPTION(kPrefShowSpeedTransparent));
                        
                free(buf);
                buf = NULL;
            } else
                panel->osdtext.clear();
        }
    }
            
    // Draw the current frame
    DrawArea();
}
        
BasicDrawingPanel::BasicDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanel(parent, _width, _height)
{
    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }
            
    DrawingPanelInit();
}
        
void BasicDrawingPanel::DrawArea(wxWindowDC& dc)
{
    wxImage* im;
            
    if (out_24) {
        // never scaled, no borders, no transformations needed
        im = new wxImage(width, height, todraw, true);
    } else if (out_8) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        uint8_t* src = (uint8_t*)todraw + (int)std::ceil((width + 2) * scale); // skip top border
        uint8_t* dst = im->GetData();
                
        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                // White color fix
                if (*src == 0xff) {
                    *dst++ = 0xff;
                    *dst++ = 0xff;
                    *dst++ = 0xff;
                } else {
                    *dst++ = (((*src >> 5) & 0x7) << 5);
                    *dst++ = (((*src >> 2) & 0x7) << 5);
                    *dst++ = ((*src & 0x3) << 6);
                }
            }
                    
            src += 2;
        }
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
    } else if (OPTION(kDispFilter) != config::Filter::kNone) {
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

bool GLDrawingPanel::SetContext()
{
#ifndef wxGL_IMPLICIT_CONTEXT
    // Check if the current context is valid
    if (!ctx
#if wxCHECK_VERSION(3, 1, 0)
        || !ctx->IsOK()
#endif
        )
    {
        // Delete the old context
        if (ctx) {
            delete ctx;
            ctx = nullptr;
        }
                
        // Create a new context
        ctx = new wxGLContext(this);
        DrawingPanelInit();
    }
    return wxGLCanvas::SetCurrent(*ctx);
#else
    return wxGLContext::SetCurrent(*this);
#endif
}
        
GLDrawingPanel::GLDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanelBase(_width, _height)
        , wxglc(parent, wxID_ANY, glopts, wxPoint(0, 0), parent->GetClientSize(),
                wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
    widgets::RequestHighResolutionOpenGlSurfaceForWindow(this);
    SetContext();
}
        
GLDrawingPanel::~GLDrawingPanel()
{
    // this should be automatically deleted w/ context
    // it's also unsafe if panel no longer displayed
    if (did_init)
    {
        SetContext();
        glDeleteLists(vlist, 1);
        glDeleteTextures(1, &texid);
    }
            
#ifndef wxGL_IMPLICIT_CONTEXT
    delete ctx;
#endif
}
        
void GLDrawingPanel::DrawingPanelInit()
{
    SetContext();

    DrawingPanelBase::DrawingPanelInit();
            
    AdjustViewport();
            
    const bool bilinear = OPTION(kDispBilinear);
            
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
                    bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    bilinear ? GL_LINEAR : GL_NEAREST);
            
#define int_fmt out_16 ? GL_RGB5 : GL_RGB
#define tex_fmt out_8  ? GL_RGB : \
    out_16 ? GL_BGRA : \
    out_24 ? GL_RGB : GL_RGBA, \
    out_8 ? GL_UNSIGNED_BYTE_3_3_2 : \
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
        if (OPTION(kPrefVsync))
            wxLogDebug(_("Enabling EGL VSync."));
        else
            wxLogDebug(_("Disabling EGL VSync."));
                
        eglSwapInterval(0, OPTION(kPrefVsync));
#endif
    }
    else {
        if (OPTION(kPrefVsync))
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
                glXSwapIntervalEXT(glXGetCurrentDisplay(),
                                    glXGetCurrentDrawable(), OPTION(kPrefVsync));
            else
                systemScreenMessage(_("Failed to set glXSwapIntervalEXT"));
        }
        if (strstr(glxQuery, "GLX_SGI_swap_control") != NULL)
        {
            glXSwapIntervalSGI = reinterpret_cast<PFNGLXSWAPINTERVALSGIPROC>(glXGetProcAddress((const GLubyte*)("glXSwapIntervalSGI")));
                    
            if (glXSwapIntervalSGI)
                glXSwapIntervalSGI(OPTION(kPrefVsync));
            else
                systemScreenMessage(_("Failed to set glXSwapIntervalSGI"));
        }
        if (strstr(glxQuery, "GLX_MESA_swap_control") != NULL)
        {
            glXSwapIntervalMESA = reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(glXGetProcAddress((const GLubyte*)("glXSwapIntervalMESA")));
                    
            if (glXSwapIntervalMESA)
                glXSwapIntervalMESA(OPTION(kPrefVsync));
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
        wglSwapIntervalEXT(OPTION(kPrefVsync));
    else
        systemScreenMessage(_("Failed to set wglSwapIntervalEXT"));
#elif defined(__WXMAC__)
    int swap_interval = OPTION(kPrefVsync) ? 1 : 0;
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
    SetContext();
            
    int x, y;
    widgets::GetRealPixelClientSize(this, &x, &y);
    glViewport(0, 0, x, y);
}
        
void GLDrawingPanel::RefreshGL()
{
    SetContext();
            
    // Rebind any textures or other OpenGL resources here
            
    glBindTexture(GL_TEXTURE_2D, texid);
}
        
void GLDrawingPanel::DrawArea(wxWindowDC& dc)
{
    uint8_t* src = NULL;
    uint8_t* dst = NULL;
            
    (void)dc; // unused params
    SetContext();
    RefreshGL();
            
    if (!did_init)
        DrawingPanelInit();
            
    if (todraw) {
        int rowlen = std::ceil(width * scale) + (out_8 ? 0 : out_16 ? 2 : out_24 ? 0 : 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlen);
#if wxBYTE_ORDER == wxBIG_ENDIAN
                
                // FIXME: is this necessary?
        if (out_16)
            glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
                
#endif
        if (out_8) {
            src = (uint8_t*)todraw + (int)std::ceil((width + 2) * ((systemColorDepth >> 3) * scale)); // skip top border
            dst = (uint8_t*)todraw;
                    
            for (int y = 0; y < std::ceil(height * scale); y++) {
                for (int x = 0; x < std::ceil(width * scale); x++) {
                    *dst++ = *src++;
                }
                        
                src += 2;
            }
                    
            glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, (int)std::ceil(width * scale), (int)std::ceil(height * scale),
                            0, tex_fmt, todraw);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, (int)std::ceil(width * scale), (int)std::ceil(height * scale),
                            0, tex_fmt, todraw + (int)std::ceil(rowlen * ((systemColorDepth >> 3) * scale)));
        }
                
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
            return _("Memory allocation error");
                    
        case recording::MRET_ERR_NOCODEC:
            return _("Error initializing codec");
                    
        case recording::MRET_ERR_FERR:
            return _("Error writing to output file");
                    
        case recording::MRET_ERR_FMTGUESS:
            return _("Can't guess output format from file name");

        default:
            //    case MRET_ERR_RECORDING:
            //    case MRET_ERR_BUFSIZE:
            return _("Programming error; aborting!");
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
        wxLogError(_("Error in audio / video recording (%s); aborting"),
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
        
void GameArea::OnGBBorderChanged(config::Option* option) {
    if (game_type() == IMAGE_GB && gbSgbMode) {
        if (option->GetBool()) {
            AddBorder();
            gbSgbRenderBorder();
        } else {
            DelBorder();
        }
    }
}
        
void GameArea::UpdateLcdFilter() {
    if (loaded == IMAGE_GBA)
        gbafilter_update_colors(OPTION(kGBALCDFilter));
    else if (loaded == IMAGE_GB)
        gbafilter_update_colors(OPTION(kGBLCDFilter));
    else
        gbafilter_update_colors(false);
}
        
void GameArea::SuspendScreenSaver() {
#ifdef HAVE_XSS
    if (xscreensaver_suspended || !gopts.suspend_screensaver)
        return;
    // suspend screensaver
    if (emulating && !wxGetApp().UsingWayland()) {
        auto display = GetX11Display();
        XScreenSaverSuspend(display, true);
        xscreensaver_suspended = true;
    }
#endif // HAVE_XSS
}
        
void GameArea::UnsuspendScreenSaver() {
#ifdef HAVE_XSS
            // unsuspend screensaver
    if (xscreensaver_suspended) {
        auto display = GetX11Display();
        XScreenSaverSuspend(display, false);
        xscreensaver_suspended = false;
    }
#endif // HAVE_XSS
}
        
void GameArea::OnAudioRateChanged() {
    if (loaded == IMAGE_UNKNOWN) {
        return;
    }
            
    switch (game_type()) {
        case IMAGE_UNKNOWN:
            break;
                    
        case IMAGE_GB:
            gbSoundSetSampleRate(GetSampleRate());
            break;
                    
        case IMAGE_GBA:
            soundSetSampleRate(GetSampleRate());
            break;
    }
}
        
void GameArea::OnVolumeChanged(config::Option* option) {
    const int volume = option->GetInt();
    soundSetVolume((float)volume / 100.0);
    systemScreenMessage(wxString::Format(_("Volume: %d %%"), volume));
}
        
#ifdef __WXMAC__
#ifndef NO_METAL
MetalDrawingPanel::MetalDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanel(parent, _width, _height)
{
    memset(delta, 0xff, sizeof(delta));
            
    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }
            
    DrawingPanelInit();
}
        
void MetalDrawingPanel::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = systemColorDepth >> 3;
    int outrb = out_8 ? 2 : out_24 ? 0 : 4;
    int outstride = std::ceil(width * outbpp * scale) + outrb;
            
    // FIXME: filters race condition?
    const int max_threads = 1;
            
    if (!pixbuf2) {
        int allocstride = outstride, alloch = height;
                
        // gb may write borders, so allocate enough for them
        if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
            allocstride = std::ceil(GameArea::SGBWidth * outbpp * scale) + outrb;
            alloch = GameArea::SGBHeight;
        }
                
        pixbuf2 = (uint8_t*)calloc(allocstride, std::ceil((alloch + 2) * scale));
    }

    if (OPTION(kDispFilter) == config::Filter::kNone) {
        todraw = *data;
        // *data is assigned below, after old buf has been processed
        pixbuf1 = pixbuf2;
        pixbuf2 = todraw;
    } else
        todraw = pixbuf2;

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
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

            nthreads = max_threads;
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
    if (OPTION(kDispFilter) == config::Filter::kNone) {
        *data = pixbuf1;
    }
            
    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        if (panel->osdstat.size())
            drawText(todraw + outstride * (systemColorDepth != 24), outstride,
                        10, 20, UTF8(panel->osdstat), OPTION(kPrefShowSpeedTransparent));
                
        if (!panel->osdtext.empty()) {
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
                                OPTION(kPrefShowSpeedTransparent));
                    cury += 10;
                    nlines--;
                    ptr += linelen;
                    *ptr = lchar;
                }

                drawText(todraw + outstride * (systemColorDepth != 24),
                            outstride, 10, cury, ptr,
                            OPTION(kPrefShowSpeedTransparent));

                free(buf);
                buf = NULL;
            } else
                panel->osdtext.clear();
        }
    }
 
    // Draw the current frame
    DrawArea();
}
        
void MetalDrawingPanel::PaintEv(wxPaintEvent& ev)
{
    // FIXME: implement
    if (!did_init) {
        DrawingPanelInit();
    }

    (void)ev; // unused params
            
    if (!todraw) {
        // since this is set for custom background, not drawing anything
        // will cause garbage to be displayed, so draw a black area
        draw_black_background(GetWindow());
        return;
    }

    if (todraw) {
        DrawArea();
    }
}
#endif
#endif


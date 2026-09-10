// The game area: game loading, the emulation loop, save states/battery,
// rewind, recording, fullscreen and the drawing-panel lifecycle. Ported from
// the GameArea parts of src/wx/panel.cpp.

#include "qt/game-area.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QProcess>
#include <QResizeEvent>
#include <QScreen>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWindow>

#ifdef __APPLE__
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

#include <zlib.h>

#include "components/filters/filters.h"
#include "components/filters_agb/filters_agb.h"
#include "components/filters_cgb/filters_cgb.h"
#include "components/filters_interframe/interframe.h"
#include "core/base/check.h"
#include "core/base/file_util.h"
#include "core/base/patch.h"
#include "core/base/system.h"
#include "core/base/version.h"
#include "core/gb/gb.h"
#include "core/gb/gbCartData.h"
#include "core/gb/gbCheats.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbPrinter.h"
#include "core/gb/gbSound.h"
#include "core/gba/gba.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaPrint.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"
#include "qt/app.h"
#include "qt/config/cmdtab.h"
#include "qt/config/emulated-gamepad.h"
#include "qt/config/option-id.h"
#include "qt/config/option-proxy.h"
#include "qt/config/option.h"
#include "qt/drawing-panel.h"
#include "qt/renderers/sdl-panel.h"
#include "qt/widgets/option-binding.h"
#if defined(_WIN32)
#include "qt/renderers/d3d-panel.h"
#elif defined(__APPLE__)
#include "qt/renderers/metal-panel.h"
#endif
#ifndef NO_VULKAN
#include "qt/renderers/vulkan-panel.h"
#endif
#include "qt/log.h"
#include "qt/lua/lua_engine.h"
#include "qt/main-window.h"
#include "qt/opts.h"
#include "qt/sys.h"

#ifndef NO_LINK
#include "core/gba/gbaLink.h"
#endif

#define TR(s) QCoreApplication::translate("GameArea", s)

int emulating;

namespace {

long GetSampleRate() {
    switch (OPTION(kSoundAudioRate)) {
        case config::AudioRate::k48kHz:
            return 48000;
        case config::AudioRate::k44kHz:
            return 44100;
        case config::AudioRate::k22kHz:
            return 22050;
        case config::AudioRate::k11kHz:
            return 11025;
        case config::AudioRate::kLast:
            VBAM_NOTREACHED_RETURN(44100);
    }
    VBAM_NOTREACHED_RETURN(44100);
}

// Returns a valid override key for the loaded GBA ROM: the 4-char game code if
// all bytes are printable ASCII, otherwise "CRC_XXXXXXXX".
QString gbaGetOverrideId() {
    bool valid = true;
    for (int i = 0; i < 4; i++) {
        uint8_t c = g_rom[0xac + i];
        if (c < 0x21 || c > 0x7e) { valid = false; break; }
    }
    if (valid)
        return QString::fromLatin1(reinterpret_cast<const char*>(&g_rom[0xac]), 4);
    uint32_t romcrc = crc32(0L, g_rom, gbaGetRomSize());
    return QStringLiteral("CRC_%1").arg(romcrc, 8, 16, QLatin1Char('0')).toUpper();
}

// Get system name string for the currently loaded ROM.
QString GetCurrentSystemName(IMAGE_TYPE game_type) {
    if (game_type == IMAGE_GBA)
        return QStringLiteral("GameBoy Advance");

    if (game_type == IMAGE_GB) {
        if (gbCgbMode)
            return QStringLiteral("GameBoy Color");
        if (gbSgbMode)
            return QStringLiteral("Super GameBoy");
        return QStringLiteral("GameBoy");
    }

    return QStringLiteral("GameBoy Advance");
}

// Expand %s in path to system name
QString ExpandSystemPath(const QString& path, IMAGE_TYPE game_type) {
    QString result = path;
    if (result.contains(QStringLiteral("%s")))
        result.replace(QStringLiteral("%s"), GetCurrentSystemName(game_type));
    return result;
}

bool IsWritableDir(const QString& dir) {
    QFileInfo fi(dir);
    return fi.isDir() && fi.isWritable();
}

// Reads a value from an override group; QSettings stores them as strings.
template <typename T>
T ReadOverride(QSettings* cfg, const QString& key, T def) {
    const QVariant v = cfg->value(key);
    if (!v.isValid())
        return def;
    bool ok = false;
    const long long n = v.toString().toLongLong(&ok, 0);
    return ok ? static_cast<T>(n) : def;
}

// Candidates for the first-launch filter probe, highest to lowest cost.
constexpr config::Filter kFilterProbeCandidates[] = {
    config::Filter::kXbrz9x,
    config::Filter::kXbrz6x,
    config::Filter::kXbrz2x,
    config::Filter::kNone,
};
constexpr int kNbFilterProbeCandidates =
    static_cast<int>(sizeof(kFilterProbeCandidates) / sizeof(kFilterProbeCandidates[0]));

constexpr double kFilterProbeStartupDelayMs = 50.0;
constexpr int kFilterProbeStableFrames = 8;
constexpr double kFilterProbeStableTolMs = 2.0;
constexpr double kFilterProbeStabilizeCapMs = 2000.0;
constexpr double kFilterProbeMeasureMs = 350.0;
constexpr double kFilterProbeTargetFps = 55.0;

const char* FilterProbeName(config::Filter f) {
    switch (f) {
        case config::Filter::kXbrz9x:    return "xBRZ 9x";
        case config::Filter::kXbrz6x:    return "xBRZ 6x";
        case config::Filter::kScaleFX3x: return "ScaleFX 3x";
        case config::Filter::kXbrz2x:    return "xBRZ 2x";
        case config::Filter::kNone:      return "None";
        default:                         return "?";
    }
}

#ifdef __APPLE__
IOPMAssertionID g_screensaver_assertion = kIOPMNullAssertionID;
#endif

}  // namespace

bool g_default_filter_probe_pending = false;

GameArea::GameArea(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(false);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);

    // all renderers prefer 32-bit
    systemColorDepth = (OPTION(kBitDepth) + 1) << 3;

    hq2x_init(32);
    Init_2xSaI(32);

    idle_timer_.setSingleShot(true);
    idle_timer_.setInterval(0);
    idle_timer_.setTimerType(Qt::PreciseTimer);
    connect(&idle_timer_, &QTimer::timeout, this, &GameArea::OnIdle);

    connect(vbamApp().input_dispatcher(), &widgets::InputDispatcher::inputBatch, this,
            &GameArea::OnInputBatch);

    render_observer_ = std::make_unique<config::OptionsObserver>(
        std::vector<config::OptionID>{
            config::OptionID::kDispBilinear, config::OptionID::kDispFilterPlugin,
            config::OptionID::kDispRenderMethod, config::OptionID::kDispIFB,
            config::OptionID::kDispStretch, config::OptionID::kPrefVsync,
            config::OptionID::kBitDepth, config::OptionID::kDispMaxThreads},
        [this](config::Option*) { SchedulePanelReset(); });
    // kDispFilter is handled separately: a filter change can often be adopted
    // in place (no panel rebuild / no black flash), unlike the options above.
    disp_filter_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kDispFilter, [this](config::Option*) { OnDispFilterChanged(); });
    scale_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kDispScale, [this](config::Option*) { AdjustSize(true); });
    gb_border_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kPrefBorderOn,
        [this](config::Option* option) { OnGBBorderChanged(option); });
    gb_palette_observer_ = std::make_unique<config::OptionsObserver>(
        std::vector<config::OptionID>{config::OptionID::kGBPalette0, config::OptionID::kGBPalette1,
                                      config::OptionID::kGBPalette2,
                                      config::OptionID::kPrefGBPaletteOption},
        [](config::Option*) { gbResetPalette(); });
    gb_declick_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kSoundGBDeclicking,
        [](config::Option* option) { gbSoundSetDeclicking(option->GetBool()); });
    lcd_filters_observer_ = std::make_unique<config::OptionsObserver>(
        std::vector<config::OptionID>{
            config::OptionID::kGBLCDFilter, config::OptionID::kGBADarken,
            config::OptionID::kGBLighten, config::OptionID::kDispColorCorrectionProfile,
            config::OptionID::kGBALCDFilter, config::OptionID::kGBALCDFilterVariant,
            config::OptionID::kGBLCDFilterVariant, config::OptionID::kDispColorCorrectionAuto},
        [this](config::Option*) { UpdateLcdFilter(); });
    audio_rate_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kSoundAudioRate, [this](config::Option*) { OnAudioRateChanged(); });
    audio_volume_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kSoundVolume,
        [this](config::Option* option) { OnVolumeChanged(option); });
    audio_observer_ = std::make_unique<config::OptionsObserver>(
        std::vector<config::OptionID>{
            config::OptionID::kSoundAudioAPI, config::OptionID::kSoundAudioDevice,
            config::OptionID::kSoundBuffers, config::OptionID::kSoundDSoundHWAccel,
            config::OptionID::kSoundUpmix},
        [this](config::Option*) { schedule_audio_restart_ = true; });
    // The hide-menu-bar option arms a mouse-idle auto-hide (see HideMenuBar());
    // only un-hiding needs to happen when it is switched off.
    menu_bar_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kUIHideMenuBar, [this](config::Option* option) {
            if (!option->GetBool())
                ShowMenuBar();
        });
}

GameArea::~GameArea() {
    UnloadGame(true);

    if (rewind_mem)
        free(rewind_mem);
}

QString GameArea::game_dir() {
    return QFileInfo(loaded_game).absolutePath();
}

QString GameArea::game_name() {
    return QFileInfo(loaded_game).fileName();
}

QString GameArea::game_base_name() {
    return QFileInfo(loaded_game).completeBaseName();
}

void GameArea::LoadGame(const QString& name)
{
    rom_scene_rls = QStringLiteral("-");
    rom_scene_rls_name = QStringLiteral("-");
    rom_name.clear();
    // fex just crashes if file does not exist and it's compressed,
    // so check first
    QFileInfo fnfn(name);
    bool badfile = !fnfn.isReadable() || !fnfn.isFile();

    // if path was relative, look for it before giving up
    if (badfile && !fnfn.isAbsolute()) {
        const QString rp = fnfn.path();
        const QString file = fnfn.fileName();

        // can't really decide which dir to use, so try GBA first, then GB
        for (const QString& dir_opt : {OPTION(kGBAROMDir).Get(), OPTION(kGBROMDir).Get(),
                                       OPTION(kGBGBCROMDir).Get()}) {
            if (!badfile)
                break;
            const QString abs = vbamApp().GetAbsolutePath(dir_opt);
            if (abs.isEmpty())
                continue;
            fnfn = QFileInfo(QDir(abs + QLatin1Char('/') + rp).filePath(file));
            badfile = !fnfn.isReadable() || !fnfn.isFile();
        }
    }

    const std::string fn_std = vbam::ToPath(fnfn.absoluteFilePath());
    const char* fn = fn_std.c_str();
    IMAGE_TYPE t = badfile ? IMAGE_UNKNOWN : utilFindType(fn);

    if (t == IMAGE_UNKNOWN) {
        QMessageBox::critical(parentWidget(), TR("Problem loading file"),
                              TR("%1 is not a valid ROM file").arg(name));
        return;
    }

    if (!OPTION(kGenFreezeRecent)) {
        gopts.recent.AddFileToHistory(fnfn.absoluteFilePath());
        vbamApp().SaveRecentList();
        vbamApp().frame->ResetRecentMenu();
    }

    UnloadGame();
    // strip extension from actual game file name
    loaded_game = fnfn.absoluteFilePath();
    // load patch, if enabled
    bool loadpatch = OPTION(kPrefAutoPatch);
    QString pfn = loaded_game;
    int ovSaveType = 0;

    if (loadpatch) {
        pfn = loaded_game + QStringLiteral(".ips");
        if (!QFileInfo(pfn).isReadable()) {
            pfn = loaded_game + QStringLiteral(".ups");
            if (!QFileInfo(pfn).isReadable()) {
                pfn = loaded_game + QStringLiteral(".bps");
                if (!QFileInfo(pfn).isReadable()) {
                    pfn = loaded_game + QStringLiteral(".ppf");
                    loadpatch = QFileInfo(pfn).isReadable();
                }
            }
        }
    }

    if (t == IMAGE_GB) {
        if (!gbLoadRom(fn)) {
            QMessageBox::critical(parentWidget(), TR("Problem loading file"),
                                  TR("Unable to load Game Boy ROM %1").arg(name));
            return;
        }

        if (loadpatch) {
            gbApplyPatch(vbam::ToPath(pfn).c_str());
        }

        // Apply overrides.
        QSettings* cfg = vbamApp().gb_overrides();
        const QString title = QString::fromStdString(g_gbCartData.title());
        if (cfg && !title.isEmpty() && cfg->childGroups().contains(title)) {
            cfg->beginGroup(title);
            coreOptions.gbPrinterEnabled =
                ReadOverride<int>(cfg, QStringLiteral("gbPrinter"), coreOptions.gbPrinterEnabled);
            cfg->endGroup();
        }

        // start sound; this must happen before CPU stuff
        gb_effects_config.enabled = OPTION(kSoundGBEnableEffects);
        gb_effects_config.surround = OPTION(kSoundGBSurround);
        gb_effects_config.echo = (float)OPTION(kSoundGBEcho) / 100.0;
        gb_effects_config.stereo = (float)OPTION(kSoundGBStereo) / 100.0;
        // soundInit() silently falls back to the null (timer-paced) driver when
        // the selected driver can't initialize; suppress the error dialog.
        {
            vbam::LogNull no_sound_error_dialog;
            soundInit();
        }
        soundSetEnable(gopts.sound_en);
        gbSoundSetSampleRate(GetSampleRate());
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(coreOptions.throttle);
        gbGetHardwareType();

        // Disable bios loading when using colorizer hack.
        if (OPTION(kPrefUseBiosGB) && OPTION(kGBColorizerHack)) {
            vbam::LogError(TR("Cannot use Game Boy BIOS file when Colorizer Hack is enabled, disabling Game Boy BIOS file."));
            OPTION(kPrefUseBiosGB) = false;
        }

        // Set up the core for the colorizer hack.
        setColorizerHack(OPTION(kGBColorizerHack));

        // Load BIOS for the actual system being emulated
        // gbHardware: 1=GB, 2=GBC, 4=SGB/SGB2, 8=GBA
        bool use_bios = false;
        QString bios_file;

        if (gbHardware == 2) {
            use_bios = OPTION(kPrefUseBiosGBC).Get();
            bios_file = OPTION(kGBGBCBiosFile).Get();
        } else if (gbHardware == 1) {
            use_bios = OPTION(kPrefUseBiosGB).Get();
            bios_file = OPTION(kGBBiosFile).Get();
        }

        gbCPUInit(vbam::ToPath(bios_file).c_str(), use_bios);

        if (use_bios && !coreOptions.useBios) {
            vbam::LogError(TR("Could not load BIOS %1").arg(bios_file));
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
            QMessageBox::critical(parentWidget(), TR("Problem loading file"),
                                  TR("Unable to load Game Boy Advance ROM %1").arg(name));
            return;
        }

        rom_crc32 = crc32(0L, g_rom, rom_size);

        if (loadpatch) {
            // don't use real rom size or it might try to resize rom[]
            int size = 0x2000000 < rom_size ? 0x2000000 : rom_size;
            applyPatch(vbam::ToPath(pfn).c_str(), &g_rom, &size);
            gbaUpdateRomSize(size);
        }

        QSettings* cfg = vbamApp().overrides();
        const QString id = gbaGetOverrideId();

        if (cfg && cfg->childGroups().contains(id)) {
            cfg->beginGroup(id);
            // Apply RTC override
            const bool enable_rtc =
                ReadOverride<int>(cfg, QStringLiteral("rtcEnabled"), coreOptions.rtcEnabled) != 0;
            rtcEnable(enable_rtc);

            // Set Flash size from config; fallback to global preference
            int fsz = ReadOverride<int>(cfg, QStringLiteral("flashSize"), 0);
            if (fsz != 0x10000 && fsz != 0x20000)
                fsz = 0x10000 << OPTION(kPrefFlashSize);
            flashSetSize(fsz);

            // Set save type from override; fallback to detection if set to Auto (0)
            ovSaveType = ReadOverride<int>(cfg, QStringLiteral("saveType"), coreOptions.cpuSaveType);
            if (ovSaveType < 0 || ovSaveType > 5)
                ovSaveType = 0;

            if (ovSaveType == 0) {
                flashDetectSaveType(rom_size);
            } else {
                coreOptions.saveType = ovSaveType;
            }

            // Initialize eepromMask to prevent DMA freeze on un-initialized battery
            if (coreOptions.saveType == GBA_SAVE_EEPROM) {
                eepromSetSize(SIZE_EEPROM_512);
            }

            coreOptions.mirroringEnable =
                ReadOverride<int>(cfg, QStringLiteral("mirroringEnabled"), 1) != 0;
            cfg->endGroup();
        } else {
            rtcEnable(coreOptions.rtcEnabled);
            flashSetSize(0x10000 << OPTION(kPrefFlashSize));

            if (coreOptions.cpuSaveType < 0 || coreOptions.cpuSaveType > 5)
                coreOptions.cpuSaveType = 0;

            if (coreOptions.cpuSaveType == 0) {
                flashDetectSaveType(rom_size);
            } else {
                coreOptions.saveType = coreOptions.cpuSaveType;
            }

            if (coreOptions.saveType == GBA_SAVE_EEPROM) {
                eepromSetSize(SIZE_EEPROM_512);
            }

            coreOptions.mirroringEnable = false;
        }

        doMirroring(coreOptions.mirroringEnable);
        // start sound; this must happen before CPU stuff
        {
            vbam::LogNull no_sound_error_dialog;
            soundInit();
        }
        soundSetEnable(gopts.sound_en);
        soundSetSampleRate(GetSampleRate());
        soundSetThrottle(coreOptions.throttle);
        soundFiltering = (float)OPTION(kSoundGBAFiltering) / 100.0f;

        rtcEnableRumble(true);

        CPUInit(vbam::ToPath(gopts.gba_bios).c_str(), OPTION(kPrefUseBiosGBA));

        if (OPTION(kPrefUseBiosGBA) && !coreOptions.useBios) {
            vbam::LogError(TR("Could not load BIOS %1").arg(gopts.gba_bios));
        }

        CPUReset();
        basic_width = GBAWidth;
        basic_height = GBAHeight;
        emusys = &GBASystem;
    }

    // Set sound volume. The --mute command-line switch forces silence for the
    // session without altering the saved volume.
    soundSetVolume(vbamApp().mute ? 0.0 : (float)OPTION(kSoundVolume) / 100.0);

    if (OPTION(kGeomFullScreen)) {
        GameArea::ShowFullScreen(true);
    }

    loaded = t;
    SetFrameTitle();
    setFocus();
    // Use custom geometry
    AdjustSize(false);
    emulating = true;
    was_paused = true;
    schedule_audio_restart_ = false;
    MainWindow* mf = vbamApp().frame;
    mf->cmd_enable &= ~(CMDEN_GB | CMDEN_GBA);
    mf->cmd_enable |= ONLOAD_CMDEN;
    mf->cmd_enable |= loaded == IMAGE_GB ? CMDEN_GB : (CMDEN_GBA | CMDEN_NGDB_GBA);
    mf->enable_menus();
#ifndef NO_LINK
    gbSerialFunction = gbStartLink;

    // The effective link protocol depends on the ROM class (GB serial vs GBA
    // cable). If a local (IPC) link is attached in the wrong mode, re-attach in
    // the right one; never tear down an established network session.
    {
        const LinkMode active = GetLinkMode();
        const LinkMode wanted = mf->GetConfiguredLinkMode(); // ROM-aware

        if (active != LINK_DISCONNECTED && active != wanted) {
            const bool ipc_swap =
                (active == LINK_CABLE_IPC || active == LINK_GAMEBOY_IPC) &&
                (wanted == LINK_CABLE_IPC || wanted == LINK_GAMEBOY_IPC);

            if (ipc_swap) {
                CloseLink();

                if (InitLink(wanted) != LINK_OK)
                    CloseLink();
                else if (wanted == LINK_GAMEBOY_IPC)
                    gbInitLink(); // gbReset already ran above
            } else {
                systemScreenMessage(
                    TR("Active link mode does not match this ROM; use Options > Link > Start Link to reconnect."));
            }
        }
    }
#else
    gbSerialFunction = nullptr;
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
        QString bname = game_base_name();
#ifndef NO_LINK
        // MakeInstanceFilename doesn't do QString, so just add slave ID here
        int playerId = GetLinkPlayerId();

        if (playerId >= 0) {
            bname.append(QLatin1Char('-'));
            bname.append(QChar('1' + playerId));
        }
#endif
        bname.append(QStringLiteral(".sav"));
        const QString bat = QDir(batdir).filePath(bname);

        if (emusys->emuReadBattery(vbam::ToPath(bat).c_str())) {
            systemScreenMessage(TR("Loaded battery %1").arg(bat));

            if (coreOptions.cpuSaveType == 0 && ovSaveType == 0 && t == IMAGE_GBA) {
                const qint64 bat_size = QFileInfo(bat).size();
                switch (bat_size) {
                case 0x200:
                case 0x2000:
                    coreOptions.saveType = GBA_SAVE_EEPROM;
                    eepromSetSize(static_cast<int>(bat_size));
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
                    flashSetSize(static_cast<int>(bat_size));
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
    do_rewind = gopts.rewind_interval > 0;
    cheats_dirty = (did_autoload && !coreOptions.skipSaveGameCheats) ||
                   (loaded == IMAGE_GB ? gbCheatNumber > 0 : cheatsNumber > 0);

    if (OPTION(kPrefAutoSaveLoadCheatList) && (!did_autoload || coreOptions.skipSaveGameCheats)) {
        const QString cfn = loaded_game + QStringLiteral(".clt");

        if (QFileInfo(cfn).isReadable()) {
            bool cld;

            if (loaded == IMAGE_GB)
                cld = gbCheatsLoadCheatList(vbam::ToPath(cfn).c_str());
            else
                cld = cheatsLoadCheatList(vbam::ToPath(cfn).c_str());

            if (cld) {
                systemScreenMessage(TR("Loaded cheats"));
                cheats_dirty = false;
            }
        }
    }

#ifndef NO_LINK
    if (OPTION(kGBALinkAuto)) {
        BootLink(mf->GetConfiguredLinkMode(), vbam::ToStd(gopts.link_host).c_str(),
                 gopts.link_timeout, OPTION(kGBALinkFast), gopts.link_num_players);
    }

    // The title and the link menu were set earlier in the load, before either
    // the boot attach above or the IPC mode swap could assign a player id.
    SetFrameTitle();
    mf->EnableNetworkMenu();
#endif

    vbam::lua::LuaSetRomName(vbam::ToStd(game_base_name()));

#if defined(VBAM_ENABLE_DEBUGGER)
    if (OPTION(kPrefGDBBreakOnLoad)) {
        mf->GDBBreak();
    }
#endif  // defined(VBAM_ENABLE_DEBUGGER)

    RequestMore();
}

void GameArea::SetFrameTitle()
{
    QString tit;

    if (loaded != IMAGE_UNKNOWN) {
        tit.append(game_name());
        tit.append(QStringLiteral(" - "));
    }

    tit.append(QStringLiteral("VisualBoyAdvance-M "));
    tit.append(QString::fromStdString(kVbamVersion));

#ifndef NO_LINK
    int playerId = GetLinkPlayerId();

    if (playerId >= 0) {
        tit.append(TR(" player "));
        tit.append(QChar('1' + playerId));
    }
#endif
    if (vbamApp().frame)
        vbamApp().frame->setWindowTitle(tit);
}

void GameArea::recompute_dirs()
{
    batdir = ExpandSystemPath(OPTION(kGenBatteryDir), loaded);

    if (batdir.isEmpty()) {
        batdir = game_dir();
    } else {
        batdir = vbamApp().GetAbsolutePath(batdir);
        if (!QDir(batdir).exists()) {
            if (!QDir().mkpath(batdir)) {
                QMessageBox::critical(
                    this, TR("Directory Error"),
                    TR("Could not create Native Saves directory:\n%1\n\nPlease check your configured path in Options > Directories.").arg(batdir));
            }
        }
    }

    if (!IsWritableDir(batdir)) {
        batdir = vbamApp().GetDataDir();
    }

    statedir = ExpandSystemPath(OPTION(kGenStateDir), loaded);

    if (statedir.isEmpty()) {
        statedir = game_dir();
    } else {
        statedir = vbamApp().GetAbsolutePath(statedir);
        if (!QDir(statedir).exists()) {
            if (!QDir().mkpath(statedir)) {
                QMessageBox::critical(
                    this, TR("Directory Error"),
                    TR("Could not create Emulator Saves directory:\n%1\n\nPlease check your configured path in Options > Directories.").arg(statedir));
            }
        }
    }

    if (!IsWritableDir(statedir)) {
        statedir = vbamApp().GetDataDir();
    }
}

void GameArea::UnloadGame(bool destruct)
{
    if (!emulating)
        return;

    // last opportunity to autosave cheats
    if (OPTION(kPrefAutoSaveLoadCheatList) && cheats_dirty) {
        const QString cfn = loaded_game + QStringLiteral(".clt");

        if (loaded == IMAGE_GB) {
            if (!gbCheatNumber)
                QFile::remove(cfn);
            else
                gbCheatsSaveCheatList(vbam::ToPath(cfn).c_str());
        } else {
            if (!cheatsNumber)
                QFile::remove(cfn);
            else
                cheatsSaveCheatList(vbam::ToPath(cfn).c_str());
        }
    }

    // if timer was counting down for save, go ahead and save
    if (systemSaveUpdateCounter > SYSTEM_SAVE_NOT_UPDATED) {
        SaveBattery();
    }

    MainWindow* mf = vbamApp().frame;
#ifndef NO_FFMPEG
    snd_rec.Stop();
    vid_rec.Stop();
#endif
    systemStopGameRecording();
    systemStopGamePlayback();

    vbam::lua::LuaOnGameUnload();
    vbam::lua::LuaSetRomName("");

#if defined(VBAM_ENABLE_DEBUGGER)
    debugger = false;
    remoteCleanUp();
    if (mf)
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
    emusys = nullptr;
    soundShutdown();

    idle_timer_.stop();

    if (destruct)
        return;

    ResetPanel();

    if (!mf)
        return;

    // close any game-related viewer windows
    while (!mf->popups.empty())
        mf->popups.front()->close();

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
    int slot = vbamApp().frame->newest_state_slot();

    if (slot < 1)
        return false;

    return LoadState(slot);
}

bool GameArea::LoadState(int slot)
{
    const QString fname = QString(SAVESLOT_FMT).arg(game_base_name())
                              .arg(slot, 2, 10, QLatin1Char('0'));
    return LoadState(QDir(statedir).filePath(fname));
}

bool GameArea::LoadState(const QString& fname)
{
    if (!emusys)
        return false;
    // FIXME: first save to backup state if not backup state
    bool ret = emusys->emuReadState(vbam::ToPath(fname).c_str());

    if (ret && num_rewind_states) {
        MainWindow* mf = vbamApp().frame;
        mf->cmd_enable &= ~CMDEN_REWIND;
        mf->enable_menus();
        num_rewind_states = 0;
        // do an immediate rewind save
        do_rewind = true;
        rewind_time = gopts.rewind_interval * 6;
    }

    if (ret) {
        // forget old save writes
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
        // no point in blending after abrupt change
        InterframeClear();
        // frame rate calc should probably reset as well
        was_paused = true;
        // save state had a screen frame, so draw it
        systemDrawScreen();
    }

    systemScreenMessage((ret ? TR("Loaded state %1") : TR("Error loading state %1")).arg(fname));
    return ret;
}

bool GameArea::SaveState()
{
    return SaveState(vbamApp().frame->oldest_state_slot());
}

bool GameArea::SaveState(int slot)
{
    const QString fname = QString(SAVESLOT_FMT).arg(game_base_name())
                              .arg(slot, 2, 10, QLatin1Char('0'));
    return SaveState(QDir(statedir).filePath(fname));
}

bool GameArea::SaveState(const QString& fname)
{
    if (!emusys)
        return false;
    // FIXME: first copy to backup state if not backup state
    bool ret = emusys->emuWriteState(vbam::ToPath(fname).c_str());
    vbamApp().frame->update_state_ts(true);
    systemScreenMessage((ret ? TR("Saved state %1") : TR("Error saving state %1")).arg(fname));
    return ret;
}

void GameArea::SaveBattery()
{
    if (!emusys)
        return;
    // MakeInstanceFilename doesn't do QString, so just add slave ID here
    QString bname = game_base_name();
#ifndef NO_LINK
    int playerId = GetLinkPlayerId();

    if (playerId >= 0) {
        bname.append(QLatin1Char('-'));
        bname.append(QChar('1' + playerId));
    }
#endif
    bname.append(QStringLiteral(".sav"));
    QDir().mkpath(batdir);
    const QString fn = QDir(batdir).filePath(bname);

    // FIXME: add option to support ring of backups
    if (!emusys->emuWriteBattery(vbam::ToPath(fn).c_str()))
        vbam::LogError(TR("Error writing battery %1").arg(fn));

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
    if (vbamApp().frame)
        vbamApp().frame->adjustSize();
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
    if (vbamApp().frame)
        vbamApp().frame->adjustSize();
    ResetPanel();
}

void GameArea::AdjustMinSize()
{
    MainWindow* frame = vbamApp().frame;
    const double display_scale = OPTION(kDispScale);

    // note: could safely set min size to 1x or less regardless of video_scale
    // but setting it to scaled size makes resizing to default easier
    const QSize sz(static_cast<int>(std::ceil(basic_width * display_scale)),
                   static_cast<int>(std::ceil(basic_height * display_scale)));
    setMinimumSize(sz);
    if (frame && frame->centralWidget()) {
        // Qt's minimum size is in client coordinates; the layout adds the
        // menu/status bars itself.
        frame->setMinimumSize(sz + (frame->size() - frame->centralWidget()->size()));
    }
}

void GameArea::LowerMinSize()
{
    QWidget* frame = vbamApp().frame;
    const QSize sz(basic_width, basic_height);
    setMinimumSize(sz);
    if (frame)
        frame->setMinimumSize(sz);
}

void GameArea::AdjustSize(bool force)
{
    AdjustMinSize();

    if (fullscreen)
        return;

    const double display_scale = OPTION(kDispScale);
    const QSize newsz(static_cast<int>(std::ceil(basic_width * display_scale)),
                      static_cast<int>(std::ceil(basic_height * display_scale)));

    if (!force) {
        const QSize sz = size();
        if (sz.width() >= newsz.width() && sz.height() >= newsz.height())
            return;
    }

    resize(newsz);
    MainWindow* frame = vbamApp().frame;
    if (frame && frame->centralWidget() && !frame->isMaximized()) {
        // Size the frame so its central widget is exactly newsz.
        const QSize decorations = frame->size() - frame->centralWidget()->size();
        frame->resize(newsz + decorations);
    }
}

void GameArea::ResetPanel() {
    if (panel) {
        // Pause emulation while panel is being reset to prevent crashes from
        // accessing invalid panel state during the transition. Force
        // paused=true directly because Pause() skips pausing in link mode.
        bool was_running = !paused;
        if (was_running) {
            paused = was_paused = true;
            UnsuspendScreenSaver();
            vbamApp().emulated_gamepad()->Reset();
            if (loaded != IMAGE_UNKNOWN)
                soundPause();
        }

        // Stop filter threads SYNCHRONOUSLY before cleaning up InterframeManager.
        panel->StopFilterThreads();
        InterframeCleanup();

        QWidget* w = panel->GetWindow();
        if (w) {
            w->removeEventFilter(this);
            layout_->removeWidget(w);
        }
        // Delete through the concrete widget so the whole object goes away
        // synchronously (GL resources, filter buffers).
        if (auto* gl = dynamic_cast<GLDrawingPanel*>(panel)) {
            delete gl;
        } else if (auto* sw = dynamic_cast<SoftwareDrawingPanel*>(panel)) {
            delete sw;
        } else {
            panel->Destroy();
        }
        panel = nullptr;

        if (was_running)
            pending_resume_after_panel_ = true;

        update();
        RequestMore();
    }
}

void GameArea::SchedulePanelReset() {
    // Defer the reset to the start of the next idle tick: never destroy the
    // panel while CPULoop may be in the middle of rendering.
    pending_panel_reset_ = true;
    RequestMore();
}

void GameArea::OnDispFilterChanged() {
    // A filter change only alters the intermediate (filtered) image size, so
    // every renderer can adopt it in place -- no panel teardown. A plugin
    // filter (on either side of the change) alters the color format and
    // pipeline, so it still needs a full rebuild.
    if (panel && OPTION(kDispFilter) != config::Filter::kPlugin &&
        !panel->IsUsingFilterPlugin() && panel->SupportsInPlaceFilterChange()) {
        panel->ApplyInPlaceFilterChange();
        return;
    }
    SchedulePanelReset();
}

// ---------------------------------------------------------------------------
// Runtime display-filter auto-probe (first launch only).
// ---------------------------------------------------------------------------

void GameArea::BeginFilterProbeCandidate() {
    filter_probe_state_ = FilterProbeState::kStabilize;
    filter_probe_frames_ = 0;
    filter_probe_stable_count_ = 0;
    filter_probe_min_interval_ms_ = 0.0;
    filter_probe_intervals_.clear();
    filter_probe_phase_start_ = std::chrono::steady_clock::now();
    OPTION(kDispFilter) = kFilterProbeCandidates[filter_probe_index_];
}

void GameArea::FinishFilterProbe() {
    g_default_filter_probe_pending = false;
    filter_probe_state_ = FilterProbeState::kInactive;
    update_opts();
    systemScreenMessage(TR("Done"));
    vbam::LogDebug(QStringLiteral("Filter probe settled on %1")
                       .arg(QLatin1String(FilterProbeName(kFilterProbeCandidates[filter_probe_index_]))));
}

void GameArea::AdvanceFilterProbe(double measured_fps) {
    const bool pass = measured_fps >= kFilterProbeTargetFps;
    vbam::LogDebug(QStringLiteral("Filter probe: %1 -> %2 fps (%3)")
                       .arg(QLatin1String(FilterProbeName(kFilterProbeCandidates[filter_probe_index_])))
                       .arg(measured_fps, 0, 'f', 1)
                       .arg(pass ? QStringLiteral("accept") : QStringLiteral("step down")));
    if (pass || filter_probe_index_ + 1 >= kNbFilterProbeCandidates) {
        FinishFilterProbe();
        return;
    }
    ++filter_probe_index_;
    BeginFilterProbeCandidate();
}

void GameArea::StepFilterProbe() {
    if (!g_default_filter_probe_pending)
        return;

    const auto now = std::chrono::steady_clock::now();

    if (filter_probe_state_ == FilterProbeState::kInactive) {
        filter_probe_state_ = FilterProbeState::kStartupDelay;
        filter_probe_phase_start_ = now;
        return;
    }
    if (filter_probe_state_ == FilterProbeState::kStartupDelay) {
        const double settle_ms = std::chrono::duration<double, std::milli>(
            now - filter_probe_phase_start_).count();
        if (settle_ms >= kFilterProbeStartupDelayMs) {
            systemScreenMessage(TR("Running quick first-time performance test"));
            filter_probe_index_ = 0;
            BeginFilterProbeCandidate();
        }
        return;
    }

    ++filter_probe_frames_;
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        now - filter_probe_phase_start_).count();

    if (filter_probe_state_ == FilterProbeState::kStabilize) {
        if (filter_probe_frames_ == 1) {
            filter_probe_last_frame_ = now;
            filter_probe_phase_start_ = now;
            return;
        }
        const double interval_ms = std::chrono::duration<double, std::milli>(
            now - filter_probe_last_frame_).count();
        filter_probe_last_frame_ = now;
        if (filter_probe_min_interval_ms_ <= 0.0 ||
            interval_ms < filter_probe_min_interval_ms_)
            filter_probe_min_interval_ms_ = interval_ms;

        if (interval_ms <= filter_probe_min_interval_ms_ + kFilterProbeStableTolMs)
            ++filter_probe_stable_count_;
        else
            filter_probe_stable_count_ = 0;

        if (filter_probe_stable_count_ >= kFilterProbeStableFrames ||
            elapsed_ms >= kFilterProbeStabilizeCapMs) {
            filter_probe_state_ = FilterProbeState::kMeasure;
            filter_probe_frames_ = 0;
            filter_probe_intervals_.clear();
            filter_probe_phase_start_ = std::chrono::steady_clock::now();
        }
        return;
    }

    // kMeasure
    if (filter_probe_frames_ == 1) {
        filter_probe_last_frame_ = now;
        return;
    }
    filter_probe_intervals_.push_back(
        std::chrono::duration<double, std::milli>(now - filter_probe_last_frame_).count());
    filter_probe_last_frame_ = now;

    if (elapsed_ms >= kFilterProbeMeasureMs) {
        double fps = 0.0;
        if (!filter_probe_intervals_.empty()) {
            std::sort(filter_probe_intervals_.begin(), filter_probe_intervals_.end());
            const size_t idx = (filter_probe_intervals_.size() - 1) / 4;
            const double best_sustained_ms = filter_probe_intervals_[idx];
            if (best_sustained_ms > 0.0)
                fps = 1000.0 / best_sustained_ms;
        }
        AdvanceFilterProbe(fps);
    }
}

void GameArea::ShowFullScreen(bool full)
{
    MainWindow* tlw = vbamApp().frame;
    if (!tlw)
        return;

    if (full == fullscreen) {
        // in case the tlw somehow lost its mind, force it to proper mode
        if (tlw->isFullScreen() != fullscreen) {
            if (full)
                tlw->showFullScreen();
            else
                tlw->showNormal();
        }
        return;
    }

    fullscreen = full;

    // just in case screen mode is going to change, go ahead and preemptively
    // delete panel to be recreated immediately after resize
    SchedulePanelReset();

    static bool cursz_valid = false;
    static QSize cursz;
    static QPoint curpos;
    static bool was_maximized = false;

    if (!full) {
        tlw->showNormal();
        tlw->SetMenuBarVisible(true);
        tlw->SetStatusBarVisible(OPTION(kGenStatusBar));

        if (!cursz_valid) {
            cursz = tlw->minimumSize();
            curpos = QPoint();
        }

        if (was_maximized) {
            tlw->showMaximized();
        } else {
            tlw->resize(cursz);
            if (!curpos.isNull())
                tlw->move(curpos);
        }
        AdjustMinSize();
    } else {
        // close all non-modal dialogs
        while (!tlw->popups.empty())
            tlw->popups.front()->close();

        // mouse stays blank whenever full-screen
        HidePointer();
        cursz_valid = true;
        cursz = tlw->size();
        curpos = tlw->pos();
        was_maximized = tlw->isMaximized();
        LowerMinSize();

        tlw->SetMenuBarVisible(false);
        tlw->SetStatusBarVisible(false);
        tlw->showFullScreen();
    }
}

void GameArea::focusOutEvent(QFocusEvent* event)
{
    vbamApp().emulated_gamepad()->Reset();
    QWidget::focusOutEvent(event);
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

    // when the game is paused like this, we should not allow any input to
    // remain pressed, because they could be released outside of the game zone.
    vbamApp().emulated_gamepad()->Reset();

    if (loaded != IMAGE_UNKNOWN)
        soundPause();
}

void GameArea::Resume()
{
    if (!paused)
        return;

    paused = false;
    SuspendScreenSaver();

    if (loaded != IMAGE_UNKNOWN)
        soundResume();

    setFocus();
    RequestMore();
}

void GameArea::RequestMore()
{
    if (!idle_timer_.isActive())
        idle_timer_.start();
}

QWidget* GameArea::PanelWidget() const
{
    return panel ? panel->GetWindow() : nullptr;
}

void GameArea::OnIdle()
{
    const QString pl = vbamApp().pending_load;
    MainWindow* mf = vbamApp().frame;
    if (!mf)
        return;

    // The OSD text expires on its own in the draw path, but the status bar copy
    // does not; take it down here, on the main thread.
    if (systemGetClock() - osdtime >= OSD_TIME)
        systemClearStatusMessage();

    if (!pl.isEmpty()) {
        // clear before LoadGame() so a nested tick cannot re-enter it.
        vbamApp().pending_load.clear();
        LoadGame(pl);

#if defined(VBAM_ENABLE_DEBUGGER)
        if (OPTION(kPrefGDBBreakOnLoad)) {
            mf->GDBBreak();
        }

        if (debugger && loaded != IMAGE_GBA) {
            vbam::LogError(TR("Not a valid Game Boy Advance cartridge"));
            UnloadGame();
        }
#endif  // defined(VBAM_ENABLE_DEBUGGER)
    }

    if (!emusys) {
        // No game loaded, can't create panel
        return;
    }

    // Handle deferred panel reset (set by option observer callbacks) BEFORE
    // running emulation.
    if (pending_panel_reset_) {
        pending_panel_reset_ = false;
        ResetPanel();
        RequestMore();
        return;
    }

    // Keep systemColorDepth in sync with the bit depth option, but only when no
    // plugin filter is managing color depth.
    if (OPTION(kDispFilter) != config::Filter::kPlugin) {
        int newColorDepth = (OPTION(kBitDepth) + 1) << 3;
        if (newColorDepth != systemColorDepth) {
            systemColorDepth = newColorDepth;
            if (systemColorDepth == 24 || systemColorDepth == 32) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
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
                systemRedShift = 10;
                systemGreenShift = 5;
                systemBlueShift = 0;
                RGB_LOW_BITS_MASK = 0x0421;
            }
        }
    }

    if (schedule_audio_restart_) {
        soundShutdown();
        {
            vbam::LogNull no_sound_error_dialog;
            soundInit();
        }
        schedule_audio_restart_ = false;
    }

    if (!panel) {
        panel = NewPanelForRenderMethod(OPTION(kDispRenderMethod));
        if (!panel)
            return;

        QWidget* w = panel->GetWindow();
        w->installEventFilter(this);
        w->setMouseTracking(true);

        if (gopts.max_scale)
            w->setMaximumSize(basic_width * gopts.max_scale, basic_height * gopts.max_scale);

        // if user changed Display/Scale config, this needs to run
        AdjustMinSize();
        AdjustSize(false);

        layout_->addWidget(w);
        w->show();
        resizeEvent(nullptr);

        if (pointer_blanked)
            w->setCursor(Qt::BlankCursor);

        w->setFocus();

        // generate system color maps (after output module init)
        UpdateLcdFilter();

        // Let the panel fully initialize before running emulation.
        RequestMore();
        return;
    }

    // If the active renderer failed to initialize (no device / context /
    // swapchain), fall back to the next renderer in the platform priority
    // list. The option write fires the render observer, which schedules a
    // panel reset; the next idle tick recreates the panel with the new method.
    if (panel->DrawingInitFailed()) {
        EvaluateRenderer();
        RequestMore();
        return;
    }

    // Resume emulation if we paused for a panel reset and the panel is now ready
    if (pending_resume_after_panel_ && panel) {
        pending_resume_after_panel_ = false;
        Resume();
    }

    if (!paused && panel) {
        if (!g_pix) {
            vbam::LogDebug(QStringLiteral("g_pix is NULL, skipping emulation frame"));
            return;
        }

        HidePointer();
        HideMenuBar();
        RequestMore();

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

        // Drain joystick events so the joypad state is current for this frame.
        if (vbamApp().sdl_poller())
            vbamApp().sdl_poller()->Poll();

        emusys->emuMain(emusys->emuCount);

        // First-launch display-filter probe; no-op unless armed.
        StepFilterProbe();
#ifndef NO_LINK
        // GB sessions need this too: it drains a deferred link close.
        if ((loaded == IMAGE_GBA || loaded == IMAGE_GB) && GetLinkMode() != LINK_DISCONNECTED)
            CheckLinkConnection();
#endif
    } else {
        was_paused = true;
        ShowMenuBar();
    }

    if (do_rewind && emusys && emusys->emuWriteMemState) {
        if (!rewind_mem) {
            rewind_mem = (char*)malloc(NUM_REWINDS * REWIND_SIZE);
            num_rewind_states = next_rewind_state = 0;
        }

        if (!rewind_mem) {
            vbam::LogError(TR("No memory for rewinding"));
            mf->close();
            return;
        }

        long resize;

        if (!emusys->emuWriteMemState(&rewind_mem[REWIND_SIZE * next_rewind_state],
                REWIND_SIZE, resize /* actual size */))
            // if you see a lot of these, maybe increase REWIND_SIZE
            vbam::LogInfo(TR("Error writing rewind state"));
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

void GameArea::OnInputBatch(const widgets::UserInputBatch& batch) {
    // Keyboard inputs are applied synchronously through the sync sink wired in
    // VbamApp before this batch is delivered. Joystick inputs do NOT go
    // through the sink, so update the gamepad state for those here.
    bool emulated_key_pressed = false;
    const config::Bindings* const bindings = vbamApp().bindings();
    for (const auto& event_data : batch.data) {
        if (event_data.input.is_keyboard()) {
            const auto command = bindings->CommandForInput(event_data.input);
            if (command && command->is_game()) {
                emulated_key_pressed = true;
            }
        } else if (event_data.pressed) {
            if (vbamApp().emulated_gamepad()->OnInputPressed(event_data.input)) {
                emulated_key_pressed = true;
            }
        } else {
            if (vbamApp().emulated_gamepad()->OnInputReleased(event_data.input)) {
                emulated_key_pressed = true;
            }
        }
    }

    if (emulated_key_pressed && emusys && !paused)
        RequestMore();
}

DrawingPanelBase* GameArea::NewPanelForRenderMethod(config::RenderMethod method) {
    switch (method) {
        case config::RenderMethod::kOpenGL:
            return new GLDrawingPanel(this, basic_width, basic_height);
        case config::RenderMethod::kSDL:
            return new SDLDrawingPanel(this, basic_width, basic_height);
#if defined(_WIN32)
#if !defined(NO_D3D12)
        case config::RenderMethod::kDirect3d12:
            return new DX12DrawingPanel(this, basic_width, basic_height);
#endif
#if !defined(NO_D3D)
        case config::RenderMethod::kDirect3d:
            return new DXDrawingPanel(this, basic_width, basic_height);
#endif
#elif defined(__APPLE__)
        case config::RenderMethod::kQuartz2d:
            return new QuartzDrawingPanel(this, basic_width, basic_height);
#ifndef NO_METAL
        case config::RenderMethod::kMetal:
            return new MetalDrawingPanel(this, basic_width, basic_height);
#endif
#endif
#ifndef NO_VULKAN
        case config::RenderMethod::kVulkan:
            return new VKDrawingPanel(this, basic_width, basic_height);
#endif
        case config::RenderMethod::kSimple:
        case config::RenderMethod::kLast:
            break;
    }
    return new SoftwareDrawingPanel(this, basic_width, basic_height);
}

void GameArea::EvaluateRenderer() {
    if (!panel || !panel->DrawingInitFailed())
        return;

    using RM = config::RenderMethod;
    const RM current = OPTION(kDispRenderMethod);

    // Per-platform renderer priority. Only methods compiled in on this platform
    // appear (the #if guards mirror the RenderMethod enum).
    static const RM kRendererPriority[] = {
#if defined(_WIN32) && !defined(NO_D3D12)
        RM::kDirect3d12,
#endif
#if defined(__APPLE__) && !defined(NO_METAL)
        RM::kMetal,
#endif
#ifndef NO_VULKAN
        RM::kVulkan,
#endif
        RM::kSDL,
#ifndef NO_OGL
        RM::kOpenGL,
#endif
#if defined(_WIN32) && !defined(NO_D3D)
        RM::kDirect3d,  // legacy D3D9, after OpenGL
#endif
#if defined(__APPLE__)
        RM::kQuartz2d,
#endif
        RM::kSimple,
    };

    render_init_failed_.insert(current);
    for (const RM m : kRendererPriority) {
#ifndef NO_VULKAN
        // Skip Vulkan when its loader is absent at run time.
        if (m == RM::kVulkan && !VbamQtVulkanRuntimeAvailable())
            continue;
#endif
        if (m != current && render_init_failed_.count(m) == 0) {
            vbam::LogWarning(
                TR("The %1 renderer could not be initialized; trying the next available renderer.")
                    .arg(widgets::RenderMethodLabels().value(static_cast<int>(current))));
            OPTION(kDispRenderMethod) = m;  // observer schedules a panel reset
            return;
        }
    }

    // Nothing left to try: keep the (non-working) panel rather than loop.
    vbam::LogError(TR("No renderer could be initialized."));
}

void GameArea::paintEvent(QPaintEvent* event)
{
    (void)event;
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
}

void GameArea::resizeEvent(QResizeEvent* event)
{
    if (event)
        QWidget::resizeEvent(event);

    // Aspect-fit the panel inside the area when "retain aspect ratio" is on;
    // otherwise the layout stretches it to fill.
    QWidget* w = PanelWidget();
    if (!w)
        return;

    if (OPTION(kDispStretch)) {
        layout_->setAlignment(w, Qt::AlignCenter);
        const QSize area = size();
        if (area.isEmpty() || basic_width <= 0 || basic_height <= 0)
            return;
        const double sx = static_cast<double>(area.width()) / basic_width;
        const double sy = static_cast<double>(area.height()) / basic_height;
        const double s = std::min(sx, sy);
        const QSize fitted(static_cast<int>(std::floor(basic_width * s)),
                           static_cast<int>(std::floor(basic_height * s)));
        w->setFixedSize(fitted);
    } else {
        layout_->setAlignment(w, Qt::Alignment());
        w->setMinimumSize(0, 0);
        w->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        if (gopts.max_scale)
            w->setMaximumSize(basic_width * gopts.max_scale, basic_height * gopts.max_scale);
    }
}

bool GameArea::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == PanelWidget()) {
        switch (event->type()) {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::Wheel:
                MouseActivity();
                break;
            case QEvent::FocusOut:
                vbamApp().emulated_gamepad()->Reset();
                break;
            case QEvent::ContextMenu:
                // Let the main window pop up its context menu.
                QApplication::sendEvent(vbamApp().frame, event);
                return true;
            default:
                break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void GameArea::MouseActivity()
{
    mouse_active_time = systemGetClock();

    const QPoint cur_pos = QCursor::pos();

    // Ignore small movements.
    if (std::abs(cur_pos.x() - mouse_last_pos.x()) >= 11 ||
        std::abs(cur_pos.y() - mouse_last_pos.y()) >= 11) {
        ShowPointer();
        ShowMenuBar();
    }

    mouse_last_pos = cur_pos;
}

void GameArea::mouseMoveEvent(QMouseEvent* event)
{
    MouseActivity();
    QWidget::mouseMoveEvent(event);
}

void GameArea::mousePressEvent(QMouseEvent* event)
{
    mouse_active_time = systemGetClock();
    ShowPointer();
    ShowMenuBar();
    QWidget::mousePressEvent(event);
}

void GameArea::wheelEvent(QWheelEvent* event)
{
    mouse_active_time = systemGetClock();
    ShowPointer();
    ShowMenuBar();
    QWidget::wheelEvent(event);
}

void GameArea::ShowPointer()
{
    if (!pointer_blanked || fullscreen) return;

    pointer_blanked = false;
    unsetCursor();

    if (QWidget* w = PanelWidget())
        w->unsetCursor();
}

void GameArea::HidePointer()
{
    if (pointer_blanked || !main_frame) return;

    // FIXME: make time configurable
    if ((fullscreen || (systemGetClock() - mouse_active_time) > 3000) &&
        !(main_frame->MenusOpened() || main_frame->DialogOpened())) {
        pointer_blanked = true;
        setCursor(Qt::BlankCursor);

        if (QWidget* w = PanelWidget())
            w->setCursor(Qt::BlankCursor);
    }
}

// We do not hide the menubar on mac, on mac it is not part of the main frame
// and the user can adjust hiding behavior herself.
void GameArea::HideMenuBar()
{
#if !defined(__APPLE__)
    if (!main_frame || menu_bar_hidden || !gopts.hide_menu_bar) return;

    if (((systemGetClock() - mouse_active_time) > 3000) && !main_frame->MenusOpened()) {
        main_frame->SetMenuBarVisible(false);
        menu_bar_hidden = true;
    }
#endif
}

void GameArea::ShowMenuBar()
{
#if !defined(__APPLE__)
    if (!main_frame || !menu_bar_hidden) return;

    if (!fullscreen)
        main_frame->SetMenuBarVisible(true);
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
    int DCCP = 0;

    switch (OPTION(kDispColorCorrectionProfile)) {
        case config::ColorCorrectionProfile::kSRGB:
            DCCP = 0;
            break;

        case config::ColorCorrectionProfile::kDCI:
            DCCP = 1;
            break;

        case config::ColorCorrectionProfile::kRec2020:
            DCCP = 2;
            break;

        case config::ColorCorrectionProfile::kLast:
            DCCP = 0;
            break;
    }

    const int gba_variant = static_cast<int>(OPTION(kGBALCDFilterVariant));
    const int gb_variant = static_cast<int>(OPTION(kGBLCDFilterVariant));
    const float gba_darken = ((float)OPTION(kGBADarken)) / 100;
    const float gb_lighten = ((float)OPTION(kGBLighten)) / 100;

    // NSO GBC shares one matrix across all three profiles, so Rec2020 gains no
    // gamut over sRGB and only clips. In auto mode the profile is not the
    // user's pick, so use sRGB; an explicit pick is honored.
    int gb_DCCP = DCCP;
    if (gb_variant == kGbcFilterNso && OPTION(kDispColorCorrectionAuto))
        gb_DCCP = 0;

    if (loaded == IMAGE_GBA) {
        gbafilter_set_params(DCCP, gba_darken, gba_variant);
        gbafilter_update_colors(OPTION(kGBALCDFilter));
    } else if (loaded == IMAGE_GB) {
        if (gbHardware & 4) { // Emulated Hardware is SGB
            // Prevent CGB LCD filter from applying
            gbcfilter_update_colors(false);
        } else if (gbHardware & 8) { // Emulated Hardware is GBA
            // Apply GBA LCD filter instead of the GBC LCD Filter
            gbafilter_set_params(DCCP, gba_darken, gba_variant);
            gbafilter_update_colors(OPTION(kGBLCDFilter));
        } else {
            // Apply GBC LCD filter for GB/GBC modes
            gbcfilter_set_params(gb_DCCP, gb_lighten, gb_variant);
            gbcfilter_update_colors(OPTION(kGBLCDFilter));
        }
    } else {
        gbafilter_set_params(DCCP, gba_darken, gba_variant);
        gbcfilter_set_params(gb_DCCP, gb_lighten, gb_variant);
        gbafilter_update_colors(false);
        gbcfilter_update_colors(false);
    }
}

void GameArea::SuspendScreenSaver() {
    if (screensaver_suspended || !gopts.suspend_screensaver || !emulating)
        return;
#if defined(__APPLE__)
    IOPMAssertionID id = kIOPMNullAssertionID;
    const IOReturn r = IOPMAssertionCreateWithName(
        kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
        CFSTR("VisualBoyAdvance-M is running a game"), &id);
    if (r == kIOReturnSuccess) {
        g_screensaver_assertion = id;
        screensaver_suspended = true;
    }
#elif defined(_WIN32)
    // Handled by SetThreadExecutionState in the Windows build.
    screensaver_suspended = true;
#else
    // xdg-screensaver works for X11 and most Wayland desktops.
    if (QWindow* win = window()->windowHandle()) {
        QProcess::startDetached(QStringLiteral("xdg-screensaver"),
                                {QStringLiteral("suspend"), QString::number(win->winId())});
        screensaver_suspended = true;
    }
#endif
}

void GameArea::UnsuspendScreenSaver() {
    if (!screensaver_suspended)
        return;
#if defined(__APPLE__)
    if (g_screensaver_assertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(g_screensaver_assertion);
        g_screensaver_assertion = kIOPMNullAssertionID;
    }
#elif defined(_WIN32)
#else
    if (QWindow* win = window()->windowHandle()) {
        QProcess::startDetached(QStringLiteral("xdg-screensaver"),
                                {QStringLiteral("resume"), QString::number(win->winId())});
    }
#endif
    screensaver_suspended = false;
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
    soundSetVolume(vbamApp().mute ? 0.0 : (float)volume / 100.0);
    systemScreenMessage(TR("Volume: %1 %").arg(volume));
}

// ---------------------------------------------------------------------------
// A/V recording (ffmpeg)
// ---------------------------------------------------------------------------

#ifndef NO_FFMPEG
static QString media_err(recording::MediaRet ret)
{
    switch (ret) {
        case recording::MRET_OK:
            return QString();

        case recording::MRET_ERR_NOMEM:
            return TR("Memory allocation error");

        case recording::MRET_ERR_NOCODEC:
            return TR("Error initializing codec");

        case recording::MRET_ERR_FERR:
            return TR("Error writing to output file");

        case recording::MRET_ERR_FMTGUESS:
            return TR("Can't guess output format from file name");

        default:
            return TR("Programming error; aborting!");
    }
}

void GameArea::StartVidRecording(const QString& fname)
{
    recording::MediaRet ret;

    vid_rec.SetSampleRate(soundGetSampleRate());
    if ((ret = vid_rec.Record(vbam::ToPath(fname).c_str(), basic_width, basic_height,
                              systemColorDepth)) != recording::MRET_OK) {
        vbam::LogError(TR("Unable to begin recording to %1 (%2)").arg(fname, media_err(ret)));
    } else {
        MainWindow* mf = vbamApp().frame;
        mf->cmd_enable &= ~(CMDEN_NVREC | CMDEN_NREC_ANY);
        mf->cmd_enable |= CMDEN_VREC;
        mf->enable_menus();
    }
}

void GameArea::StopVidRecording()
{
    vid_rec.Stop();
    MainWindow* mf = vbamApp().frame;
    mf->cmd_enable &= ~CMDEN_VREC;
    mf->cmd_enable |= CMDEN_NVREC;

    if (!(mf->cmd_enable & (CMDEN_VREC | CMDEN_SREC)))
        mf->cmd_enable |= CMDEN_NREC_ANY;

    mf->enable_menus();
}

void GameArea::StartSoundRecording(const QString& fname)
{
    recording::MediaRet ret;

    snd_rec.SetSampleRate(soundGetSampleRate());
    if ((ret = snd_rec.Record(vbam::ToPath(fname).c_str())) != recording::MRET_OK) {
        vbam::LogError(TR("Unable to begin recording to %1 (%2)").arg(fname, media_err(ret)));
    } else {
        MainWindow* mf = vbamApp().frame;
        mf->cmd_enable &= ~(CMDEN_NSREC | CMDEN_NREC_ANY);
        mf->cmd_enable |= CMDEN_SREC;
        mf->enable_menus();
    }
}

void GameArea::StopSoundRecording()
{
    snd_rec.Stop();
    MainWindow* mf = vbamApp().frame;
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
        vbam::LogError(TR("Error in audio / video recording (%1); aborting").arg(media_err(ret)));
        StopVidRecording();
    }

    if ((ret = snd_rec.AddFrame(data, length)) != recording::MRET_OK) {
        vbam::LogError(TR("Error in audio recording (%1); aborting").arg(media_err(ret)));
        StopSoundRecording();
    }
}

void GameArea::AddFrame(const uint8_t* data)
{
    recording::MediaRet ret;

    if ((ret = vid_rec.AddFrame(data)) != recording::MRET_OK) {
        vbam::LogError(TR("Error in video recording (%1); aborting").arg(media_err(ret)));
        StopVidRecording();
    }
}
#endif

// ---------------------------------------------------------------------------
// Game (input) recording / playback
// ---------------------------------------------------------------------------

void GameArea::StartGameRecording(const QString& fname)
{
    systemStartGameRecording(fname, MV_FORMAT_ID_VMV2);
}

void GameArea::StopGameRecording()
{
    systemStopGameRecording();
}

void GameArea::StartGamePlayback(const QString& fname)
{
    systemStartGamePlayback(fname, MV_FORMAT_ID_VMV);
}

void GameArea::StopGamePlayback()
{
    systemStopGamePlayback();
}

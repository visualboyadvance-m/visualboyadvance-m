// The main menu bar, built in code. Mirrors src/wx/xrc/MainMenu.xrc item for
// item (menus, submenus, separators, checkable and radio items, in order).
//
// Every command item is a QAction whose data() is its cmd::Id; triggering it
// runs MainWindow::ExecuteCommand(). Menu labels use Qt's '&' mnemonics (the
// XRC '_' markers converted). Shortcuts are NOT set on the actions: they are
// resolved from config::Bindings through the InputDispatcher, and only shown
// in the menu as text (see MainWindow::ResetMenuAccelerators).

#include "qt/main-window.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>

#include "qt/app.h"
#include "qt/config/cmdtab.h"
#include "qt/config/option-proxy.h"
#include "qt/opts.h"

namespace {

// Converts an XRC-style label ("Open _Game Boy...", "&amp;Map") to a Qt label
// ("Open &Game Boy...", "&Map"). A '_' followed by a letter or digit is the
// mnemonic marker; other underscores are kept.
QString MenuLabel(const QString& xrc_label) {
    QString label = xrc_label;
    label.replace("&amp;", "&");
    QString out;
    out.reserve(label.size() + 1);
    for (int i = 0; i < label.size(); i++) {
        const QChar c = label[i];
        if (c == '_' && i + 1 < label.size() && label[i + 1].isLetterOrNumber()) {
            out += '&';
        } else if (c == '&' && (i + 1 >= label.size() || label[i + 1] != '&')) {
            out += '&';
        } else {
            out += c;
        }
    }
    return out;
}

}  // namespace

QAction* MainWindow::AddCommandAction(QMenu* menu, int cmd_id, bool checkable,
                                      QActionGroup* group) {
    // The label comes from the command table's helper string unless the menu
    // definition supplies one (see BuildMenus).
    // The action is parented to a hidden widget that never has focus, with a
    // Qt::WidgetShortcut context: Qt's own shortcut map then never fires it,
    // so the shortcut set by ResetActionAccelerator() is display-only and the
    // command runs exactly once, through the bindings (InputDispatcher). The
    // native macOS menu bar is the exception: Cocoa handles key equivalents
    // itself before Qt sees the key, and fires the action directly -- still
    // exactly once, since the key event never reaches the keyboard handler.
    if (!action_host_) {
        action_host_ = new QWidget(this);
        action_host_->hide();
    }
    QAction* action = new QAction(config::GetCommandHelper(cmd_id), action_host_);
    menu->addAction(action);
    action->setData(cmd_id);
    action->setCheckable(checkable);
    if (group) {
        group->addAction(action);
    }
    action->setShortcutContext(Qt::WidgetShortcut);
    connect(action, &QAction::triggered, this, &MainWindow::OnActionTriggered);
    actions_[cmd_id] = action;
    return action;
}

void MainWindow::BuildMenus() {
    QMenuBar* bar = menuBar();
    int menu_index = 0;

    // Adds a command item with an explicit XRC-style label.
    auto item = [this](QMenu* menu, int cmd_id, const QString& label, bool checkable = false,
                       QActionGroup* group = nullptr) -> QAction* {
        QAction* action = AddCommandAction(menu, cmd_id, checkable, group);
        action->setText(MenuLabel(label));
        base_labels_[cmd_id] = action->text();
        return action;
    };
    auto check = [&item](QMenu* menu, int cmd_id, const QString& label) -> QAction* {
        return item(menu, cmd_id, label, true, nullptr);
    };
    auto submenu = [](QMenu* parent, const QString& label) -> QMenu* {
        return parent->addMenu(MenuLabel(label));
    };
    auto top = [&](const QString& label) -> QMenu* {
        QMenu* menu = bar->addMenu(MenuLabel(label));
        connect(menu, &QMenu::aboutToShow, this, &MainWindow::OnMenuAboutToShow);
        connect(menu, &QMenu::aboutToHide, this, &MainWindow::OnMenuAboutToHide);
        if (menu_index < static_cast<int>(sizeof(menus_) / sizeof(menus_[0]))) {
            menus_[menu_index++] = menu;
        }
        return menu;
    };

    // ---- File ---------------------------------------------------------------
    {
        QMenu* file = top(tr("_File"));
        item(file, cmd::kOpen, tr("_Open..."));
        item(file, cmd::kOpenGB, tr("Open _Game Boy..."));
        item(file, cmd::kOpenGBC, tr("Open Game Boy _Color..."));

        recent_menu = submenu(file, tr("Open rece_nt"));
        item(recent_menu, cmd::kRecentReset, tr("_Reset recent list"));
        check(recent_menu, cmd::kRecentFreeze, tr("_Freeze recent list"));
        recent_menu->addSeparator();
        for (int i = 0; i < RecentFiles::kMaxFiles; i++) {
            QAction* a = item(recent_menu, cmd::kFile1 + i,
                              QString("&%1 %2").arg((i + 1) % 10).arg(tr("(empty)")));
            a->setVisible(false);
        }

        item(file, cmd::kRomInformation, tr("ROM in_formation..."));
        file->addSeparator();

        QMenu* ereader = submenu(file, tr("_e-Reader"));
        item(ereader, cmd::kResetLoadingDotCodeFile, tr("_Reset Loading Dot Code"));
        item(ereader, cmd::kSetLoadingDotCodeFile, tr("_Load Dot Code..."));
        item(ereader, cmd::kResetSavingDotCodeFile, tr("_Reset Saving Dot Code"));
        item(ereader, cmd::kSetSavingDotCodeFile, tr("_Save Dot Code..."));
        file->addSeparator();

        QMenu* load = submenu(file, tr("_Load state"));
        item(load, cmd::kLoadGameRecent, tr("Most _recent"));
        item(load, cmd::kLoadGameSlot, tr("Load current state slot"));
        check(load, cmd::kLoadGameAutoLoad, tr("_Auto load most recent"));
        load->addSeparator();
        for (int i = 0; i < 10; i++) {
            const QString label = i == 9 ? QString("1_0") : QString("_%1").arg(i + 1);
            loadst_mi[i] = item(load, cmd::kLoadGame01 + i, label);
        }
        load->addSeparator();
        item(load, cmd::kLoad, tr("From _File..."));
        load->addSeparator();
        check(load, cmd::kKeepSaves, tr("Do not change _battery save"));
        check(load, cmd::kKeepCheats, tr("Do not change _cheat list"));

        QMenu* save = submenu(file, tr("_Save state"));
        item(save, cmd::kSaveGameOldest, tr("_Oldest slot"));
        item(save, cmd::kSaveGameSlot, tr("Save current state slot"));
        item(save, cmd::kIncrGameSlotSave, tr("Increase state slot number and save"));
        save->addSeparator();
        for (int i = 0; i < 10; i++) {
            const QString label = i == 9 ? QString("1_0") : QString("_%1").arg(i + 1);
            savest_mi[i] = item(save, cmd::kSaveGame01 + i, label);
        }
        save->addSeparator();
        item(save, cmd::kSave, tr("To _File..."));

        item(file, cmd::kIncrGameSlot, tr("Increase state slot number"));
        item(file, cmd::kDecrGameSlot, tr("Decrease state slot number"));
        file->addSeparator();

        QMenu* import = submenu(file, tr("_Import"));
        item(import, cmd::kImportBatteryFile, tr("_Battery file..."));
        item(import, cmd::kImportGamesharkCodeFile, tr("Game Shark _code file..."));
        item(import, cmd::kImportGamesharkActionReplaySnapshot, tr("_Game Shark snapshot..."));

        QMenu* exp = submenu(file, tr("_Export"));
        item(exp, cmd::kExportBatteryFile, tr("_Battery file..."));
        item(exp, cmd::kExportGamesharkSnapshot, tr("_Game Shark snapshot..."));
        file->addSeparator();

        item(file, cmd::kScreenCapture, tr("Screen capt_ure..."));

        QMenu* record = submenu(file, tr("_Record"));
#ifndef NO_FFMPEG
        item(record, cmd::kRecordSoundStartRecording, tr("Start _sound recording..."));
        item(record, cmd::kRecordSoundStopRecording, tr("Stop s_ound recording"));
        item(record, cmd::kRecordAVIStartRecording, tr("Start _video recording..."));
        item(record, cmd::kRecordAVIStopRecording, tr("Stop v_ideo recording"));
#endif
        item(record, cmd::kRecordMovieStartRecording, tr("Start _game recording..."));
        item(record, cmd::kRecordMovieStopRecording, tr("Stop g_ame recording"));

        QMenu* play = submenu(file, tr("_Play"));
        item(play, cmd::kPlayMovieStartPlaying, tr("Start playing _movie..."));
        item(play, cmd::kPlayMovieStopPlaying, tr("Stop playing m_ovie"));
        file->addSeparator();

        item(file, cmd::kClose, tr("_Close"));
        QAction* quit = item(file, cmd::kExit, tr("_Quit"));
        quit->setMenuRole(QAction::QuitRole);
    }

    // ---- Emulation ----------------------------------------------------------
    {
        QMenu* emu = top(tr("_Emulation"));
        check(emu, cmd::kPause, tr("_Pause"));
        item(emu, cmd::kNextFrame, tr("_Next frame"));
        item(emu, cmd::kRewind, tr("Re_wind"));
        emu->addSeparator();
        item(emu, cmd::kToggleFullscreen, tr("Toggle _full screen"));
        emu->addSeparator();
        check(emu, cmd::kEmulatorSpeedupToggle, tr("_Turbo mode"));
        check(emu, cmd::kVSync, tr("_VSync"));
        check(emu, cmd::kFrameSkipAuto, tr("_Auto skip frames"));
        emu->addSeparator();
        check(emu, cmd::kSkipIntro, tr("_Skip BIOS"));
        check(emu, cmd::kApplyPatches, tr("_Auto IPS / UPS / IPF patch"));
        check(emu, cmd::kPauseWhenInactive, tr("_Pause when inactive"));
        emu->addSeparator();
        item(emu, cmd::kReset, tr("_Reset"));
    }

    // ---- Options ------------------------------------------------------------
    {
        QMenu* options = top(tr("_Options"));

#ifndef NO_LINK
        QMenu* link = submenu(options, tr("_Link"));
        item(link, cmd::kLanLink, tr("Start _Link..."));
        QMenu* link_type = submenu(link, tr("_Type"));
        QActionGroup* link_group = new QActionGroup(this);
        link_group->setExclusive(true);
        item(link_type, cmd::kLinkType0Nothing, tr("_Nothing"), true, link_group);
        item(link_type, cmd::kLinkType1Cable, tr("_Cable"), true, link_group);
        item(link_type, cmd::kLinkType2Wireless, tr("_Wireless"), true, link_group);
        item(link_type, cmd::kLinkType3GameCube, "_Game Cube", true, link_group);
        item(link_type, cmd::kLinkType4Gameboy, "_Game Boy", true, link_group);
        check(link, cmd::kLinkProto, tr("_Local mode"));
        check(link, cmd::kLinkAuto, tr("_Link at boot"));
        check(link, cmd::kSpeedOn, tr("_Speed hack"));
        item(link, cmd::kLinkConfigure, tr("_Configure..."));
#endif

        QMenu* video = submenu(options, tr("_Video"));
        item(video, cmd::kDisplayConfigure, tr("_Configure..."));
        video->addSeparator();
        check(video, cmd::kFullscreen, tr("_Start in full screen"));
        QMenu* scaled = submenu(video, tr("_Scaled resize"));
        item(scaled, cmd::kSetSize1x, tr("_1x"));
        item(scaled, cmd::kSetSize2x, tr("_2x"));
        item(scaled, cmd::kSetSize3x, tr("_3x"));
        item(scaled, cmd::kSetSize4x, tr("_4x"));
        item(scaled, cmd::kSetSize5x, tr("_5x"));
        item(scaled, cmd::kSetSize6x, tr("_6x"));
        item(video, cmd::kChangeFilter, tr("Change pixel filter"));
        item(video, cmd::kChangeIFB, tr("Change interframe blending"));
        check(video, cmd::kRetainAspect, tr("_Retain aspect ratio"));
        video->addSeparator();
        check(video, cmd::kBilinear, tr("_Bilinear filter"));
        video->addSeparator();
        check(video, cmd::kKeepOnTop, tr("_Keep window on top"));
        video->addSeparator();
        check(video, cmd::kNoStatusMsg, tr("_Disable on-screen display"));

        QMenu* audio = submenu(options, tr("_Audio"));
        item(audio, cmd::kSoundConfigure, tr("_Configure..."));
        item(audio, cmd::kIncreaseVolume, tr("_Increase volume"));
        item(audio, cmd::kDecreaseVolume, tr("_Decrease volume"));
        item(audio, cmd::kToggleSound, tr("_Toggle sound"));
        audio->addSeparator();
        check(audio, cmd::kGBASoundInterpolation, tr("_Game Boy Advance sound interpolation"));
        audio->addSeparator();
        check(audio, cmd::kGBEnhanceSound, tr("_Game Boy sound enhancement"));
        check(audio, cmd::kGBSurround, tr("_Game Boy surround sound effect"));
        check(audio, cmd::kGBDeclicking, tr("_Game Boy sound declicking"));

        QMenu* input = submenu(options, tr("_Input"));
        item(input, cmd::kJoypadConfigure, tr("_Configure..."));
#if !defined(__APPLE__)
        check(input, cmd::kAllowKeyboardBackgroundInput, tr("Allow _keyboard background input"));
#endif
        check(input, cmd::kAllowJoystickBackgroundInput, tr("Allow _joystick background input"));
        input->addSeparator();
        QMenu* autofire_menu = submenu(input, tr("_Autofire"));
        check(autofire_menu, cmd::kJoypadAutofireA, "_A");
        check(autofire_menu, cmd::kJoypadAutofireB, "_B");
        check(autofire_menu, cmd::kJoypadAutofireL, "_L");
        check(autofire_menu, cmd::kJoypadAutofireR, "_R");
        QMenu* autohold_menu = submenu(input, tr("_Autohold"));
        check(autohold_menu, cmd::kJoypadAutoholdUp, tr("Up"));
        check(autohold_menu, cmd::kJoypadAutoholdDown, tr("Down"));
        check(autohold_menu, cmd::kJoypadAutoholdLeft, tr("Left"));
        check(autohold_menu, cmd::kJoypadAutoholdRight, tr("Right"));
        check(autohold_menu, cmd::kJoypadAutoholdA, tr("_A"));
        check(autohold_menu, cmd::kJoypadAutoholdB, tr("_B"));
        check(autohold_menu, cmd::kJoypadAutoholdL, tr("_L"));
        check(autohold_menu, cmd::kJoypadAutoholdR, tr("_R"));
        check(autohold_menu, cmd::kJoypadAutoholdSelect, tr("Select"));
        check(autohold_menu, cmd::kJoypadAutoholdStart, tr("Start"));

        QMenu* gba = submenu(options, "_Game Boy Advance");
        item(gba, cmd::kGameBoyAdvanceConfigure, tr("Configure..."));
        gba->addSeparator();
        check(gba, cmd::kRtc, tr("_Real-time clock"));
        check(gba, cmd::kBootRomEn, tr("_Use BIOS file"));
        check(gba, cmd::kAGBPrinter, tr("_Debug print"));
        check(gba, cmd::kGBALcdFilter, tr("_LCD Filter"));

        QMenu* gb = submenu(options, "_Game Boy");
        item(gb, cmd::kGameBoyConfigure, tr("Configure..."));
        gb->addSeparator();
        check(gb, cmd::kGBLcdFilter, tr("_LCD Filter"));
        check(gb, cmd::kColorizerHack, tr("_Game Boy Colorizer Hack [requires restart]"));
        check(gb, cmd::kPrinter, tr("_Game Boy printer"));
        check(gb, cmd::kPrintGather, tr("_Gather a full page before printing"));
        check(gb, cmd::kPrintSnap, tr("_Save printouts as screen captures"));
        gb->addSeparator();
        check(gb, cmd::kBootRomGB, tr("_Use Game Boy BIOS file [requires restart]"));
        check(gb, cmd::kBootRomGBC, tr("_Use Game Boy Color BIOS file"));

        item(options, cmd::kGeneralConfigure, tr("_General..."));
        item(options, cmd::kSpeedupConfigure, tr("_Speedup / Turbo..."));
        item(options, cmd::kEmulatorDirectories, tr("D_irectories..."));
        QAction* customize = item(options, cmd::kCustomize, tr("_Key Shortcuts..."));
        customize->setMenuRole(QAction::PreferencesRole);

        QMenu* ui = submenu(options, tr("UI Settings"));
        check(ui, cmd::kStatusBar, tr("Enable _Status bar"));
#if !defined(__APPLE__)
        // Meaningless with the native macOS menu bar.
        check(ui, cmd::kHideMenuBar, tr("Hide _Menu Bar"));
#endif
        check(ui, cmd::kOnScreenController, tr("Show On-Screen _Controller"));
        check(ui, cmd::kSuspendScreenSaver, tr("Suspend _Screen Saver"));
    }

    // ---- Tools --------------------------------------------------------------
    {
        QMenu* tools = top(tr("_Tools"));

        QMenu* cheats = submenu(tools, tr("_Cheats"));
        item(cheats, cmd::kCheatsList, tr("List _cheats..."));
        item(cheats, cmd::kCheatsSearch, tr("Find c_heat..."));
        cheats->addSeparator();
        check(cheats, cmd::kCheatsAutoSaveLoad, tr("A_utomatically save / load cheats"));
        check(cheats, cmd::kCheatsEnable, tr("_Enable cheats"));
        tools->addSeparator();

#if defined(VBAM_ENABLE_DEBUGGER)
        QMenu* gdb = submenu(tools, "_GDB");
        item(gdb, cmd::kDebugGDBBreak, tr("_Break into GDB"));
        gdb->addSeparator();
        item(gdb, cmd::kDebugGDBPort, tr("_Configure port..."));
        check(gdb, cmd::kDebugGDBBreakOnLoad, tr("_Break on load"));
        gdb->addSeparator();
        item(gdb, cmd::kDebugGDBDisconnect, tr("_Disconnect"));
#endif

        item(tools, cmd::kDisassemble, tr("_Disassemble..."));
#if defined(GBA_LOGGING)
        item(tools, cmd::kLogging, tr("_Logging..."));
#endif
        item(tools, cmd::kIOViewer, tr("_IO Viewer..."));
        item(tools, cmd::kMapViewer, tr("&Map Viewer..."));
        item(tools, cmd::kMemoryViewer, tr("M_emory Viewer..."));
        tools->addSeparator();

#if defined(VBAM_ENABLE_LUA)
        QMenu* lua = submenu(tools, tr("L_ua scripting"));
        item(lua, cmd::kLuaRunScript, tr("_Run script..."));
        item(lua, cmd::kLuaStopScript, tr("_Stop script"));
        lua->addSeparator();
        item(lua, cmd::kLuaConsole, tr("Show _console"));
        item(lua, cmd::kLuaEditor, tr("Show _editor"));
#endif

        item(tools, cmd::kOAMViewer, tr("_OAM Viewer..."));
        item(tools, cmd::kPaletteViewer, tr("_Palette Viewer..."));
        item(tools, cmd::kTileViewer, tr("_Tile Viewer..."));
        tools->addSeparator();

        QMenu* layers = submenu(tools, tr("_View Layers"));
        item(layers, cmd::kVideoLayersReset, tr("Show all video layers"));
        layers->addSeparator();
        check(layers, cmd::kVideoLayersBG0, "BG _0")->setChecked(true);
        check(layers, cmd::kVideoLayersBG1, "BG _1")->setChecked(true);
        check(layers, cmd::kVideoLayersBG2, "BG _2")->setChecked(true);
        check(layers, cmd::kVideoLayersBG3, "BG _3")->setChecked(true);
        check(layers, cmd::kVideoLayersOBJ, "_OBJ")->setChecked(true);
        check(layers, cmd::kVideoLayersWIN0, "_WIN 0")->setChecked(true);
        check(layers, cmd::kVideoLayersWIN1, "W_IN 1")->setChecked(true);
        check(layers, cmd::kVideoLayersOBJWIN, "O_BJ WIN")->setChecked(true);

        QMenu* channels = submenu(tools, tr("_Sound Channels"));
        check(channels, cmd::kSoundChannel1, tr("Channel _1"))->setChecked(true);
        check(channels, cmd::kSoundChannel2, tr("Channel _2"))->setChecked(true);
        check(channels, cmd::kSoundChannel3, tr("Channel _3"))->setChecked(true);
        check(channels, cmd::kSoundChannel4, tr("Channel _4"))->setChecked(true);
        check(channels, cmd::kDirectSoundA, tr("Direct Sound _A"))->setChecked(true);
        check(channels, cmd::kDirectSoundB, tr("Direct Sound _B"))->setChecked(true);
    }

    // ---- Help ---------------------------------------------------------------
    {
        QMenu* help = top(tr("_Help"));
        item(help, cmd::kBugReport, tr("Report _Bugs"));
        item(help, cmd::kFaq, tr("Visual Boy Advance-M Support _Forum"));
        item(help, cmd::kTranslate, tr("Translations"));
#if !defined(NO_ONLINEUPDATES)
        item(help, cmd::kUpdateEmu, tr("Check for updates"));
#endif
        item(help, cmd::kFactoryReset, tr("_Factory Reset..."));
        help->addSeparator();
        QAction* about = item(help, cmd::kAbout, tr("_About..."));
        about->setMenuRole(QAction::AboutRole);
    }

    // ---- Languages ----------------------------------------------------------
    {
        QMenu* languages = top(tr("_Languages"));
        QActionGroup* group = new QActionGroup(this);
        group->setExclusive(true);
        // Keep in sync with the kLanguages table in app.cpp and the
        // Language<N> handlers in cmd-handlers.cpp.
        static const char* const kLabels[] = {
            "Default Language",     "Spanish [Latin American]", "Spanish",
            "French [France]",      "Hebrew [Israel]",          "Hungarian [Hungary]",
            "Indonesian",           "Italian",                  "Korean [Korea]",
            "Polish [Poland]",      "Portuguese [Brazil]",      "Swedish",
            "Turkish",              "Ukrainian",                "Chinese [China]",
        };
        for (int i = 0; i < VbamApp::LanguageCount(); i++) {
            item(languages, cmd::kLanguage0 + i, tr(kLabels[i]), true, group);
        }
        check(languages, cmd::kExternalTranslations, tr("_Use external translations"));
    }
}

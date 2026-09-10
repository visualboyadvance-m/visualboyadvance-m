#include "qt/main-window.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextStream>
#include <QWindow>

#include <functional>

#include "core/base/check.h"
#include "core/base/system.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "qt/app.h"
#include "qt/config/bindings.h"
#include "qt/config/option-proxy.h"
#include "qt/config/option.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/opts.h"
#include "qt/sys.h"
#include "qt/viewers/log-dialog.h"

// Config dialogs (DIALOGS agent).
#include "qt/dialogs/accel-config.h"
#include "qt/dialogs/directories-config.h"
#include "qt/dialogs/display-config.h"
#include "qt/dialogs/game-boy-advance-config.h"
#include "qt/dialogs/game-boy-config.h"
#include "qt/dialogs/general-config.h"
#include "qt/dialogs/joypad-config.h"
#include "qt/dialogs/sound-config.h"
#include "qt/dialogs/speedup-config.h"
// Cheats / ROM info / link dialogs (DIALOGS2 agent).
#include "qt/dialogs/cheat-add.h"
#include "qt/dialogs/cheat-edit.h"
#include "qt/dialogs/cheat-list.h"
#include "qt/dialogs/cheat-search.h"
#include "qt/dialogs/code-select.h"
#include "qt/dialogs/export-sps.h"
#include "qt/dialogs/gb-rom-info.h"
#include "qt/dialogs/gba-rom-info.h"
#ifndef NO_LINK
#include "qt/dialogs/link-config.h"
#include "qt/dialogs/net-link.h"
#endif

namespace {

const char kDotDir[] = "visualboyadvance-m";

// Get system name string for the currently loaded ROM.
QString GetSystemName(GameArea* panel) {
    if (!panel)
        return "GameBoy Advance";

    IMAGE_TYPE game_type = panel->game_type();
    if (game_type == IMAGE_GBA)
        return "GameBoy Advance";

    // For Game Boy ROMs, check for GBC and SGB modes
    if (game_type == IMAGE_GB) {
        if (gbCgbMode)
            return "GameBoy Color";
        if (gbSgbMode)
            return "Super GameBoy";
        return "GameBoy";
    }

    return "GameBoy Advance";
}

// Strips the shortcut hint ("\tCtrl+O") from an action text.
QString StripAccel(const QString& text) {
    const int tab = text.indexOf('\t');
    return tab >= 0 ? text.left(tab) : text;
}

}  // namespace

// Defined in app.cpp.
bool VbamAppIsActive();

ModalPause::ModalPause() {
    vbamApp().frame->StartModal();
}

ModalPause::~ModalPause() {
    vbamApp().frame->StopModal();
}

MainWindow::MainWindow() : QMainWindow(nullptr) {
    cmd_enable = 0;
    setWindowTitle("VisualBoyAdvance-M");
    setAcceptDrops(true);
    setUnifiedTitleAndToolBarOnMac(true);

#ifndef NO_LINK
    gba_link_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kGBALinkHost, [this](config::Option*) { EnableNetworkMenu(); });
#endif
    keep_on_top_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kDispKeepOnTop, [this](config::Option*) { OnKeepOnTopChanged(); });
    status_bar_observer_ = std::make_unique<config::OptionsObserver>(
        config::OptionID::kGenStatusBar, [this](config::Option*) { OnStatusBarChanged(); });
}

MainWindow::~MainWindow() {
#ifndef NO_LINK
    CloseLink();
#endif
    vbam::SetLogUpdateCallback(nullptr);
    // sys.cpp guards its GUI access on the frame still existing.
    vbamApp().frame = nullptr;
    // The popups are children of this window and are destroyed with it.
}

// -----------------------------------------------------------------------------
// Setup

bool MainWindow::BindControls() {
    // The game area is the central widget; it hosts the drawing panel and
    // drives the emulation loop.
    panel = new GameArea(this);
    panel->SetMainFrame(this);
    setCentralWidget(panel);

    // Status bar: two fields like the wx frame.
    QStatusBar* sb = statusBar();
    status_field1_ = new QLabel(sb);
    sb->addPermanentWidget(status_field1_);
    sb->setVisible(OPTION(kGenStatusBar));
    SetStatusText("");

    BuildMenus();

    // Save all menu items in the command table and collect checkable items.
    for (cmditem& cmd_item : cmdtab) {
        auto it = actions_.find(cmd_item.cmd_id);
        cmd_item.action = it == actions_.end() ? nullptr : it->second;
        if (!cmd_item.action) {
            continue;
        }

        if (cmd_item.action->isCheckable()) {
            checkable_mi_t cmi = {cmd_item.cmd_id, cmd_item.action, 0, 0};
            checkable_mi.push_back(cmi);

            for (const config::Option& option : config::Option::All()) {
                if (cmd_item.cmd == option.command()) {
                    if (option.is_int()) {
                        MenuOptionIntMask(cmd_item.cmd_id, option.GetInt(), (1 << 0));
                    } else if (option.is_bool()) {
                        MenuOptionBool(cmd_item.cmd_id, option.GetBool());
                    }
                }
            }
        }
    }

    // Recent files.
    ResetRecentMenu();

    // Save/load state menu items were stored by BuildMenus().

    // just setting to UNLOAD_CMDEN_KEEP is invalid
    // so just set individual flags here
    cmd_enable = CMDEN_NGDB_ANY | CMDEN_NREC_ANY | CMDEN_LINK_OFF;
    update_state_ts(true);

    // set pointers for checkable menu items and set initial checked status
    if (!checkable_mi.empty()) {
        MenuOptionBool(cmd::kRecentFreeze, OPTION(kGenFreezeRecent));
        MenuOptionBool(cmd::kOnScreenController, OPTION(kUIShowOnScreenController));
        MenuOptionBool(cmd::kPause, paused);
        MenuOptionIntMask(cmd::kSoundChannel1, gopts.sound_en, (1 << 0));
        MenuOptionIntMask(cmd::kSoundChannel2, gopts.sound_en, (1 << 1));
        MenuOptionIntMask(cmd::kSoundChannel3, gopts.sound_en, (1 << 2));
        MenuOptionIntMask(cmd::kSoundChannel4, gopts.sound_en, (1 << 3));
        MenuOptionIntMask(cmd::kDirectSoundA, gopts.sound_en, (1 << 8));
        MenuOptionIntMask(cmd::kDirectSoundB, gopts.sound_en, (1 << 9));
        MenuOptionIntMask(cmd::kVideoLayersBG0, coreOptions.layerSettings, (1 << 8));
        MenuOptionIntMask(cmd::kVideoLayersBG1, coreOptions.layerSettings, (1 << 9));
        MenuOptionIntMask(cmd::kVideoLayersBG2, coreOptions.layerSettings, (1 << 10));
        MenuOptionIntMask(cmd::kVideoLayersBG3, coreOptions.layerSettings, (1 << 11));
        MenuOptionIntMask(cmd::kVideoLayersOBJ, coreOptions.layerSettings, (1 << 12));
        MenuOptionIntMask(cmd::kVideoLayersWIN0, coreOptions.layerSettings, (1 << 13));
        MenuOptionIntMask(cmd::kVideoLayersWIN1, coreOptions.layerSettings, (1 << 14));
        MenuOptionIntMask(cmd::kVideoLayersOBJWIN, coreOptions.layerSettings, (1 << 15));
        MenuOptionBool(cmd::kCheatsAutoSaveLoad, OPTION(kPrefAutoSaveLoadCheatList));
        MenuOptionIntMask(cmd::kCheatsEnable, coreOptions.cheatsEnabled, 1);
        SetMenuOption(cmd::kColorizerHack, OPTION(kGBColorizerHack));
        MenuOptionIntMask(cmd::kKeepSaves, coreOptions.skipSaveGameBattery, 1);
        MenuOptionIntMask(cmd::kKeepCheats, coreOptions.skipSaveGameCheats, 1);
        MenuOptionBool(cmd::kLoadGameAutoLoad, OPTION(kGenAutoLoadLastState));
        MenuOptionIntMask(cmd::kJoypadAutofireA, autofire, KEYM_A);
        MenuOptionIntMask(cmd::kJoypadAutofireB, autofire, KEYM_B);
        MenuOptionIntMask(cmd::kJoypadAutofireL, autofire, KEYM_L);
        MenuOptionIntMask(cmd::kJoypadAutofireR, autofire, KEYM_R);
        MenuOptionIntMask(cmd::kJoypadAutoholdUp, autohold, KEYM_UP);
        MenuOptionIntMask(cmd::kJoypadAutoholdDown, autohold, KEYM_DOWN);
        MenuOptionIntMask(cmd::kJoypadAutoholdLeft, autohold, KEYM_LEFT);
        MenuOptionIntMask(cmd::kJoypadAutoholdRight, autohold, KEYM_RIGHT);
        MenuOptionIntMask(cmd::kJoypadAutoholdA, autohold, KEYM_A);
        MenuOptionIntMask(cmd::kJoypadAutoholdB, autohold, KEYM_B);
        MenuOptionIntMask(cmd::kJoypadAutoholdL, autohold, KEYM_L);
        MenuOptionIntMask(cmd::kJoypadAutoholdR, autohold, KEYM_R);
        MenuOptionIntMask(cmd::kJoypadAutoholdSelect, autohold, KEYM_SELECT);
        MenuOptionIntMask(cmd::kJoypadAutoholdStart, autohold, KEYM_START);
        MenuOptionBool(cmd::kEmulatorSpeedupToggle, turbo);
        MenuOptionIntRadioValue(cmd::kLinkType0Nothing, gopts.gba_link_type, 0);
        MenuOptionIntRadioValue(cmd::kLinkType1Cable, gopts.gba_link_type, 1);
        MenuOptionIntRadioValue(cmd::kLinkType2Wireless, gopts.gba_link_type, 2);
        MenuOptionIntRadioValue(cmd::kLinkType3GameCube, gopts.gba_link_type, 3);
        MenuOptionIntRadioValue(cmd::kLinkType4Gameboy, gopts.gba_link_type, 4);
        // Keep in sync with the Language<N> handlers in cmd-handlers.cpp and
        // the kLanguages table in app.cpp.
        const int locale = OPTION(kLocale);
        for (int i = 0; i < VbamApp::LanguageCount(); i++) {
            MenuOptionIntRadioValue(cmd::kLanguage0 + i, locale, i);
        }
        MenuOptionBool(cmd::kExternalTranslations, OPTION(kExternalTranslations));
    }

    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (!checkable_mi[i].initialized) {
            vbam::LogError(tr("Invalid menu item %1; removing")
                               .arg(StripAccel(checkable_mi[i].action->text()).remove('&')));
            checkable_mi[i].action->setVisible(false);
            checkable_mi[i].action = nullptr;
        }
    }

    ResetMenuAccelerators();

    // Refresh the Logging dialog whenever the log text changes.
    vbam::SetLogUpdateCallback([this] {
        if (logdlg_ && logdlg_->isVisible()) {
            logdlg_->Update();
        }
    });

    // Shortcuts: pressed inputs bound to shortcut commands run them.
    connect(vbamApp().input_dispatcher(), &widgets::InputDispatcher::inputBatch, this,
            &MainWindow::OnInputBatch);

    // delayed fullscreen
    if (vbamApp().pending_fullscreen) {
        panel->ShowFullScreen(true);
    }

#ifndef NO_LINK
    LinkMode link_mode = GetConfiguredLinkMode();

    if (link_mode == LINK_GAMECUBE_DOLPHIN) {
        bool isv = !gopts.link_host.isEmpty();

        if (isv) {
            isv = SetLinkServerHost(vbam::ToStd(gopts.link_host).c_str());
        }

        if (!isv) {
            vbam::LogError(tr("JoyBus host invalid; disabling"));
            link_mode = LINK_DISCONNECTED;
        }
    }

    ConnectionState linkState = InitLink(link_mode);

    if (linkState != LINK_OK) {
        CloseLink();
    }

    if (GetLinkMode() != LINK_DISCONNECTED) {
        cmd_enable |= CMDEN_LINK_ANY;
        SetLinkTimeout(gopts.link_timeout);
        EnableSpeedHacks(OPTION(kGBALinkFast));
    }

    EnableNetworkMenu();
#endif
    enable_menus();
    panel->SetFrameTitle();

    // Re-adjust size now.
    panel->AdjustSize(false);

    OnKeepOnTopChanged();

    // Frame initialization is complete.
    init_complete_ = true;

    return true;
}

// -----------------------------------------------------------------------------
// Commands / actions

QAction* MainWindow::GetAction(int cmd_id) const {
    auto it = actions_.find(cmd_id);
    return it == actions_.end() ? nullptr : it->second;
}

void MainWindow::OnActionTriggered() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }
    ExecuteCommand(action->data().toInt());
}

void MainWindow::OnInputBatch(const widgets::UserInputBatch& batch) {
    // Mirrors wxvbamApp::FilterEvent: the first pressed input bound to a
    // shortcut command runs it.
    if (!CanProcessShortcuts()) {
        return;
    }

    // Don't fire hotkey/menu shortcuts when the app is not the foreground app,
    // even if the user has "Allow keyboard background input" on. Background
    // input is for joypad controls only, which run through the synchronous
    // sink in VbamApp.
    if (!VbamAppIsActive()) {
        return;
    }

    for (const auto& data : batch.data) {
        if (!data.pressed) {
            continue;
        }
        if (HandleShortcutInput(data.input)) {
            break;
        }
    }
}

bool MainWindow::HandleShortcutInput(const config::UserInput& input) {
    const nonstd::optional<config::Command> command =
        vbamApp().bindings()->CommandForInput(input);
    if (command == nonstd::nullopt || !command->is_shortcut()) {
        return false;
    }

    const int command_id = command->shortcut().id();

    // Toggle the associated checkable menu item (if any), as a menu click
    // would have.
    QAction* action = GetAction(command_id);
    if (action && action->isCheckable()) {
        action->setChecked(!action->isChecked());
    }

    ExecuteCommand(command_id);
    return true;
}

// -----------------------------------------------------------------------------
// Menu option helpers

void MainWindow::MenuOptionBool(int cmd, bool field) {
    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != cmd || !checkable_mi[i].action)
            continue;

        checkable_mi[i].initialized = true;
        checkable_mi[i].action->setChecked(field);
        break;
    }
}

void MainWindow::MenuOptionIntMask(int cmd, int field, int mask) {
    int value = mask;

    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != cmd || !checkable_mi[i].action)
            continue;

        checkable_mi[i].initialized = true;
        checkable_mi[i].mask = mask;
        checkable_mi[i].val = value;
        checkable_mi[i].action->setChecked((field & mask) == value);
        break;
    }
}

void MainWindow::MenuOptionIntRadioValue(int cmd, int field, int value) {
    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != cmd || !checkable_mi[i].action)
            continue;

        checkable_mi[i].initialized = true;
        checkable_mi[i].val = field;
        checkable_mi[i].action->setChecked(field == value);
        break;
    }
}

void MainWindow::GetMenuOptionBool(int cmd, bool* field) {
    VBAM_CHECK(field);
    *field = !*field;

    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != cmd || !checkable_mi[i].action)
            continue;

        *field = checkable_mi[i].action->isChecked();
        break;
    }
}

void MainWindow::GetMenuOptionConfig(int cmd, const config::OptionID& option_id) {
    config::Option* option = config::Option::ByID(option_id);
    VBAM_CHECK(option);

    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != cmd || !checkable_mi[i].action)
            continue;

        const bool is_checked = checkable_mi[i].action->isChecked();
        switch (option->type()) {
            case config::Option::Type::kBool:
                option->SetBool(is_checked);
                break;
            case config::Option::Type::kInt:
                option->SetInt(is_checked);
                break;
            default:
                VBAM_CHECK(false);
                return;
        }
        break;
    }
}

void MainWindow::GetMenuOptionInt(int cmd, int* field, int mask) {
    VBAM_CHECK(field);
    int value = mask;
    bool is_checked = ((*field) & (mask)) != (value);

    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != cmd || !checkable_mi[i].action)
            continue;

        is_checked = checkable_mi[i].action->isChecked();
        break;
    }

    *field = ((*field) & ~(mask)) | (is_checked ? (value) : 0);
}

void MainWindow::SetMenuOption(int cmd, bool value) {
    for (size_t i = 0; i < checkable_mi.size(); i++) {
        if (checkable_mi[i].cmd != cmd || !checkable_mi[i].action)
            continue;

        checkable_mi[i].action->setChecked(value);
        break;
    }
}

// -----------------------------------------------------------------------------
// Menus state

void MainWindow::enable_menus() {
    for (const cmditem& cmd_item : cmdtab) {
        if (cmd_item.mask_flags && cmd_item.action) {
            cmd_item.action->setEnabled((cmd_item.mask_flags & cmd_enable) != 0);
        }
    }

    if (cmd_enable & CMDEN_SAVST)
        for (int i = 0; i < 10; i++)
            if (loadst_mi[i])
                loadst_mi[i]->setEnabled(state_ts[i].isValid());
}

void MainWindow::update_state_ts(bool force) {
    bool any_states = false;

    for (int i = 0; i < 10; i++) {
        if (force)
            state_ts[i] = QDateTime();

        if (panel->game_type() != IMAGE_UNKNOWN) {
            const QString fn =
                QString(SAVESLOT_FMT).arg(panel->game_name()).arg(i + 1, 2, 10, QChar('0'));
            const QFileInfo fp(QDir(panel->state_dir()).filePath(fn));
            QDateTime ts;

            if (fp.isReadable()) {
                ts = fp.lastModified();
                any_states = true;
            }

            if (ts.isValid() != state_ts[i].isValid() || (ts.isValid() && ts != state_ts[i]) ||
                (force && !ts.isValid())) {
                // Use a real date and substitute all digits with '-' for an
                // empty slot, so the column stays aligned.
                const QDateTime fts = ts.isValid() ? ts : QDateTime::currentDateTime();
                QString df = "0&0 " + QLocale().toString(fts, QLocale::ShortFormat);

                if (!ts.isValid())
                    for (int j = 0; j < df.size(); j++)
                        if (df[j].isDigit())
                            df[j] = '-';

                df[0] = i == 9 ? '1' : ' ';
                df[2] = QChar('0' + (i + 1) % 10);

                if (loadst_mi[i]) {
                    base_labels_[cmd::kLoadGame01 + i] = df;
                    ResetActionAccelerator(cmd::kLoadGame01 + i);
                    loadst_mi[i]->setEnabled(ts.isValid());
                }

                if (savest_mi[i]) {
                    base_labels_[cmd::kSaveGame01 + i] = df;
                    ResetActionAccelerator(cmd::kSaveGame01 + i);
                }
            }

            state_ts[i] = ts;
        }
    }

    int cmd_flg = any_states ? CMDEN_SAVST : 0;

    if ((cmd_enable & CMDEN_SAVST) != cmd_flg) {
        cmd_enable = (cmd_enable & ~CMDEN_SAVST) | cmd_flg;
        enable_menus();
    }
}

int MainWindow::oldest_state_slot() {
    QDateTime ot;
    int os = -1;

    for (int i = 0; i < 10; i++) {
        if (!state_ts[i].isValid())
            return i + 1;

        if (os < 0 || state_ts[i] < ot) {
            os = i;
            ot = state_ts[i];
        }
    }

    return os + 1;
}

int MainWindow::newest_state_slot() {
    QDateTime nt;
    int ns = -1;

    for (int i = 0; i < 10; i++) {
        if (!state_ts[i].isValid())
            continue;

        if (ns < 0 || state_ts[i] > nt) {
            ns = i;
            nt = state_ts[i];
        }
    }

    return ns + 1;
}

void MainWindow::ResetRecentMenu() {
    const int count = gopts.recent.GetCount();
    for (int i = 0; i < RecentFiles::kMaxFiles; i++) {
        QAction* action = GetAction(cmd::kFile1 + i);
        if (!action) {
            continue;
        }
        if (i < count) {
            QString path = gopts.recent.GetHistoryFile(i);
            path.replace("&", "&&");
            base_labels_[cmd::kFile1 + i] =
                QString("&%1 %2").arg((i + 1) % 10).arg(path);
            action->setVisible(true);
            ResetActionAccelerator(cmd::kFile1 + i);
        } else {
            action->setVisible(false);
        }
    }
    vbamApp().SaveRecentList();
}

void MainWindow::ResetActionAccelerator(int cmd) {
    QAction* action = GetAction(cmd);
    if (!action) {
        return;
    }
    auto it = base_labels_.find(cmd);
    QString label = it == base_labels_.end() ? StripAccel(action->text()) : it->second;
    if (it == base_labels_.end()) {
        base_labels_[cmd] = label;
    }

    const std::unordered_set<config::UserInput> inputs =
        vbamApp().bindings()->InputsForCommand(config::ShortcutCommand(cmd));
    // Show the first keyboard binding as the action's (display-only, see
    // AddCommandAction) shortcut, so every platform renders it in its native
    // shortcut column. Bindings a QKeySequence cannot express -- joystick
    // controls, left/right-specific modifiers -- go into the label after a
    // tab, which QMenu also renders as a shortcut column.
    QKeySequence sequence;
    QString hint;
    for (const config::UserInput& input : inputs) {
        if (!input.is_keyboard())
            continue;
        const config::KeyboardInput& key = input.keyboard_input();
        if (key.has_extended_modifiers())
            continue;
        int combo = key.key();
        const uint32_t mod = key.mod();
        if (mod & config::kKeyModControl) combo |= Qt::CTRL;
        if (mod & config::kKeyModShift) combo |= Qt::SHIFT;
        if (mod & config::kKeyModAlt) combo |= Qt::ALT;
        if (mod & config::kKeyModMeta) combo |= Qt::META;
        sequence = QKeySequence(combo);
        break;
    }
    if (sequence.isEmpty() && !inputs.empty()) {
        hint = inputs.begin()->ToLocalizedString();
    }
    action->setShortcut(sequence);
    if (!hint.isEmpty()) {
        label += '\t' + hint;
    }
    action->setText(label);
}

void MainWindow::ResetMenuAccelerators() {
    for (const cmditem& cmd_item : cmdtab) {
        if (!cmd_item.action) {
            continue;
        }
        ResetActionAccelerator(cmd_item.cmd_id);
    }
}

void MainWindow::OnMenuAboutToShow() {
    // A screen message sitting in the status bar would otherwise linger.
    systemClearStatusMessage();
    SetMenusOpened(true);
}

void MainWindow::OnMenuAboutToHide() {
    // Only the top-level menus track the "menus opened" state; a submenu
    // hiding while its parent is still open must not clear it.
    QMenu* menu = qobject_cast<QMenu*>(sender());
    for (QMenu* m : menus_) {
        if (m && m != menu && m->isVisible()) {
            return;
        }
    }
    SetMenusOpened(false);
}

void MainWindow::SetMenusOpened(bool state) {
    menus_opened = state;

    if (state) {
        // A menu takes keyboard focus without deactivating the application, so
        // nothing else clears input state here. A button held as the menu
        // opens has its release delivered to the menu instead of to us, so
        // let go of everything now.
        vbamApp().keyboard_input_handler().Reset();
        vbamApp().emulated_gamepad()->Reset();
    }
#if defined(_WIN32)
    // On Windows, opening the menubar will stop the app, but DirectSound will
    // loop, so we pause audio here.
    if (menus_opened)
        soundPause();
    else if (!paused)
        soundResume();
#endif
}

// ShowModal that also disables emulator loop
// uses dialog_opened as a nesting counter
int MainWindow::ShowModal(QDialog* dlg) {
    if (!dlg) {
        vbam::LogError(tr("Failed to load dialog"));
        return QDialog::Rejected;
    }
    StartModal();
    int ret = dlg->exec();
    StopModal();
    return ret;
}

void MainWindow::StartModal() {
    // unblank pointer when dialog popped up; it will auto-hide again once the
    // game resumes
    if (panel)
        panel->ShowPointer();
    // Emulation is deliberately left running (see the wx port for why): the
    // panel-rebuild race is handled in GameArea::ResetPanel().
    ++dialog_opened;
}

void MainWindow::StopModal() {
    if (!dialog_opened)  // technically an error in the caller
        return;

    --dialog_opened;

    if (!IsPaused() && panel)
        panel->Resume();
}

#ifndef NO_LINK

LinkMode MainWindow::GetConfiguredLinkMode() {
    switch (gopts.gba_link_type) {
        case 0:
            return LINK_DISCONNECTED;

        case 1: {
            // A GB/GBC ROM talks over the Game Boy serial protocol; the GBA
            // multi-cable modes never move GB serial data. Map "Cable" to
            // whatever the loaded ROM actually needs so one setting works for
            // both.
            const bool gb_rom = panel && panel->game_type() == IMAGE_GB;

            if (OPTION(kGBALinkProto))
                return gb_rom ? LINK_GAMEBOY_IPC : LINK_CABLE_IPC;
            else
                return gb_rom ? LINK_GAMEBOY_SOCKET : LINK_CABLE_SOCKET;
        }

        case 2:
            if (OPTION(kGBALinkProto))
                return LINK_RFU_IPC;
            else
                return LINK_RFU_SOCKET;

        case 3:
            return LINK_GAMECUBE_DOLPHIN;

        case 4:
            if (OPTION(kGBALinkProto))
                return LINK_GAMEBOY_IPC;
            else
                return LINK_GAMEBOY_SOCKET;

        default:
            return LINK_DISCONNECTED;
    }
}

#endif  // NO_LINK

void MainWindow::IdentifyRom() {
    if (!panel->rom_name.isEmpty())
        return;

    panel->rom_name = panel->game_name();
    QString name;
    QString scene_rls;
    QString scene_name;
    const QString rom_crc32_str =
        QString("%1").arg(panel->rom_crc32, 8, 16, QChar('0')).toUpper();

    auto read_lines = [](const QString& path, const std::function<bool(const QString&)>& fn) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QTextStream in(&f);
        while (!in.atEnd()) {
            if (fn(in.readLine()))
                break;
        }
    };

    if (QFileInfo(vbamApp().rom_database_nointro).isFile()) {
        read_lines(vbamApp().rom_database_nointro, [&](const QString& line) {
            if (line.contains("<releaseNumber>")) {
                scene_rls = line.section('>', 1).section('<', 0, -2);
            }
            if (line.contains("romCRC ") && line.contains(rom_crc32_str)) {
                panel->rom_scene_rls = scene_rls;
                return true;
            }
            return false;
        });
    }

    if (QFileInfo(vbamApp().rom_database_scene).isFile()) {
        read_lines(vbamApp().rom_database_scene, [&](const QString& line) {
            if (line.startsWith("\tname")) {
                scene_name = line.section(' ', -1).section('"', 0, -2);
            }
            if (line.startsWith("\trom") && line.contains("crc " + rom_crc32_str)) {
                panel->rom_scene_rls_name = scene_name;
                panel->rom_name = scene_name;
                return true;
            }
            return false;
        });
    }

    if (QFileInfo(vbamApp().rom_database).isFile()) {
        read_lines(vbamApp().rom_database, [&](const QString& line) {
            if (line.startsWith("\tname")) {
                name = line.section('"', 1).section('"', 0, -2);
            }
            if (line.startsWith("\trom") && line.contains("crc " + rom_crc32_str)) {
                panel->rom_name = name;
                return true;
            }
            return false;
        });
    }
}

QString MainWindow::GetGamePath(QString path) {
    QString game_path = path;

    // Expand %s to system name (GBA, GBC, SGB, or GB)
    if (game_path.contains("%s")) {
        game_path.replace("%s", GetSystemName(panel));
    }

    if (!game_path.isEmpty()) {
        game_path = vbamApp().GetAbsolutePath(game_path);
        // Try to create the directory if it doesn't exist
        if (!QFileInfo(game_path).isDir()) {
            if (!QDir().mkpath(game_path)) {
                QMessageBox::critical(
                    this, tr("Directory Error"),
                    tr("Could not create directory:\n%1\n\nPlease check your configured path "
                       "in Options > Directories.")
                        .arg(game_path));
            }
        }
    } else {
        game_path = panel->game_dir();
        QDir().mkpath(game_path);
    }

    if (!QFileInfo(game_path).isDir())
        game_path = QDir::currentPath();

    if (!QFileInfo(game_path).isWritable()) {
        game_path = vbamApp().GetAbsolutePath(
            QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                .filePath(kDotDir));
        QDir().mkpath(game_path);
    }

    return game_path;
}

// -----------------------------------------------------------------------------
// Dialogs

QDialog* MainWindow::LoadDialog(const QString& name) {
    auto it = dialogs_.find(name);
    if (it != dialogs_.end()) {
        return it->second;
    }

    QDialog* dlg = nullptr;
    if (name == "DisplayConfig") {
        dlg = dialogs::DisplayConfig::NewInstance(this);
    } else if (name == "SoundConfig") {
        dlg = dialogs::SoundConfig::NewInstance(this);
    } else if (name == "JoypadConfig") {
        dlg = dialogs::JoypadConfig::NewInstance(
            this, std::bind(&VbamApp::bindings, &vbamApp()));
    } else if (name == "DirectoriesConfig") {
        dlg = dialogs::DirectoriesConfig::NewInstance(this);
    } else if (name == "GameBoyConfig") {
        dlg = dialogs::GameBoyConfig::NewInstance(this);
    } else if (name == "GameBoyAdvanceConfig") {
        dlg = dialogs::GameBoyAdvanceConfig::NewInstance(this);
    } else if (name == "GeneralConfig") {
        dlg = dialogs::GeneralConfig::NewInstance(this);
    } else if (name == "SpeedupConfig") {
        dlg = dialogs::SpeedupConfig::NewInstance(this);
    } else if (name == "AccelConfig") {
        dlg = dialogs::AccelConfig::NewInstance(
            this, menuBar(), std::bind(&VbamApp::bindings, &vbamApp()));
    } else if (name == "CheatList") {
        dlg = dialogs::CheatList::NewInstance(this);
    } else if (name == "CheatCreate") {
        dlg = dialogs::CheatSearch::NewInstance(this);
    } else if (name == "CheatEdit") {
        dlg = dialogs::CheatEdit::NewInstance(this);
    } else if (name == "CheatAdd") {
        dlg = dialogs::CheatAdd::NewInstance(this);
    } else if (name == "CodeSelect") {
        dlg = dialogs::CodeSelect::NewInstance(this);
    } else if (name == "ExportSPS") {
        dlg = dialogs::ExportSps::NewInstance(this);
    } else if (name == "GBAROMInfo") {
        dlg = dialogs::GbaRomInfo::NewInstance(this);
    } else if (name == "GBROMInfo") {
        dlg = dialogs::GbRomInfo::NewInstance(this);
#ifndef NO_LINK
    } else if (name == "NetLink") {
        dlg = dialogs::NetLink::NewInstance(this);
    } else if (name == "LinkConfig") {
        dlg = dialogs::LinkConfig::NewInstance(this);
#endif
    } else if (name == "Logging") {
        dlg = GetLogDialog();
    }

    if (!dlg) {
        vbam::LogError(tr("Unknown dialog %1").arg(name));
        return nullptr;
    }

    dialogs_[name] = dlg;
    return dlg;
}

LogDialog* MainWindow::GetLogDialog() {
    if (!logdlg_) {
        logdlg_ = std::make_unique<LogDialog>(this);
    }
    return logdlg_.get();
}

// -----------------------------------------------------------------------------
// Frame decorations / status bar

void MainWindow::SetMenuBarVisible(bool visible) {
    if (menuBar()->isNativeMenuBar()) {
        return;
    }
    menuBar()->setVisible(visible);
}

void MainWindow::SetStatusBarVisible(bool visible) {
    statusBar()->setVisible(visible && OPTION(kGenStatusBar));
}

void MainWindow::SetStatusText(const QString& text, int field) {
    if (field == 0) {
        if (text.isEmpty()) {
            statusBar()->clearMessage();
        } else {
            statusBar()->showMessage(text);
        }
    } else if (status_field1_) {
        status_field1_->setText(text);
    }
}

void MainWindow::OnStatusBarChanged() {
    statusBar()->setVisible(OPTION(kGenStatusBar));
    if (panel) {
        panel->AdjustSize(false);
    }
}

void MainWindow::OnKeepOnTopChanged() {
    const bool on_top = OPTION(kDispKeepOnTop);
    if (bool(windowFlags() & Qt::WindowStaysOnTopHint) == on_top) {
        return;
    }
    const bool was_visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, on_top);
    if (was_visible) {
        show();
    }
}

void MainWindow::ShowContextMenu(const QPoint& global_pos) {
    if (!ctx_menu_) {
        ctx_menu_ = new QMenu(this);
        for (QMenu* m : menus_) {
            if (m) {
                ctx_menu_->addMenu(m);
            }
        }
    }
    ctx_menu_->popup(global_pos);
}

// -----------------------------------------------------------------------------
// Window events

void MainWindow::OnActivate(bool focused) {
    if (!panel) {
        // Nothing more to do if no game is active.
        return;
    }

    if (focused) {
        // Set the focus to the game panel.
        panel->setFocus();
    }

    if (OPTION(kPrefPauseWhenInactive)) {
#ifndef NO_LINK
        // Never pause a live link session on focus loss: two instances on one
        // machine (the whole point of the local/IPC link) can only have one
        // focused window, and a paused peer is indistinguishable from a dead
        // one.
        if (GetLinkMode() != LINK_DISCONNECTED)
            return;
#endif
        if (focused && !paused) {
            panel->Resume();
        } else if (!focused) {
            panel->Pause();
        }
    }
}

void MainWindow::OnIconize(bool minimized) {
    if (!init_complete_ || !minimized) {
        return;
    }

    if (OPTION(kPrefPauseWhenInactive)) {
#ifndef NO_LINK
        // Keep a live link session running while iconized (see OnActivate).
        if (GetLinkMode() != LINK_DISCONNECTED)
            return;
#endif
        if (panel)
            panel->Pause();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (panel) {
        panel->UnloadGame(true);
    }
    // Save the geometry one last time.
    update_opts();
    // Close every viewer popup.
    for (QDialog* d : popups) {
        d->close();
    }
    event->accept();
    QApplication::quit();
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        if (!init_complete_) {
            return;
        }
        const Qt::WindowStates state = windowState();
        OnIconize(state & Qt::WindowMinimized);
        if (!(state & Qt::WindowMinimized)) {
            OPTION(kGeomIsMaximized) = bool(state & Qt::WindowMaximized);
            OPTION(kGeomFullScreen) = bool(state & Qt::WindowFullScreen);
        }
    } else if (event->type() == QEvent::ActivationChange) {
        if (init_complete_) {
            OnActivate(isActiveWindow());
        }
    }
}

void MainWindow::moveEvent(QMoveEvent* event) {
    QMainWindow::moveEvent(event);
    if (!init_complete_) {
        return;
    }
    if (!isFullScreen() && !isMaximized() && !isMinimized()) {
        const QPoint window_pos = frameGeometry().topLeft();
        OPTION(kGeomWindowX) = window_pos.x();
        OPTION(kGeomWindowY) = window_pos.y();
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (!init_complete_) {
        return;
    }
    if (!isFullScreen() && !isMaximized() && !isMinimized()) {
        const QRect window_rect = frameGeometry();
        if (window_rect.height() > 0 && window_rect.width() > 0) {
            OPTION(kGeomWindowHeight) = window_rect.height();
            OPTION(kGeomWindowWidth) = window_rect.width();
        }
        OPTION(kGeomWindowX) = window_rect.x();
        OPTION(kGeomWindowY) = window_rect.y();
    }

    OPTION(kGeomIsMaximized) = isMaximized();
    OPTION(kGeomFullScreen) = isFullScreen();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        return;
    }
    // ignore all but last
    const QUrl& url = urls.last();
    if (url.isLocalFile()) {
        vbamApp().LoadGameLater(url.toLocalFile());
        event->acceptProposedAction();
    }
}

void MainWindow::contextMenuEvent(QContextMenuEvent* event) {
    if (isFullScreen() || (panel && panel->IsFullScreen()) || !menuBar()->isVisible()) {
        ShowContextMenu(event->globalPos());
        event->accept();
        return;
    }
    QMainWindow::contextMenuEvent(event);
}

bool MainWindow::event(QEvent* event) {
    return QMainWindow::event(event);
}

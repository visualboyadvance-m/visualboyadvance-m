#ifndef VBAM_QT_MAIN_WINDOW_H_
#define VBAM_QT_MAIN_WINDOW_H_

#include <array>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <QDateTime>
#include <QMainWindow>
#include <QString>

#include "qt/cmd-ids.h"
#include "qt/config/cmdtab.h"
#include "qt/config/option-observer.h"
#include "qt/config/option.h"
#include "qt/widgets/input-dispatcher.h"

#ifndef NO_LINK
#include "core/gba/gbaLink.h"
#endif

class QAction;
class QActionGroup;
class QCloseEvent;
class QDialog;
class QDragEnterEvent;
class QDropEvent;
class QMenu;
class QMenuBar;
class QMoveEvent;
class QResizeEvent;

class GameArea;
class LogDialog;

namespace dialogs {
class BaseDialog;
}

namespace viewers {
class Viewer;
}

// A checkable menu item bound to an option bit or value. Equivalent of the wx
// port's checkable_mi_t: `mask` is the bit(s) the item controls in `field`
// and `val` the value that means "checked" (radio items).
struct checkable_mi_t {
    int cmd;
    QAction* action;
    int mask, val;
    bool initialized = false;
};
typedef std::vector<checkable_mi_t> checkable_mi_array_t;

typedef std::list<QDialog*> dialog_list_t;

// true if pause should happen at next frame
extern bool pause_next;

// The main window. Equivalent of MainFrame in the wx port.
//
// Menus are built in code (menu-def.cpp) from the command table: every menu
// item is a QAction whose data() is its cmd::Id; triggering it calls
// ExecuteCommand(id), which dispatches to the On<Name>() handler implemented
// in cmd-handlers.cpp. Shortcuts are not set on the QActions: they are
// resolved through config::Bindings from the InputDispatcher so keyboard AND
// joystick shortcuts work identically.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() override;

    // Builds menus, status bar, the game area and connects everything. Called
    // once by VbamApp::Init() after the config has been loaded.
    bool BindControls();

    // Executes a command by ID (menu, shortcut, on-screen menu, command line).
    // Returns false if the command is disabled by cmd_enable or unknown.
    bool ExecuteCommand(int cmd_id);

    // Returns the action for `cmd_id`, or nullptr.
    QAction* GetAction(int cmd_id) const;

    // Lazily creates and returns the dialog `name` (see dialogs/). Names match
    // the wx XRC names: "DisplayConfig", "SoundConfig", "JoypadConfig",
    // "DirectoriesConfig", "GameBoyConfig", "GameBoyAdvanceConfig",
    // "GeneralConfig", "SpeedupConfig", "AccelConfig", "CheatList",
    // "CheatEdit", "CheatCreate", "CheatAdd", "CodeSelect", "ExportSPS",
    // "GBPrinter", "GBAROMInfo", "GBROMInfo", "NetLink", "LinkConfig",
    // "Logging".
    QDialog* LoadDialog(const QString& name);
    std::map<QString, QDialog*> dialogs_;

    // Menu option helpers: bind a checkable action to an option / field.
    void MenuOptionIntMask(int cmd, int field, int mask);
    void MenuOptionIntRadioValue(int cmd, int field, int value);
    void MenuOptionBool(int cmd, bool field);
    void GetMenuOptionConfig(int cmd, const config::OptionID& option_id);
    void GetMenuOptionInt(int cmd, int* field, int mask);
    void GetMenuOptionBool(int cmd, bool* field);
    void SetMenuOption(int cmd, bool value);

    GameArea* GetPanel() { return panel; }

    // Returns `path` with the system-specific variables expanded.
    QString GetGamePath(QString path);

    bool CanProcessShortcuts() const { return !menus_opened && !dialog_opened; }

    // Shows `dlg` modally while pausing emulation. Returns the QDialog result.
    int ShowModal(QDialog* dlg);
    // Wrappers for use when ShowModal() isn't possible.
    void StartModal();
    void StopModal();

    // flags for enabling commands
    int cmd_enable;

    // adjust menus based on current cmd_enable
    void enable_menus();
#ifndef NO_LINK
    void EnableNetworkMenu();
#endif

    // adjust menus based on available save game states
    void update_state_ts(bool force = false);

    // retrieve oldest/newest slot; returns lowest-numbered slot on ties
    int oldest_state_slot(); // or empty slot if available
    int newest_state_slot(); // or 0 if none

    // Rebuilds the Recent submenu from gopts.recent.
    void ResetRecentMenu();
    // Refreshes the shortcut hints shown in the menus from the bindings.
    void ResetMenuAccelerators();

#ifndef NO_LINK
    // Returns the link mode to set according to the options
    LinkMode GetConfiguredLinkMode();
#endif

    void IdentifyRom();

    // Start GDB listener
    void GDBBreak();

    // The various viewer popups; these can be popped up as often as desired.
    void Disassemble();
    void IOViewer();
    void MapViewer();
    void MemViewer();
    void OAMViewer();
    void PaletteViewer();
    void TileViewer();

    // since they will all have to be destroyed on game unload:
    dialog_list_t popups;

    // The log dialog is kept for the whole session; only one is ever up and it
    // needs to be pinged when new messages arrive.
    LogDialog* GetLogDialog();

    // the cheat search dialog isn't destroyed or tracked, but it needs
    // to be cleared between games
    void ResetCheatSearch();

    // call this to update the viewers once a frame:
    void UpdateViewers();

    bool MenusOpened() const { return menus_opened; }
    void SetMenusOpened(bool state);
    bool DialogOpened() const { return dialog_opened != 0; }

    bool IsPaused(bool incidental = false) {
        return (paused && !pause_next && !incidental) || dialog_opened;
    }

    // Shows the full menu bar as a context menu (fullscreen / hidden menu bar).
    void ShowContextMenu(const QPoint& global_pos);

    // Fullscreen handling of the frame decorations.
    void SetMenuBarVisible(bool visible);
    void SetStatusBarVisible(bool visible);

    // Status bar text helpers (used by systemScreenMessage and speed display).
    void SetStatusText(const QString& text, int field = 0);

    // Called by the InputDispatcher connection: resolves shortcut bindings for
    // pressed inputs and executes the bound command. Returns true if consumed.
    bool HandleShortcutInput(const config::UserInput& input);

    // Application / window activation (pause when inactive, focus the panel).
    void OnActivate(bool active);
    void OnIconize(bool minimized);

    // Command handlers (see cmd-handlers.cpp). One per cmd::Id, named after the
    // command config name. Handlers of masked commands check cmd_enable first.
#include "qt/cmd-handlers.h"

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool event(QEvent* event) override;

private Q_SLOTS:
    void OnActionTriggered();
    void OnMenuAboutToShow();
    void OnMenuAboutToHide();
    void OnInputBatch(const widgets::UserInputBatch& batch);

private:
    // menu-def.cpp: builds the menu bar and fills actions_ / cmdtab actions.
    void BuildMenus();
    QAction* AddCommandAction(QMenu* menu, int cmd_id, bool checkable = false,
                              QActionGroup* group = nullptr);

    GameArea* panel = nullptr;

    bool paused = false, menus_opened = false;
    int dialog_opened = 0;

    bool autoLoadMostRecent = false;
    // load/save states menu items
    QAction* loadst_mi[10] = {};
    QAction* savest_mi[10] = {};
    QDateTime state_ts[10];
    // checkable menu items
    checkable_mi_array_t checkable_mi;
    // recent files menu
    QMenu* recent_menu = nullptr;
    QMenu* menus_[16] = {};  // top level menus in order (File, Emulation, ...)
    std::map<int, QAction*> actions_;
    std::unique_ptr<LogDialog> logdlg_;

    // One-time toggle to indicate that this object is fully initialized. This
    // used to filter events that are sent during initialization.
    bool init_complete_ = false;
#ifndef NO_LINK
    std::unique_ptr<config::OptionsObserver> gba_link_observer_;
#endif
    std::unique_ptr<config::OptionsObserver> keep_on_top_observer_;
    std::unique_ptr<config::OptionsObserver> status_bar_observer_;

    void OnStatusBarChanged();
    void OnKeepOnTopChanged();

    // Untranslated-with-mnemonic labels of every command action, so the
    // shortcut hint appended by ResetMenuAccelerators() can be recomputed.
    std::map<int, QString> base_labels_;
    // Second status bar field (the first is QStatusBar's message area).
    class QLabel* status_field1_ = nullptr;
    // Full-menu-bar clone shown as the context menu in full screen.
    QMenu* ctx_menu_ = nullptr;
    // Appends the current binding of `cmd` to its action text.
    void ResetActionAccelerator(int cmd);
    // Hidden parent of every command QAction; see AddCommandAction().
    QWidget* action_host_ = nullptr;
};

// a helper class to avoid forgetting StopModal()
class ModalPause {
public:
    ModalPause();
    ~ModalPause();
};

enum showspeed {
    // this order must match order of option enum and selector widget
    SS_NONE,
    SS_PERCENT,
    SS_DETAILED
};

// Save state file name format: <game>-<slot 2 digits>.sgm
#define SAVESLOT_FMT "%1-%2.sgm"

// display time, in ms
#define OSD_TIME 3000

#endif  // VBAM_QT_MAIN_WINDOW_H_

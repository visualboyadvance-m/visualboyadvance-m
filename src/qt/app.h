#ifndef VBAM_QT_APP_H_
#define VBAM_QT_APP_H_

#include <memory>

#include <QApplication>
#include <QElapsedTimer>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QTranslator>

#include "qt/config/bindings.h"
#include "qt/config/emulated-gamepad.h"
#include "qt/widgets/input-dispatcher.h"
#include "qt/widgets/keyboard-input-handler.h"
#include "qt/widgets/sdl-poller.h"

class MainWindow;

// The application object. Equivalent of wxvbamApp.
//
// Owns the global configuration (QSettings in INI format), the shortcut /
// joypad bindings, the input producers (keyboard, SDL joysticks) and the
// InputDispatcher they feed, and the main window.
class VbamApp final : public QApplication {
    Q_OBJECT

public:
    VbamApp(int& argc, char** argv);
    ~VbamApp() override;

    // Parses the command line, loads the configuration, creates the main
    // window. Returns false (with console_status set) if the process should
    // exit right away (e.g. --help, or a --config-only invocation).
    bool Init();
    int console_status() const { return console_status_; }

    // QApplication override: global keyboard hook feeding
    // KeyboardInputHandler for every key event in the process, so shortcuts
    // and game keys work regardless of which widget has focus. Also
    // implements the pause-when-inactive tracking.
    bool notify(QObject* receiver, QEvent* event) override;

    // Configuration paths.
    QString GetConfigDir();
    QString GetDataDir();
    // Full path of the INI file in use.
    QString GetConfigurationPath();
    // Directory searched for filter plugins (rpi .dll/.so/.dylib).
    QString GetPluginsDir();
    // Resolves `path` relative to the data dir when it is not absolute.
    QString GetAbsolutePath(QString path);
    // Locates a file in the data path list (share/vbam, next to the binary,
    // the bundle Resources dir ...). Empty if not found.
    QString FindDataFile(const QString& name);

    // Plugin enumeration cache - pre-populated on startup for fast hotkey cycling.
    void EnumeratePlugins();
    const QStringList& GetValidPlugins() const { return valid_plugins_; }
    bool ArePluginsEnumerated() const { return plugins_enumerated_; }
    void InvalidatePluginCache() { plugins_enumerated_ = false; valid_plugins_.clear(); }

    // The main configuration store (INI). Never null after Init().
    QSettings* config() { return config_.get(); }
    // vba-over.ini / gb-over.ini game override databases. May be null.
    QSettings* overrides() { return overrides_.get(); }
    QSettings* gb_overrides() { return gb_overrides_.get(); }

    // Loads the translations for the configured locale (kLocale option).
    void LoadTranslations();

    // The UI languages offered in the Languages menu. kLocale stores an index
    // into this table (0 = system default). Keep in sync with the Language<N>
    // commands.
    static int LanguageCount();
    static QString LocaleNameForLanguage(int index);  // "" for default
    static QString LanguageDescription(int index);    // untranslated label

    // Persists gopts.recent under [Recent] (file1..fileN).
    void SaveRecentList();

    // Name of a file to load at the earliest opportunity. Set through
    // LoadGameLater() once the main window exists so the game area's idle
    // loop is woken up to pick it up.
    QString pending_load;
    void LoadGameLater(const QString& path);
    // List of options to set after the config file is loaded (--option=name=value).
    QStringList pending_optset;
    // Set fullscreen mode after init.
    bool pending_fullscreen = false;
    // Mute audio for this session without persisting it to the config.
    bool mute = false;

    // ROM databases.
    QString rom_database;
    QString rom_database_scene;
    QString rom_database_nointro;

    QString data_path;

    MainWindow* frame = nullptr;

    // Milliseconds since program launch.
    QElapsedTimer timer;

    // Log messages are appended here for the log viewer (see vbam::LogMessage).
    QString log;

    // Accessors for configuration data.
    config::Bindings* bindings() { return &bindings_; }
    config::EmulatedGamepad* emulated_gamepad() { return &emulated_gamepad_; }
    widgets::InputDispatcher* input_dispatcher() { return &input_dispatcher_; }
    widgets::SdlPoller* sdl_poller() { return sdl_poller_.get(); }
    widgets::KeyboardInputHandler& keyboard_input_handler() {
        return keyboard_input_handler_;
    }

    // True if the main window (or one of its children) is the active window.
    bool IsActive() const;

    // True while --check-dialogs runs: windows created then must stay off
    // screen (see viewers::Viewer).
    bool self_test() const { return check_dialogs_; }

private:
    // Command line handling. Returns false to exit.
    bool ParseCommandLine();
    void PrintUsage();

    bool console_mode_ = false;
    // --check-dialogs: construct and show-cycle every dialog, then exit.
    bool check_dialogs_ = false;
    int console_status_ = 0;

    config::Bindings bindings_;
    config::EmulatedGamepad emulated_gamepad_;

    widgets::InputDispatcher input_dispatcher_;
    std::unique_ptr<widgets::SdlPoller> sdl_poller_;
    widgets::KeyboardInputHandler keyboard_input_handler_;

    // Main configuration file.
    QString config_file_;
    std::unique_ptr<QSettings> config_;
    std::unique_ptr<QSettings> overrides_;
    std::unique_ptr<QSettings> gb_overrides_;
    // Backing files for the override databases: the built-in vba-over.ini /
    // gb-over.ini are copied here (with the user's vba-over.ini merged on top)
    // so QSettings has a writable, non-resource file to work on.
    std::unique_ptr<QTemporaryFile> overrides_file_;
    std::unique_ptr<QTemporaryFile> gb_overrides_file_;

    void LoadOverrides();
    QString config_dir_;  // resolved configuration directory

    QStringList config_path_;  // search path for data files

    std::unique_ptr<QTranslator> translator_;
    std::unique_ptr<QTranslator> qt_translator_;

    // Plugin enumeration cache
    QStringList valid_plugins_;
    bool plugins_enumerated_ = false;
};

// Returns the application object. Equivalent of wxGetApp().
inline VbamApp& vbamApp() { return *static_cast<VbamApp*>(QCoreApplication::instance()); }

#endif  // VBAM_QT_APP_H_

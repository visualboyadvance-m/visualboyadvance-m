#include "qt/app.h"

#include <cstdio>
#include <cstdlib>

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDialog>
#include <QCommandLineParser>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLibrary>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QWidget>

#ifndef ENABLE_SDL3
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif

#include "core/base/check.h"
#include "core/base/system.h"
#include "core/base/version.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaRemote.h"
#include "qt/config/cmdtab.h"
#include "qt/config/option-proxy.h"
#include "qt/config/option.h"
#include "qt/config/strutils.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"
#include "qt/opts.h"
#include "qt/sys.h"

#ifndef NO_LINK
#include "core/gba/gbaLink.h"
#endif

#if defined(VBAM_ENABLE_DEBUGGER)
void (*dbgMain)() = remoteStubMain;
void (*dbgSignal)(int, int) = remoteStubSignal;
void (*dbgOutput)(const char*, uint32_t) = debuggerOutput;
#endif  // defined(VBAM_ENABLE_DEBUGGER)

namespace {

const char kDotDir[] = "visualboyadvance-m";
const char kConfigFileName[] = "vbam-qt.ini";

// Tracks whether our application is the foreground app. Used to gate hotkey
// firing so background-input synthetic events update joypad state but don't
// fire shortcuts (see MainWindow::HandleShortcutInput).
bool g_app_is_active = true;

// The UI languages offered in the Languages menu, in the order of the
// Language<N> commands. kLocale stores the index; 0 is the system default.
// Keep in sync with the Language<N> handlers in cmd-handlers.cpp and the menu
// items in menu-def.cpp; entries exist only for languages with a catalog.
struct LanguageEntry {
    const char* locale;
    const char* description;
};

const LanguageEntry kLanguages[] = {
    {"", "Default Language"},
    {"es_419", "Spanish (Latin American)"},
    {"es", "Spanish"},
    {"fr_FR", "French (France)"},
    {"he_IL", "Hebrew (Israel)"},
    {"hu_HU", "Hungarian (Hungary)"},
    {"id", "Indonesian"},
    {"it_IT", "Italian"},
    {"ko_KR", "Korean (Korea)"},
    {"pl_PL", "Polish (Poland)"},
    {"pt_BR", "Portuguese (Brazil)"},
    {"sv", "Swedish"},
    {"tr", "Turkish"},
    {"uk", "Ukrainian"},
    {"zh_CN", "Chinese (China)"},
};

// True when `input` is bound to a joypad control, as opposed to a hotkey or
// nothing at all.
bool IsJoypadBound(const config::Bindings* const bindings, const config::UserInput& input) {
    if (!bindings)
        return false;
    const nonstd::optional<config::Command> command = bindings->CommandForInput(input);
    return command != nonstd::nullopt && command->is_game();
}

// True when `key` is a modifier key in its own right.
bool KeyIsModifier(int key) {
    switch (key) {
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Alt:
        case Qt::Key_Meta:
        case Qt::Key_AltGr:
            return true;
        default:
            return false;
    }
}

bool IsWritableDir(const QString& dir) {
    QFileInfo fi(dir);
    return fi.isDir() && fi.isWritable();
}

bool ParentIsWritable(const QString& dir) {
    QFileInfo parent(QFileInfo(dir).absolutePath());
    return parent.isDir() && parent.isWritable();
}

}  // namespace

bool VbamAppIsActive() { return g_app_is_active; }

VbamApp::VbamApp(int& argc, char** argv)
    : QApplication(argc, argv),
      emulated_gamepad_(std::bind(&VbamApp::bindings, this)),
      // The KeyboardInputHandler's sync sink updates joypad state directly for
      // every key event, before the batch is dispatched:
      //   (a) presses/releases can't split across focus changes and leave a
      //       key stuck;
      //   (b) lower input latency (no waiting for the dispatched batch);
      //   (c) background input routes ONLY joypad-mapped keys to the emulator
      //       while never firing hotkeys.
      // A key held with a modifier arrives here twice: once as the modified
      // input and once plain, because the shortcut consumer needs both to pick
      // the more specific binding. The joypad wants the opposite -- a direction
      // is that direction whatever else is held -- so pass the modified form
      // on only when it is bound as a joypad control in its own right.
      keyboard_input_handler_(&input_dispatcher_,
                              [this](const config::UserInput& input, bool pressed) {
                                  if (input.is_keyboard() &&
                                      !KeyIsModifier(input.keyboard_input().key()) &&
                                      input.keyboard_input().mod_extended() !=
                                          config::kKeyModNone &&
                                      !IsJoypadBound(bindings(), input)) {
                                      return;
                                  }
                                  if (pressed) {
                                      emulated_gamepad_.OnInputPressed(input);
                                  } else {
                                      emulated_gamepad_.OnInputReleased(input);
                                  }
                              }) {
    timer.start();

    connect(this, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                const bool active = state == Qt::ApplicationActive;
                g_app_is_active = active;
                if (!active) {
                    // App is deactivating. Clear keyboard tracking AND joypad
                    // state together so a key held across the focus change
                    // cannot stay latched down when the user comes back.
                    keyboard_input_handler_.Reset();
                    emulated_gamepad_.Reset();
                }
                if (frame) {
                    frame->OnActivate(active);
                }
            });
}

VbamApp::~VbamApp() {
    if (sdl_poller_) {
        sdl_poller_.reset();
    }
    frame = nullptr;
}

int VbamApp::LanguageCount() {
    return static_cast<int>(sizeof(kLanguages) / sizeof(kLanguages[0]));
}

QString VbamApp::LocaleNameForLanguage(int index) {
    if (index <= 0 || index >= LanguageCount()) {
        return QString();
    }
    return QString::fromLatin1(kLanguages[index].locale);
}

QString VbamApp::LanguageDescription(int index) {
    if (index < 0 || index >= LanguageCount()) {
        return QString();
    }
    return QString::fromLatin1(kLanguages[index].description);
}

QString VbamApp::GetConfigDir() {
    if (!config_dir_.isEmpty()) {
        return config_dir_;
    }

    QString base;
#if defined(Q_OS_WIN)
    base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    }
#elif defined(Q_OS_MACOS)
    // ~/Library/Application Support/visualboyadvance-m, like the wx port.
    base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#else
    // XDG: ${XDG_CONFIG_HOME:-$HOME/.config}/visualboyadvance-m, unless the
    // legacy $HOME/.vbam exists.
    const QString old_config = QDir::homePath() + "/.vbam";
    if (QFileInfo(old_config).isDir()) {
        config_dir_ = old_config;
        return config_dir_;
    }
    base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif

    config_dir_ = QDir(base).filePath(kDotDir);
    return config_dir_;
}

QString VbamApp::GetDataDir() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return GetConfigDir();
#else
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(kDotDir);
#endif
}

QString VbamApp::GetConfigurationPath() {
    // first check if a config file exists in reverse order
    // (from system paths to more local paths.)
    if (data_path.isEmpty()) {
        for (int i = config_path_.size() - 1; i >= 0; i--) {
            QFileInfo fn(QDir(config_path_[i]).filePath(kConfigFileName));
            if (fn.isFile() && fn.isWritable()) {
                data_path = config_path_[i];
                break;
            }
        }
    }

    // if no config file was found, search for a writable config dir, or a dir
    // whose parent is writable so it can be created, in normal order (from
    // user paths to system paths.)
    if (data_path.isEmpty()) {
        for (const QString& dir : config_path_) {
            if (IsWritableDir(dir) || ParentIsWritable(dir)) {
                data_path = dir;
                break;
            }
        }
    }

    if (data_path.isEmpty()) {
        data_path = GetConfigDir();
    }

    return data_path;
}

QString VbamApp::GetPluginsDir() {
    const QString config_dir = OPTION(kDispPluginDir);
    if (!config_dir.isEmpty()) {
        return GetAbsolutePath(config_dir);
    }
    return QDir(GetConfigurationPath()).filePath("plugins");
}

QString VbamApp::GetAbsolutePath(QString path) {
    if (path.isEmpty()) {
        return path;
    }
    if (path.startsWith("~/")) {
        path = QDir::homePath() + path.mid(1);
    }
    QFileInfo fn(path);
    if (fn.isRelative()) {
#ifdef Q_OS_WIN
        // Anchor to the exe directory on Windows so file-association launches
        // (which set CWD to the opened file's directory) don't change where
        // relative config paths resolve.
        return QDir::cleanPath(QDir(applicationDirPath()).absoluteFilePath(path));
#else
        return QDir::cleanPath(QDir(GetConfigurationPath()).absoluteFilePath(path));
#endif
    }
    return QDir::cleanPath(path);
}

QString VbamApp::FindDataFile(const QString& name) {
    for (const QString& dir : config_path_) {
        const QString candidate = QDir(dir).filePath(name);
        if (QFileInfo(candidate).isReadable()) {
            return candidate;
        }
    }
    const QString resource = ":/" + name;
    if (QFile::exists(resource)) {
        return resource;
    }
    return QString();
}

void VbamApp::EnumeratePlugins() {
    if (plugins_enumerated_) {
        return;
    }

    valid_plugins_.clear();
    const QString plugin_path = GetPluginsDir();

    QDirIterator it(plugin_path, QStringList() << "*.rpi" << "*.dll" << "*.so" << "*.dylib",
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString plugin = it.next();
        // Check the library has the required export WITHOUT calling
        // RenderPluginGetInfo(): calling it would modify the plugin's shared
        // static RENDER_PLUGIN_INFO, corrupting the Flags the rendering panel
        // has already configured.
        QLibrary lib(plugin);
        if (lib.load()) {
            if (lib.resolve("RenderPluginGetInfo") != nullptr) {
                valid_plugins_.append(plugin);
            }
            lib.unload();
        }
    }

    valid_plugins_.sort();
    plugins_enumerated_ = true;
}

void VbamApp::LoadTranslations() {
    if (translator_) {
        removeTranslator(translator_.get());
        translator_.reset();
    }
    if (qt_translator_) {
        removeTranslator(qt_translator_.get());
        qt_translator_.reset();
    }

    const int language = OPTION(kLocale);
    QLocale locale;
    if (language > 0 && language < LanguageCount()) {
        locale = QLocale(LocaleNameForLanguage(language));
    }

    // Search paths for vbam-qt_<locale>.qm: the data dirs (share/vbam,
    // <config>/translations ...) and the compiled-in resources.
    QStringList search_dirs;
    for (const QString& dir : config_path_) {
        search_dirs << dir << QDir(dir).filePath("translations");
    }
    search_dirs << ":/translations";
    if (!OPTION(kExternalTranslations)) {
        // Prefer the compiled-in catalogs when external ones are not wanted.
        search_dirs.prepend(":/translations");
    }

    auto translator = std::make_unique<QTranslator>(this);
    bool loaded = false;
    for (const QString& dir : search_dirs) {
        if (translator->load(locale, "vbam-qt", "_", dir)) {
            loaded = true;
            break;
        }
    }
    if (loaded) {
        installTranslator(translator.get());
        translator_ = std::move(translator);
    }

    auto qt_translator = std::make_unique<QTranslator>(this);
    if (qt_translator->load(locale, "qtbase", "_",
                            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        installTranslator(qt_translator.get());
        qt_translator_ = std::move(qt_translator);
    }
}

void VbamApp::SaveRecentList() {
    QSettings* cfg = config();
    if (!cfg) {
        return;
    }
    cfg->beginGroup("Recent");
    cfg->remove("");
    const QStringList& files = gopts.recent.files();
    for (int i = 0; i < files.size(); i++) {
        cfg->setValue(QString("file%1").arg(i + 1), files[i]);
    }
    cfg->endGroup();
    cfg->sync();
}

bool VbamApp::IsActive() const {
    return g_app_is_active;
}

void VbamApp::PrintUsage() {
    // Handled by QCommandLineParser::showHelp().
}

bool VbamApp::ParseCommandLine() {
    QCommandLineParser parser;
    parser.setApplicationDescription("VisualBoyAdvance-M");
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsCompactedShortOptions);
    const QCommandLineOption help_option = parser.addHelpOption();
    const QCommandLineOption version_option = parser.addVersionOption();

    // While I would rather the long options be translated, there is merit to
    // the idea that command-line syntax should not change based on locale.
    QCommandLineOption print_cfg_path("print-cfg-path",
                                      tr("Print configuration path and exit"));
    QCommandLineOption fullscreen(QStringList() << "f" << "fullscreen",
                                  tr("Start in full-screen mode"));
    QCommandLineOption mute_option(
        "mute", tr("Mute the audio for this session, without saving to the config"));
    QCommandLineOption config_option(QStringList() << "c" << "config",
                                     tr("Set a configuration file"), tr("file"));
    QCommandLineOption list_options(QStringList() << "o" << "list-options",
                                    tr("List all settable options and exit"));
    QCommandLineOption save_over("save-over", tr("Save built-in vba-over.ini and exit"),
                                 tr("file"));
    parser.addOption(print_cfg_path);
    parser.addOption(fullscreen);
    parser.addOption(mute_option);
    parser.addOption(config_option);
    parser.addOption(list_options);
    parser.addOption(save_over);
    // Developer self-test: instantiates every dialog (off screen) so option
    // binding mistakes surface without clicking through the menus.
    QCommandLineOption check_dialogs("check-dialogs",
                                     tr("Construct every dialog off screen and exit"));
    check_dialogs.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(check_dialogs);
#if !defined(NO_LINK) && !defined(_WIN32)
    QCommandLineOption delete_shared(QStringList() << "s" << "delete-shared-state",
                                     tr("Delete shared link state first, if it exists"));
    parser.addOption(delete_shared);
#endif
    parser.addPositionalArgument(tr("ROM file"), tr("ROM file"), tr("[ROM file]"));
    parser.addPositionalArgument(tr("<config>=<value>"), tr("Set a configuration option"),
                                 tr("[<config>=<value>...]"));

    if (!parser.parse(arguments())) {
        fprintf(stderr, "%s\n", qPrintable(parser.errorText()));
        fputs(qPrintable(parser.helpText()), stderr);
        console_mode_ = true;
        console_status_ = 1;
        return false;
    }

    if (parser.isSet(help_option)) {
        fputs(qPrintable(parser.helpText()), stdout);
        console_mode_ = true;
        return false;
    }

    if (parser.isSet(version_option)) {
        printf("%s %s\n", qPrintable(applicationDisplayName()), qPrintable(applicationVersion()));
        console_mode_ = true;
        return false;
    }

    if (parser.isSet(print_cfg_path)) {
        QString lm = tr("Configuration is read from, in order:");
        for (const QString& dir : config_path_) {
            lm += "\n\t" + dir;
        }
        fprintf(stderr, "%s\n", qPrintable(lm));
        console_mode_ = true;
        return false;
    }

    if (parser.isSet(save_over)) {
        const QString out = parser.value(save_over);
        const QString builtin = FindDataFile("vba-over.ini");
        bool ok = false;
        if (!builtin.isEmpty()) {
            QFile::remove(out);
            ok = QFile::copy(builtin, out);
        }
        if (!ok) {
            fprintf(stderr, "%s\n",
                    qPrintable(tr("Configuration / build error: can't find built-in vba-over.ini")));
            console_status_ = 1;
        } else {
            QString lm = tr("Wrote built-in override file to %1\n"
                            "To override, delete all but changed section. First found "
                            "section is used from search path:")
                             .arg(out);
            for (const QString& dir : config_path_) {
                lm += "\n\t" + QDir(dir).filePath("vba-over.ini");
            }
            lm += tr("\n\tbuilt-in");
            fprintf(stderr, "%s\n", qPrintable(lm));
        }
        console_mode_ = true;
        return false;
    }

    if (parser.isSet(fullscreen)) {
        pending_fullscreen = true;
    }

    if (parser.isSet(mute_option)) {
        mute = true;
    }

    if (parser.isSet(check_dialogs)) {
        check_dialogs_ = true;
    }

    if (parser.isSet(list_options)) {
        printf("%s",
               qPrintable(tr("Options set from the command line are saved if any"
                             " configuration changes are made in the user interface.\n\n"
                             "For flag options, true and false are specified as 1 and 0, "
                             "respectively.\n\n")));

        for (const config::Option& opt : config::Option::All()) {
            printf("%s\n", qPrintable(opt.ToHelperString()));
        }

        printf("%s", qPrintable(tr("The commands available for the Keyboard/* option are:\n\n")));

        for (const cmditem& cmd_item : cmdtab) {
            printf("%s (%s)\n", qPrintable(cmd_item.cmd),
                   qPrintable(QCoreApplication::translate("vbam", cmd_item.name)));
        }

        console_mode_ = true;
        return false;
    }

    if (parser.isSet(config_option)) {
        const QString s = parser.value(config_option);
        if (!QFileInfo(s).isFile()) {
            fprintf(stderr, "%s\n", qPrintable(tr("Configuration file not found.")));
            console_mode_ = true;
            console_status_ = 1;
            return false;
        }
        config_file_ = s;
    }

#if !defined(NO_LINK) && !defined(_WIN32)
    if (parser.isSet(delete_shared)) {
        CleanLocalLink();
    }
#endif

    bool complained = false, gotfile = false;

    for (const QString& p : parser.positionalArguments()) {
        const QStringList parts = config::str_split(p, "=");

        if (parts.size() > 1) {
            // Applied after the config file is loaded, see Init().
            pending_optset.push_back(p);
        } else {
            if (!gotfile) {
                pending_load = p;
                gotfile = true;
            } else {
                if (!complained) {
                    fprintf(stderr, "%s\n",
                            qPrintable(tr("Bad configuration option or multiple ROM files given:")));
                    fprintf(stderr, "%s\n", qPrintable(pending_load));
                    complained = true;
                }
                fprintf(stderr, "%s\n", qPrintable(p));
            }
        }
    }

    return true;
}

void VbamApp::LoadOverrides() {
    // Regex capturing the "# comment" line preceding a [group] header.
    // EOL can be \n (unix), \r\n (dos), or \r (old mac).
    auto read_all = [](const QString& path) -> QString {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return QString();
        }
        return QString::fromUtf8(f.readAll());
    };

    auto comment_for_group = [](const QString& text, const QString& group) -> QString {
        QRegularExpression re("(^|[\\n\\r])# ?([^\\n\\r]*)(\\r?\\n|\\r)\\[" +
                              QRegularExpression::escape(group) + "\\]");
        const QRegularExpressionMatch m = re.match(text);
        return m.hasMatch() ? m.captured(2) : QString();
    };

    // gb-over.ini: built-in only.
    {
        const QString builtin = FindDataFile("gb-over.ini");
        gb_overrides_file_ = std::make_unique<QTemporaryFile>(
            QDir::temp().filePath("vbam-qt-gb-over-XXXXXX.ini"));
        if (gb_overrides_file_->open()) {
            if (!builtin.isEmpty()) {
                gb_overrides_file_->write(read_all(builtin).toUtf8());
            }
            gb_overrides_file_->flush();
            gb_overrides_ = std::make_unique<QSettings>(gb_overrides_file_->fileName(),
                                                        QSettings::IniFormat);
        }
    }

    // vba-over.ini: built-in, with the user's file merged on top, group by
    // group. Each group also gets a "comment" entry (the comment line above
    // the header) and, for user groups, a "path" entry naming where it came
    // from, so the GBA config dialog can show and rewrite the user's entries.
    {
        const QString builtin = FindDataFile("vba-over.ini");
        const QString bovs = builtin.isEmpty() ? QString() : read_all(builtin);

        overrides_file_ = std::make_unique<QTemporaryFile>(
            QDir::temp().filePath("vbam-qt-vba-over-XXXXXX.ini"));
        if (!overrides_file_->open()) {
            return;
        }
        overrides_file_->write(bovs.toUtf8());
        overrides_file_->flush();
        overrides_ =
            std::make_unique<QSettings>(overrides_file_->fileName(), QSettings::IniFormat);

        for (const QString& group : overrides_->childGroups()) {
            overrides_->setValue(group + "/comment", comment_for_group(bovs, group));
        }

        const QString user_file = QDir(GetConfigurationPath()).filePath("vba-over.ini");
        if (QFileInfo(user_file).isFile()) {
            const QString text = read_all(user_file);
            QSettings ov(user_file, QSettings::IniFormat);
            for (const QString& group : ov.childGroups()) {
                overrides_->remove(group);
                overrides_->beginGroup(group);
                ov.beginGroup(group);
                overrides_->setValue("path", GetConfigurationPath());
                overrides_->setValue("comment", comment_for_group(text, group));
                for (const QString& key : ov.childKeys()) {
                    overrides_->setValue(key, ov.value(key));
                }
                ov.endGroup();
                overrides_->endGroup();
            }
        }
    }
}

bool VbamApp::Init() {
    setApplicationVersion(QString::fromStdString(kVbamVersion));

    // Build the search path for data files, from the most local to the most
    // global. Only existing, readable directories are kept.
    QStringList candidates;
    candidates << GetConfigDir();
    candidates << QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    candidates << QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                      .filePath(kDotDir);
    candidates << applicationDirPath();
#if defined(Q_OS_MACOS)
    candidates << QDir::cleanPath(applicationDirPath() + "/../Resources");
#endif
    candidates << QDir::cleanPath(applicationDirPath() + "/../share/vbam");
    candidates << QDir::cleanPath(applicationDirPath() + "/../share/visualboyadvance-m");
    candidates << QDir::cleanPath(applicationDirPath() + "/../share/vbam-qt");
    for (const QString& dir : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        candidates << QDir(dir).filePath("vbam") << QDir(dir).filePath(kDotDir);
    }
    candidates << "/usr/local/share/vbam" << "/usr/share/vbam";

    // The config dir is always kept (it may not exist yet).
    config_path_ << GetConfigDir();
    for (const QString& dir : candidates) {
        if (dir.isEmpty() || config_path_.contains(dir)) {
            continue;
        }
        if (QFileInfo(dir).isDir()) {
            config_path_ << dir;
        }
    }

    if (!ParseCommandLine()) {
        return false;
    }

    if (console_mode_) {
        return false;
    }

    // Sort the command table so CommandFromConfigString() can binary-search.
    config::SortCmdTab();

    if (config_file_.isEmpty()) {
        // Default configuration file, in a subdir to support other config too.
        config_file_ = QDir(GetConfigurationPath()).filePath(kConfigFileName);
    }

    if (QFileInfo(config_file_).isDir()) {
        vbam::LogError(tr("Invalid configuration file provided: %1").arg(config_file_));
        console_status_ = 1;
        return false;
    }

    // QSettings does not create the directories by itself so do it here.
    const QString config_dir = QFileInfo(config_file_).absolutePath();
    if (!QFileInfo(config_dir).isDir()) {
        QDir().mkpath(config_dir);
    }

    const bool first_run = !QFileInfo(config_file_).exists();
    config_ = std::make_unique<QSettings>(config_file_, QSettings::IniFormat);

    // Load the default options.
    load_opts(first_run);

    // On first launch only (no config file yet), arm the runtime display-filter
    // probe. Once a ROM is running, GameArea cycles candidate filters, measures
    // the real frame rate of each, and settles on the highest that sustains
    // ~60fps, then persists the choice. This runs solely when there was no
    // config, so it never overrides a filter the user has explicitly chosen.
    if (first_run) {
        g_default_filter_probe_pending = true;
    }

    LoadTranslations();

    // process command-line options
    for (const QString& optset : pending_optset) {
        const QStringList parts = config::str_split(optset, "=");
        if (parts.size() > 1) {
            opt_set(parts[0], parts[1]);
        }
    }
    pending_optset.clear();

    // ROM databases.
    {
        QDir cfg_dir(GetConfigurationPath());
        const QStringList nointro =
            cfg_dir.entryList(QStringList() << "Official No-Intro Nintendo Gameboy Advance Number (Date).xml",
                              QDir::Files | QDir::Readable);
        if (!nointro.isEmpty()) {
            rom_database_nointro = cfg_dir.filePath(nointro.first());
        }
        const QStringList scene = cfg_dir.entryList(
            QStringList() << "Nintendo - Game Boy Advance (Scene)*.dat", QDir::Files | QDir::Readable);
        if (!scene.isEmpty()) {
            rom_database_scene = cfg_dir.filePath(scene.first());
        }
        const QStringList rdb = cfg_dir.entryList(
            QStringList() << "Nintendo - Game Boy Advance*.dat", QDir::Files | QDir::Readable);
        for (const QString& f : rdb) {
            const QString full = cfg_dir.filePath(f);
            if (full != rom_database_scene) {
                rom_database = full;
                break;
            }
        }
    }

    LoadOverrides();

    // SDL: joystick / game controller input (and audio, per the audio driver).
    sdl_poller_ = std::make_unique<widgets::SdlPoller>(&input_dispatcher_);

    // We need to gather this information before creating the main window as
    // the move/resize handlers can fire during construction.
    const QRect client_rect(OPTION(kGeomWindowX).Get(), OPTION(kGeomWindowY).Get(),
                            OPTION(kGeomWindowWidth).Get(), OPTION(kGeomWindowHeight).Get());
    const bool dimensions_unset =
        OPTION(kGeomWindowWidth).Get() == 0 || OPTION(kGeomWindowHeight).Get() == 0;
    const bool is_fullscreen = OPTION(kGeomFullScreen);
    const bool is_maximized = OPTION(kGeomIsMaximized);

    // Create the main window.
    frame = new MainWindow();
    if (!frame->BindControls()) {
        return false;
    }

    if (check_dialogs_) {
        static const char* const kDialogNames[] = {
            "DisplayConfig", "SoundConfig",     "JoypadConfig", "DirectoriesConfig",
            "GameBoyConfig", "GameBoyAdvanceConfig", "GeneralConfig", "SpeedupConfig",
            "AccelConfig",   "CheatList",       "CheatCreate",  "CheatEdit",
            "CheatAdd",      "CodeSelect",      "ExportSPS",    "GBAROMInfo",
            "GBROMInfo",
#ifndef NO_LINK
            "NetLink",       "LinkConfig",
#endif
            "Logging",
        };
        for (const char* name : kDialogNames) {
            QDialog* dlg = frame->LoadDialog(QString::fromLatin1(name));
            if (!dlg) {
                fprintf(stderr, "FAIL %s (not created)\n", name);
                console_status_ = 1;
                continue;
            }
            // show()/hide() deliver the show/hide events (bindings load) without
            // mapping a window.
            dlg->setAttribute(Qt::WA_DontShowOnScreen, true);
            dlg->show();
            dlg->hide();
            fprintf(stderr, "ok   %s\n", name);
        }
        // With a ROM on the command line, also open every debug viewer for
        // that system and refresh it once, then unload.
        if (!pending_load.isEmpty()) {
            GameArea* panel = frame->GetPanel();
            panel->LoadGame(pending_load);
            pending_load.clear();
            if (panel->game_type() == IMAGE_UNKNOWN) {
                fprintf(stderr, "FAIL ROM did not load\n");
                console_status_ = 1;
            } else {
                struct Opener { const char* name; void (MainWindow::*fn)(); };
                static const Opener kOpeners[] = {
                    {"Disassemble", &MainWindow::Disassemble}, {"IOViewer", &MainWindow::IOViewer},
                    {"MapViewer", &MainWindow::MapViewer},     {"MemViewer", &MainWindow::MemViewer},
                    {"OAMViewer", &MainWindow::OAMViewer},     {"PaletteViewer", &MainWindow::PaletteViewer},
                    {"TileViewer", &MainWindow::TileViewer},
                };
                for (const Opener& opener : kOpeners) {
                    (frame->*opener.fn)();
                    frame->UpdateViewers();
                    fprintf(stderr, "ok   %s (%s)\n", opener.name,
                            panel->game_type() == IMAGE_GBA ? "GBA" : "GB");
                }
                // The ROM info dialogs read the loaded cartridge header.
                for (const char* name : {"GBAROMInfo", "GBROMInfo"}) {
                    QDialog* dlg = frame->LoadDialog(QString::fromLatin1(name));
                    dlg->setAttribute(Qt::WA_DontShowOnScreen, true);
                    dlg->show();
                    dlg->hide();
                    fprintf(stderr, "ok   %s (loaded)\n", name);
                }
                panel->UnloadGame();
            }
        }
        console_mode_ = true;
        return false;
    }

    if (dimensions_unset) {
        // First run: default to roughly 90% of the primary display while
        // keeping the game panel at the native GBA aspect ratio (240x160 ==
        // 3:2), then center on that display.
        QScreen* screen = primaryScreen();
        const QRect display_rect = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);
        constexpr double kAspectRatio = 3.0 / 2.0;
        const QSize chrome = frame->frameGeometry().size() - frame->centralWidget()->size();

        int client_w = display_rect.width() * 9 / 10 - chrome.width();
        int client_h = display_rect.height() * 9 / 10 - chrome.height();

        if (client_w > static_cast<int>(client_h * kAspectRatio)) {
            client_w = static_cast<int>(client_h * kAspectRatio);
        } else {
            client_h = static_cast<int>(client_w / kAspectRatio);
        }

        frame->resize(client_w + chrome.width(), client_h + chrome.height());
        const QSize window_size = frame->frameGeometry().size();
        frame->move(display_rect.x() + (display_rect.width() - window_size.width()) / 2,
                    display_rect.y() + (display_rect.height() - window_size.height()) / 2);
    } else {
        // Ensure we are not drawing out of bounds.
        bool visible = false;
        for (QScreen* screen : screens()) {
            if (screen->geometry().intersects(client_rect)) {
                visible = true;
                break;
            }
        }
        if (visible) {
            frame->move(client_rect.topLeft());
            frame->resize(client_rect.size());
        }
    }

    if (is_maximized) {
        frame->showMaximized();
    }

    frame->show();

    if (is_fullscreen && !pending_load.isEmpty()) {
        frame->GetPanel()->ShowFullScreen(true);
    }

    // The idle loop only runs while it has work; wake it so a ROM given on
    // the command line is loaded (the wx port got a free idle event here).
    if (!pending_load.isEmpty()) {
        frame->GetPanel()->RequestMore();
    }

    // Pre-enumerate plugins so hotkey cycling and display config are instant.
    // Deferred so the main window paints first.
    QTimer::singleShot(0, this, [this] { EnumeratePlugins(); });

    return true;
}

void VbamApp::LoadGameLater(const QString& path) {
    pending_load = path;
    if (frame && frame->GetPanel()) {
        frame->GetPanel()->RequestMore();
    }
}

bool VbamApp::notify(QObject* receiver, QEvent* event) {
    if (frame && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        // Key events propagate to parent widgets inside this very call, so this
        // runs once per key event.
        QKeyEvent* key_event = static_cast<QKeyEvent*>(event);

        // Skip keyboard processing when focus is on a text entry control to
        // allow typing special characters. Exception: UserInputCtrl needs
        // keyboard processing to capture key bindings.
        QWidget* focused = focusWidget();
        bool is_text_widget = false;
        if (focused && !focused->inherits("widgets::UserInputCtrl")) {
            if (qobject_cast<QLineEdit*>(focused) || qobject_cast<QTextEdit*>(focused) ||
                qobject_cast<QPlainTextEdit*>(focused) ||
                qobject_cast<QAbstractSpinBox*>(focused) ||
                qobject_cast<QAbstractScrollArea*>(focused)) {
                // Text entry, and the item views / custom editors (the memory
                // viewer's hex editor) that navigate with the keyboard.
                is_text_widget = true;
            } else if (QComboBox* combo = qobject_cast<QComboBox*>(focused)) {
                is_text_widget = combo->isEditable();
            } else if (focused->property("vbamConsumesKeys").toBool()) {
                // A widget that asked for plain key events (viewers).
                is_text_widget = true;
            } else if (focused->parentWidget() &&
                       (qobject_cast<QAbstractSpinBox*>(focused->parentWidget()) ||
                        qobject_cast<QComboBox*>(focused->parentWidget()) ||
                        qobject_cast<QAbstractScrollArea*>(focused->parentWidget()))) {
                // The QLineEdit embedded in a spin box / editable combo box, or
                // a scroll area's viewport.
                is_text_widget = true;
            }
        }

        if (!is_text_widget) {
            // Generates user input batches on the InputDispatcher; game keys
            // reach the emulated gamepad synchronously through the sink.
            keyboard_input_handler_.ProcessKeyEvent(key_event);
        }
    }

    return QApplication::notify(receiver, event);
}

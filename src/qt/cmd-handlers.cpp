// Command handlers of MainWindow. Ported from src/wx/cmdevents.cpp.
//
// The handlers for the cheats / ROM info / link commands live in
// cmd-handlers-cheats.cpp and the Lua ones in cmd-handlers-lua.cpp; the
// dispatch switch at the bottom of this file (ExecuteCommand) covers them all.

#include "qt/main-window.h"

#include "qt/android-compat.h"

#include <cstring>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QTimer>
#include <QUrl>

#include "components/filters_interframe/interframe.h"
#include "core/base/check.h"
#include "core/base/system.h"
#include "core/base/version.h"
#include "core/gb/gb.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbPrinter.h"
#include "core/gb/gbSound.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaPrint.h"
#if defined(VBAM_ENABLE_DEBUGGER)
#include "core/gba/gbaRemote.h"
#endif
#include "core/gba/gbaSound.h"
#include "qt/app.h"
#include "qt/config/cmdtab.h"
#include "qt/config/option-proxy.h"
#include "qt/config/option.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/opts.h"
#include "qt/sys.h"
#include "qt/viewers/log-dialog.h"

#include "qt/dialogs/game-boy-advance-config.h"

#ifndef NO_LINK
#include "core/gba/gbaLink.h"
#endif

#ifndef NO_FFMPEG
#include "components/av_recording/av_recording.h"
#endif

namespace {

// Applies the menu item's checked state to `globalVar`: when the menu item
// and the variable agree, the command came from a shortcut, so toggle.
void toggleBooleanVar(bool* menuValue, bool* globalVar) {
    if (*menuValue == *globalVar)  // used accelerator
        *globalVar = !(*globalVar);
    else  // used menu item
        *globalVar = *menuValue;
}

void toggleBitVar(bool* menuValue, int* globalVar, int mask) {
    bool isEnabled = ((*globalVar) & (mask)) != (mask);
    if (*menuValue == isEnabled)
        *globalVar = ((*globalVar) & ~(mask)) | (!isEnabled ? (mask) : 0);
    else
        *globalVar = ((*globalVar) & ~(mask)) | (*menuValue ? (mask) : 0);
    *menuValue = ((*globalVar) & (mask)) != (mask);
}

const char* kAllFilesFilter = QT_TRANSLATE_NOOP("vbam", "All files (*)");

// Qt file dialog filter for the given "Name|*.a;*.b|Name2|..." style list,
// as one entry per format: "Name (*.a *.b)".
QString MakeFilter(const QString& name, const QString& exts) {
    QString e = exts;
    e.replace(';', ' ');
    return QString("%1 (%2)").arg(name, e);
}

const QString kGbaPatterns =
    "*.agb *.gba *.bin *.elf *.mb "
    "*.agb.lz *.gba.lz *.bin.lz *.elf.lz *.mb.lz "
    "*.agb.xz *.gba.xz *.bin.xz *.elf.xz *.mb.xz "
    "*.agb.bz2 *.gba.bz2 *.bin.bz2 *.elf.bz2 *.mb.bz2 "
    "*.agb.gz *.gba.gz *.bin.gz *.elf.gz *.mb.gz "
    "*.agb.z *.gba.z *.bin.z *.elf.z *.mb.z "
    "*.zip *.7z *.rar";

const QString kGbPatterns =
    "*.dmg *.gb *.gbc *.cgb *.sgb "
    "*.dmg.lz *.gb.lz *.gbc.lz *.cgb.lz *.sgb.lz "
    "*.dmg.xz *.gb.xz *.gbc.xz *.cgb.xz *.sgb.xz "
    "*.dmg.bz2 *.gb.bz2 *.gbc.bz2 *.cgb.bz2 *.sgb.bz2 "
    "*.dmg.gz *.gb.gz *.gbc.gz *.cgb.gz *.sgb.gz "
    "*.dmg.z *.gb.z *.gbc.z *.cgb.z *.sgb.z "
    "*.tar *.zip *.7z *.rar";

// Runs a file dialog through MainWindow::ShowModal so emulation state is
// handled like every other modal dialog. Returns the selected path or an
// empty string.
QString RunFileDialog(MainWindow* frame, QFileDialog& dlg) {
    if (frame->ShowModal(&dlg) != QDialog::Accepted) {
        return QString();
    }
    const QStringList files = dlg.selectedFiles();
    if (files.isEmpty()) {
        return QString();
    }
    // On Android the picker returns Storage-Access-Framework content:// URIs.
    // They are handed back as they are: each call site resolves (for reading)
    // or stages and commits (for writing) at the point that owns the file, the
    // same way the wx port does (see android-compat.h).
    return files.first();
}

// Helper function to get list of valid plugin paths.
QStringList GetValidPluginPaths() {
    VbamApp& app = vbamApp();
    if (!app.ArePluginsEnumerated()) {
        app.EnumeratePlugins();
    }
    return app.GetValidPlugins();
}

// Builds the "Format files (*.ext *.ext2);;..." filter list for a recorder
// format table and returns the index of the entry whose extensions contain
// `preferred` (or the "all files" entry).
QStringList BuildFormatFilters(const std::vector<char*>& fmts, const std::vector<char*>& exts,
                               const char* preferred, int* preferred_index) {
    QStringList filters;
    *preferred_index = -1;
    for (size_t i = 0; i < fmts.size() && i < exts.size(); ++i) {
        QString ext = QString::fromLatin1(exts[i]);
        ext.replace(",", " *.");
        ext.prepend("*.");
        if (*preferred_index < 0 && ext.contains(preferred)) {
            *preferred_index = static_cast<int>(i);
        }
        filters << QString("%1%2 (%3)")
                       .arg(QString::fromLatin1(fmts[i]),
                            QCoreApplication::translate("vbam", " files"), ext);
    }
    filters << QCoreApplication::translate("vbam", kAllFilesFilter);
    if (*preferred_index < 0) {
        *preferred_index = static_cast<int>(fmts.size());
    }
    return filters;
}

// Default extension of the format at `index` in a recorder format table.
QString DefaultExtension(const std::vector<char*>& exts, int index) {
    if (index < 0 || index >= static_cast<int>(exts.size())) {
        return QString();
    }
    return "." + QString::fromLatin1(exts[index]).section(',', 0, 0);
}

int state_slot = 0;
QString st_dir;

}  // namespace

//// File menu

void MainWindow::OnOpen() {
    static int open_ft = 0;
    static QString last_gba_picked_dir;
    const QString gba_rom_dir = OPTION(kGBAROMDir);

    // Hold Shift while invoking this menu item (or its accelerator) to open at
    // the last directory a file was actually picked from, instead of the
    // configured ROM directory.
    const bool use_remembered =
        (QApplication::keyboardModifiers() & Qt::ShiftModifier) && !last_gba_picked_dir.isEmpty();

    QString start_dir;
    if (use_remembered) {
        start_dir = last_gba_picked_dir;
    } else if (!gba_rom_dir.isEmpty()) {
        start_dir = vbamApp().GetAbsolutePath(gba_rom_dir);
    }

    const QStringList filters = {
        MakeFilter(tr("Game Boy Advance Files"), kGbaPatterns),
        MakeFilter(tr("Game Boy Files"), kGbPatterns),
        tr(kAllFilesFilter),
    };

    QFileDialog dlg(this, tr("Open ROM file"), start_dir);
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setNameFilters(filters);
    dlg.selectNameFilter(filters.value(open_ft));

    const QString path = RunFileDialog(this, dlg);
    if (!path.isEmpty()) {
        last_gba_picked_dir = dlg.directory().absolutePath();
        vbamApp().LoadGameLater(path);
    }

    open_ft = filters.indexOf(dlg.selectedNameFilter());
    if (gba_rom_dir.isEmpty()) {
        OPTION(kGBAROMDir) = dlg.directory().absolutePath();
    }
}

void MainWindow::OnOpenGB() {
    static int open_ft = 0;
    static QString last_gb_picked_dir;
    const QString gb_rom_dir = OPTION(kGBROMDir);

    const bool use_remembered =
        (QApplication::keyboardModifiers() & Qt::ShiftModifier) && !last_gb_picked_dir.isEmpty();

    QString start_dir;
    if (use_remembered) {
        start_dir = last_gb_picked_dir;
    } else if (!gb_rom_dir.isEmpty()) {
        start_dir = vbamApp().GetAbsolutePath(gb_rom_dir);
    }

    const QStringList filters = {
        MakeFilter(tr("Game Boy Files"), kGbPatterns),
        tr(kAllFilesFilter),
    };

    QFileDialog dlg(this, tr("Open GB ROM file"), start_dir);
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setNameFilters(filters);
    dlg.selectNameFilter(filters.value(open_ft));

    const QString path = RunFileDialog(this, dlg);
    if (!path.isEmpty()) {
        last_gb_picked_dir = dlg.directory().absolutePath();
        vbamApp().LoadGameLater(path);
    }

    open_ft = filters.indexOf(dlg.selectedNameFilter());
    if (gb_rom_dir.isEmpty()) {
        OPTION(kGBROMDir) = dlg.directory().absolutePath();
    }
}

void MainWindow::OnOpenGBC() {
    static int open_ft = 0;
    static QString last_gbc_picked_dir;
    const QString gbc_rom_dir = OPTION(kGBGBCROMDir);

    const bool use_remembered =
        (QApplication::keyboardModifiers() & Qt::ShiftModifier) && !last_gbc_picked_dir.isEmpty();

    QString start_dir;
    if (use_remembered) {
        start_dir = last_gbc_picked_dir;
    } else if (!gbc_rom_dir.isEmpty()) {
        start_dir = vbamApp().GetAbsolutePath(gbc_rom_dir);
    }

    const QStringList filters = {
        MakeFilter(tr("Game Boy Color Files"), kGbPatterns),
        tr(kAllFilesFilter),
    };

    QFileDialog dlg(this, tr("Open GBC ROM file"), start_dir);
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setNameFilters(filters);
    dlg.selectNameFilter(filters.value(open_ft));

    const QString path = RunFileDialog(this, dlg);
    if (!path.isEmpty()) {
        last_gbc_picked_dir = dlg.directory().absolutePath();
        vbamApp().LoadGameLater(path);
    }

    open_ft = filters.indexOf(dlg.selectedNameFilter());
    if (gbc_rom_dir.isEmpty()) {
        OPTION(kGBGBCROMDir) = dlg.directory().absolutePath();
    }
}

void MainWindow::OnRecentReset() {
    // only save config if there were items to remove
    if (gopts.recent.GetCount()) {
        gopts.recent.Clear();
        ResetRecentMenu();
    }
}

void MainWindow::OnRecentFreeze() {
    GetMenuOptionConfig(cmd::kRecentFreeze, config::OptionID::kGenFreezeRecent);
}

// following 10 should really be a single ranged handler
#define RECENT_HANDLER(n)                                                       \
    void MainWindow::OnFile##n() {                                              \
        const QString file = gopts.recent.GetHistoryFile(n - 1);                \
        if (!file.isEmpty())                                                    \
            panel->LoadGame(file);                                              \
    }
RECENT_HANDLER(1)
RECENT_HANDLER(2)
RECENT_HANDLER(3)
RECENT_HANDLER(4)
RECENT_HANDLER(5)
RECENT_HANDLER(6)
RECENT_HANDLER(7)
RECENT_HANDLER(8)
RECENT_HANDLER(9)
RECENT_HANDLER(10)
#undef RECENT_HANDLER

void MainWindow::DoResetLoadingDotCodeFile() {
    ResetLoadDotCodeFile();
}

void MainWindow::DoSetLoadingDotCodeFile() {
    static QString loaddotcodefile_path;
    QFileDialog dlg(this, tr("Select Dot Code file"), loaddotcodefile_path,
                    tr("E-Reader Dot Code (*.bin *.raw)"));
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);

    const QString path = RunFileDialog(this, dlg);
    if (path.isEmpty())
        return;

    loaddotcodefile_path = path;
    SetLoadDotCodeFile(vbam::ToPath(loaddotcodefile_path).c_str());
}

void MainWindow::DoResetSavingDotCodeFile() {
    ResetSaveDotCodeFile();
}

void MainWindow::DoSetSavingDotCodeFile() {
    static QString savedotcodefile_path;
    QFileDialog dlg(this, tr("Select Dot Code file"), savedotcodefile_path,
                    tr("E-Reader Dot Code (*.bin *.raw)"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setFileMode(QFileDialog::AnyFile);

    const QString path = RunFileDialog(this, dlg);
    if (path.isEmpty())
        return;

    savedotcodefile_path = path;
    SetSaveDotCodeFile(vbam::ToPath(savedotcodefile_path).c_str());
}

void MainWindow::DoScreenCapture() {
    QString scap_path = GetGamePath(OPTION(kGenScreenshotDir));
    QString def_name = panel->game_name();

    const int capture_format = OPTION(kPrefCaptureFormat);
    if (capture_format == 0)
        def_name.append(".png");
    else
        def_name.append(".bmp");

    const QStringList filters = {tr("PNG images (*.png)"), tr("BMP images (*.bmp)")};

    QFileDialog dlg(this, tr("Select output file"), QDir(scap_path).filePath(def_name));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setFileMode(QFileDialog::AnyFile);
    dlg.setNameFilters(filters);
    dlg.selectNameFilter(filters.value(capture_format));

    const QString fn = RunFileDialog(this, dlg);
    if (fn.isEmpty())
        return;

    int fmt = filters.indexOf(dlg.selectedNameFilter());
    if (fmt < 0)
        fmt = capture_format;

    if (fn.endsWith(".bmp", Qt::CaseInsensitive))
        fmt = 1;
    else if (fn.endsWith(".png", Qt::CaseInsensitive))
        fmt = 0;

    // The Android file picker hands back a Storage-Access-Framework content://
    // URI, which the stdio-based image writers cannot open. Write to a local
    // staging file -- named after the picked document, so the format check
    // below still has an extension to read -- and transfer it once written.
    // A no-op for a real path.
    QString out_name = VbamStageAndroidOutputFile(fn, fmt == 0 ? "png" : "bmp");
    if (out_name.endsWith(".bmp", Qt::CaseInsensitive))
        fmt = 1;
    else if (out_name.endsWith(".png", Qt::CaseInsensitive))
        fmt = 0;
    else
        out_name += fmt == 0 ? ".png" : ".bmp";

    bool ok = fmt == 0 ? panel->emusys->emuWritePNG(vbam::ToPath(out_name).c_str())
                       : panel->emusys->emuWriteBMP(vbam::ToPath(out_name).c_str());

    if (ok)
        ok = VbamCommitAndroidOutputFile(out_name);
    else
        VbamDiscardAndroidOutputFile(out_name);

    QString msg;
    if (ok)
        msg = tr("Wrote snapshot %1").arg(fn);
    else
        msg = tr("Error saving snapshot file %1").arg(fn);

    systemScreenMessage(msg);
}

void MainWindow::DoRecordSoundStartRecording() {
#ifndef NO_FFMPEG
    static int sound_extno = -1;
    static QString sound_path;

    const std::vector<char*> fmts = recording::getSupAudNames();
    const std::vector<char*> exts = recording::getSupAudExts();
    int preferred = 0;
    const QStringList filters = BuildFormatFilters(fmts, exts, "*.wav", &preferred);
    if (sound_extno < 0)
        sound_extno = preferred;

    sound_path = GetGamePath(OPTION(kGenRecordingDir));
    const QString def_name = panel->game_name() + DefaultExtension(exts, sound_extno);

    QFileDialog dlg(this, tr("Select output file"), QDir(sound_path).filePath(def_name));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setFileMode(QFileDialog::AnyFile);
    dlg.setNameFilters(filters);
    dlg.selectNameFilter(filters.value(sound_extno));

    const QString path = RunFileDialog(this, dlg);
    sound_extno = filters.indexOf(dlg.selectedNameFilter());
    sound_path = dlg.directory().absolutePath();

    if (path.isEmpty())
        return;

    panel->StartSoundRecording(path);
#endif
}

void MainWindow::DoRecordSoundStopRecording() {
#ifndef NO_FFMPEG
    panel->StopSoundRecording();
#endif
}

void MainWindow::DoRecordAVIStartRecording() {
#ifndef NO_FFMPEG
    static int vid_extno = -1;
    static QString vid_path;

    const std::vector<char*> fmts = recording::getSupVidNames();
    const std::vector<char*> exts = recording::getSupVidExts();
    int preferred = 0;
    const QStringList filters = BuildFormatFilters(fmts, exts, "*.avi", &preferred);
    if (vid_extno < 0)
        vid_extno = preferred;

    vid_path = GetGamePath(OPTION(kGenRecordingDir));
    const QString def_name = panel->game_name() + DefaultExtension(exts, vid_extno);

    QFileDialog dlg(this, tr("Select output file"), QDir(vid_path).filePath(def_name));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setFileMode(QFileDialog::AnyFile);
    dlg.setNameFilters(filters);
    dlg.selectNameFilter(filters.value(vid_extno));

    const QString path = RunFileDialog(this, dlg);
    vid_extno = filters.indexOf(dlg.selectedNameFilter());
    vid_path = dlg.directory().absolutePath();

    if (path.isEmpty())
        return;

    panel->StartVidRecording(path);
#endif
}

void MainWindow::DoRecordAVIStopRecording() {
#ifndef NO_FFMPEG
    panel->StopVidRecording();
#endif
}

void MainWindow::DoRecordMovieStartRecording() {
    static int mov_extno = -1;
    static QString mov_path;

    const std::vector<char*> fmts = getSupMovNamesToRecord();
    const std::vector<char*> exts = getSupMovExtsToRecord();
    int preferred = 0;
    const QStringList filters = BuildFormatFilters(fmts, exts, "*.vmv", &preferred);
    if (mov_extno < 0)
        mov_extno = preferred;

    mov_path = GetGamePath(OPTION(kGenRecordingDir));
    const QString def_name = panel->game_name() + DefaultExtension(exts, mov_extno);

    QFileDialog dlg(this, tr("Select output file"), QDir(mov_path).filePath(def_name));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setFileMode(QFileDialog::AnyFile);
    dlg.setNameFilters(filters);
    dlg.selectNameFilter(filters.value(mov_extno));

    const QString path = RunFileDialog(this, dlg);
    mov_extno = filters.indexOf(dlg.selectedNameFilter());
    mov_path = dlg.directory().absolutePath();

    if (path.isEmpty())
        return;

    const std::vector<MVFormatID> formats = getSupMovFormatsToRecord();
    const MVFormatID format = mov_extno >= 0 && mov_extno < static_cast<int>(formats.size())
                                  ? formats[mov_extno]
                                  : MV_FORMAT_ID_VMV;
    systemStartGameRecording(path, format);
}

void MainWindow::DoRecordMovieStopRecording() {
    systemStopGameRecording();
}

void MainWindow::DoPlayMovieStartPlaying() {
    static int mov_extno = -1;
    static QString mov_path;

    const std::vector<char*> fmts = getSupMovNamesToPlayback();
    const std::vector<char*> exts = getSupMovExtsToPlayback();
    int preferred = 0;
    const QStringList filters = BuildFormatFilters(fmts, exts, "*.vmv", &preferred);
    if (mov_extno < 0)
        mov_extno = preferred;

    mov_path = GetGamePath(OPTION(kGenRecordingDir));
    systemStopGamePlayback();
    const QString def_name = panel->game_name() + DefaultExtension(exts, mov_extno);

    QFileDialog dlg(this, tr("Select file"), QDir(mov_path).filePath(def_name));
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setNameFilters(filters);
    dlg.selectNameFilter(filters.value(mov_extno));

    const QString path = RunFileDialog(this, dlg);
    mov_extno = filters.indexOf(dlg.selectedNameFilter());
    mov_path = dlg.directory().absolutePath();

    if (path.isEmpty())
        return;

    const std::vector<MVFormatID> formats = getSupMovFormatsToPlayback();
    const MVFormatID format = mov_extno >= 0 && mov_extno < static_cast<int>(formats.size())
                                  ? formats[mov_extno]
                                  : MV_FORMAT_ID_VMV;
    systemStartGamePlayback(path, format);
}

void MainWindow::DoPlayMovieStopPlaying() {
    systemStopGamePlayback();
}

// formerly Close
void MainWindow::DoClose() {
    panel->UnloadGame();
}

// formerly Exit
void MainWindow::OnExit() {
    close();
}

//// Emulation menu

void MainWindow::OnPause() {
    bool menuPress = false;
    GetMenuOptionBool(cmd::kPause, &menuPress);
    toggleBooleanVar(&menuPress, &paused);
    SetMenuOption(cmd::kPause, paused ? 1 : 0);

    if (paused)
        panel->Pause();
    else if (!IsPaused())
        panel->Resume();

    // undo next-frame's zeroing of frameskip
    const int frame_skip = OPTION(kPrefFrameSkip);
    if (frame_skip != -1) {
        systemFrameSkip = frame_skip;
    }
}

void MainWindow::DoEmulatorSpeedupToggle() {
    bool menuPress = false;
    GetMenuOptionBool(cmd::kEmulatorSpeedupToggle, &menuPress);
    toggleBooleanVar(&menuPress, &turbo);
    SetMenuOption(cmd::kEmulatorSpeedupToggle, turbo ? 1 : 0);
}

void MainWindow::DoReset() {
    panel->emusys->emuReset();
}

void MainWindow::OnToggleFullscreen() {
    panel->ShowFullScreen(!panel->IsFullScreen());
}

#define AUTOFIRE_HANDLER(name, var, keym)                    \
    void MainWindow::On##name() {                            \
        bool menuPress = false;                              \
        GetMenuOptionBool(cmd::k##name, &menuPress);         \
        toggleBitVar(&menuPress, &var, keym);                \
        SetMenuOption(cmd::k##name, menuPress ? 1 : 0);      \
        GetMenuOptionInt(cmd::k##name, &var, keym);          \
    }

AUTOFIRE_HANDLER(JoypadAutofireA, autofire, KEYM_A)
AUTOFIRE_HANDLER(JoypadAutofireB, autofire, KEYM_B)
AUTOFIRE_HANDLER(JoypadAutofireL, autofire, KEYM_L)
AUTOFIRE_HANDLER(JoypadAutofireR, autofire, KEYM_R)
AUTOFIRE_HANDLER(JoypadAutoholdUp, autohold, KEYM_UP)
AUTOFIRE_HANDLER(JoypadAutoholdDown, autohold, KEYM_DOWN)
AUTOFIRE_HANDLER(JoypadAutoholdLeft, autohold, KEYM_LEFT)
AUTOFIRE_HANDLER(JoypadAutoholdRight, autohold, KEYM_RIGHT)
AUTOFIRE_HANDLER(JoypadAutoholdA, autohold, KEYM_A)
AUTOFIRE_HANDLER(JoypadAutoholdB, autohold, KEYM_B)
AUTOFIRE_HANDLER(JoypadAutoholdL, autohold, KEYM_L)
AUTOFIRE_HANDLER(JoypadAutoholdR, autohold, KEYM_R)
AUTOFIRE_HANDLER(JoypadAutoholdSelect, autohold, KEYM_SELECT)
AUTOFIRE_HANDLER(JoypadAutoholdStart, autohold, KEYM_START)
#undef AUTOFIRE_HANDLER

void MainWindow::OnAllowKeyboardBackgroundInput() {
    GetMenuOptionConfig(cmd::kAllowKeyboardBackgroundInput,
                        config::OptionID::kUIAllowKeyboardBackgroundInput);
}

void MainWindow::OnAllowJoystickBackgroundInput() {
    GetMenuOptionConfig(cmd::kAllowJoystickBackgroundInput,
                        config::OptionID::kUIAllowJoystickBackgroundInput);
}

void MainWindow::DoLoadGameRecent() {
    panel->LoadState();
}

void MainWindow::OnLoadGameAutoLoad() {
    GetMenuOptionConfig(cmd::kLoadGameAutoLoad, config::OptionID::kGenAutoLoadLastState);
}

#define LOADSTATE_HANDLER(n, slot) \
    void MainWindow::DoLoadGame##n() { panel->LoadState(slot); }
LOADSTATE_HANDLER(01, 1)
LOADSTATE_HANDLER(02, 2)
LOADSTATE_HANDLER(03, 3)
LOADSTATE_HANDLER(04, 4)
LOADSTATE_HANDLER(05, 5)
LOADSTATE_HANDLER(06, 6)
LOADSTATE_HANDLER(07, 7)
LOADSTATE_HANDLER(08, 8)
LOADSTATE_HANDLER(09, 9)
LOADSTATE_HANDLER(10, 10)
#undef LOADSTATE_HANDLER

void MainWindow::DoLoad() {
    if (st_dir.isEmpty())
        st_dir = panel->state_dir();

    QFileDialog dlg(this, tr("Select state file"), st_dir,
                    tr("Visual Boy Advance saved game files (*.sgm)"));
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);

    const QString path = RunFileDialog(this, dlg);
    st_dir = dlg.directory().absolutePath();

    if (path.isEmpty())
        return;

    // An Android content:// URI becomes a local copy the state reader can open.
    panel->LoadState(VbamResolveAndroidContentUri(path));
}

void MainWindow::OnKeepSaves() {
    bool menuPress = false;
    GetMenuOptionBool(cmd::kKeepSaves, &menuPress);
    toggleBitVar(&menuPress, &coreOptions.skipSaveGameBattery, 1);
    SetMenuOption(cmd::kKeepSaves, menuPress ? 1 : 0);
    GetMenuOptionInt(cmd::kKeepSaves, &coreOptions.skipSaveGameBattery, 1);
    update_opts();
}

void MainWindow::OnKeepCheats() {
    bool menuPress = false;
    GetMenuOptionBool(cmd::kKeepCheats, &menuPress);
    toggleBitVar(&menuPress, &coreOptions.skipSaveGameCheats, 1);
    SetMenuOption(cmd::kKeepCheats, menuPress ? 1 : 0);
    GetMenuOptionInt(cmd::kKeepCheats, &coreOptions.skipSaveGameCheats, 1);
    update_opts();
}

void MainWindow::DoSaveGameOldest() {
    panel->SaveState();
}

#define SAVESTATE_HANDLER(n, slot) \
    void MainWindow::DoSaveGame##n() { panel->SaveState(slot); }
SAVESTATE_HANDLER(01, 1)
SAVESTATE_HANDLER(02, 2)
SAVESTATE_HANDLER(03, 3)
SAVESTATE_HANDLER(04, 4)
SAVESTATE_HANDLER(05, 5)
SAVESTATE_HANDLER(06, 6)
SAVESTATE_HANDLER(07, 7)
SAVESTATE_HANDLER(08, 8)
SAVESTATE_HANDLER(09, 9)
SAVESTATE_HANDLER(10, 10)
#undef SAVESTATE_HANDLER

void MainWindow::DoSave() {
    if (st_dir.isEmpty())
        st_dir = panel->state_dir();

    QFileDialog dlg(this, tr("Select state file"), st_dir,
                    tr("Visual Boy Advance saved game files (*.sgm)"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setFileMode(QFileDialog::AnyFile);
    dlg.setDefaultSuffix("sgm");

    const QString path = RunFileDialog(this, dlg);
    st_dir = dlg.directory().absolutePath();

    if (path.isEmpty())
        return;

    // The state writer cannot open an Android content:// URI, so write to a
    // local staging file and transfer it to the picked document. A no-op for
    // a real path.
    const QString out_name = VbamStageAndroidOutputFile(path, "sgm");

    if (!panel->SaveState(out_name)) {
        VbamDiscardAndroidOutputFile(out_name);
        return;
    }

    if (!VbamCommitAndroidOutputFile(out_name)) {
        // SaveState() already reported success for the staging file, so say
        // that the state did not reach the file the user actually picked.
        systemScreenMessage(tr("Error saving state %1").arg(path));
    }
}

void MainWindow::DoLoadGameSlot() {
    panel->LoadState(state_slot + 1);
}

void MainWindow::DoSaveGameSlot() {
    panel->SaveState(state_slot + 1);
}

void MainWindow::DoIncrGameSlot() {
    state_slot = (state_slot + 1) % 10;
    systemScreenMessage(tr("Current state slot #%1").arg(state_slot));
}

void MainWindow::DoDecrGameSlot() {
    state_slot = (state_slot + 9) % 10;
    systemScreenMessage(tr("Current state slot #%1").arg(state_slot));
}

void MainWindow::DoIncrGameSlotSave() {
    state_slot = (state_slot + 1) % 10;
    panel->SaveState(state_slot + 1);
    systemScreenMessage(tr("Current state slot #%1").arg(state_slot));
}

void MainWindow::DoRewind() {
    int rew_st = (panel->next_rewind_state + NUM_REWINDS - 1) % NUM_REWINDS;

    // if within 5 seconds of last one, and > 1 state, delete last state & move back
    // FIXME: 5 should actually be user-configurable
    // maybe instead of 5, 10% of rewind_interval
    if (panel->num_rewind_states > 1 &&
        (gopts.rewind_interval <= 5 ||
         (int)panel->rewind_time / 6 > gopts.rewind_interval - 5)) {
        --panel->num_rewind_states;
        panel->next_rewind_state = rew_st;

        if (gopts.rewind_interval > 5)
            rew_st = (rew_st + NUM_REWINDS - 1) % NUM_REWINDS;
    }

    panel->emusys->emuReadMemState(&panel->rewind_mem[rew_st * REWIND_SIZE], REWIND_SIZE);
    InterframeCleanup();
    // FIXME: if(paused) blank screen
    panel->do_rewind = false;
    panel->rewind_time = gopts.rewind_interval * 6;
}

void MainWindow::OnColorizerHack() {
    GetMenuOptionConfig(cmd::kColorizerHack, config::OptionID::kGBColorizerHack);
    if (OPTION(kGBColorizerHack) && OPTION(kPrefUseBiosGB)) {
        vbam::LogError(tr("Cannot use Colorizer Hack when Game Boy BIOS File is enabled."));
        SetMenuOption(cmd::kColorizerHack, 0);
        OPTION(kGBColorizerHack) = false;
    }
}

//// Debug menu

#define LAYER_HANDLER(name, bit)                                          \
    void MainWindow::Do##name() {                                         \
        bool menuPress = false;                                           \
        GetMenuOptionBool(cmd::k##name, &menuPress);                      \
        toggleBitVar(&menuPress, &coreOptions.layerSettings, (1 << bit)); \
        SetMenuOption(cmd::k##name, menuPress ? 1 : 0);                   \
        GetMenuOptionInt(cmd::k##name, &coreOptions.layerSettings, (1 << bit)); \
        coreOptions.layerEnable = DISPCNT & coreOptions.layerSettings;    \
        CPUUpdateRenderBuffers(false);                                    \
    }
LAYER_HANDLER(VideoLayersBG0, 8)
LAYER_HANDLER(VideoLayersBG1, 9)
LAYER_HANDLER(VideoLayersBG2, 10)
LAYER_HANDLER(VideoLayersBG3, 11)
LAYER_HANDLER(VideoLayersOBJ, 12)
LAYER_HANDLER(VideoLayersWIN0, 13)
LAYER_HANDLER(VideoLayersWIN1, 14)
LAYER_HANDLER(VideoLayersOBJWIN, 15)
#undef LAYER_HANDLER

void MainWindow::DoVideoLayersReset() {
    coreOptions.layerSettings = 0x7f00;
    coreOptions.layerEnable = DISPCNT & coreOptions.layerSettings;
    SetMenuOption(cmd::kVideoLayersBG0, true);
    SetMenuOption(cmd::kVideoLayersBG1, true);
    SetMenuOption(cmd::kVideoLayersBG2, true);
    SetMenuOption(cmd::kVideoLayersBG3, true);
    SetMenuOption(cmd::kVideoLayersOBJ, true);
    SetMenuOption(cmd::kVideoLayersWIN0, true);
    SetMenuOption(cmd::kVideoLayersWIN1, true);
    SetMenuOption(cmd::kVideoLayersOBJWIN, true);
    CPUUpdateRenderBuffers(false);
}

#define SOUND_CHANNEL_HANDLER(name, bit)                                  \
    void MainWindow::Do##name() {                                         \
        bool menuPress = false;                                           \
        GetMenuOptionBool(cmd::k##name, &menuPress);                      \
        toggleBitVar(&menuPress, &gopts.sound_en, (1 << bit));            \
        SetMenuOption(cmd::k##name, menuPress ? 1 : 0);                   \
        GetMenuOptionInt(cmd::k##name, &gopts.sound_en, (1 << bit));      \
        soundSetEnable(gopts.sound_en);                                   \
        update_opts();                                                    \
    }
SOUND_CHANNEL_HANDLER(SoundChannel1, 0)
SOUND_CHANNEL_HANDLER(SoundChannel2, 1)
SOUND_CHANNEL_HANDLER(SoundChannel3, 2)
SOUND_CHANNEL_HANDLER(SoundChannel4, 3)
SOUND_CHANNEL_HANDLER(DirectSoundA, 8)
SOUND_CHANNEL_HANDLER(DirectSoundB, 9)
#undef SOUND_CHANNEL_HANDLER

void MainWindow::OnToggleSound() {
    bool en = gopts.sound_en == 0;
    gopts.sound_en = en ? 0x30f : 0;
    SetMenuOption(cmd::kSoundChannel1, en);
    SetMenuOption(cmd::kSoundChannel2, en);
    SetMenuOption(cmd::kSoundChannel3, en);
    SetMenuOption(cmd::kSoundChannel4, en);
    SetMenuOption(cmd::kDirectSoundA, en);
    SetMenuOption(cmd::kDirectSoundB, en);
    soundSetEnable(gopts.sound_en);
    update_opts();
    systemScreenMessage(en ? tr("Sound enabled") : tr("Sound disabled"));
}

void MainWindow::OnIncreaseVolume() {
    OPTION(kSoundVolume) += 5;
}

void MainWindow::OnDecreaseVolume() {
    OPTION(kSoundVolume) -= 5;
}

void MainWindow::DoNextFrame() {
    SetMenuOption(cmd::kPause, true);
    paused = true;
    pause_next = true;

    if (!IsPaused())
        panel->Resume();

    systemFrameSkip = 0;
}

void MainWindow::DoDisassemble() {
    Disassemble();
}

void MainWindow::OnLogging() {
    LogDialog* dlg = GetLogDialog();
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::DoIOViewer() {
    IOViewer();
}

void MainWindow::DoMapViewer() {
    MapViewer();
}

void MainWindow::DoMemoryViewer() {
    MemViewer();
}

void MainWindow::DoOAMViewer() {
    OAMViewer();
}

void MainWindow::DoPaletteViewer() {
    PaletteViewer();
}

void MainWindow::DoTileViewer() {
    TileViewer();
}

//// GDB

#if defined(VBAM_ENABLE_DEBUGGER)
extern int remotePort;

namespace {

int GetGDBPort(MainWindow* mf) {
    ModalPause mp;
    bool ok = false;
    const int port = QInputDialog::getInt(
        mf, QCoreApplication::translate("vbam", "GDB Connection"),
#ifdef _WIN32
        QCoreApplication::translate("vbam", "Port to wait for connection:"),
#else
        QCoreApplication::translate("vbam", "Port to wait for connection:") + "\n" +
            QCoreApplication::translate("vbam", "Set to 0 for pseudo tty"),
#endif
        gopts.gdb_port,
#ifdef _WIN32
        1025,
#else
        0,
#endif
        65535, 1, &ok);
    return ok ? port : -1;
}

}  // namespace
#endif  // defined(VBAM_ENABLE_DEBUGGER)

void MainWindow::OnDebugGDBPort() {
#if defined(VBAM_ENABLE_DEBUGGER)
    int port_selected = GetGDBPort(this);

    if (port_selected != -1) {
        gopts.gdb_port = port_selected;
        update_opts();
    }
#endif  // defined(VBAM_ENABLE_DEBUGGER)
}

void MainWindow::OnDebugGDBBreakOnLoad() {
#if defined(VBAM_ENABLE_DEBUGGER)
    GetMenuOptionConfig(cmd::kDebugGDBBreakOnLoad, config::OptionID::kPrefGDBBreakOnLoad);
#endif  // defined(VBAM_ENABLE_DEBUGGER)
}

void MainWindow::GDBBreak() {
#if defined(VBAM_ENABLE_DEBUGGER)
    ModalPause mp;

    if (gopts.gdb_port == 0) {
        int port_selected = GetGDBPort(this);

        if (port_selected != -1) {
            gopts.gdb_port = port_selected;
            update_opts();
        }
    }

    if (gopts.gdb_port > 0) {
        if (!remotePort) {
            QString msg;
#ifndef _WIN32
            if (!gopts.gdb_port) {
                if (!debugOpenPty())
                    return;

                msg = tr("Waiting for connection at %1").arg(debugGetSlavePty());
            } else
#endif
            {
                if (!debugStartListen(gopts.gdb_port))
                    return;

                msg = tr("Waiting for connection on port %1").arg(gopts.gdb_port);
            }

            // Poll for the connection while showing a cancellable progress
            // dialog (the wx PulseActionDialog).
            bool connected = false;
            QProgressDialog dlg(msg, tr("Cancel"), 0, 0, this);
            dlg.setWindowTitle(tr("Waiting for GDB..."));
            dlg.setWindowModality(Qt::ApplicationModal);
            dlg.setMinimumDuration(0);
            QTimer poll;
            poll.setInterval(100);
            connect(&poll, &QTimer::timeout, &dlg, [&] {
#ifndef _WIN32
                if (!gopts.gdb_port)
                    connected = debugWaitPty();
                else
#endif
                    connected = debugWaitSocket();
                if (connected)
                    dlg.accept();
            });
            poll.start();
            dlg.exec();
            poll.stop();

            if (connected) {
                remotePort = gopts.gdb_port;
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
#endif  // defined(VBAM_ENABLE_DEBUGGER)
}

void MainWindow::DoDebugGDBBreak() {
#if defined(VBAM_ENABLE_DEBUGGER)
    GDBBreak();
#endif  // defined(VBAM_ENABLE_DEBUGGER)
}

void MainWindow::DoDebugGDBDisconnect() {
#if defined(VBAM_ENABLE_DEBUGGER)
    debugger = false;
    dbgMain = nullptr;
    dbgSignal = nullptr;
    dbgOutput = nullptr;
    remotePort = 0;
    remoteCleanUp();
    cmd_enable &= ~CMDEN_GDB;
    cmd_enable |= CMDEN_NGDB_GBA | CMDEN_NGDB_ANY;
    enable_menus();
#endif  // defined(VBAM_ENABLE_DEBUGGER)
}

//// Options menu

void MainWindow::OnGeneralConfigure() {
    int rew = gopts.rewind_interval;
    QDialog* dlg = LoadDialog("GeneralConfig");

    if (ShowModal(dlg) == QDialog::Accepted)
        update_opts();

    if (panel->game_type() != IMAGE_UNKNOWN)
        soundSetThrottle(coreOptions.throttle);

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

void MainWindow::OnSpeedupConfigure() {
    QDialog* dlg = LoadDialog("SpeedupConfig");

    unsigned save_speedup_throttle = coreOptions.speedup_throttle;
    unsigned save_speedup_frame_skip = coreOptions.speedup_frame_skip;
    bool save_speedup_throttle_frame_skip = coreOptions.speedup_throttle_frame_skip;

    if (ShowModal(dlg) == QDialog::Accepted)
        update_opts();
    else {
        // Restore values if cancel pressed.
        coreOptions.speedup_throttle = save_speedup_throttle;
        coreOptions.speedup_frame_skip = save_speedup_frame_skip;
        coreOptions.speedup_throttle_frame_skip = save_speedup_throttle_frame_skip;
    }
}

void MainWindow::OnGameBoyConfigure() {
    ShowModal(LoadDialog("GameBoyConfig"));
}

void MainWindow::OnSetSize1x() { OPTION(kDispScale) = 1; }
void MainWindow::OnSetSize2x() { OPTION(kDispScale) = 2; }
void MainWindow::OnSetSize3x() { OPTION(kDispScale) = 3; }
void MainWindow::OnSetSize4x() { OPTION(kDispScale) = 4; }
void MainWindow::OnSetSize5x() { OPTION(kDispScale) = 5; }
void MainWindow::OnSetSize6x() { OPTION(kDispScale) = 6; }

void MainWindow::OnGameBoyAdvanceConfigure() {
    // The dialog loads the per-game overrides (vba-over.ini) when shown and
    // writes them back on accept (see dialogs::GameBoyAdvanceConfig).
    QDialog* dlg = LoadDialog("GameBoyAdvanceConfig");

    if (ShowModal(dlg) != QDialog::Accepted)
        return;

    if (panel->game_type() == IMAGE_GBA) {
        agbPrintEnable(OPTION(kPrefAgbPrint));
    }

    update_opts();
}

void MainWindow::DoDisplayConfigure() {
    QDialog* dlg = LoadDialog("DisplayConfig");
    if (ShowModal(dlg) != QDialog::Accepted) {
        return;
    }

    const uint32_t bitdepth = OPTION(kBitDepth);
    // Don't override systemColorDepth if a plugin filter is active
    // Plugin filters have specific color depth requirements
    if (OPTION(kDispFilter) != config::Filter::kPlugin) {
        systemColorDepth = (int)((bitdepth + 1) << 3);
    }

    const int frame_skip = OPTION(kPrefFrameSkip);
    if (frame_skip != -1) {
        systemFrameSkip = frame_skip;
    }

    update_opts();
}

void MainWindow::DoChangeFilter() {
    const config::Filter current_filter = OPTION(kDispFilter);
    const QString current_plugin = OPTION(kDispFilterPlugin);
    QString msg;

    if (current_filter == config::Filter::kPlugin) {
        // Currently on a plugin - cycle to next plugin or wrap to kNone
        QStringList plugins = GetValidPluginPaths();

        if (plugins.isEmpty()) {
            // No plugins available, go to None
            OPTION(kDispFilter) = config::Filter::kNone;
            OPTION(kDispFilterPlugin) = QString();
            msg = tr("Filter: %1").arg(
                config::Option::ByID(config::OptionID::kDispFilter)->GetEnumString());
        } else {
            int current_index = plugins.indexOf(current_plugin);
            int next_index = current_index + 1;
            if (next_index >= plugins.size()) {
                // Wrapped past last plugin, go to None
                OPTION(kDispFilter) = config::Filter::kNone;
                OPTION(kDispFilterPlugin) = QString();
                msg = tr("Filter: %1").arg(
                    config::Option::ByID(config::OptionID::kDispFilter)->GetEnumString());
            } else {
                // Go to next plugin
                OPTION(kDispFilterPlugin) = plugins[next_index];
                msg = tr("Filter: Plugin (%1)").arg(QFileInfo(plugins[next_index]).completeBaseName());
            }
        }
    } else {
        // Not on plugin - cycle through built-in filters
        const int old_value = static_cast<int>(current_filter);
        const int max_builtin = static_cast<int>(config::Filter::kPlugin);
        int new_value = old_value + 1;

        if (new_value >= max_builtin) {
            // Reached kPlugin - check if plugins are available
            QStringList plugins = GetValidPluginPaths();
            if (!plugins.isEmpty()) {
                // Switch to first plugin
                OPTION(kDispFilter) = config::Filter::kPlugin;
                OPTION(kDispFilterPlugin) = plugins[0];
                msg = tr("Filter: Plugin (%1)").arg(QFileInfo(plugins[0]).completeBaseName());
            } else {
                // No plugins, wrap to None
                OPTION(kDispFilter) = config::Filter::kNone;
                msg = tr("Filter: %1").arg(
                    config::Option::ByID(config::OptionID::kDispFilter)->GetEnumString());
            }
        } else {
            // Normal filter cycling
            OPTION(kDispFilter) = static_cast<config::Filter>(new_value);
            msg = tr("Filter: %1").arg(
                config::Option::ByID(config::OptionID::kDispFilter)->GetEnumString());
        }
    }

    systemScreenMessage(msg);
}

void MainWindow::DoChangeIFB() {
    OPTION(kDispIFB).Next();
    systemScreenMessage(tr("Interframe Blending: %1")
                            .arg(config::Option::ByID(config::OptionID::kDispIFB)->GetEnumString()));
}

void MainWindow::DoSoundConfigure() {
    if (ShowModal(LoadDialog("SoundConfig")) != QDialog::Accepted)
        return;

    // No point in observing these since they can only be set in this dialog.
    gb_effects_config.echo = (float)OPTION(kSoundGBEcho) / 100.0;
    gb_effects_config.stereo = (float)OPTION(kSoundGBStereo) / 100.0;
    soundFiltering = (float)OPTION(kSoundGBAFiltering) / 100.0f;
}

void MainWindow::OnEmulatorDirectories() {
    ShowModal(LoadDialog("DirectoriesConfig"));
}

void MainWindow::OnJoypadConfigure() {
    if (ShowModal(LoadDialog("JoypadConfig")) == QDialog::Accepted) {
        update_shortcut_opts();
    }
}

void MainWindow::OnCustomize() {
    if (ShowModal(LoadDialog("AccelConfig")) == QDialog::Accepted) {
        update_shortcut_opts();
        ResetMenuAccelerators();
    }
}

void MainWindow::OnUpdateEmu() {
    // Online update checks are not available in the Qt port.
    QMessageBox::information(this, tr("Check for updates"),
                             tr("Online update checks are not available in this build.\n"
                                "Please visit https://visualboyadvance-m.org/ for the latest "
                                "release."));
}

void MainWindow::OnFactoryReset() {
    const int ret = QMessageBox::question(
        this, tr("FACTORY RESET"), tr("YOUR CONFIGURATION WILL BE DELETED!\n\nAre you sure?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        vbamApp().config()->clear();
        vbamApp().config()->sync();
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList());
        // Do not let the close handler write the geometry back.
        QFile::remove(vbamApp().GetConfigurationPath() + "/vbam-qt.ini");
        QApplication::exit(0);
    }
}

void MainWindow::OnBugReport() {
    QDesktopServices::openUrl(
        QUrl("https://github.com/visualboyadvance-m/visualboyadvance-m/issues"));
}

void MainWindow::OnFaq() {
    QDesktopServices::openUrl(QUrl("https://github.com/visualboyadvance-m/visualboyadvance-m/"));
}

void MainWindow::OnTranslate() {
    QDesktopServices::openUrl(QUrl("https://explore.transifex.com/bgk/vba-m/"));
}

// was About
void MainWindow::OnAbout() {
    const QString version = QString::fromStdString(kVbamVersion);
    const QString developers =
        "Forgotten, kxu, Pokemonhacker, Spacy51, mudlord, Nach, jbo_85, bgK, "
        "Jonas Quinn, DJRobX, Spacy, Squall Leonhart, Thomas J. Moore, blargg, Costis, "
        "chrono, xKiv, skidau, TheCanadianBacon, rkitover, Mystro256, retro-wertz, denisfa, "
        "orbea, andyvand, mGBA, Orig. VBA team, ... many contributors who send us patches/PRs";
    const QString artists = "Matteo Drera, Jakub Steiner, Jones Lee";

    QString text;
    text += QString("<h2>VisualBoyAdvance-M %1</h2>").arg(version.toHtmlEscaped());
    text += "<p>" + tr("Nintendo Game Boy / Color / Advance emulator.").toHtmlEscaped() + "</p>";
    text += "<p><a href=\"http://visualboyadvance-m.org/\">http://visualboyadvance-m.org/</a></p>";
    text += "<p>" +
            tr("Copyright (C) 1999-2003 Forgotten\nCopyright (C) 2004-2006 VBA development team\n"
               "Copyright (C) 2007-2020 VBA-M development team")
                .toHtmlEscaped()
                .replace("\n", "<br>") +
            "</p>";
    text += "<p><b>" + tr("Developers:") + "</b> " + developers.toHtmlEscaped() + "</p>";
    text += "<p><b>" + tr("Artists:") + "</b> " + artists.toHtmlEscaped() + "</p>";
    text += "<p><small>" +
            tr("This program is free software: you can redistribute it and / or modify\n"
               "it under the terms of the GNU General Public License as published by\n"
               "the Free Software Foundation, either version 2 of the License, or\n"
               "(at your option) any later version.\n"
               "\n"
               "This program is distributed in the hope that it will be useful,\n"
               "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
               "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
               "GNU General Public License for more details.\n"
               "\n"
               "You should have received a copy of the GNU General Public License\n"
               "along with this program. If not, see http://www.gnu.org/licenses .")
                .toHtmlEscaped()
                .replace("\n", "<br>") +
            "</small></p>";

    ModalPause mp;
    QMessageBox::about(this, tr("About VisualBoyAdvance-M"), text);
}

void MainWindow::OnBilinear() {
    GetMenuOptionConfig(cmd::kBilinear, config::OptionID::kDispBilinear);
}

void MainWindow::OnRetainAspect() {
    GetMenuOptionConfig(cmd::kRetainAspect, config::OptionID::kDispStretch);
}

#ifndef NO_LINK
// The Game Boy printer and a link session are mutually exclusive uses of the
// serial port, so the printer is turned off for the duration of a session.
// Remembered here rather than written through to the option, so an enforced
// off does not become the user's saved preference.
static bool printer_enabled_before_link = false;
static bool printer_suspended_by_link = false;

// Reattaches the GB serial handler to whichever of the two owns it now.
static void UpdateGbSerialFunction() {
    gbSerialFunction = coreOptions.gbPrinterEnabled ? gbPrinterSend : gbStartLink;
}
#endif  // NO_LINK

void MainWindow::DoPrinter() {
    GetMenuOptionInt(cmd::kPrinter, &coreOptions.gbPrinterEnabled, 1);
#ifndef NO_LINK
    gbSerialFunction = gbStartLink;
    // An explicit toggle is the user's preference, so it survives the next
    // link session rather than being overwritten when that session ends.
    printer_enabled_before_link = coreOptions.gbPrinterEnabled != 0;
#else
    gbSerialFunction = nullptr;
#endif
    if (coreOptions.gbPrinterEnabled)
        gbSerialFunction = gbPrinterSend;

    update_opts();
}

void MainWindow::OnPrintGather() {
    GetMenuOptionConfig(cmd::kPrintGather, config::OptionID::kGBPrintAutoPage);
}

void MainWindow::OnPrintSnap() {
    GetMenuOptionConfig(cmd::kPrintSnap, config::OptionID::kGBPrintScreenCap);
}

void MainWindow::OnGBASoundInterpolation() {
    GetMenuOptionConfig(cmd::kGBASoundInterpolation, config::OptionID::kSoundGBAInterpolation);
}

void MainWindow::OnGBDeclicking() {
    GetMenuOptionConfig(cmd::kGBDeclicking, config::OptionID::kSoundGBDeclicking);
}

void MainWindow::OnGBEnhanceSound() {
    GetMenuOptionConfig(cmd::kGBEnhanceSound, config::OptionID::kSoundGBEnableEffects);
}

void MainWindow::OnGBSurround() {
    GetMenuOptionConfig(cmd::kGBSurround, config::OptionID::kSoundGBSurround);
}

void MainWindow::OnAGBPrinter() {
    GetMenuOptionConfig(cmd::kAGBPrinter, config::OptionID::kPrefAgbPrint);
}

void MainWindow::DoGBALcdFilter() {
    GetMenuOptionConfig(cmd::kGBALcdFilter, config::OptionID::kGBALCDFilter);
}

void MainWindow::DoGBLcdFilter() {
    GetMenuOptionConfig(cmd::kGBLcdFilter, config::OptionID::kGBLCDFilter);
}

void MainWindow::OnApplyPatches() {
    GetMenuOptionConfig(cmd::kApplyPatches, config::OptionID::kPrefAutoPatch);
}

void MainWindow::OnKeepOnTop() {
    GetMenuOptionConfig(cmd::kKeepOnTop, config::OptionID::kDispKeepOnTop);
}

void MainWindow::OnStatusBar() {
    GetMenuOptionConfig(cmd::kStatusBar, config::OptionID::kGenStatusBar);
}

void MainWindow::OnNoStatusMsg() {
    GetMenuOptionConfig(cmd::kNoStatusMsg, config::OptionID::kPrefDisableStatus);
}

void MainWindow::OnBitDepth() {
    GetMenuOptionConfig(cmd::kBitDepth, config::OptionID::kBitDepth);
}

void MainWindow::OnFrameSkipAuto() {
    GetMenuOptionConfig(cmd::kFrameSkipAuto, config::OptionID::kPrefAutoFrameSkip);
}

void MainWindow::OnFullscreen() {
    GetMenuOptionConfig(cmd::kFullscreen, config::OptionID::kGeomFullScreen);
}

void MainWindow::OnPauseWhenInactive() {
    GetMenuOptionConfig(cmd::kPauseWhenInactive, config::OptionID::kPrefPauseWhenInactive);
}

void MainWindow::OnRtc() {
    GetMenuOptionInt(cmd::kRtc, &coreOptions.rtcEnabled, 1);
    update_opts();
}

void MainWindow::OnSkipIntro() {
    GetMenuOptionConfig(cmd::kSkipIntro, config::OptionID::kPrefSkipBios);
}

void MainWindow::OnBootRomEn() {
    GetMenuOptionConfig(cmd::kBootRomEn, config::OptionID::kPrefUseBiosGBA);
}

void MainWindow::OnBootRomGB() {
    GetMenuOptionConfig(cmd::kBootRomGB, config::OptionID::kPrefUseBiosGB);
    if (OPTION(kPrefUseBiosGB) && OPTION(kGBColorizerHack)) {
        vbam::LogError(tr("Cannot use Game Boy BIOS when Colorizer Hack is enabled."));
        SetMenuOption(cmd::kBootRomGB, 0);
        OPTION(kPrefUseBiosGB) = false;
    }
}

void MainWindow::OnBootRomGBC() {
    GetMenuOptionConfig(cmd::kBootRomGBC, config::OptionID::kPrefUseBiosGBC);
}

void MainWindow::OnVSync() {
    GetMenuOptionConfig(cmd::kVSync, config::OptionID::kPrefVsync);
}

void MainWindow::OnHideMenuBar() {
    GetMenuOptionConfig(cmd::kHideMenuBar, config::OptionID::kUIHideMenuBar);
}

void MainWindow::OnOnScreenController() {
    GetMenuOptionConfig(cmd::kOnScreenController, config::OptionID::kUIShowOnScreenController);
}

void MainWindow::OnSuspendScreenSaver() {
    GetMenuOptionConfig(cmd::kSuspendScreenSaver, config::OptionID::kUISuspendScreenSaver);

    // Apply right away rather than at the next pause/resume.
    if (panel) {
        if (OPTION(kUISuspendScreenSaver))
            panel->SuspendScreenSaver();
        else
            panel->UnsuspendScreenSaver();
    }
}

#ifndef NO_LINK

void MainWindow::EnableNetworkMenu() {
    const bool linked = GetLinkMode() != LINK_DISCONNECTED;

    cmd_enable &= ~(CMDEN_LINK_ANY | CMDEN_LINK_OFF);

    // "Start Link..." attaches either transport: the NetLink dialog in network
    // mode, or a direct IPC attach in local mode. It only stays disabled while
    // the link type is "Nothing".
    if (gopts.gba_link_type != 0)
        cmd_enable |= CMDEN_LINK_ANY;

    if (!linked)
        cmd_enable |= CMDEN_LINK_OFF;

    // Suspend the printer for the session, restoring the user's setting when
    // the session ends.
    if (linked && !printer_suspended_by_link) {
        printer_enabled_before_link = coreOptions.gbPrinterEnabled != 0;
        printer_suspended_by_link = true;
        coreOptions.gbPrinterEnabled = 0;
        SetMenuOption(cmd::kPrinter, 0);
        UpdateGbSerialFunction();
    } else if (!linked && printer_suspended_by_link) {
        printer_suspended_by_link = false;
        coreOptions.gbPrinterEnabled = printer_enabled_before_link ? 1 : 0;
        SetMenuOption(cmd::kPrinter, coreOptions.gbPrinterEnabled ? 1 : 0);
        UpdateGbSerialFunction();
    }

    // The one menu entry both starts and stops, so it says which it will do.
    if (GetAction(cmd::kLanLink)) {
        base_labels_[cmd::kLanLink] = linked ? tr("Stop &Link") : tr("Start &Link...");
        ResetActionAccelerator(cmd::kLanLink);
    }

    enable_menus();
}

#endif  // NO_LINK

void MainWindow::OnExternalTranslations() {
    GetMenuOptionConfig(cmd::kExternalTranslations, config::OptionID::kExternalTranslations);
}

// Applies a language picked from the Languages menu: persists the choice and
// reloads the message catalogs. Widgets already created keep their current
// text until they are rebuilt; the user is told to restart for a full switch.
namespace {

void SetUiLanguage(MainWindow* frame, int lang) {
    OPTION(kLocale) = lang;
    update_opts();
    vbamApp().LoadTranslations();
    for (int i = 0; i < VbamApp::LanguageCount(); i++) {
        frame->SetMenuOption(cmd::kLanguage0 + i, i == lang);
    }
    QMessageBox::information(
        frame, QCoreApplication::translate("vbam", "Language"),
        QCoreApplication::translate(
            "vbam", "The language will be fully applied the next time the emulator is started."));
}

}  // namespace

// The Language<N> handlers are keyed by index into VbamApp's kLanguages table,
// which must agree with the menu items in menu-def.cpp, item for item.
#define LANGUAGE_HANDLER(n) \
    void MainWindow::OnLanguage##n() { SetUiLanguage(this, n); }
LANGUAGE_HANDLER(0)
LANGUAGE_HANDLER(1)
LANGUAGE_HANDLER(2)
LANGUAGE_HANDLER(3)
LANGUAGE_HANDLER(4)
LANGUAGE_HANDLER(5)
LANGUAGE_HANDLER(6)
LANGUAGE_HANDLER(7)
LANGUAGE_HANDLER(8)
LANGUAGE_HANDLER(9)
LANGUAGE_HANDLER(10)
LANGUAGE_HANDLER(11)
LANGUAGE_HANDLER(12)
LANGUAGE_HANDLER(13)
LANGUAGE_HANDLER(14)
#undef LANGUAGE_HANDLER

// Dummy for disabling system key bindings
void MainWindow::DoNoop() {
}

// -----------------------------------------------------------------------------
// Masked command wrappers (the wx EVT_HANDLER_MASK expansion): the command
// runs only if one of its enable flags is set in cmd_enable.

void MainWindow::OnResetLoadingDotCodeFile() {
    if (!(cmd_enable & (CMDEN_GBA)))
        return;
    DoResetLoadingDotCodeFile();
}

void MainWindow::OnSetLoadingDotCodeFile() {
    if (!(cmd_enable & (CMDEN_GBA)))
        return;
    DoSetLoadingDotCodeFile();
}

void MainWindow::OnResetSavingDotCodeFile() {
    if (!(cmd_enable & (CMDEN_GBA)))
        return;
    DoResetSavingDotCodeFile();
}

void MainWindow::OnSetSavingDotCodeFile() {
    if (!(cmd_enable & (CMDEN_GBA)))
        return;
    DoSetSavingDotCodeFile();
}

void MainWindow::OnScreenCapture() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoScreenCapture();
}

void MainWindow::OnRecordSoundStartRecording() {
    if (!(cmd_enable & (CMDEN_NSREC)))
        return;
    DoRecordSoundStartRecording();
}

void MainWindow::OnRecordSoundStopRecording() {
    if (!(cmd_enable & (CMDEN_SREC)))
        return;
    DoRecordSoundStopRecording();
}

void MainWindow::OnRecordAVIStartRecording() {
    if (!(cmd_enable & (CMDEN_NVREC)))
        return;
    DoRecordAVIStartRecording();
}

void MainWindow::OnRecordAVIStopRecording() {
    if (!(cmd_enable & (CMDEN_VREC)))
        return;
    DoRecordAVIStopRecording();
}

void MainWindow::OnRecordMovieStartRecording() {
    if (!(cmd_enable & (CMDEN_NGREC)))
        return;
    DoRecordMovieStartRecording();
}

void MainWindow::OnRecordMovieStopRecording() {
    if (!(cmd_enable & (CMDEN_GREC)))
        return;
    DoRecordMovieStopRecording();
}

void MainWindow::OnPlayMovieStartPlaying() {
    if (!(cmd_enable & (CMDEN_NGREC | CMDEN_NGPLAY)))
        return;
    DoPlayMovieStartPlaying();
}

void MainWindow::OnPlayMovieStopPlaying() {
    if (!(cmd_enable & (CMDEN_GPLAY)))
        return;
    DoPlayMovieStopPlaying();
}

void MainWindow::OnClose() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoClose();
}

void MainWindow::OnEmulatorSpeedupToggle() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoEmulatorSpeedupToggle();
}

void MainWindow::OnReset() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoReset();
}

void MainWindow::OnLoadGameRecent() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGameRecent();
}

void MainWindow::OnLoadGame01() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame01();
}

void MainWindow::OnLoadGame02() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame02();
}

void MainWindow::OnLoadGame03() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame03();
}

void MainWindow::OnLoadGame04() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame04();
}

void MainWindow::OnLoadGame05() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame05();
}

void MainWindow::OnLoadGame06() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame06();
}

void MainWindow::OnLoadGame07() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame07();
}

void MainWindow::OnLoadGame08() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame08();
}

void MainWindow::OnLoadGame09() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame09();
}

void MainWindow::OnLoadGame10() {
    if (!(cmd_enable & (CMDEN_SAVST)))
        return;
    DoLoadGame10();
}

void MainWindow::OnLoad() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoLoad();
}

void MainWindow::OnSaveGameOldest() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGameOldest();
}

void MainWindow::OnSaveGame01() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame01();
}

void MainWindow::OnSaveGame02() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame02();
}

void MainWindow::OnSaveGame03() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame03();
}

void MainWindow::OnSaveGame04() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame04();
}

void MainWindow::OnSaveGame05() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame05();
}

void MainWindow::OnSaveGame06() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame06();
}

void MainWindow::OnSaveGame07() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame07();
}

void MainWindow::OnSaveGame08() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame08();
}

void MainWindow::OnSaveGame09() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame09();
}

void MainWindow::OnSaveGame10() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGame10();
}

void MainWindow::OnSave() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSave();
}

void MainWindow::OnLoadGameSlot() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoLoadGameSlot();
}

void MainWindow::OnSaveGameSlot() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSaveGameSlot();
}

void MainWindow::OnIncrGameSlot() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoIncrGameSlot();
}

void MainWindow::OnDecrGameSlot() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoDecrGameSlot();
}

void MainWindow::OnIncrGameSlotSave() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoIncrGameSlotSave();
}

void MainWindow::OnRewind() {
    if (!(cmd_enable & (CMDEN_REWIND)))
        return;
    DoRewind();
}

void MainWindow::OnVideoLayersBG0() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersBG0();
}

void MainWindow::OnVideoLayersBG1() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersBG1();
}

void MainWindow::OnVideoLayersBG2() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersBG2();
}

void MainWindow::OnVideoLayersBG3() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersBG3();
}

void MainWindow::OnVideoLayersOBJ() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersOBJ();
}

void MainWindow::OnVideoLayersWIN0() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersWIN0();
}

void MainWindow::OnVideoLayersWIN1() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersWIN1();
}

void MainWindow::OnVideoLayersOBJWIN() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersOBJWIN();
}

void MainWindow::OnVideoLayersReset() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoVideoLayersReset();
}

void MainWindow::OnSoundChannel1() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSoundChannel1();
}

void MainWindow::OnSoundChannel2() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSoundChannel2();
}

void MainWindow::OnSoundChannel3() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSoundChannel3();
}

void MainWindow::OnSoundChannel4() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoSoundChannel4();
}

void MainWindow::OnDirectSoundA() {
    if (!(cmd_enable & (CMDEN_GBA)))
        return;
    DoDirectSoundA();
}

void MainWindow::OnDirectSoundB() {
    if (!(cmd_enable & (CMDEN_GBA)))
        return;
    DoDirectSoundB();
}

void MainWindow::OnNextFrame() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoNextFrame();
}

void MainWindow::OnDisassemble() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoDisassemble();
}

void MainWindow::OnIOViewer() {
    if (!(cmd_enable & (CMDEN_GBA)))
        return;
    DoIOViewer();
}

void MainWindow::OnMapViewer() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoMapViewer();
}

void MainWindow::OnMemoryViewer() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoMemoryViewer();
}

void MainWindow::OnOAMViewer() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoOAMViewer();
}

void MainWindow::OnPaletteViewer() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoPaletteViewer();
}

void MainWindow::OnTileViewer() {
    if (!(cmd_enable & (CMDEN_GB | CMDEN_GBA)))
        return;
    DoTileViewer();
}

void MainWindow::OnDebugGDBBreak() {
    if (!(cmd_enable & (CMDEN_NGDB_GBA | CMDEN_GDB)))
        return;
    DoDebugGDBBreak();
}

void MainWindow::OnDebugGDBDisconnect() {
    if (!(cmd_enable & (CMDEN_GDB)))
        return;
    DoDebugGDBDisconnect();
}

void MainWindow::OnDisplayConfigure() {
    if (!(cmd_enable & (CMDEN_NREC_ANY)))
        return;
    DoDisplayConfigure();
}

void MainWindow::OnChangeFilter() {
    if (!(cmd_enable & (CMDEN_NREC_ANY)))
        return;
    DoChangeFilter();
}

void MainWindow::OnChangeIFB() {
    if (!(cmd_enable & (CMDEN_NREC_ANY)))
        return;
    DoChangeIFB();
}

void MainWindow::OnSoundConfigure() {
    if (!(cmd_enable & (CMDEN_NREC_ANY)))
        return;
    DoSoundConfigure();
}

void MainWindow::OnPrinter() {
    if (!(cmd_enable & (CMDEN_LINK_OFF)))
        return;
    DoPrinter();
}

void MainWindow::OnGBALcdFilter() {
    if (!(cmd_enable & (CMDEN_GBA)))
        return;
    DoGBALcdFilter();
}

void MainWindow::OnGBLcdFilter() {
    if (!(cmd_enable & (CMDEN_GB)))
        return;
    DoGBLcdFilter();
}

void MainWindow::OnNoop() {
    if (!(cmd_enable & (CMDEN_NEVER)))
        return;
    DoNoop();
}

// -----------------------------------------------------------------------------
// Command dispatch.

bool MainWindow::ExecuteCommand(int cmd_id) {
    return DispatchCommand(cmd_id);
}

bool MainWindow::DispatchCommand(int cmd_id) {
    // Commands with an enable mask are refused while disabled, whatever the
    // source (menu, shortcut, command line).
    for (const cmditem& cmd_item : cmdtab) {
        if (cmd_item.cmd_id == cmd_id) {
            if (cmd_item.mask_flags && !(cmd_enable & cmd_item.mask_flags))
                return false;
            break;
        }
    }

    switch (static_cast<cmd::Id>(cmd_id)) {
        case cmd::kOpen:
            OnOpen();
            return true;
        case cmd::kOpenGB:
            OnOpenGB();
            return true;
        case cmd::kOpenGBC:
            OnOpenGBC();
            return true;
        case cmd::kRecentReset:
            OnRecentReset();
            return true;
        case cmd::kRecentFreeze:
            OnRecentFreeze();
            return true;
        case cmd::kFile1:
            OnFile1();
            return true;
        case cmd::kFile2:
            OnFile2();
            return true;
        case cmd::kFile3:
            OnFile3();
            return true;
        case cmd::kFile4:
            OnFile4();
            return true;
        case cmd::kFile5:
            OnFile5();
            return true;
        case cmd::kFile6:
            OnFile6();
            return true;
        case cmd::kFile7:
            OnFile7();
            return true;
        case cmd::kFile8:
            OnFile8();
            return true;
        case cmd::kFile9:
            OnFile9();
            return true;
        case cmd::kFile10:
            OnFile10();
            return true;
        case cmd::kRomInformation:
            OnRomInformation();
            return true;
        case cmd::kResetLoadingDotCodeFile:
            OnResetLoadingDotCodeFile();
            return true;
        case cmd::kSetLoadingDotCodeFile:
            OnSetLoadingDotCodeFile();
            return true;
        case cmd::kResetSavingDotCodeFile:
            OnResetSavingDotCodeFile();
            return true;
        case cmd::kSetSavingDotCodeFile:
            OnSetSavingDotCodeFile();
            return true;
        case cmd::kImportBatteryFile:
            OnImportBatteryFile();
            return true;
        case cmd::kImportGamesharkCodeFile:
            OnImportGamesharkCodeFile();
            return true;
        case cmd::kImportGamesharkActionReplaySnapshot:
            OnImportGamesharkActionReplaySnapshot();
            return true;
        case cmd::kExportBatteryFile:
            OnExportBatteryFile();
            return true;
        case cmd::kExportGamesharkSnapshot:
            OnExportGamesharkSnapshot();
            return true;
        case cmd::kScreenCapture:
            OnScreenCapture();
            return true;
        case cmd::kRecordSoundStartRecording:
            OnRecordSoundStartRecording();
            return true;
        case cmd::kRecordSoundStopRecording:
            OnRecordSoundStopRecording();
            return true;
        case cmd::kRecordAVIStartRecording:
            OnRecordAVIStartRecording();
            return true;
        case cmd::kRecordAVIStopRecording:
            OnRecordAVIStopRecording();
            return true;
        case cmd::kRecordMovieStartRecording:
            OnRecordMovieStartRecording();
            return true;
        case cmd::kRecordMovieStopRecording:
            OnRecordMovieStopRecording();
            return true;
        case cmd::kPlayMovieStartPlaying:
            OnPlayMovieStartPlaying();
            return true;
        case cmd::kPlayMovieStopPlaying:
            OnPlayMovieStopPlaying();
            return true;
        case cmd::kClose:
            OnClose();
            return true;
        case cmd::kExit:
            OnExit();
            return true;
        case cmd::kPause:
            OnPause();
            return true;
        case cmd::kEmulatorSpeedupToggle:
            OnEmulatorSpeedupToggle();
            return true;
        case cmd::kReset:
            OnReset();
            return true;
        case cmd::kToggleFullscreen:
            OnToggleFullscreen();
            return true;
        case cmd::kJoypadAutofireA:
            OnJoypadAutofireA();
            return true;
        case cmd::kJoypadAutofireB:
            OnJoypadAutofireB();
            return true;
        case cmd::kJoypadAutofireL:
            OnJoypadAutofireL();
            return true;
        case cmd::kJoypadAutofireR:
            OnJoypadAutofireR();
            return true;
        case cmd::kJoypadAutoholdUp:
            OnJoypadAutoholdUp();
            return true;
        case cmd::kJoypadAutoholdDown:
            OnJoypadAutoholdDown();
            return true;
        case cmd::kJoypadAutoholdLeft:
            OnJoypadAutoholdLeft();
            return true;
        case cmd::kJoypadAutoholdRight:
            OnJoypadAutoholdRight();
            return true;
        case cmd::kJoypadAutoholdA:
            OnJoypadAutoholdA();
            return true;
        case cmd::kJoypadAutoholdB:
            OnJoypadAutoholdB();
            return true;
        case cmd::kJoypadAutoholdL:
            OnJoypadAutoholdL();
            return true;
        case cmd::kJoypadAutoholdR:
            OnJoypadAutoholdR();
            return true;
        case cmd::kJoypadAutoholdSelect:
            OnJoypadAutoholdSelect();
            return true;
        case cmd::kJoypadAutoholdStart:
            OnJoypadAutoholdStart();
            return true;
        case cmd::kAllowKeyboardBackgroundInput:
            OnAllowKeyboardBackgroundInput();
            return true;
        case cmd::kAllowJoystickBackgroundInput:
            OnAllowJoystickBackgroundInput();
            return true;
        case cmd::kLoadGameRecent:
            OnLoadGameRecent();
            return true;
        case cmd::kLoadGameAutoLoad:
            OnLoadGameAutoLoad();
            return true;
        case cmd::kLoadGame01:
            OnLoadGame01();
            return true;
        case cmd::kLoadGame02:
            OnLoadGame02();
            return true;
        case cmd::kLoadGame03:
            OnLoadGame03();
            return true;
        case cmd::kLoadGame04:
            OnLoadGame04();
            return true;
        case cmd::kLoadGame05:
            OnLoadGame05();
            return true;
        case cmd::kLoadGame06:
            OnLoadGame06();
            return true;
        case cmd::kLoadGame07:
            OnLoadGame07();
            return true;
        case cmd::kLoadGame08:
            OnLoadGame08();
            return true;
        case cmd::kLoadGame09:
            OnLoadGame09();
            return true;
        case cmd::kLoadGame10:
            OnLoadGame10();
            return true;
        case cmd::kLoad:
            OnLoad();
            return true;
        case cmd::kKeepSaves:
            OnKeepSaves();
            return true;
        case cmd::kKeepCheats:
            OnKeepCheats();
            return true;
        case cmd::kSaveGameOldest:
            OnSaveGameOldest();
            return true;
        case cmd::kSaveGame01:
            OnSaveGame01();
            return true;
        case cmd::kSaveGame02:
            OnSaveGame02();
            return true;
        case cmd::kSaveGame03:
            OnSaveGame03();
            return true;
        case cmd::kSaveGame04:
            OnSaveGame04();
            return true;
        case cmd::kSaveGame05:
            OnSaveGame05();
            return true;
        case cmd::kSaveGame06:
            OnSaveGame06();
            return true;
        case cmd::kSaveGame07:
            OnSaveGame07();
            return true;
        case cmd::kSaveGame08:
            OnSaveGame08();
            return true;
        case cmd::kSaveGame09:
            OnSaveGame09();
            return true;
        case cmd::kSaveGame10:
            OnSaveGame10();
            return true;
        case cmd::kSave:
            OnSave();
            return true;
        case cmd::kLoadGameSlot:
            OnLoadGameSlot();
            return true;
        case cmd::kSaveGameSlot:
            OnSaveGameSlot();
            return true;
        case cmd::kIncrGameSlot:
            OnIncrGameSlot();
            return true;
        case cmd::kDecrGameSlot:
            OnDecrGameSlot();
            return true;
        case cmd::kIncrGameSlotSave:
            OnIncrGameSlotSave();
            return true;
        case cmd::kRewind:
            OnRewind();
            return true;
        case cmd::kCheatsList:
            OnCheatsList();
            return true;
        case cmd::kCheatsSearch:
            OnCheatsSearch();
            return true;
        case cmd::kCheatsAutoSaveLoad:
            OnCheatsAutoSaveLoad();
            return true;
        case cmd::kCheatsEnable:
            OnCheatsEnable();
            return true;
        case cmd::kColorizerHack:
            OnColorizerHack();
            return true;
        case cmd::kVideoLayersBG0:
            OnVideoLayersBG0();
            return true;
        case cmd::kVideoLayersBG1:
            OnVideoLayersBG1();
            return true;
        case cmd::kVideoLayersBG2:
            OnVideoLayersBG2();
            return true;
        case cmd::kVideoLayersBG3:
            OnVideoLayersBG3();
            return true;
        case cmd::kVideoLayersOBJ:
            OnVideoLayersOBJ();
            return true;
        case cmd::kVideoLayersWIN0:
            OnVideoLayersWIN0();
            return true;
        case cmd::kVideoLayersWIN1:
            OnVideoLayersWIN1();
            return true;
        case cmd::kVideoLayersOBJWIN:
            OnVideoLayersOBJWIN();
            return true;
        case cmd::kVideoLayersReset:
            OnVideoLayersReset();
            return true;
        case cmd::kSoundChannel1:
            OnSoundChannel1();
            return true;
        case cmd::kSoundChannel2:
            OnSoundChannel2();
            return true;
        case cmd::kSoundChannel3:
            OnSoundChannel3();
            return true;
        case cmd::kSoundChannel4:
            OnSoundChannel4();
            return true;
        case cmd::kDirectSoundA:
            OnDirectSoundA();
            return true;
        case cmd::kDirectSoundB:
            OnDirectSoundB();
            return true;
        case cmd::kToggleSound:
            OnToggleSound();
            return true;
        case cmd::kIncreaseVolume:
            OnIncreaseVolume();
            return true;
        case cmd::kDecreaseVolume:
            OnDecreaseVolume();
            return true;
        case cmd::kNextFrame:
            OnNextFrame();
            return true;
        case cmd::kDisassemble:
            OnDisassemble();
            return true;
        case cmd::kLogging:
            OnLogging();
            return true;
        case cmd::kIOViewer:
            OnIOViewer();
            return true;
        case cmd::kMapViewer:
            OnMapViewer();
            return true;
        case cmd::kMemoryViewer:
            OnMemoryViewer();
            return true;
        case cmd::kOAMViewer:
            OnOAMViewer();
            return true;
        case cmd::kPaletteViewer:
            OnPaletteViewer();
            return true;
        case cmd::kTileViewer:
            OnTileViewer();
            return true;
#if defined(VBAM_ENABLE_LUA)
        case cmd::kLuaRunScript:
            OnLuaRunScript();
            return true;
#endif
#if defined(VBAM_ENABLE_LUA)
        case cmd::kLuaStopScript:
            OnLuaStopScript();
            return true;
#endif
#if defined(VBAM_ENABLE_LUA)
        case cmd::kLuaConsole:
            OnLuaConsole();
            return true;
#endif
#if defined(VBAM_ENABLE_LUA)
        case cmd::kLuaEditor:
            OnLuaEditor();
            return true;
#endif
        case cmd::kDebugGDBPort:
            OnDebugGDBPort();
            return true;
        case cmd::kDebugGDBBreakOnLoad:
            OnDebugGDBBreakOnLoad();
            return true;
        case cmd::kDebugGDBBreak:
            OnDebugGDBBreak();
            return true;
        case cmd::kDebugGDBDisconnect:
            OnDebugGDBDisconnect();
            return true;
        case cmd::kGeneralConfigure:
            OnGeneralConfigure();
            return true;
        case cmd::kSpeedupConfigure:
            OnSpeedupConfigure();
            return true;
        case cmd::kGameBoyConfigure:
            OnGameBoyConfigure();
            return true;
        case cmd::kSetSize1x:
            OnSetSize1x();
            return true;
        case cmd::kSetSize2x:
            OnSetSize2x();
            return true;
        case cmd::kSetSize3x:
            OnSetSize3x();
            return true;
        case cmd::kSetSize4x:
            OnSetSize4x();
            return true;
        case cmd::kSetSize5x:
            OnSetSize5x();
            return true;
        case cmd::kSetSize6x:
            OnSetSize6x();
            return true;
        case cmd::kGameBoyAdvanceConfigure:
            OnGameBoyAdvanceConfigure();
            return true;
        case cmd::kDisplayConfigure:
            OnDisplayConfigure();
            return true;
        case cmd::kChangeFilter:
            OnChangeFilter();
            return true;
        case cmd::kChangeIFB:
            OnChangeIFB();
            return true;
        case cmd::kSoundConfigure:
            OnSoundConfigure();
            return true;
        case cmd::kEmulatorDirectories:
            OnEmulatorDirectories();
            return true;
        case cmd::kJoypadConfigure:
            OnJoypadConfigure();
            return true;
        case cmd::kCustomize:
            OnCustomize();
            return true;
        case cmd::kUpdateEmu:
            OnUpdateEmu();
            return true;
        case cmd::kFactoryReset:
            OnFactoryReset();
            return true;
        case cmd::kBugReport:
            OnBugReport();
            return true;
        case cmd::kFaq:
            OnFaq();
            return true;
        case cmd::kTranslate:
            OnTranslate();
            return true;
        case cmd::kAbout:
            OnAbout();
            return true;
        case cmd::kBilinear:
            OnBilinear();
            return true;
        case cmd::kRetainAspect:
            OnRetainAspect();
            return true;
        case cmd::kPrinter:
            OnPrinter();
            return true;
        case cmd::kPrintGather:
            OnPrintGather();
            return true;
        case cmd::kPrintSnap:
            OnPrintSnap();
            return true;
        case cmd::kGBASoundInterpolation:
            OnGBASoundInterpolation();
            return true;
        case cmd::kGBDeclicking:
            OnGBDeclicking();
            return true;
        case cmd::kGBEnhanceSound:
            OnGBEnhanceSound();
            return true;
        case cmd::kGBSurround:
            OnGBSurround();
            return true;
        case cmd::kAGBPrinter:
            OnAGBPrinter();
            return true;
        case cmd::kGBALcdFilter:
            OnGBALcdFilter();
            return true;
        case cmd::kGBLcdFilter:
            OnGBLcdFilter();
            return true;
        case cmd::kApplyPatches:
            OnApplyPatches();
            return true;
        case cmd::kKeepOnTop:
            OnKeepOnTop();
            return true;
        case cmd::kStatusBar:
            OnStatusBar();
            return true;
        case cmd::kNoStatusMsg:
            OnNoStatusMsg();
            return true;
        case cmd::kBitDepth:
            OnBitDepth();
            return true;
        case cmd::kFrameSkipAuto:
            OnFrameSkipAuto();
            return true;
        case cmd::kFullscreen:
            OnFullscreen();
            return true;
        case cmd::kPauseWhenInactive:
            OnPauseWhenInactive();
            return true;
        case cmd::kRtc:
            OnRtc();
            return true;
        case cmd::kSkipIntro:
            OnSkipIntro();
            return true;
        case cmd::kBootRomEn:
            OnBootRomEn();
            return true;
        case cmd::kBootRomGB:
            OnBootRomGB();
            return true;
        case cmd::kBootRomGBC:
            OnBootRomGBC();
            return true;
        case cmd::kVSync:
            OnVSync();
            return true;
        case cmd::kHideMenuBar:
            OnHideMenuBar();
            return true;
        case cmd::kOnScreenController:
            OnOnScreenController();
            return true;
        case cmd::kSuspendScreenSaver:
            OnSuspendScreenSaver();
            return true;
        case cmd::kLanLink:
            OnLanLink();
            return true;
        case cmd::kLinkType0Nothing:
            OnLinkType0Nothing();
            return true;
        case cmd::kLinkType1Cable:
            OnLinkType1Cable();
            return true;
        case cmd::kLinkType2Wireless:
            OnLinkType2Wireless();
            return true;
        case cmd::kLinkType3GameCube:
            OnLinkType3GameCube();
            return true;
        case cmd::kLinkType4Gameboy:
            OnLinkType4Gameboy();
            return true;
        case cmd::kLinkAuto:
            OnLinkAuto();
            return true;
        case cmd::kSpeedOn:
            OnSpeedOn();
            return true;
        case cmd::kLinkProto:
            OnLinkProto();
            return true;
        case cmd::kLinkConfigure:
            OnLinkConfigure();
            return true;
        case cmd::kExternalTranslations:
            OnExternalTranslations();
            return true;
        case cmd::kLanguage0:
            OnLanguage0();
            return true;
        case cmd::kLanguage1:
            OnLanguage1();
            return true;
        case cmd::kLanguage2:
            OnLanguage2();
            return true;
        case cmd::kLanguage3:
            OnLanguage3();
            return true;
        case cmd::kLanguage4:
            OnLanguage4();
            return true;
        case cmd::kLanguage5:
            OnLanguage5();
            return true;
        case cmd::kLanguage6:
            OnLanguage6();
            return true;
        case cmd::kLanguage7:
            OnLanguage7();
            return true;
        case cmd::kLanguage8:
            OnLanguage8();
            return true;
        case cmd::kLanguage9:
            OnLanguage9();
            return true;
        case cmd::kLanguage10:
            OnLanguage10();
            return true;
        case cmd::kLanguage11:
            OnLanguage11();
            return true;
        case cmd::kLanguage12:
            OnLanguage12();
            return true;
        case cmd::kLanguage13:
            OnLanguage13();
            return true;
        case cmd::kLanguage14:
            OnLanguage14();
            return true;
        case cmd::kNoop:
            OnNoop();
            return true;
        case cmd::kInvalid:
        case cmd::kLast:
            break;
    }
    return false;
}

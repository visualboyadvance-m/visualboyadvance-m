// MainWindow command handlers for cheats, battery / snapshot import & export,
// ROM information and GBA link. Ported from src/wx/cmdevents.cpp.

#include <cstring>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QtEndian>

#include "core/base/system.h"
#include "core/gb/gb.h"
#include "core/gb/gbCheats.h"
#include "core/gba/gba.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaGlobals.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/dialogs/cheat-list.h"
#include "qt/dialogs/cheat-search.h"
#include "qt/dialogs/code-select.h"
#include "qt/dialogs/export-sps.h"
#include "qt/dialogs/gb-rom-info.h"
#include "qt/dialogs/gba-rom-info.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"
#include "qt/opts.h"

#ifndef NO_LINK
#include "core/gba/gbaLink.h"
#include "qt/dialogs/link-config.h"
#include "qt/dialogs/net-link.h"
#endif

namespace {

// Directories remembered across invocations of the file dialogs.
QString batimp_path;
QString gss_path;

// Toggles `mask` in `*globalVar` following a checkable menu item, keeping the
// menu value in sync. Same as the wx port's toggleBitVar.
void toggleBitVar(bool* menuValue, int* globalVar, int mask) {
    bool isEnabled = ((*globalVar) & (mask)) != (mask);
    if (*menuValue == isEnabled)
        *globalVar = ((*globalVar) & ~(mask)) | (!isEnabled ? (mask) : 0);
    else
        *globalVar = ((*globalVar) & ~(mask)) | (*menuValue ? (mask) : 0);
    *menuValue = ((*globalVar) & (mask)) != (mask);
}

// Masked handler helper: On<Name>() checks cmd_enable, Do<Name>() does the work.
#define MASKED_HANDLER(Name, mask)              \
    void MainWindow::On##Name() {               \
        if (!(cmd_enable & (mask)))             \
            return;                             \
        Do##Name();                             \
    }                                           \
    void MainWindow::Do##Name()

}  // namespace

//// File menu

MASKED_HANDLER(RomInformation, CMDEN_GB | CMDEN_GBA) {
    switch (panel->game_type()) {
        case IMAGE_GB:
            ShowModal(LoadDialog("GBROMInfo"));
            break;
        case IMAGE_GBA:
            IdentifyRom();
            ShowModal(LoadDialog("GBAROMInfo"));
            break;
        default:
            break;
    }
}

MASKED_HANDLER(ImportBatteryFile, CMDEN_GB | CMDEN_GBA) {
    if (batimp_path.isEmpty())
        batimp_path = panel->bat_dir();

    StartModal();
    const QString fn = QFileDialog::getOpenFileName(
        this, tr("Select battery file"), batimp_path,
        tr("Battery file (*.sav);;Flash save (*.dat)"));
    StopModal();

    if (fn.isEmpty())
        return;
    batimp_path = QFileInfo(fn).absolutePath();

    const int ret = QMessageBox::warning(
        this, tr("Confirm import"),
        tr("Importing a battery file will overwrite the current .sav file and reset the "
           "current running game. Do you want to continue?"),
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        QString msg;

        if (panel->emusys->emuReadBattery(vbam::ToPath(fn).c_str())) {
            panel->emusys->emuReset();
            msg = tr("Loaded battery %1").arg(fn);
        } else {
            msg = tr("Error loading battery %1").arg(fn);
        }

        systemScreenMessage(msg);
    }
}

MASKED_HANDLER(ImportGamesharkCodeFile, CMDEN_GB | CMDEN_GBA) {
    static QString path;

    StartModal();
    const QString fn = QFileDialog::getOpenFileName(
        this, tr("Select code file"), path,
        panel->game_type() == IMAGE_GBA ? tr("Game Shark Code File (*.spc *.xpc)")
                                        : tr("Game Shark Code File (*.gcf)"));
    StopModal();

    if (fn.isEmpty())
        return;
    path = QFileInfo(fn).absolutePath();

    const int ret = QMessageBox::warning(
        this, tr("Confirm import"),
        tr("Importing a code file will replace any loaded cheats. Do you want to continue?"),
        QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    QString msg;
    bool res;

    if (panel->game_type() == IMAGE_GB)
        // FIXME: this routine will not work on big-endian systems if the
        // underlying file format is little-endian (fix in gb/gbCheats.cpp)
        res = gbCheatReadGSCodeFile(vbam::ToPath(fn).c_str());
    else {
        // need to select game first
        QFile f(fn);

        if (!f.open(QIODevice::ReadOnly)) {
            vbam::LogError(tr("Cannot open file %1").arg(fn));
            return;
        }

        // The file format is little-endian.
        auto read_u32 = [&f](uint32_t* out) {
            char buf[4];
            if (f.read(buf, 4) != 4)
                return false;
            *out = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(buf));
            return true;
        };

        uint32_t len;
        char buf[14];

        if (!read_u32(&len) || len != 14 || f.read(buf, 14) != 14 ||
            std::memcmp(buf, "SharkPortCODES", 14)) {
            vbam::LogError(tr("Unsupported code file %1").arg(fn));
            return;
        }

        f.seek(0x1e);

        if (!read_u32(&len))
            len = 0;

        uint32_t game = 0;

        if (len > 1) {
            QStringList games;

            while (len-- > 0) {
                uint32_t slen;

                if (!read_u32(&slen) || slen > 1024) // arbitrary upper bound
                    break;

                char buf2[1024];

                if (f.read(buf2, slen) != static_cast<qint64>(slen))
                    break;

                games.append(QString::fromLatin1(buf2, slen));
                uint32_t ncodes;

                if (!read_u32(&ncodes))
                    break;

                for (; ncodes > 0; ncodes--) {
                    if (!read_u32(&slen))
                        break;

                    f.seek(f.pos() + slen);

                    if (!read_u32(&slen))
                        break;

                    f.seek(f.pos() + slen + 4);

                    if (!read_u32(&slen))
                        break;

                    f.seek(f.pos() + slen * 12);
                }
            }

            auto* seldlg = static_cast<dialogs::CodeSelect*>(LoadDialog("CodeSelect"));
            seldlg->SetCodes(games);

            if (ShowModal(seldlg) != QDialog::Accepted)
                return;

            const int sel = seldlg->selection();
            game = sel < 0 ? 0 : static_cast<uint32_t>(sel);
        }

        f.close();

        const bool v3 = fn.endsWith(".xpc", Qt::CaseInsensitive);
        // FIXME: this routine will not work on big-endian systems if the
        // underlying file format is little-endian (fix in gba/Cheats.cpp)
        res = cheatsImportGSACodeFile(vbam::ToPath(fn).c_str(), game, v3);
    }

    if (res)
        msg = tr("Loaded code file %1").arg(fn);
    else
        msg = tr("Error loading code file %1").arg(fn);

    systemScreenMessage(msg);
}

MASKED_HANDLER(ImportGamesharkActionReplaySnapshot, CMDEN_GB | CMDEN_GBA) {
    StartModal();
    const QString fn = QFileDialog::getOpenFileName(
        this, tr("Select snapshot file"), gss_path,
        panel->game_type() == IMAGE_GBA
            ? tr("Game Shark & PAC Snapshots (*.sps *.xps);;Game Shark SP Snapshots (*.gsv)")
            : tr("Game Boy Snapshot (*.gbs)"));
    StopModal();

    if (fn.isEmpty())
        return;
    gss_path = QFileInfo(fn).absolutePath();

    const int ret = QMessageBox::warning(
        this, tr("Confirm import"),
        tr("Importing a snapshot file will erase any saved games (permanently after the "
           "next write). Do you want to continue?"),
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        QString msg;
        bool res;

        if (panel->game_type() == IMAGE_GB)
            res = gbReadGSASnapshot(vbam::ToPath(fn).c_str());
        else {
            const bool gsv = fn.endsWith(".gsv", Qt::CaseInsensitive);

            if (gsv)
                // FIXME: this will fail on big-endian machines if file format
                // is little-endian; fix in GBA.cpp
                res = CPUReadGSASPSnapshot(vbam::ToPath(fn).c_str());
            else
                res = CPUReadGSASnapshot(vbam::ToPath(fn).c_str());
        }

        if (res)
            msg = tr("Loaded snapshot file %1").arg(fn);
        else
            msg = tr("Error loading snapshot file %1").arg(fn);

        systemScreenMessage(msg);
    }
}

MASKED_HANDLER(ExportBatteryFile, CMDEN_GB | CMDEN_GBA) {
    if (batimp_path.isEmpty())
        batimp_path = panel->bat_dir();

    StartModal();
    const QString fn = QFileDialog::getSaveFileName(
        this, tr("Select battery file"), batimp_path,
        tr("Battery file (*.sav);;Flash save (*.dat)"));
    StopModal();

    if (fn.isEmpty())
        return;
    batimp_path = QFileInfo(fn).absolutePath();

    QString msg;

    if (panel->emusys->emuWriteBattery(vbam::ToPath(fn).c_str()))
        msg = tr("Wrote battery %1").arg(fn);
    else
        msg = tr("Error writing battery %1").arg(fn);

    systemScreenMessage(msg);
}

MASKED_HANDLER(ExportGamesharkSnapshot, CMDEN_GBA) {
    if (eepromInUse) {
        vbam::LogError(tr("EEPROM saves cannot be exported"));
        return;
    }

    QString def_name = panel->game_name();
    def_name.append(".sps");

    StartModal();
    const QString fn = QFileDialog::getSaveFileName(
        this, tr("Select snapshot file"), QDir(gss_path).filePath(def_name),
        tr("Game Shark Snapshot (*.sps)"));
    StopModal();

    if (fn.isEmpty())
        return;
    gss_path = QFileInfo(fn).absolutePath();

    auto* infodlg = static_cast<dialogs::ExportSps*>(LoadDialog("ExportSPS"));
    infodlg->SetTitle(QString::fromLatin1(reinterpret_cast<const char*>(&g_rom[0xa0]),
                                          qstrnlen(reinterpret_cast<const char*>(&g_rom[0xa0]), 12)));
    infodlg->SetDescription(QDateTime::currentDateTime().toString(Qt::TextDate));
    infodlg->SetNotes(tr("Exported from Visual Boy Advance-M"));

    if (ShowModal(infodlg) != QDialog::Accepted)
        return;

    QString msg;

    // FIXME: this will fail on big-endian machines if file format is
    // little-endian; fix in GBA.cpp
    if (CPUWriteGSASnapshot(vbam::ToPath(fn).c_str(), infodlg->title().toUtf8().constData(),
                            infodlg->description().toUtf8().constData(),
                            infodlg->notes().toUtf8().constData()))
        msg = tr("Saved snapshot file %1").arg(fn);
    else
        msg = tr("Error saving snapshot file %1").arg(fn);

    systemScreenMessage(msg);
}

//// Emulation menu: cheats

MASKED_HANDLER(CheatsList, CMDEN_GB | CMDEN_GBA) {
    ShowModal(LoadDialog("CheatList"));
}

MASKED_HANDLER(CheatsSearch, CMDEN_GB | CMDEN_GBA) {
    ShowModal(LoadDialog("CheatCreate"));
}

void MainWindow::OnCheatsAutoSaveLoad() {
    GetMenuOptionConfig(cmd::kCheatsAutoSaveLoad, config::OptionID::kPrefAutoSaveLoadCheatList);
}

// was CheatsDisable; changed for convenience to match internal variable
// functionality
void MainWindow::OnCheatsEnable() {
    bool menuPress = false;
    GetMenuOptionBool(cmd::kCheatsEnable, &menuPress);
    toggleBitVar(&menuPress, &coreOptions.cheatsEnabled, 1);
    SetMenuOption(cmd::kCheatsEnable, menuPress ? 1 : 0);
    GetMenuOptionInt(cmd::kCheatsEnable, &coreOptions.cheatsEnabled, 1);
    update_opts();
}

//// Options menu: link

#ifndef NO_LINK

namespace {

// Brings a link session up for the configured mode without going through the
// menu handler. Returns true if a session is running afterwards. Network and
// Dolphin modes need the NetLink dialog, so they are not restarted silently.
bool RestartLinkSession() {
    MainWindow* mf = vbamApp().frame;
    const LinkMode configured = mf->GetConfiguredLinkMode();

    if (configured == LINK_DISCONNECTED)
        return false;

    if (configured == LINK_GAMECUBE_DOLPHIN || !OPTION(kGBALinkProto))
        return false;

    SetLinkTimeout(gopts.link_timeout);
    EnableSpeedHacks(OPTION(kGBALinkFast));

    if (InitLink(configured) != LINK_OK) {
        // InitIPC already reported the specific failure.
        CloseLink();
        return false;
    }

    if (configured == LINK_GAMEBOY_IPC)
        gbInitLink();

    systemScreenMessage(QCoreApplication::translate("vbam", "Started local link as player %1")
                            .arg(GetLinkPlayerId() + 1));
    return true;
}

// Drops the current session and brings it back up under the settings that
// just changed. Nothing to do when no session was running.
void RelinkAfterSettingChange() {
    MainWindow* mf = vbamApp().frame;

    if (GetLinkMode() == LINK_DISCONNECTED) {
        mf->EnableNetworkMenu();
        return;
    }

    CloseLink();

    if (!RestartLinkSession()) {
        systemScreenMessage(QCoreApplication::translate(
            "vbam", "Link stopped: restart it to apply the new settings"));
    }

    mf->GetPanel()->SetFrameTitle();
    mf->EnableNetworkMenu();
}

void SetLinkTypeMenu(int type_cmd, int value) {
    MainWindow* mf = vbamApp().frame;
    mf->SetMenuOption(cmd::kLinkType0Nothing, 0);
    mf->SetMenuOption(cmd::kLinkType1Cable, 0);
    mf->SetMenuOption(cmd::kLinkType2Wireless, 0);
    mf->SetMenuOption(cmd::kLinkType3GameCube, 0);
    mf->SetMenuOption(cmd::kLinkType4Gameboy, 0);
    mf->SetMenuOption(type_cmd, 1);
    gopts.gba_link_type = value;
    update_opts();
    // The mode is part of the session, so an open one is rebuilt under the
    // new type rather than left running as the old one.
    RelinkAfterSettingChange();
}

}  // namespace

#endif  // NO_LINK

MASKED_HANDLER(LanLink, CMDEN_LINK_ANY) {
#ifndef NO_LINK
    // The menu entry reads "Stop Link" while a session is up, so acting on it
    // is unambiguous and needs no confirmation.
    if (GetLinkMode() != LINK_DISCONNECTED) {
        CloseLink();
        systemScreenMessage(tr("Link stopped"));
        panel->SetFrameTitle();
        EnableNetworkMenu();
        return;
    }

    const LinkMode configured = GetConfiguredLinkMode();

    if (configured == LINK_DISCONNECTED)
        return;  // the CMDEN_LINK_ANY gate should prevent this

    // Dolphin and the network modes collect host / player count in the
    // NetLink dialog. Everything else in local mode attaches directly.
    if (configured == LINK_GAMECUBE_DOLPHIN || !OPTION(kGBALinkProto)) {
        ShowModal(LoadDialog("NetLink"));
        panel->SetFrameTitle();
        EnableNetworkMenu();
        return;
    }

    // Local (IPC) mode: the attach is synchronous, no dialog needed.
    CloseLink();
    RestartLinkSession();
    panel->SetFrameTitle();
    EnableNetworkMenu();
#endif
}

void MainWindow::OnLinkType0Nothing() {
#ifndef NO_LINK
    SetLinkTypeMenu(cmd::kLinkType0Nothing, 0);
#endif
}

void MainWindow::OnLinkType1Cable() {
#ifndef NO_LINK
    SetLinkTypeMenu(cmd::kLinkType1Cable, 1);
#endif
}

void MainWindow::OnLinkType2Wireless() {
#ifndef NO_LINK
    SetLinkTypeMenu(cmd::kLinkType2Wireless, 2);
#endif
}

void MainWindow::OnLinkType3GameCube() {
#ifndef NO_LINK
    SetLinkTypeMenu(cmd::kLinkType3GameCube, 3);
#endif
}

void MainWindow::OnLinkType4Gameboy() {
#ifndef NO_LINK
    SetLinkTypeMenu(cmd::kLinkType4Gameboy, 4);
#endif
}

void MainWindow::OnLinkAuto() {
#ifndef NO_LINK
    GetMenuOptionConfig(cmd::kLinkAuto, config::OptionID::kGBALinkAuto);
#endif
}

void MainWindow::OnSpeedOn() {
#ifndef NO_LINK
    GetMenuOptionConfig(cmd::kSpeedOn, config::OptionID::kGBALinkFast);
#endif
}

void MainWindow::OnLinkProto() {
#ifndef NO_LINK
    GetMenuOptionConfig(cmd::kLinkProto, config::OptionID::kGBALinkProto);
    // Local vs network is the transport, so an open session is rebuilt on it.
    RelinkAfterSettingChange();
#endif
}

void MainWindow::OnLinkConfigure() {
#ifndef NO_LINK
    // Link type is selected via the Options->Link->Type menu radio group;
    // this dialog only configures the timeout.
    QDialog* dlg = LoadDialog("LinkConfig");

    if (ShowModal(dlg) != QDialog::Accepted)
        return;

    SetLinkTimeout(gopts.link_timeout);
    update_opts();
#endif
}

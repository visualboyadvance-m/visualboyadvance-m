// MainWindow entry points for the debugging viewers: create the GBA or GB
// variant for the loaded game and show it, and the per-frame update hook.

#include "qt/game-area.h"
#include "qt/main-window.h"
#include "qt/viewers/viewer.h"

namespace {

void ShowViewer(viewers::Viewer* viewer) {
    if (!viewer)
        return;
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
}

}  // namespace

void MainWindow::Disassemble() {
    switch (GetPanel()->game_type()) {
        case IMAGE_GBA:
            ShowViewer(viewers::NewDisassembleViewer(this));
            break;

        case IMAGE_GB:
            ShowViewer(viewers::NewGBDisassembleViewer(this));
            break;

        case IMAGE_UNKNOWN:
            // do nothing
            break;
    }
}

void MainWindow::IOViewer() {
    ShowViewer(viewers::NewIOViewer(this));
}

void MainWindow::MapViewer() {
    switch (GetPanel()->game_type()) {
        case IMAGE_GBA:
            ShowViewer(viewers::NewMapViewer(this));
            break;

        case IMAGE_GB:
            ShowViewer(viewers::NewGBMapViewer(this));
            break;

        case IMAGE_UNKNOWN:
            // do nothing
            break;
    }
}

void MainWindow::MemViewer() {
    switch (GetPanel()->game_type()) {
        case IMAGE_GBA:
            ShowViewer(viewers::NewMemViewer(this));
            break;

        case IMAGE_GB:
            ShowViewer(viewers::NewGBMemViewer(this));
            break;

        default:
            break;
    }
}

void MainWindow::OAMViewer() {
    switch (GetPanel()->game_type()) {
        case IMAGE_GBA:
            ShowViewer(viewers::NewOAMViewer(this));
            break;

        case IMAGE_GB:
            ShowViewer(viewers::NewGBOAMViewer(this));
            break;

        case IMAGE_UNKNOWN:
            // do nothing
            break;
    }
}

void MainWindow::PaletteViewer() {
    switch (GetPanel()->game_type()) {
        case IMAGE_GBA:
            ShowViewer(viewers::NewPaletteViewer(this));
            break;

        case IMAGE_GB:
            ShowViewer(viewers::NewGBPaletteViewer(this));
            break;

        case IMAGE_UNKNOWN:
            // do nothing
            break;
    }
}

void MainWindow::TileViewer() {
    switch (GetPanel()->game_type()) {
        case IMAGE_GBA:
            ShowViewer(viewers::NewTileViewer(this));
            break;

        case IMAGE_GB:
            ShowViewer(viewers::NewGBTileViewer(this));
            break;

        case IMAGE_UNKNOWN:
            // do nothing
            break;
    }
}

void MainWindow::UpdateViewers() {
    // A viewer's Update() may close it (removing it from popups), so iterate
    // over a copy.
    const dialog_list_t snapshot = popups;
    for (QDialog* dlg : snapshot) {
        viewers::Viewer* d = qobject_cast<viewers::Viewer*>(dlg);
        if (d && d->auto_update)
            d->Update();
    }
}

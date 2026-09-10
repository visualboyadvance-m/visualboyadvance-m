#ifndef VBAM_QT_DIALOGS_GBA_ROM_INFO_H_
#define VBAM_QT_DIALOGS_GBA_ROM_INFO_H_

#include <QLabel>

#include "qt/dialogs/base-dialog.h"

namespace dialogs {

// "ROM Information" for a loaded Game Boy Advance cartridge. The fields are
// re-read from the ROM header every time the dialog is shown.
class GbaRomInfo : public BaseDialog {
    Q_OBJECT

public:
    static GbaRomInfo* NewInstance(QWidget* parent);
    ~GbaRomInfo() override = default;

    // Fills the labels from the loaded ROM (g_rom header + GameArea info).
    void Populate();

protected:
    void showEvent(QShowEvent* event) override;

private:
    explicit GbaRomInfo(QWidget* parent);

    QLabel* title_;
    QLabel* int_title_;
    QLabel* scene_;
    QLabel* release_;
    QLabel* crc32_;
    QLabel* game_code_;
    QLabel* maker_code_;
    QLabel* maker_name_;
    QLabel* unit_code_;
    QLabel* device_type_;
    QLabel* version_;
    QLabel* crc_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_GBA_ROM_INFO_H_

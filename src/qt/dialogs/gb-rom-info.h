#ifndef VBAM_QT_DIALOGS_GB_ROM_INFO_H_
#define VBAM_QT_DIALOGS_GB_ROM_INFO_H_

#include <QLabel>

#include "qt/dialogs/base-dialog.h"

namespace dialogs {

// "ROM Information" for a loaded Game Boy / Game Boy Color cartridge. The
// fields come from g_gbCartData and are refreshed on every show.
class GbRomInfo : public BaseDialog {
    Q_OBJECT

public:
    static GbRomInfo* NewInstance(QWidget* parent);
    ~GbRomInfo() override = default;

    void Populate();

protected:
    void showEvent(QShowEvent* event) override;

private:
    explicit GbRomInfo(QWidget* parent);

    QLabel* title_;
    QLabel* maker_code_;
    QLabel* maker_name_;
    QLabel* cartridge_type_;
    QLabel* sgb_code_;
    QLabel* cgb_code_;
    QLabel* rom_size_;
    QLabel* ram_size_;
    QLabel* dest_code_;
    QLabel* lic_code_;
    QLabel* version_;
    QLabel* header_checksum_;
    QLabel* cartridge_checksum_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_GB_ROM_INFO_H_

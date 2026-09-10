#include "qt/dialogs/directories-config.h"

#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "qt/widgets/option-binding.h"

namespace dialogs {

// static
DirectoriesConfig* DirectoriesConfig::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new DirectoriesConfig(parent);
}

DirectoriesConfig::DirectoriesConfig(QWidget* parent)
    : BaseDialog(parent, "DirectoriesConfig") {
    setWindowTitle(tr("Directories"));

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    struct Entry {
        QString label;
        config::OptionID id;
    };
    const Entry entries[] = {
        {tr("Game Boy Advance ROMs"), config::OptionID::kGBAROMDir},
        {tr("Game Boy ROMs"), config::OptionID::kGBROMDir},
        {tr("Game Boy Color ROMs"), config::OptionID::kGBGBCROMDir},
        {tr("Native Saves"), config::OptionID::kGenBatteryDir},
        {tr("Emulator Saves"), config::OptionID::kGenStateDir},
        {tr("Screenshots"), config::OptionID::kGenScreenshotDir},
        {tr("Recordings"), config::OptionID::kGenRecordingDir},
    };
    for (const Entry& entry : entries) {
        auto* picker = new widgets::PathPicker(this, /*dir=*/true);
        picker->setMinimumWidth(400);
        bindings().BindPathPicker(picker, entry.id);
        form->addRow(entry.label, picker);
    }
    layout->addLayout(form);

    auto* note = new QLabel(
        tr("Use %s in paths for the system name (GameBoy Advance, GameBoy Color, "
           "Super GameBoy, or GameBoy)."),
        this);
    note->setWordWrap(true);
    layout->addWidget(note);

    layout->addWidget(CreateOkCancel());
}

}  // namespace dialogs

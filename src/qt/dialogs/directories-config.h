#ifndef VBAM_QT_DIALOGS_DIRECTORIES_CONFIG_H_
#define VBAM_QT_DIALOGS_DIRECTORIES_CONFIG_H_

#include "qt/dialogs/base-dialog.h"

namespace dialogs {

// The Directories configuration dialog: ROM, save, state, screenshot and
// recording directories.
class DirectoriesConfig final : public BaseDialog {
    Q_OBJECT

public:
    static DirectoriesConfig* NewInstance(QWidget* parent);
    ~DirectoriesConfig() override = default;

private:
    explicit DirectoriesConfig(QWidget* parent);
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_DIRECTORIES_CONFIG_H_

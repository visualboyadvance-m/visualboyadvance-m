#ifndef VBAM_QT_DIALOGS_CODE_SELECT_H_
#define VBAM_QT_DIALOGS_CODE_SELECT_H_

#include <QListWidget>
#include <QStringList>

#include "qt/dialogs/base-dialog.h"

namespace dialogs {

// Lets the user pick one game out of a multi-game Game Shark code file
// (ImportGamesharkCodeFile).
class CodeSelect : public BaseDialog {
    Q_OBJECT

public:
    static CodeSelect* NewInstance(QWidget* parent);
    ~CodeSelect() override = default;

    // Replaces the list contents. Items are shown in the given order (the
    // index returned by selection() refers to it).
    void SetCodes(const QStringList& codes);

    // Index of the selected item in the list passed to SetCodes(), or -1.
    int selection() const;

private:
    explicit CodeSelect(QWidget* parent);

    QListWidget* list_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_CODE_SELECT_H_

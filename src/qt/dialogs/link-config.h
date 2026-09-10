#ifndef VBAM_QT_DIALOGS_LINK_CONFIG_H_
#define VBAM_QT_DIALOGS_LINK_CONFIG_H_

#ifndef NO_LINK

#include <QSpinBox>

#include "qt/dialogs/base-dialog.h"

namespace dialogs {

// Link options. The link type is selected via the Options->Link->Type menu
// radio group; this dialog only configures the timeout (gopts.link_timeout).
class LinkConfig : public BaseDialog {
    Q_OBJECT

public:
    static LinkConfig* NewInstance(QWidget* parent);
    ~LinkConfig() override = default;

protected:
    void showEvent(QShowEvent* event) override;

private:
    explicit LinkConfig(QWidget* parent);
    void OnAccepted();

    QSpinBox* timeout_;
};

}  // namespace dialogs

#endif  // NO_LINK

#endif  // VBAM_QT_DIALOGS_LINK_CONFIG_H_

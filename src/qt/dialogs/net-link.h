#ifndef VBAM_QT_DIALOGS_NET_LINK_H_
#define VBAM_QT_DIALOGS_NET_LINK_H_

#ifndef NO_LINK

#include "qt/dialogs/base-dialog.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

namespace dialogs {

// "Start Network Link": server / client, player count, host or bind address
// and port. Accepting the dialog closes any running link session and starts a
// new one for MainWindow::GetConfiguredLinkMode(), waiting for the peers with
// a cancellable progress dialog. The dialog only closes with Accepted when a
// session is up afterwards.
class NetLink : public BaseDialog {
    Q_OBJECT

public:
    static NetLink* NewInstance(QWidget* parent);
    ~NetLink() override = default;

protected:
    void OnDialogShown() override;
    bool OnAccept() override;

private:
    explicit NetLink(QWidget* parent);

    void OnModeChanged();
    void PopulateLocalAddresses();

    QRadioButton* server_rb_;
    QRadioButton* client_rb_;
    QRadioButton* players_rb_[3];  // 2, 3, 4
    QComboBox* server_ip_;
    QLineEdit* server_port_;
    QPushButton* ok_button_;

    int n_players_ = 2;
    bool server_ = false;
};

}  // namespace dialogs

#endif  // NO_LINK

#endif  // VBAM_QT_DIALOGS_NET_LINK_H_

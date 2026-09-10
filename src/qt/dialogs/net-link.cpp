#include "qt/dialogs/net-link.h"

#ifndef NO_LINK

#include <QComboBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "core/gba/gbaLink.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"
#include "qt/opts.h"

namespace dialogs {

// static
NetLink* NetLink::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new NetLink(parent);
}

NetLink::NetLink(QWidget* parent) : BaseDialog(parent, "NetLink") {
    setWindowTitle(tr("Start Network Link"));

    auto* layout = new QVBoxLayout(this);

    auto* warning = new QLabel(tr("WARNING: Link will likely not work over the internet or LAN."), this);
    warning->setWordWrap(true);
    warning->setStyleSheet("color: #DC143C; font-weight: bold;");
    layout->addWidget(warning);

    auto* mode = new QHBoxLayout();
    server_rb_ = new QRadioButton(tr("Server"), this);
    client_rb_ = new QRadioButton(tr("Client"), this);
    mode->addWidget(server_rb_, 0, Qt::AlignCenter);
    mode->addWidget(client_rb_, 0, Qt::AlignCenter);
    layout->addLayout(mode);
    connect(server_rb_, &QRadioButton::toggled, this, [this](bool) { OnModeChanged(); });

    auto* form = new QFormLayout();

    auto* players = new QHBoxLayout();
    const char* labels[3] = {"2", "3", "4"};
    for (int i = 0; i < 3; i++) {
        players_rb_[i] = new QRadioButton(QLatin1String(labels[i]), this);
        players->addWidget(players_rb_[i], 0, Qt::AlignCenter);
    }
    // The player radios form their own group so they do not clash with
    // Server/Client.
    auto* players_box = new QWidget(this);
    players_box->setLayout(players);
    form->addRow(tr("Players:"), players_box);

    server_ip_ = new QComboBox(this);
    server_ip_->setEditable(true);
    form->addRow(tr("Server:"), server_ip_);

    server_port_ = new QLineEdit(this);
    server_port_->setValidator(new QIntValidator(1, 65535, server_port_));
    form->addRow(tr("Port:"), server_port_);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    ok_button_ = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    server_rb_->setChecked(true);
}

void NetLink::PopulateLocalAddresses() {
    const QString current = server_ip_->currentText();
    server_ip_->clear();
    server_ip_->addItem(QStringLiteral("*"));
    for (const QHostAddress& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() != QAbstractSocket::IPv4Protocol)
            continue;
        if (addr.isLoopback())
            continue;
        server_ip_->addItem(addr.toString());
    }
    server_ip_->setCurrentText(current);
}

void NetLink::OnModeChanged() {
    server_ = server_rb_->isChecked();
    if (server_) {
        ok_button_->setText(tr("Start!"));
        // Bind address for the server: the local interfaces.
        PopulateLocalAddresses();
        server_ip_->setCurrentText(gopts.server_ip);
    } else {
        ok_button_->setText(tr("Connect"));
        server_ip_->clear();
        server_ip_->setCurrentText(gopts.link_host);
    }
}

void NetLink::OnDialogShown() {
    n_players_ = gopts.link_num_players;

    // The GB serial protocol is strictly 2-player, so clamp the player count
    // and gray the 3P/4P choices for a GB session.
    const bool gb2p = vbamApp().frame->GetConfiguredLinkMode() == LINK_GAMEBOY_SOCKET;

    if (gb2p && n_players_ > 2)
        n_players_ = 2;

    players_rb_[1]->setEnabled(!gb2p);
    players_rb_[2]->setEnabled(!gb2p);

    const int idx = qBound(2, n_players_, 4) - 2;
    players_rb_[idx]->setChecked(true);

    server_port_->setText(QString::number(gopts.link_port));
    OnModeChanged();
}

bool NetLink::OnAccept() {
    static const int length = 256;

    server_ = server_rb_->isChecked();
    for (int i = 0; i < 3; i++)
        if (players_rb_[i]->isChecked())
            n_players_ = i + 2;

    bool port_ok = false;
    const uint32_t port = server_port_->text().toUInt(&port_ok);
    if (!port_ok || port == 0 || port > 65535) {
        QMessageBox::critical(this, tr("Invalid port"), tr("You must enter a valid port number"));
        return false;
    }
    gopts.link_port = port;

    if (server_)
        gopts.server_ip = server_ip_->currentText().trimmed();
    else
        gopts.link_host = server_ip_->currentText().trimmed();

    IP_LINK_PORT = gopts.link_port;
    IP_LINK_BIND_ADDRESS = vbam::ToStd(gopts.server_ip);

    if (!server_) {
        const bool valid = SetLinkServerHost(gopts.link_host.toUtf8().constData());

        if (!valid) {
            QMessageBox::critical(this, tr("Host name invalid"),
                                  tr("You must enter a valid host name"));
            return false;
        }
    }

    MainWindow* mf = vbamApp().frame;

    // Backstop for the 2-player GB serial limit: a GB server with more slaves
    // would never transfer (the exchange code requires exactly one slave).
    if (mf->GetConfiguredLinkMode() == LINK_GAMEBOY_SOCKET)
        n_players_ = 2;

    gopts.link_num_players = n_players_;
    update_opts(); // save fast flag and client host
    // Close any previous link
    CloseLink();
    QString connmsg;
    QString title;
    SetLinkTimeout(gopts.link_timeout);
    EnableSpeedHacks(OPTION(kGBALinkFast));
    EnableLinkServer(server_, gopts.link_num_players - 1);

    if (server_) {
        char host[length];
        if (!GetLinkServerHost(host, length)) {
            QMessageBox::critical(this, tr("Host name invalid"),
                                  tr("You must enter a valid host name"));
        }
        title = tr("Waiting for clients...");
        connmsg = tr("Server IP address is: %1\n").arg(QString::fromLatin1(host));
    } else {
        title = tr("Waiting for connection...");
        connmsg = tr("Connecting to %1\n").arg(gopts.link_host);
    }

    // Init link
    ConnectionState state = InitLink(mf->GetConfiguredLinkMode());
    const bool init_failed = state == LINK_ERROR;

    // Display a progress dialog while the connection is establishing
    if (state == LINK_NEEDS_UPDATE) {
        QProgressDialog pdlg(connmsg, tr("Cancel"), 0, 0, this);
        pdlg.setWindowTitle(title);
        pdlg.setWindowModality(Qt::WindowModal);
        pdlg.setMinimumDuration(0);
        pdlg.setAutoClose(false);
        pdlg.setAutoReset(false);

        QElapsedTimer elapsed;
        elapsed.start();

        QTimer poll;
        poll.setInterval(50);
        QObject::connect(&poll, &QTimer::timeout, &pdlg, [&] {
            char message[length];
            state = ConnectLinkUpdate(message, length);
            connmsg = QString::fromLatin1(message);
            pdlg.setLabelText(QStringLiteral("%1\n%2").arg(
                connmsg, tr("Elapsed: %1 s").arg(elapsed.elapsed() / 1000)));
            if (state != LINK_NEEDS_UPDATE)
                pdlg.accept();
        });
        poll.start();
        const int rc = pdlg.exec();
        poll.stop();

        if (rc == QDialog::Rejected || pdlg.wasCanceled()) {
            state = LINK_ABORT;
        }
    }

    // The user canceled the connection attempt
    if (state == LINK_ABORT) {
        CloseLink();
    }

    // Something failed during init
    if (state == LINK_ERROR) {
        CloseLink();

        // An InitLink failure already queued a specific systemMessage (bad
        // bind address, port in use, ...); only failures from the connect
        // pulse loop need reporting here, and connmsg holds the loop's last
        // status ("Connection to server timed out.").
        if (!init_failed)
            vbam::LogError(tr("Link connection failed: %1").arg(connmsg));
    }

    if (GetLinkMode() != LINK_DISCONNECTED) {
        connmsg.replace('\n', ' ');
        systemScreenMessage(connmsg);
        return true;  // all OK
    }

    return false;
}

}  // namespace dialogs

#endif  // NO_LINK

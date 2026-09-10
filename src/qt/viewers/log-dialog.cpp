#include "qt/viewers/log-dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "qt/app.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"

LogDialog::LogDialog(QWidget* parent) : dialogs::BaseDialog(parent, QStringLiteral("Logging")) {
    setWindowTitle(tr("Logging"));

    QVBoxLayout* top = new QVBoxLayout(this);
    QHBoxLayout* row = new QHBoxLayout();

    QGroupBox* verbose = new QGroupBox(tr("Verbose"), this);
    QVBoxLayout* vl = new QVBoxLayout(verbose);
    auto add_flag = [this, vl, verbose](const QString& label, int flag) {
        QCheckBox* cb = new QCheckBox(label, verbose);
        cb->setChecked(systemVerbose & flag);
        connect(cb, &QCheckBox::toggled, this, [flag](bool checked) {
            if (checked)
                systemVerbose |= flag;
            else
                systemVerbose &= ~flag;
        });
        vl->addWidget(cb);
    };
    add_flag(QStringLiteral("SW&I"), VERBOSE_SWI);
    add_flag(tr("Unaligned &memory"), VERBOSE_UNALIGNED_MEMORY);
    add_flag(tr("Illegal &write"), VERBOSE_ILLEGAL_WRITE);
    add_flag(tr("Illegal &read"), VERBOSE_ILLEGAL_READ);
    add_flag(QStringLiteral("DMA &0"), VERBOSE_DMA0);
    add_flag(QStringLiteral("DMA &1"), VERBOSE_DMA1);
    add_flag(QStringLiteral("DMA &2"), VERBOSE_DMA2);
    add_flag(QStringLiteral("DMA &3"), VERBOSE_DMA3);
    add_flag(tr("&Undefined instruction"), VERBOSE_UNDEFINED);
    add_flag(QStringLiteral("&AGBPrint"), VERBOSE_AGBPRINT);
    add_flag(tr("Soun&d output"), VERBOSE_SOUNDOUTPUT);
    vl->addStretch();
    row->addWidget(verbose);

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setLineWrapMode(QPlainTextEdit::NoWrap);
    log_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    log_->setMinimumSize(400, 300);
    row->addWidget(log_, 1);
    top->addLayout(row, 1);

    QHBoxLayout* buttons = new QHBoxLayout();
    QPushButton* save = new QPushButton(tr("&Save"), this);
    connect(save, &QPushButton::clicked, this, &LogDialog::Save);
    buttons->addWidget(save);
    QPushButton* clear = new QPushButton(tr("&Clear"), this);
    connect(clear, &QPushButton::clicked, this, &LogDialog::Clear);
    buttons->addWidget(clear);
    buttons->addStretch();
    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    buttons->addWidget(box);
    top->addLayout(buttons);

    Update();
}

LogDialog::~LogDialog() = default;

void LogDialog::OnDialogShown() {
    Update();
}

void LogDialog::Update() {
    const QString& l = vbam::LogText();

    // Buffer was cleared or truncated out from under us -- resync from scratch.
    if (shown_len_ > l.size()) {
        log_->clear();
        shown_len_ = 0;
    }

    if (shown_len_ < l.size()) {
        log_->moveCursor(QTextCursor::End);
        log_->insertPlainText(l.mid(shown_len_));
        shown_len_ = l.size();
        log_->moveCursor(QTextCursor::End);
        log_->ensureCursorVisible();
    }
}

void LogDialog::Save() {
    static QString logdir, def_name;

    if (def_name.isEmpty() && vbamApp().frame && vbamApp().frame->GetPanel())
        def_name = vbamApp().frame->GetPanel()->game_name() + QStringLiteral(".log");

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Select output file"), QDir(logdir).filePath(def_name),
        tr("Text files (*.txt *.log)") + QStringLiteral(";;") + tr("All files (*)"));

    if (path.isEmpty())
        return;

    def_name = QFileInfo(path).fileName();
    logdir = QFileInfo(path).absolutePath();

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(vbam::LogText().toUtf8());
        f.close();
    } else {
        vbam::LogError(tr("Can't open file %1").arg(path));
    }
}

void LogDialog::Clear() {
    vbam::ClearLogText();
    log_->clear();
    shown_len_ = 0;
}

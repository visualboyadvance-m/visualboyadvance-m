#include "qt/dialogs/cheat-list.h"

#include <cstdio>
#include <cstring>

#include <QAction>
#include <QDir>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStyle>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "core/gb/gbCheats.h"
#include "core/gba/gbaCheats.h"
#include "qt/app.h"
#include "qt/dialogs/cheat-edit.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"

namespace dialogs {

namespace {

constexpr int kColCode = 0;
constexpr int kColDesc = 1;

// Copies `desc` into the fixed-size description field of a cheat entry.
void SetDescField(char* p, size_t size, const QString& desc) {
    const QByteArray utf8 = desc.toUtf8();
    std::strncpy(p, utf8.constData(), size);
    p[size - 1] = 0;
}

}  // namespace

// static
CheatList* CheatList::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new CheatList(parent);
}

CheatList::CheatList(QWidget* parent) : BaseDialog(parent, "CheatList") {
    setWindowTitle(tr("Cheat List"));

    auto* layout = new QVBoxLayout(this);

    auto* toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    QStyle* st = style();

    QAction* open = toolbar->addAction(st->standardIcon(QStyle::SP_DialogOpenButton), tr("Open"));
    open->setToolTip(tr("Open cheat list"));
    connect(open, &QAction::triggered, this, &CheatList::OnOpen);

    QAction* save = toolbar->addAction(st->standardIcon(QStyle::SP_DialogSaveButton), tr("Save"));
    save->setToolTip(tr("Save cheat list"));
    connect(save, &QAction::triggered, this, &CheatList::OnSave);

    toolbar->addSeparator();

    QAction* add = toolbar->addAction(st->standardIcon(QStyle::SP_FileDialogNewFolder), tr("Add"));
    add->setToolTip(tr("Add new cheat"));
    connect(add, &QAction::triggered, this, &CheatList::OnAdd);

    remove_action_ = toolbar->addAction(st->standardIcon(QStyle::SP_DialogDiscardButton), tr("Remove"));
    remove_action_->setToolTip(tr("Delete selected cheat"));
    connect(remove_action_, &QAction::triggered, this, &CheatList::OnRemove);

    QAction* clear = toolbar->addAction(st->standardIcon(QStyle::SP_TrashIcon), tr("Clear"));
    clear->setToolTip(tr("Delete all cheats"));
    connect(clear, &QAction::triggered, this, &CheatList::OnClear);

    toolbar->addSeparator();

    QAction* toggle = toolbar->addAction(st->standardIcon(QStyle::SP_DialogApplyButton), tr("Toggle all"));
    toggle->setToolTip(tr("Toggle all Cheats"));
    connect(toggle, &QAction::triggered, this, &CheatList::OnToggleAll);

    layout->addWidget(toolbar);

    list_ = new QTableWidget(0, 2, this);
    list_->setHorizontalHeaderLabels({tr("Code"), tr("Description")});
    list_->setSelectionBehavior(QAbstractItemView::SelectRows);
    list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list_->verticalHeader()->setVisible(false);
    list_->horizontalHeader()->setStretchLastSection(true);
    list_->setMinimumSize(400, 200);
    connect(list_, &QTableWidget::itemChanged, this, &CheatList::OnItemChanged);
    connect(list_, &QTableWidget::itemActivated, this, &CheatList::OnItemActivated);
    connect(list_, &QTableWidget::itemSelectionChanged, this,
            [this] { remove_action_->setEnabled(!list_->selectedItems().isEmpty()); });
    layout->addWidget(list_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);

    remove_action_->setEnabled(false);
}

CheatEdit* CheatList::edit_dialog() {
    return static_cast<CheatEdit*>(vbamApp().frame->LoadDialog("CheatEdit"));
}

void CheatList::OnDialogShown() {
    GameArea* panel = vbamApp().frame->GetPanel();
    isgb_ = panel->game_type() == IMAGE_GB;
    dirty_ = &panel->cheats_dirty;
    cheatfn_ = panel->game_name() + ".clt";
    cheatdir_ = panel->game_dir();
    deffn_ = QFileInfo(QDir(cheatdir_), cheatfn_).filePath();
    edit_dialog()->SetIsGb(isgb_);
    Reload();
}

void CheatList::AddRow(int index, const QString& code, const QString& desc, bool enabled) {
    if (list_->rowCount() <= index)
        list_->setRowCount(index + 1);

    auto* code_item = new QTableWidgetItem(code);
    code_item->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    code_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    code_item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    list_->setItem(index, kColCode, code_item);

    auto* desc_item = new QTableWidgetItem(desc);
    desc_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    list_->setItem(index, kColDesc, desc_item);
}

void CheatList::Reload() {
    reloading_ = true;
    list_->setRowCount(0);
    reloading_ = false;
    Reload(0);
}

void CheatList::Reload(int start) {
    reloading_ = true;
    if (isgb_) {
        list_->setRowCount(gbCheatNumber);
        for (int i = start; i < gbCheatNumber; i++) {
            AddRow(i, QString::fromLatin1(gbCheatList[i].cheatCode),
                   QString::fromUtf8(gbCheatList[i].cheatDesc), gbCheatList[i].enabled);
        }
    } else {
        list_->setRowCount(cheatsNumber);
        for (int i = start; i < cheatsNumber; i++) {
            AddRow(i, QString::fromLatin1(cheatsList[i].codestring),
                   QString::fromUtf8(cheatsList[i].desc), cheatsList[i].enabled);
        }
    }
    list_->resizeColumnToContents(kColCode);
    reloading_ = false;
}

// static
void CheatList::ParseChtLine(const QString& desc, const QString& tok) {
    const QString cheat_opt = tok.section('=', 0, 0);
    const QString cheat_set = tok.section('=', 1).toUpper();

    for (const QString& addr_token : cheat_set.split(';', Qt::SkipEmptyParts)) {
        const QString cheat_addr = addr_token.section(',', 0, 0);
        const QString values = addr_token.section(',', 1);
        const QString cheat_desc = desc + ":" + cheat_opt;
        uint32_t address = 0;
        unsigned int value = 0;
        std::sscanf(cheat_addr.toUtf8().constData(), "%8x", &address);

        if (address < 0x40000)
            address += 0x2000000;
        else
            address += 0x3000000 - 0x40000;

        for (const QString& value_token : values.toUpper().split(',', Qt::SkipEmptyParts)) {
            std::sscanf(value_token.toUtf8().constData(), "%2x", &value);
            const QString cheat_line =
                QStringLiteral("%1:%2")
                    .arg(address, 8, 16, QLatin1Char('0'))
                    .arg(value, 2, 16, QLatin1Char('0'))
                    .toUpper();
            cheatsAddCheatCode(cheat_line.toUtf8().constData(), cheat_desc.toUtf8().constData());
            address++;
        }
    }
}

// static
void CheatList::LoadChtFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        vbam::LogError(QCoreApplication::translate("vbam", "Cannot open file %1").arg(path));
        return;
    }

    QTextStream text(&file);
    QString cheat_desc;

    while (!text.atEnd()) {
        QString line = text.readLine().trimmed();

        if (line.contains('[') && !line.contains('=')) {
            cheat_desc = line.section('[', 1).section(']', 0, -2);
        }

        if (line.contains('=') && cheat_desc != "GameInfo") {
            while (!text.atEnd() && (line.endsWith(';') || line.endsWith(','))) {
                line = line + text.readLine().trimmed();
            }

            ParseChtLine(cheat_desc, line);
        }
    }
}

// static
void CheatList::AddCheat(bool isgb, int type, const QString& desc, const QString& codes) {
    const QByteArray desc8 = desc.toUtf8();
    const QStringList toks = codes.toUpper().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    for (int i = 0; i < toks.size(); i++) {
        QString tok = toks[i];

        if (isgb) {
            if (!type)
                gbAddGsCheat(tok.toUtf8().constData(), desc8.constData());
            else
                gbAddGgCheat(tok.toUtf8().constData(), desc8.constData());
        } else {
            // Flashcart CHT format
            if (tok.contains('=')) {
                ParseChtLine(desc, tok);
            }
            // Generic Code
            else if (tok.contains(':'))
                cheatsAddCheatCode(tok.toUtf8().constData(), desc8.constData());
            // following determination of type by lengths is same used by
            // win32 and gtk code and like win32/gtk code, user-chosen fmt is
            // ignored
            else if (tok.size() == 12) {
                tok = tok.left(8) + ' ' + tok.mid(8);
                cheatsAddCBACode(tok.toUtf8().constData(), desc8.constData());
            } else if (tok.size() == 16)
                // not sure why 1-tok is !v3 and 2-tok is v3..
                cheatsAddGSACode(tok.toUtf8().constData(), desc8.constData(), false);
            // CBA codes are assumed to be N+4, and anything else is assumed to
            // be GSA v3 (although I assume the actual formats should be 8+4
            // and 8+8)
            else {
                if (i + 1 >= toks.size()) {
                    // throw an error appropriate to chosen type
                    if (type == 1) // GSA
                        cheatsAddGSACode(tok.toUtf8().constData(), desc8.constData(), false);
                    else
                        cheatsAddCBACode(tok.toUtf8().constData(), desc8.constData());
                } else {
                    const QString tok2 = toks[++i];

                    if (tok2.size() == 4) {
                        tok += ' ' + tok2;
                        cheatsAddCBACode(tok.toUtf8().constData(), desc8.constData());
                    } else {
                        tok += tok2;
                        cheatsAddGSACode(tok.toUtf8().constData(), desc8.constData(), true);
                    }
                }
            }
        }
    }
}

void CheatList::OnOpen() {
    QString filter = tr("VBA cheat lists (*.clt);;CHT cheat lists (*.cht)");
    QString path = QFileDialog::getOpenFileName(this, tr("Select cheat file"),
                                                QDir(cheatdir_).filePath(cheatfn_), filter);
    if (path.isEmpty())
        return;

    cheatdir_ = QFileInfo(path).absolutePath();
    cheatfn_ = path;

    if (isgb_) {
        gbCheatsLoadCheatList(vbam::ToPath(cheatfn_).c_str());
    } else {
        if (cheatfn_.endsWith(".clt", Qt::CaseInsensitive)) {
            if (cheatsLoadCheatList(vbam::ToPath(cheatfn_).c_str())) {
                *dirty_ = cheatfn_ != deffn_;
                systemScreenMessage(tr("Loaded cheats"));
            } else {
                *dirty_ = true; // attempted load always clears
            }
        } else {
            LoadChtFile(cheatfn_);
            *dirty_ = true;
        }
    }

    Reload();
}

void CheatList::OnSave() {
    QString path = QFileDialog::getSaveFileName(this, tr("Select cheat file"),
                                                QDir(cheatdir_).filePath(cheatfn_),
                                                tr("VBA cheat lists (*.clt)"));
    if (path.isEmpty())
        return;

    cheatdir_ = QFileInfo(path).absolutePath();
    cheatfn_ = path;

    // note that there is no way to test for succes of save
    if (isgb_)
        gbCheatsSaveCheatList(vbam::ToPath(cheatfn_).c_str());
    else
        cheatsSaveCheatList(vbam::ToPath(cheatfn_).c_str());

    if (cheatfn_ == deffn_)
        *dirty_ = false;

    systemScreenMessage(tr("Saved cheats"));
}

void CheatList::OnAdd() {
    const int ncheats = isgb_ ? gbCheatNumber : cheatsNumber;
    CheatEdit* dlg = edit_dialog();
    dlg->SetIsGb(isgb_);
    dlg->SetDescription(QString());
    dlg->SetCodes(QString());

    if (dlg->exec() != QDialog::Accepted)
        return;

    AddCheat(isgb_, dlg->type(), dlg->description(), dlg->codes());
    *dirty_ = true;
    Reload(ncheats);
}

void CheatList::OnRemove() {
    bool asked = false, restore = false;

    for (int i = list_->rowCount() - 1; i >= 0; i--) {
        QTableWidgetItem* item = list_->item(i, kColCode);
        if (!item || !item->isSelected())
            continue;

        reloading_ = true;
        list_->removeRow(i);
        reloading_ = false;

        if (isgb_)
            gbCheatRemove(i);
        else {
            if (!asked) {
                asked = true;
                restore = QMessageBox::question(this, tr("Removing cheats"),
                                                tr("Restore old values?")) == QMessageBox::Yes;
            }

            cheatsDelete(i, restore);
        }
        *dirty_ = true;
    }
}

void CheatList::OnClear() {
    if (isgb_) {
        if (gbCheatNumber) {
            *dirty_ = true;
            gbCheatRemoveAll();
        }
    } else {
        if (cheatsNumber) {
            const bool restore = QMessageBox::question(this, tr("Removing cheats"),
                                                       tr("Restore old values?")) == QMessageBox::Yes;
            *dirty_ = true;
            cheatsDeleteAll(restore);
        }
    }

    Reload();
}

void CheatList::OnToggleAll() {
    // FIXME: probably ought to limit to selected items if any items are selected
    *dirty_ = true;
    reloading_ = true;

    if (isgb_) {
        int i;
        for (i = 0; i < gbCheatNumber; i++)
            if (!gbCheatList[i].enabled)
                break;

        if (i < gbCheatNumber)
            for (; i < gbCheatNumber; i++) {
                gbCheatEnable(i);
                list_->item(i, kColCode)->setCheckState(Qt::Checked);
            }
        else
            for (i = 0; i < gbCheatNumber; i++) {
                gbCheatDisable(i);
                list_->item(i, kColCode)->setCheckState(Qt::Unchecked);
            }
    } else {
        int i;
        for (i = 0; i < cheatsNumber; i++)
            if (!cheatsList[i].enabled)
                break;

        if (i < cheatsNumber)
            for (; i < cheatsNumber; i++) {
                cheatsEnable(i);
                list_->item(i, kColCode)->setCheckState(Qt::Checked);
            }
        else
            for (i = 0; i < cheatsNumber; i++) {
                cheatsDisable(i);
                list_->item(i, kColCode)->setCheckState(Qt::Unchecked);
            }
    }

    reloading_ = false;
}

void CheatList::OnItemChanged(QTableWidgetItem* item) {
    if (reloading_ || !item || item->column() != kColCode)
        return;

    const int ch = item->row();
    const bool checked = item->checkState() == Qt::Checked;

    if (isgb_) {
        if (ch >= gbCheatNumber)
            return;
        if (checked && !gbCheatList[ch].enabled) {
            gbCheatEnable(ch);
            *dirty_ = true;
        } else if (!checked && gbCheatList[ch].enabled) {
            gbCheatDisable(ch);
            *dirty_ = true;
        }
    } else {
        if (ch >= cheatsNumber)
            return;
        if (checked && !cheatsList[ch].enabled) {
            cheatsEnable(ch);
            *dirty_ = true;
        } else if (!checked && cheatsList[ch].enabled) {
            cheatsDisable(ch);
            *dirty_ = true;
        }
    }
}

void CheatList::OnItemActivated(QTableWidgetItem* item) {
    if (item)
        EditCheat(item->row());
}

void CheatList::EditCheat(int id) {
    // GetItem() followed by GetText doesn't work, so retrieve from source
    QString odesc, ocode;
    bool ochecked;
    int otype;

    if (isgb_) {
        if (id >= gbCheatNumber)
            return;
        ochecked = gbCheatList[id].enabled;
        ocode = QString::fromLatin1(gbCheatList[id].cheatCode);
        odesc = QString::fromUtf8(gbCheatList[id].cheatDesc);
        otype = ocode.contains('-') ? 1 : 0;
    } else {
        if (id >= cheatsNumber)
            return;
        ochecked = cheatsList[id].enabled;
        ocode = QString::fromLatin1(cheatsList[id].codestring);
        odesc = QString::fromUtf8(cheatsList[id].desc);

        if (ocode.contains(':'))
            otype = 0;
        else if (!ocode.contains(' '))
            otype = 1;
        else
            otype = 2;
    }

    CheatEdit* dlg = edit_dialog();
    dlg->SetIsGb(isgb_);
    dlg->SetDescription(odesc);
    dlg->SetCodes(ocode);
    dlg->SetType(otype);

    if (dlg->exec() != QDialog::Accepted)
        return;

    const QString ce_desc = dlg->description();
    const QString ce_codes = dlg->codes();
    const int ce_type = dlg->type();

    if (otype != ce_type || ocode != ce_codes) {
        // vba core certainly doesn't make this easy: there is no "change"
        // function, so the only way to retain the old order is to delete this
        // and all subsequent items, and then re-add them.
        const int ncodes = isgb_ ? gbCheatNumber : cheatsNumber;

        if (ncodes > id + 1) {
            std::vector<QString> codes(ncodes - id - 1);
            std::vector<QString> descs(ncodes - id - 1);
            std::vector<bool> checked(ncodes - id - 1);
            std::vector<bool> v3(ncodes - id - 1);

            for (int i = id + 1; i < ncodes; i++) {
                codes[i - id - 1] = QString::fromLatin1(isgb_ ? gbCheatList[i].cheatCode
                                                              : cheatsList[i].codestring);
                descs[i - id - 1] =
                    QString::fromUtf8(isgb_ ? gbCheatList[i].cheatDesc : cheatsList[i].desc);
                checked[i - id - 1] = isgb_ ? gbCheatList[i].enabled : cheatsList[i].enabled;
                v3[i - id - 1] = isgb_ ? false : cheatsList[i].code == 257;
            }

            reloading_ = true;
            for (int i = ncodes - 1; i >= id; i--) {
                list_->removeRow(i);

                if (isgb_)
                    gbCheatRemove(i);
                else
                    cheatsDelete(i, cheatsList[i].enabled);
            }
            reloading_ = false;

            AddCheat(isgb_, ce_type, ce_desc, ce_codes);

            if (!ochecked) {
                if (isgb_)
                    gbCheatDisable(id);
                else
                    cheatsDisable(id);
            }

            for (int i = id + 1; i < ncodes; i++) {
                QString re_codes = codes[i - id - 1];
                int re_type;

                if (isgb_) {
                    re_type = re_codes.contains('-') ? 1 : 0;
                } else {
                    if (re_codes.contains(':'))
                        re_type = 0;
                    else if (!re_codes.contains(' ')) {
                        re_type = 1;

                        if (v3[i - id - 1])
                            re_codes.insert(8, ' ');
                    } else {
                        re_type = 2;
                    }
                }

                AddCheat(isgb_, re_type, descs[i - id - 1], re_codes);

                if (!checked[i - id - 1]) {
                    if (isgb_)
                        gbCheatDisable(i);
                    else
                        cheatsDisable(i);
                }
            }
        } else {
            reloading_ = true;
            list_->removeRow(id);
            reloading_ = false;

            if (isgb_)
                gbCheatRemove(id);
            else
                cheatsDelete(id, cheatsList[id].enabled);

            AddCheat(isgb_, ce_type, ce_desc, ce_codes);

            if (!ochecked) {
                if (isgb_)
                    gbCheatDisable(id);
                else
                    cheatsDisable(id);
            }
        }

        *dirty_ = true;
        Reload(id);
    } else if (ce_desc != odesc) {
        *dirty_ = true;
        if (isgb_)
            SetDescField(gbCheatList[id].cheatDesc, sizeof(gbCheatList[0].cheatDesc), ce_desc);
        else
            SetDescField(cheatsList[id].desc, sizeof(cheatsList[0].desc), ce_desc);

        reloading_ = true;
        list_->item(id, kColDesc)->setText(QString::fromUtf8(isgb_ ? gbCheatList[id].cheatDesc
                                                                    : cheatsList[id].desc));
        reloading_ = false;
    }
}

}  // namespace dialogs

#include "qt/dialogs/cheat-search.h"

#include <cstdlib>

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTableView>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "core/gb/gb.h"
#include "core/gb/gbCartData.h"
#include "core/gb/gbCheats.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaCheatSearch.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaGlobals.h"
#include "qt/app.h"
#include "qt/dialogs/cheat-add.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"

namespace dialogs {

namespace {

const char kValSigDigits[] = "0123456789-";
const char kValUnsDigits[] = "0123456789";
const char kValHexDigits[] = "0123456789ABCDEFabcdef";

// The single open search dialog, so MainWindow::ResetCheatSearch() can clear
// its view.
CheatSearch* g_cheat_search = nullptr;

}  // namespace

// ---- CheatSearchModel -----------------------------------------------------

CheatSearchModel::CheatSearchModel(QObject* parent) : QAbstractTableModel(parent) {}

int CheatSearchModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : row_count;
}

int CheatSearchModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 3;
}

QVariant CheatSearchModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch (section) {
        case 0:
            return tr("Address");
        case 1:
            return tr("Old Value");
        case 2:
            return tr("New Value");
    }
    return QVariant();
}

QVariant CheatSearchModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();
    if (role == Qt::DisplayRole)
        return ItemText(index.row(), index.column());
    if (role == Qt::TextAlignmentRole && index.column() > 0)
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    return QVariant();
}

void CheatSearchModel::ResetRows(int rows) {
    beginResetModel();
    row_count = rows;
    endResetModel();
}

void CheatSearchModel::Refresh() {
    beginResetModel();
    endResetModel();
}

QString CheatSearchModel::FormatValue(int32_t val) const {
    if (fmt != CFVFMT_SD && size != BITS_32)
        val &= size == BITS_8 ? 0xff : 0xffff;

    switch (fmt) {
        case CFVFMT_SD:
            return QString::number(static_cast<int>(val));
        case CFVFMT_UD:
            return QString::number(static_cast<unsigned int>(val));
        case CFVFMT_UH:
        default: {
            int width = size == BITS_8 ? 2 : size == BITS_16 ? 4 : 8;
            return QStringLiteral("%1")
                .arg(static_cast<unsigned int>(val), width, 16, QLatin1Char('0'))
                .toUpper();
        }
    }
}

QString CheatSearchModel::ItemText(int item, int column) const {
    if (addrs.empty() || !cheatSearchData.blocks)
        return QString();

    // allowing GUI to change format after search makes this a little more
    // complicated than necessary...
    int off = 0;

    if (cap_size > size) {
        off = (item & ((1 << (cap_size - size)) - 1)) << size;
        item >>= cap_size - size;
    } else if (cap_size < size) {
        for (size_t i = 0; i < addrs.size(); i++) {
            if (!(addrs[i] & ((1 << size) - 1)) && !item--) {
                item = static_cast<int>(i);
                break;
            }
        }
    }

    if (item < 0 || static_cast<size_t>(item) >= addrs.size())
        return QString();

    CheatSearchBlock* block = &cheatSearchData.blocks[addrs[item] >> 28];
    off += addrs[item] & 0xfffffff;

    switch (column) {
        case 0: {  // address
            if (isgb) {
                int bank = 0;
                int addr = block->offset;

                if (block->offset == 0xa000) {
                    bank = off / 0x2000;
                    addr += off % 0x2000;
                } else if (block->offset == 0xd000) {
                    bank = off / 0x1000;
                    addr += off % 0x1000;
                } else
                    addr += off;

                return QStringLiteral("%1:%2")
                    .arg(static_cast<unsigned int>(bank), 2, 16, QLatin1Char('0'))
                    .arg(static_cast<unsigned int>(addr), 4, 16, QLatin1Char('0'))
                    .toUpper();
            } else {
                return QStringLiteral("%1")
                    .arg(static_cast<unsigned int>(block->offset + off), 8, 16, QLatin1Char('0'))
                    .toUpper();
            }
        }
        case 1:  // old
            return FormatValue(cheatSearchSignedRead(block->saved, off, size));
        case 2:  // new
            return FormatValue(cheatSearchSignedRead(block->data, off, size));
    }
    return QString();
}

// ---- CheatSearch ------------------------------------------------------------

// static
CheatSearch* CheatSearch::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new CheatSearch(parent);
}

CheatSearch::CheatSearch(QWidget* parent) : BaseDialog(parent, "CheatCreate") {
    setWindowTitle(tr("Cheat Search"));
    g_cheat_search = this;

    auto* layout = new QVBoxLayout(this);

    model_ = new CheatSearchModel(this);
    list_ = new QTableView(this);
    list_->setModel(model_);
    list_->setSelectionBehavior(QAbstractItemView::SelectRows);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->verticalHeader()->setVisible(false);
    list_->horizontalHeader()->setStretchLastSection(true);
    list_->setMinimumHeight(200);
    connect(list_, &QTableView::activated, this, [this](const QModelIndex& index) {
        if (index.isValid())
            AddCheat(index.row());
    });
    connect(list_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { add_b_->setEnabled(list_->selectionModel()->hasSelection()); });
    layout->addWidget(list_, 1);

    auto* options = new QHBoxLayout();

    // Compare type
    {
        auto* box = new QGroupBox(tr("Compare type"), this);
        auto* v = new QVBoxLayout(box);
        const char* labels[6] = {"E&qual", "&Not equal", "&Less than", "L&ess or equal",
                                 "&Greater than", "G&reater or equal"};
        for (int i = 0; i < 6; i++) {
            op_rb_[i] = new QRadioButton(tr(labels[i]), box);
            v->addWidget(op_rb_[i]);
        }
        op_rb_[SEARCH_EQ]->setChecked(true);
        options->addWidget(box, 1);
    }

    // Signed / Unsigned + Data size
    {
        auto* col = new QVBoxLayout();
        auto* fbox = new QGroupBox(tr("Signed / Unsigned"), this);
        auto* fv = new QVBoxLayout(fbox);
        const char* flabels[3] = {"S&igned", "&Unsigned", "&Hexadecimal"};
        for (int i = 0; i < 3; i++) {
            fmt_rb_[i] = new QRadioButton(tr(flabels[i]), fbox);
            fv->addWidget(fmt_rb_[i]);
            connect(fmt_rb_[i], &QRadioButton::toggled, this, [this](bool on) {
                if (on)
                    UpdateView();
            });
        }
        fmt_rb_[CFVFMT_SD]->setChecked(true);
        col->addWidget(fbox);

        auto* sbox = new QGroupBox(tr("Data size"), this);
        auto* sv = new QVBoxLayout(sbox);
        const char* slabels[3] = {"&8 bits", "&16 bits", "&32 bits"};
        for (int i = 0; i < 3; i++) {
            size_rb_[i] = new QRadioButton(tr(slabels[i]), sbox);
            sv->addWidget(size_rb_[i]);
            connect(size_rb_[i], &QRadioButton::toggled, this, [this](bool on) {
                if (on)
                    UpdateView();
            });
        }
        size_rb_[BITS_8]->setChecked(true);
        col->addWidget(sbox);
        options->addLayout(col, 1);
    }

    // Search value
    {
        auto* box = new QGroupBox(tr("Search value"), this);
        auto* v = new QVBoxLayout(box);
        old_rb_ = new QRadioButton(tr("Ol&d value"), box);
        val_rb_ = new QRadioButton(tr("Specific &value"), box);
        val_tc_ = new QLineEdit(box);
        val_rb_->setChecked(true);
        old_rb_->setEnabled(false);
        v->addWidget(old_rb_);
        v->addWidget(val_rb_);
        v->addWidget(val_tc_, 1);
        connect(old_rb_, &QRadioButton::toggled, this, [this](bool) { EnableVal(); });
        connect(val_rb_, &QRadioButton::toggled, this, [this](bool) { EnableVal(); });
        options->addWidget(box, 1);
    }

    layout->addLayout(options);

    auto* buttons = new QHBoxLayout();
    auto* search_b = new QPushButton(tr("&Search"), this);
    update_b_ = new QPushButton(tr("U&pdate Old"), this);
    clear_b_ = new QPushButton(tr("&Clear"), this);
    add_b_ = new QPushButton(tr("&Add cheat"), this);
    auto* ok_b = new QPushButton(tr("OK"), this);
    ok_b->setDefault(true);
    update_b_->setEnabled(false);
    clear_b_->setEnabled(false);
    add_b_->setEnabled(false);
    connect(search_b, &QPushButton::clicked, this, &CheatSearch::Search);
    connect(update_b_, &QPushButton::clicked, this, &CheatSearch::UpdateVals);
    connect(clear_b_, &QPushButton::clicked, this, &CheatSearch::ResetSearch);
    connect(add_b_, &QPushButton::clicked, this, &CheatSearch::AddCheatB);
    connect(ok_b, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addStretch(1);
    buttons->addWidget(search_b);
    buttons->addWidget(update_b_);
    buttons->addWidget(clear_b_);
    buttons->addWidget(add_b_);
    buttons->addWidget(ok_b);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    SetValCharset(val_tc_);
}

CheatSearch::~CheatSearch() {
    if (g_cheat_search == this)
        g_cheat_search = nullptr;
    // not that it matters to anyone but mem leak detectors..
    cheatSearchCleanup(&cheatSearchData);
}

CheatAdd* CheatSearch::add_dialog() {
    return static_cast<CheatAdd*>(vbamApp().frame->LoadDialog("CheatAdd"));
}

void CheatSearch::OnDialogShown() {
    GameArea* panel = vbamApp().frame->GetPanel();
    isgb_ = panel->game_type() == IMAGE_GB;
    model_->isgb = isgb_;
    val_tc_->setEnabled(!valsrc_);
    ofmt_ = fmt_;
    SetValCharset(val_tc_);
}

void CheatSearch::ReadControls() {
    // The radio groups' initial setChecked() calls in the constructor fire
    // toggled() before the later controls exist; nothing to read yet then.
    if (!val_tc_ || !old_rb_ || !model_)
        return;
    for (int i = 0; i < 6; i++)
        if (op_rb_[i]->isChecked())
            op_ = i;
    for (int i = 0; i < 3; i++)
        if (size_rb_[i]->isChecked())
            size_ = i;
    for (int i = 0; i < 3; i++)
        if (fmt_rb_[i]->isChecked())
            fmt_ = i;
    valsrc_ = old_rb_->isChecked() ? 1 : 0;
    val_s_ = val_tc_->text();
    model_->size = size_;
    model_->fmt = fmt_;
}

void CheatSearch::SetValCharset(QLineEdit* tc) {
    const char* allowed = kValSigDigits;
    switch (fmt_) {
        case CFVFMT_SD:
            allowed = kValSigDigits;
            break;
        case CFVFMT_UD:
            allowed = kValUnsDigits;
            break;
        case CFVFMT_UH:
            allowed = kValHexDigits;
            break;
    }
    const QString pattern =
        QStringLiteral("[%1]*").arg(QRegularExpression::escape(QLatin1String(allowed)));
    tc->setValidator(new QRegularExpressionValidator(QRegularExpression(pattern), tc));
}

uint32_t CheatSearch::GetValue(const QString& s, int _fmt) const {
    // FIXME: probably ought to throw an error if the conversion fails or val
    // is out of range
    bool ok = false;
    long long val = s.toLongLong(&ok, _fmt == CFVFMT_UH ? 16 : 10);
    if (!ok)
        val = 0;

    if (size_ != BITS_32)
        val &= size_ == BITS_8 ? 0xff : 0xffff;

    return static_cast<uint32_t>(val);
}

int32_t CheatSearch::SignedValue(const QString& s, int _fmt) const {
    int32_t val = GetValue(s, _fmt);

    if (fmt_ == CFVFMT_SD) {
        if (size_ == BITS_8)
            val = (int32_t)(int8_t)val;
        else if (size_ == BITS_16)
            val = (int32_t)(int16_t)val;
    }

    return val;
}

void CheatSearch::Search() {
    ReadControls();

    if (!valsrc_ && val_s_.isEmpty()) {
        vbam::LogError(tr("Number cannot be empty"));
        return;
    }

    if (!cheatSearchData.count)
        ResetSearch();

    if (valsrc_)
        cheatSearch(&cheatSearchData, op_, size_, fmt_ == CFVFMT_SD);
    else
        cheatSearchValue(&cheatSearchData, op_, size_, fmt_ == CFVFMT_SD, SignedValue(val_s_, fmt_));

    Deselect();
    model_->addrs.clear();
    model_->count8 = model_->count16 = model_->count32 = 0;
    model_->cap_size = size_;

    for (int i = 0; i < cheatSearchData.count; i++) {
        CheatSearchBlock* block = &cheatSearchData.blocks[i];

        for (int j = 0; j < block->size; j += (1 << size_)) {
            if (IS_BIT_SET(block->bits, j)) {
                model_->addrs.push_back((i << 28) + j);

                if (!(j & 1))
                    model_->count16++;

                if (!(j & 3))
                    model_->count32++;
            }
        }
    }

    if (model_->addrs.empty()) {
        vbam::LogError(tr("Search produced no results"));
        // no point in keeping empty search results around
        ResetSearch();

        if (old_rb_->isChecked()) {
            val_rb_->setChecked(true);
            val_tc_->setEnabled(true);
        }

        old_rb_->setEnabled(false);
        update_b_->setEnabled(false);
        clear_b_->setEnabled(false);
    } else {
        switch (size_) {
            case BITS_32:
                model_->count16 = model_->count32 * 2;
                // fall through
            case BITS_16:
                model_->count8 = model_->count16 * 2;
                break;
            case BITS_8:
                model_->count8 = static_cast<int>(model_->addrs.size());
        }

        old_rb_->setEnabled(true);
        update_b_->setEnabled(true);
        clear_b_->setEnabled(true);
    }

    model_->ResetRows(static_cast<int>(model_->addrs.size()));
    list_->resizeColumnToContents(0);
}

void CheatSearch::UpdateVals() {
    if (cheatSearchData.count) {
        cheatSearchUpdateValues(&cheatSearchData);

        if (model_->count8)
            model_->Refresh();

        update_b_->setEnabled(false);
    }
}

void CheatSearch::ResetSearch() {
    if (!cheatSearchData.count) {
        CheatSearchBlock* block = cheatSearchData.blocks;

        if (isgb_) {
            block->offset = 0xa000;

            if (gbRam)
                block->data = gbRam;
            else
                block->data = &gbMemory[0xa000];

            block->saved = (uint8_t*)malloc(g_gbCartData.ram_size());
            block->size = g_gbCartData.ram_size();
            block->bits = (uint8_t*)malloc(g_gbCartData.ram_size() >> 3);

            if (gbCgbMode) {
                block++;
                block->offset = 0xc000;
                block->data = &gbMemory[0xc000];
                block->saved = (uint8_t*)malloc(0x1000);
                block->size = 0x1000;
                block->bits = (uint8_t*)malloc(0x1000 >> 3);
                block++;
                block->offset = 0xd000;
                block->data = gbWram;
                block->saved = (uint8_t*)malloc(0x8000);
                block->size = 0x8000;
                block->bits = (uint8_t*)malloc(0x8000 >> 3);
            } else {
                block++;
                block->offset = 0xc000;
                block->data = &gbMemory[0xc000];
                block->saved = (uint8_t*)malloc(0x2000);
                block->size = 0x2000;
                block->bits = (uint8_t*)malloc(0x2000 >> 3);
            }
        } else {
            block->size = 0x40000;
            block->offset = 0x2000000;
            block->bits = (uint8_t*)malloc(0x40000 >> 3);
            block->data = g_workRAM;
            block->saved = (uint8_t*)malloc(0x40000);
            block++;
            block->size = 0x8000;
            block->offset = 0x3000000;
            block->bits = (uint8_t*)malloc(0x8000 >> 3);
            block->data = g_internalRAM;
            block->saved = (uint8_t*)malloc(0x8000);
        }

        cheatSearchData.count = (int)((block + 1) - cheatSearchData.blocks);
    }

    cheatSearchStart(&cheatSearchData);

    if (model_->count8) {
        Deselect();
        model_->count8 = model_->count16 = model_->count32 = 0;
        model_->addrs.clear();
        model_->ResetRows(0);

        if (old_rb_->isChecked()) {
            val_rb_->setChecked(true);
            val_tc_->setEnabled(true);
        }

        old_rb_->setEnabled(false);
        update_b_->setEnabled(false);
        clear_b_->setEnabled(false);
    }
}

void CheatSearch::Deselect() {
    list_->clearSelection();
    add_b_->setEnabled(false);
}

void CheatSearch::AddCheatB() {
    const QModelIndexList rows = list_->selectionModel()->selectedRows();
    if (!rows.isEmpty())
        AddCheat(rows.first().row());
}

void CheatSearch::AddCheat(int idx) {
    ReadControls();

    QString addr_s = model_->ItemText(idx, 0);
    QString s;

    switch (size_) {
        case BITS_8:
            s = tr("8-bit ");
            break;
        case BITS_16:
            s = tr("16-bit ");
            break;
        case BITS_32:
            s = tr("32-bit ");
            break;
    }

    switch (fmt_) {
        case CFVFMT_SD:
            s += tr("Signed decimal");
            break;
        case CFVFMT_UD:
            s += tr("Unsigned decimal");
            break;
        case CFVFMT_UH:
            s += tr("Unsigned hexadecimal");
            break;
    }

    CheatAdd* subdlg = add_dialog();
    subdlg->SetAddress(addr_s);
    subdlg->SetFormat(s);
    subdlg->SetDescription(ca_desc_);
    // probably pointless (but inoffensive) to suggest a value
    subdlg->SetValue(model_->ItemText(idx, 2));  // suggest "New" value
    switch (fmt_) {
        case CFVFMT_SD:
            subdlg->SetValueCharset(QLatin1String(kValSigDigits));
            break;
        case CFVFMT_UD:
            subdlg->SetValueCharset(QLatin1String(kValUnsDigits));
            break;
        case CFVFMT_UH:
            subdlg->SetValueCharset(QLatin1String(kValHexDigits));
            break;
    }

    if (subdlg->exec() != QDialog::Accepted)
        return;

    ca_desc_ = subdlg->description();
    const QString ca_val = subdlg->value();

    if (ca_val.isEmpty()) {
        vbam::LogError(tr("Number cannot be empty"));
        return;
    }

    uint32_t val = GetValue(ca_val, fmt_);
    const QByteArray desc8 = ca_desc_.toUtf8();

    if (isgb_) {
        long bank = addr_s.left(2).toLong(nullptr, 16);
        long addr = addr_s.mid(3).toLong(nullptr, 16);

        if (addr >= 0xd000)
            bank += 0x90;
        else
            bank = 1;

        for (int i = 0; i < (1 << size_); i++) {
            const QString code = QStringLiteral("%1%2%3%4")
                                     .arg(static_cast<unsigned int>(bank), 2, 16, QLatin1Char('0'))
                                     .arg(static_cast<unsigned int>(val & 0xff), 2, 16, QLatin1Char('0'))
                                     .arg(static_cast<unsigned int>(addr & 0xff), 2, 16, QLatin1Char('0'))
                                     .arg(static_cast<unsigned int>(addr >> 8), 2, 16, QLatin1Char('0'))
                                     .toUpper();
            gbAddGsCheat(code.toUtf8().constData(), desc8.constData());
            val >>= 8;
            addr++;
        }
    } else {
        int width = size_ == BITS_8 ? 2 : size_ == BITS_16 ? 4 : 8;
        addr_s.append(QStringLiteral(":%1").arg(val, width, 16, QLatin1Char('0')).toUpper());
        cheatsAddCheatCode(addr_s.toUtf8().constData(), desc8.constData());
    }

    // Cheats added via the cheat-search dialog must also mark the list dirty
    // so UnloadGame()'s auto-save picks them up.
    if (GameArea* p = vbamApp().frame->GetPanel())
        p->cheats_dirty = true;
}

void CheatSearch::UpdateView() {
    ReadControls();

    if (ofmt_ != fmt_ && !val_s_.isEmpty()) {
        int32_t val = GetValue(val_s_, ofmt_);

        switch (fmt_) {
            case CFVFMT_SD:
                switch (size_) {
                    case BITS_8:
                        val = (int32_t)(int8_t)val;
                        break;
                    case BITS_16:
                        val = (int32_t)(int16_t)val;
                }
                val_s_ = QString::number(static_cast<int>(val));
                break;
            case CFVFMT_UD:
                val_s_ = QString::number(static_cast<unsigned int>(val));
                break;
            case CFVFMT_UH:
                val_s_ = QString::number(static_cast<unsigned int>(val), 16);
                break;
        }

        // Swap the validator before setting the text so the new value is not
        // rejected by the old charset.
        SetValCharset(val_tc_);
        val_tc_->setText(val_s_);
    } else if (ofmt_ != fmt_) {
        SetValCharset(val_tc_);
    }

    if (model_->count8 && osize_ != size_) {
        switch (size_) {
            case BITS_32:
                model_->ResetRows(model_->count32);
                break;
            case BITS_16:
                model_->ResetRows(model_->count16);
                break;
            case BITS_8:
                model_->ResetRows(model_->count8);
                break;
        }
    } else if (ofmt_ != fmt_) {
        model_->Refresh();
    }

    ofmt_ = fmt_;
    osize_ = size_;
}

void CheatSearch::EnableVal() {
    val_tc_->setEnabled(val_rb_->isChecked());
}

void CheatSearch::Reset() {
    fmt_ = size_ = op_ = valsrc_ = 0;
    ofmt_ = osize_ = 0;
    val_s_.clear();
    ca_desc_.clear();
    val_tc_->clear();
    op_rb_[SEARCH_EQ]->setChecked(true);
    size_rb_[BITS_8]->setChecked(true);
    fmt_rb_[CFVFMT_SD]->setChecked(true);
    val_rb_->setChecked(true);
    val_tc_->setEnabled(true);
    old_rb_->setEnabled(false);
    update_b_->setEnabled(false);
    clear_b_->setEnabled(false);
    Deselect();
    model_->count8 = model_->count16 = model_->count32 = 0;
    model_->addrs.clear();
    model_->ResetRows(0);
}

}  // namespace dialogs

// clear cheat find dialog between games
void MainWindow::ResetCheatSearch() {
    cheatSearchCleanup(&cheatSearchData);

    // Only touch the dialog if it has been created.
    if (dialogs::g_cheat_search)
        dialogs::g_cheat_search->Reset();
}

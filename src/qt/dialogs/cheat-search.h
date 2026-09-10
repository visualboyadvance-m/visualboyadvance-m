#ifndef VBAM_QT_DIALOGS_CHEAT_SEARCH_H_
#define VBAM_QT_DIALOGS_CHEAT_SEARCH_H_

#include <cstdint>
#include <vector>

#include <QAbstractTableModel>
#include <QString>

#include "qt/dialogs/base-dialog.h"

class QLineEdit;
class QPushButton;
class QRadioButton;
class QTableView;

namespace dialogs {

class CheatAdd;

// Value display format of the cheat search.
enum cf_vfmt {
    CFVFMT_SD,  // signed decimal
    CFVFMT_UD,  // unsigned decimal
    CFVFMT_UH   // unsigned hexadecimal
};

// Read-only model over the search results (addresses matching the last
// search). Equivalent of the wx port's virtual CheatListCtrl: the rows are
// computed from `addrs` and the current size/format, so the view can display
// hundreds of thousands of results.
class CheatSearchModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit CheatSearchModel(QObject* parent = nullptr);

    std::vector<uint32_t> addrs;  // (block << 28) | offset
    int cap_size = 0;             // size in effect when addrs were generated
    int count8 = 0, count16 = 0, count32 = 0;  // aligned address counts

    // Current display parameters (owned by the dialog).
    int size = 0;   // BITS_8 / BITS_16 / BITS_32
    int fmt = 0;    // cf_vfmt
    bool isgb = false;
    int row_count = 0;

    // Text of the given row/column, as the wx OnGetItemText.
    QString ItemText(int item, int column) const;
    void ResetRows(int rows);
    void Refresh();

    // QAbstractTableModel implementation.
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // Formats `val` in the current fmt/size.
    QString FormatValue(int32_t val) const;
};

// The cheat search dialog ("Create cheat..."). Equivalent of the wx port's
// CheatFind_t + CheatCreate.xrc. Search state lives in the core's
// cheatSearchData and survives closing the dialog; MainWindow::
// ResetCheatSearch() (implemented in cheat-search.cpp) clears it when a game
// is unloaded.
class CheatSearch : public BaseDialog {
    Q_OBJECT

public:
    static CheatSearch* NewInstance(QWidget* parent);
    ~CheatSearch() override;

    // Clears everything between games.
    void Reset();

protected:
    void OnDialogShown() override;

private:
    explicit CheatSearch(QWidget* parent);

    void ReadControls();
    void Search();
    void UpdateVals();
    void ResetSearch();
    void Deselect();
    void AddCheatB();
    void AddCheat(int idx);
    void UpdateView();
    void EnableVal();
    void SetValCharset(QLineEdit* tc);

    uint32_t GetValue(const QString& s, int _fmt) const;
    int32_t SignedValue(const QString& s, int _fmt) const;

    CheatAdd* add_dialog();

    int valsrc_ = 0, size_ = 0, op_ = 0, fmt_ = 0;
    int ofmt_ = 0, osize_ = 0;
    QString val_s_;
    bool isgb_ = false;
    QString ca_desc_;

    CheatSearchModel* model_ = nullptr;
    QTableView* list_;
    QRadioButton* op_rb_[6] = {};
    QRadioButton* size_rb_[3] = {};
    QRadioButton* fmt_rb_[3] = {};
    QRadioButton* old_rb_ = nullptr;
    QRadioButton* val_rb_ = nullptr;
    QLineEdit* val_tc_ = nullptr;
    QPushButton* update_b_;
    QPushButton* clear_b_;
    QPushButton* add_b_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_CHEAT_SEARCH_H_

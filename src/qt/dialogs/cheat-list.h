#ifndef VBAM_QT_DIALOGS_CHEAT_LIST_H_
#define VBAM_QT_DIALOGS_CHEAT_LIST_H_

#include <QString>
#include <QTableWidget>

#include "qt/dialogs/base-dialog.h"

class QAction;

namespace dialogs {

class CheatEdit;

// The cheat list: shows the core's cheat table (GB or GBA depending on the
// loaded game) with an enable checkbox per entry, and offers add / edit /
// remove / remove all / toggle all and load / save (.clt, and .cht import).
//
// Equivalent of the wx port's CheatList_t + CheatList.xrc. The dialog is
// re-synchronized with the core on every show.
class CheatList : public BaseDialog {
    Q_OBJECT

public:
    static CheatList* NewInstance(QWidget* parent);
    ~CheatList() override = default;

    // Adds the codes in `codes` (whitespace separated) as cheats of `type`
    // with description `desc`, exactly like the wx port: GB uses `type` (0 =
    // Game Shark, 1 = Game Genie); GBA infers the format from the code shape
    // and uses `type` only for ambiguous single tokens. Public so the cheat
    // search dialog and command handlers can share it.
    static void AddCheat(bool isgb, int type, const QString& desc, const QString& codes);

    // Parses one "name=ADDR,VAL[,VAL...][;ADDR,...]" Flashcart CHT line into
    // GBA generic codes. `desc` is the [section] name.
    static void ParseChtLine(const QString& desc, const QString& tok);

    // Loads a .cht cheat file (Flashcart format) into the GBA cheat table.
    static void LoadChtFile(const QString& path);

protected:
    void OnDialogShown() override;

private:
    explicit CheatList(QWidget* parent);

    void Reload();
    void Reload(int start);
    void AddRow(int index, const QString& code, const QString& desc, bool enabled);

    void OnOpen();
    void OnSave();
    void OnAdd();
    void OnRemove();
    void OnClear();
    void OnToggleAll();
    void OnItemChanged(QTableWidgetItem* item);
    void OnItemActivated(QTableWidgetItem* item);
    void EditCheat(int id);

    CheatEdit* edit_dialog();

    QTableWidget* list_;
    QAction* remove_action_;

    QString cheatdir_, cheatfn_, deffn_;
    bool isgb_ = false;
    bool* dirty_ = nullptr;
    bool reloading_ = false;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_CHEAT_LIST_H_

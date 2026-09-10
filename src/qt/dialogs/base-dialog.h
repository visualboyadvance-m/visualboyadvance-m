#ifndef VBAM_QT_DIALOGS_BASE_DIALOG_H_
#define VBAM_QT_DIALOGS_BASE_DIALOG_H_

#include <memory>

#include <QDialog>
#include <QDialogButtonBox>
#include <QString>

#include "qt/config/option-observer.h"
#include "qt/widgets/option-binding.h"

class QDialogButtonBox;

namespace dialogs {

// Base class for every dialog of the Qt frontend.
//
// - Remembers and restores its geometry under [Geometry/<name>] in the INI.
// - Honors the "keep on top" display option (Qt::WindowStaysOnTopHint follows
//   kDispKeepOnTop while the dialog is visible).
// - Owns a widgets::OptionBindings set: the widgets bound through it are
//   loaded from the options every time the dialog is shown (dialogs are
//   cached and reused) and written back to the options on accept(), matching
//   the wx port's validator TransferData semantics. A failing Save() (an
//   invalid value) keeps the dialog open.
//
// `name` is the dialog's identity for geometry persistence and for
// MainWindow::LoadDialog(); it matches the wx XRC name (e.g. "DisplayConfig").
class BaseDialog : public QDialog {
    Q_OBJECT

public:
    ~BaseDialog() override;

    const QString& name() const { return name_; }

    // QDialog overrides.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void accept() override;

protected:
    BaseDialog(QWidget* parent, const QString& name);

    // Called from showEvent() after the bindings have been (re)loaded. The
    // default does nothing.
    virtual void OnDialogShown() {}
    // Called from hideEvent(). The default does nothing.
    virtual void OnDialogHidden() {}
    // Called from accept() after the bindings have been saved successfully,
    // before the dialog closes. Return false to keep the dialog open.
    virtual bool OnAccept() { return true; }

    // The option bindings of this dialog (see widgets/option-binding.h).
    widgets::OptionBindings& bindings() { return bindings_; }

    // Creates the standard OK / Cancel button box wired to accept()/reject().
    QDialogButtonBox* CreateOkCancel();

private:
    void RepositionDialog();
    void SaveGeometry();
    void ApplyKeepOnTop();

    const QString name_;
    bool dialog_shown_ = false;
    bool geometry_restored_ = false;
    widgets::OptionBindings bindings_;
    std::unique_ptr<config::OptionsObserver> keep_on_top_observer_;
};

}  // namespace dialogs

#endif  // VBAM_QT_DIALOGS_BASE_DIALOG_H_

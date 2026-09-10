#ifndef VBAM_QT_WIDGETS_OPTION_BINDING_H_
#define VBAM_QT_WIDGETS_OPTION_BINDING_H_

// Widget <-> config::Option bindings. Replaces the wx port's validators
// (src/wx/widgets/option-validator.h): a dialog collects bindings in an
// OptionBindings object; Load() writes the option values into the widgets
// (called when the dialog is shown) and Save() writes the widget values back
// into the options (called when the user clicks OK). This is the wx
// TransferDataToWindow / TransferDataFromWindow model: nothing is applied
// until OK, and Cancel discards the edits.
//
// Widgets whose change must apply immediately (the wx "ApplyLive" cases) call
// the option setters directly from their change signals; the dialog then
// snapshots and restores the options on Cancel.

#include <functional>
#include <vector>

#include <QString>
#include <QStringList>
#include <QWidget>

#include "qt/config/option-id.h"
#include "qt/config/option.h"

class QAbstractButton;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;

namespace widgets {

// A directory / file picker: a line edit plus a "Browse..." button.
class PathPicker final : public QWidget {
    Q_OBJECT

public:
    // `dir` selects a directory picker; otherwise a file picker using `filter`
    // (a QFileDialog name filter).
    explicit PathPicker(QWidget* parent, bool dir, const QString& filter = QString());
    ~PathPicker() override;

    QString path() const;
    // Sets the path without emitting pathChanged().
    void SetPath(const QString& path);

    QLineEdit* line_edit() const { return edit_; }
    QPushButton* browse_button() const { return browse_; }

Q_SIGNALS:
    // Emitted when the user picks a path (browse dialog or editing finished).
    void pathChanged(const QString& path);

private:
    void OnBrowse();

    const bool dir_;
    const QString filter_;
    QLineEdit* edit_;
    QPushButton* browse_;
};

// A set of widget <-> option bindings owned by a dialog.
class OptionBindings final {
public:
    OptionBindings() = default;
    ~OptionBindings() = default;

    OptionBindings(const OptionBindings&) = delete;
    OptionBindings& operator=(const OptionBindings&) = delete;

    // Writes the option values to the widgets.
    void Load();
    // Writes the widget values to the options. Returns false (after reporting
    // to the user) if a value was invalid; the options already written stay.
    bool Save();
    // Removes every binding.
    void Clear() { bindings_.clear(); }

    // Generic binding.
    void Add(std::function<void()> load, std::function<bool()> save);

    // kBool option <-> checkable button.
    void BindCheckBox(QAbstractButton* button, config::OptionID id);
    // kInt option bit(s): checked when (value & mask) != 0.
    void BindCheckBoxIntMask(QAbstractButton* button, config::OptionID id, int mask);
    // kUnsigned / kInt option equal to `value` when checked (radio buttons or
    // "group" check boxes). Only a checked button writes the option.
    void BindButtonSelected(QAbstractButton* button, config::OptionID id, int value);
    // kInt / kUnsigned option <-> spin box. Sets the range from the option.
    void BindSpinBox(QSpinBox* spin, config::OptionID id);
    // kDouble option <-> double spin box. Sets the range from the option.
    void BindDoubleSpinBox(QDoubleSpinBox* spin, config::OptionID id);
    // kInt / kUnsigned option <-> slider. Sets the range from the option.
    void BindSlider(QSlider* slider, config::OptionID id);
    // kString option <-> line edit.
    void BindLineEdit(QLineEdit* edit, config::OptionID id);
    // kString option <-> path picker.
    void BindPathPicker(PathPicker* picker, config::OptionID id);
    // Enum option (Filter, Interframe, RenderMethod, ColorCorrectionProfile,
    // AudioApi, AudioRate) <-> combo box. Populates the combo with the
    // translated labels of every value (see *Labels() below).
    void BindComboBoxEnum(QComboBox* combo, config::OptionID id);
    // kInt / kUnsigned option <-> combo box where the current index is the
    // value. The combo must already be populated.
    void BindComboBoxInt(QComboBox* combo, config::OptionID id);
    // kString option <-> combo box whose item data (QString) is the value. The
    // first item is selected when the option matches none.
    void BindComboBoxString(QComboBox* combo, config::OptionID id);
    // kInt / kUnsigned option <-> exclusive buttons where the index in
    // `buttons` is the value.
    void BindRadioButtons(const std::vector<QAbstractButton*>& buttons, config::OptionID id);

    // Plain variable bindings (for gopts / coreOptions fields without an
    // Option).
    void BindSpinBoxInt(QSpinBox* spin, int* value);
    void BindSpinBoxUnsigned(QSpinBox* spin, unsigned* value);
    void BindComboBoxIndex(QComboBox* combo, int* value);
    void BindPathPickerString(PathPicker* picker, QString* value);

private:
    struct Binding {
        std::function<void()> load;
        std::function<bool()> save;
    };
    std::vector<Binding> bindings_;
};

// Translated labels of the enum option values, in enum order.
QStringList FilterLabels();
QStringList InterframeLabels();
QStringList RenderMethodLabels();
QStringList ColorCorrectionProfileLabels();
QStringList AudioApiLabels();
QStringList AudioRateLabels();
// Labels for an enum-typed option (dispatches on the option's type).
QStringList EnumLabelsForOption(config::OptionID id);

// Puts a read-only value label next to `slider`, updated as the thumb moves,
// and (when `reset_to_default` is set) a "Default" button that runs the
// callback. Returns a container widget holding the slider, to be put in the
// layout in place of the slider. `suffix` is appended to the number ("%").
QWidget* WrapSliderWithValueLabel(QSlider* slider,
                                  const QString& suffix = QString(),
                                  std::function<void()> reset_to_default = nullptr);

// Sets `slider` to the compiled-in default of the kInt / kUnsigned option.
void ResetSliderToOptionDefault(QSlider* slider, config::OptionID id);

}  // namespace widgets

#endif  // VBAM_QT_WIDGETS_OPTION_BINDING_H_

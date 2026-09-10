#include "qt/widgets/option-binding.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "qt/log.h"

namespace widgets {

namespace {

QString Tr(const char* s) {
    return QCoreApplication::translate("widgets::OptionBindings", s);
}

// True for the option types the integer binders (radio groups, index combos,
// spin boxes, bit-mask check boxes) can drive: the numeric ones and every enum,
// whose underlying value is the enumerator index.
bool IsIntLike(const config::Option* option) {
    switch (option->type()) {
        case config::Option::Type::kInt:
        case config::Option::Type::kUnsigned:
        case config::Option::Type::kFilter:
        case config::Option::Type::kInterframe:
        case config::Option::Type::kRenderMethod:
        case config::Option::Type::kColorCorrectionProfile:
        case config::Option::Type::kAudioApi:
        case config::Option::Type::kAudioRate:
            return true;
        default:
            return false;
    }
}

int OptionIntValue(const config::Option* option) {
    switch (option->type()) {
        case config::Option::Type::kUnsigned:
            return static_cast<int>(option->GetUnsigned());
        case config::Option::Type::kFilter:
            return static_cast<int>(option->GetFilter());
        case config::Option::Type::kInterframe:
            return static_cast<int>(option->GetInterframe());
        case config::Option::Type::kRenderMethod:
            return static_cast<int>(option->GetRenderMethod());
        case config::Option::Type::kColorCorrectionProfile:
            return static_cast<int>(option->GetColorCorrectionProfile());
        case config::Option::Type::kAudioApi:
            return static_cast<int>(option->GetAudioApi());
        case config::Option::Type::kAudioRate:
            return static_cast<int>(option->GetAudioRate());
        default:
            return option->GetInt();
    }
}

bool OptionSetIntValue(config::Option* option, int value) {
    switch (option->type()) {
        case config::Option::Type::kUnsigned:
            if (value < 0) {
                return false;
            }
            return option->SetUnsigned(static_cast<uint32_t>(value));
        case config::Option::Type::kFilter:
        case config::Option::Type::kInterframe:
        case config::Option::Type::kRenderMethod:
        case config::Option::Type::kColorCorrectionProfile:
        case config::Option::Type::kAudioApi:
        case config::Option::Type::kAudioRate:
            if (value < 0 || static_cast<size_t>(value) >= option->GetEnumMax()) {
                return false;
            }
            switch (option->type()) {
                case config::Option::Type::kFilter:
                    return option->SetFilter(static_cast<config::Filter>(value));
                case config::Option::Type::kInterframe:
                    return option->SetInterframe(static_cast<config::Interframe>(value));
                case config::Option::Type::kRenderMethod:
                    return option->SetRenderMethod(static_cast<config::RenderMethod>(value));
                case config::Option::Type::kColorCorrectionProfile:
                    return option->SetColorCorrectionProfile(
                        static_cast<config::ColorCorrectionProfile>(value));
                case config::Option::Type::kAudioApi:
                    return option->SetAudioApi(static_cast<config::AudioApi>(value));
                default:
                    return option->SetAudioRate(static_cast<config::AudioRate>(value));
            }
        default:
            return option->SetInt(value);
    }
}

int OptionIntMin(const config::Option* option) {
    if (option->is_unsigned()) {
        return static_cast<int>(option->GetUnsignedMin());
    }
    if (option->is_int()) {
        return option->GetIntMin();
    }
    return 0;
}

int OptionIntMax(const config::Option* option) {
    if (option->is_unsigned()) {
        return static_cast<int>(option->GetUnsignedMax());
    }
    if (option->is_int()) {
        return option->GetIntMax();
    }
    return static_cast<int>(option->GetEnumMax()) - 1;
}

}  // namespace

// PathPicker -----------------------------------------------------------------

PathPicker::PathPicker(QWidget* parent, bool dir, const QString& filter)
    : QWidget(parent), dir_(dir), filter_(filter) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    edit_ = new QLineEdit(this);
    browse_ = new QPushButton(Tr("Browse..."), this);
    layout->addWidget(edit_, 1);
    layout->addWidget(browse_);
    connect(browse_, &QPushButton::clicked, this, &PathPicker::OnBrowse);
    connect(edit_, &QLineEdit::editingFinished, this,
            [this] { Q_EMIT pathChanged(edit_->text()); });
}

PathPicker::~PathPicker() = default;

QString PathPicker::path() const {
    return edit_->text();
}

void PathPicker::SetPath(const QString& path) {
    edit_->setText(path);
}

void PathPicker::OnBrowse() {
    QString result;
    if (dir_) {
        result = QFileDialog::getExistingDirectory(this, Tr("Select a directory"), edit_->text());
    } else {
        result = QFileDialog::getOpenFileName(this, Tr("Select a file"), edit_->text(), filter_);
    }
    if (result.isEmpty()) {
        return;
    }
    edit_->setText(result);
    Q_EMIT pathChanged(result);
}

// OptionBindings -------------------------------------------------------------

void OptionBindings::Load() {
    for (const auto& binding : bindings_) {
        binding.load();
    }
}

bool OptionBindings::Save() {
    bool ok = true;
    for (const auto& binding : bindings_) {
        if (!binding.save()) {
            ok = false;
        }
    }
    return ok;
}

void OptionBindings::Add(std::function<void()> load, std::function<bool()> save) {
    bindings_.push_back({std::move(load), std::move(save)});
}

void OptionBindings::BindCheckBox(QAbstractButton* button, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(option->is_bool());
    button->setCheckable(true);
    Add([button, option] { button->setChecked(option->GetBool()); },
        [button, option] { return option->SetBool(button->isChecked()); });
}

void OptionBindings::BindCheckBoxIntMask(QAbstractButton* button, config::OptionID id, int mask) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(IsIntLike(option));
    button->setCheckable(true);
    Add([button, option, mask] { button->setChecked((OptionIntValue(option) & mask) != 0); },
        [button, option, mask] {
            int value = OptionIntValue(option);
            if (button->isChecked()) {
                value |= mask;
            } else {
                value &= ~mask;
            }
            return OptionSetIntValue(option, value);
        });
}

void OptionBindings::BindButtonSelected(QAbstractButton* button, config::OptionID id, int value) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(IsIntLike(option));
    button->setCheckable(true);
    Add([button, option, value] { button->setChecked(OptionIntValue(option) == value); },
        [button, option, value] {
            if (button->isChecked()) {
                return OptionSetIntValue(option, value);
            }
            return true;
        });
}

void OptionBindings::BindSpinBox(QSpinBox* spin, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(IsIntLike(option));
    spin->setRange(OptionIntMin(option), OptionIntMax(option));
    Add([spin, option] { spin->setValue(OptionIntValue(option)); },
        [spin, option] {
            spin->interpretText();
            return OptionSetIntValue(option, spin->value());
        });
}

void OptionBindings::BindDoubleSpinBox(QDoubleSpinBox* spin, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(option->is_double());
    spin->setRange(option->GetDoubleMin(), option->GetDoubleMax());
    Add([spin, option] { spin->setValue(option->GetDouble()); },
        [spin, option] {
            spin->interpretText();
            return option->SetDouble(spin->value());
        });
}

void OptionBindings::BindSlider(QSlider* slider, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(IsIntLike(option));
    slider->setRange(OptionIntMin(option), OptionIntMax(option));
    Add([slider, option] { slider->setValue(OptionIntValue(option)); },
        [slider, option] { return OptionSetIntValue(option, slider->value()); });
}

void OptionBindings::BindLineEdit(QLineEdit* edit, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(option->is_string());
    Add([edit, option] { edit->setText(option->GetString()); },
        [edit, option] { return option->SetString(edit->text()); });
}

void OptionBindings::BindPathPicker(PathPicker* picker, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(option->is_string());
    Add([picker, option] { picker->SetPath(option->GetString()); },
        [picker, option] { return option->SetString(picker->path()); });
}

void OptionBindings::BindComboBoxEnum(QComboBox* combo, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    const QStringList labels = EnumLabelsForOption(id);
    VBAM_CHECK(!labels.isEmpty());
    combo->clear();
    combo->addItems(labels);
    Add([combo, option] {
            int index = 0;
            switch (option->type()) {
                case config::Option::Type::kFilter:
                    index = static_cast<int>(option->GetFilter());
                    break;
                case config::Option::Type::kInterframe:
                    index = static_cast<int>(option->GetInterframe());
                    break;
                case config::Option::Type::kRenderMethod:
                    index = static_cast<int>(option->GetRenderMethod());
                    break;
                case config::Option::Type::kColorCorrectionProfile:
                    index = static_cast<int>(option->GetColorCorrectionProfile());
                    break;
                case config::Option::Type::kAudioApi:
                    index = static_cast<int>(option->GetAudioApi());
                    break;
                case config::Option::Type::kAudioRate:
                    index = static_cast<int>(option->GetAudioRate());
                    break;
                default:
                    VBAM_NOTREACHED();
            }
            if (index >= 0 && index < combo->count()) {
                combo->setCurrentIndex(index);
            }
        },
        [combo, option] {
            const int index = combo->currentIndex();
            if (index < 0 || static_cast<size_t>(index) >= option->GetEnumMax()) {
                return false;
            }
            switch (option->type()) {
                case config::Option::Type::kFilter:
                    return option->SetFilter(static_cast<config::Filter>(index));
                case config::Option::Type::kInterframe:
                    return option->SetInterframe(static_cast<config::Interframe>(index));
                case config::Option::Type::kRenderMethod:
                    return option->SetRenderMethod(static_cast<config::RenderMethod>(index));
                case config::Option::Type::kColorCorrectionProfile:
                    return option->SetColorCorrectionProfile(
                        static_cast<config::ColorCorrectionProfile>(index));
                case config::Option::Type::kAudioApi:
                    return option->SetAudioApi(static_cast<config::AudioApi>(index));
                case config::Option::Type::kAudioRate:
                    return option->SetAudioRate(static_cast<config::AudioRate>(index));
                default:
                    VBAM_NOTREACHED_RETURN(false);
            }
        });
}

void OptionBindings::BindComboBoxInt(QComboBox* combo, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(IsIntLike(option));
    Add([combo, option] {
            const int value = OptionIntValue(option);
            if (value >= 0 && value < combo->count()) {
                combo->setCurrentIndex(value);
            }
        },
        [combo, option] {
            const int index = combo->currentIndex();
            if (index < 0) {
                return false;
            }
            return OptionSetIntValue(option, index);
        });
}

void OptionBindings::BindComboBoxString(QComboBox* combo, config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(option->is_string());
    Add([combo, option] {
            const int index = combo->findData(option->GetString());
            combo->setCurrentIndex(index >= 0 ? index : (combo->count() > 0 ? 0 : -1));
        },
        [combo, option] {
            const int index = combo->currentIndex();
            if (index < 0) {
                return option->SetString(QString());
            }
            return option->SetString(combo->itemData(index).toString());
        });
}

void OptionBindings::BindRadioButtons(const std::vector<QAbstractButton*>& buttons,
                                      config::OptionID id) {
    config::Option* option = config::Option::ByID(id);
    VBAM_CHECK(IsIntLike(option));
    Add([buttons, option] {
            const int value = OptionIntValue(option);
            for (size_t i = 0; i < buttons.size(); i++) {
                buttons[i]->setChecked(static_cast<int>(i) == value);
            }
        },
        [buttons, option] {
            for (size_t i = 0; i < buttons.size(); i++) {
                if (buttons[i]->isChecked()) {
                    return OptionSetIntValue(option, static_cast<int>(i));
                }
            }
            return true;
        });
}

void OptionBindings::BindSpinBoxInt(QSpinBox* spin, int* value) {
    VBAM_CHECK(value);
    Add([spin, value] { spin->setValue(*value); },
        [spin, value] {
            spin->interpretText();
            *value = spin->value();
            return true;
        });
}

void OptionBindings::BindSpinBoxUnsigned(QSpinBox* spin, unsigned* value) {
    VBAM_CHECK(value);
    Add([spin, value] { spin->setValue(static_cast<int>(*value)); },
        [spin, value] {
            spin->interpretText();
            *value = static_cast<unsigned>(std::max(0, spin->value()));
            return true;
        });
}

void OptionBindings::BindComboBoxIndex(QComboBox* combo, int* value) {
    VBAM_CHECK(value);
    Add([combo, value] {
            if (*value >= 0 && *value < combo->count()) {
                combo->setCurrentIndex(*value);
            }
        },
        [combo, value] {
            if (combo->currentIndex() < 0) {
                return false;
            }
            *value = combo->currentIndex();
            return true;
        });
}

void OptionBindings::BindPathPickerString(PathPicker* picker, QString* value) {
    VBAM_CHECK(value);
    Add([picker, value] { picker->SetPath(*value); },
        [picker, value] {
            *value = picker->path();
            return true;
        });
}

// Enum labels ----------------------------------------------------------------

QStringList FilterLabels() {
    // The order must match config::Filter.
    return {
        Tr("None"),
        Tr("Super 2xSaI"),
        Tr("Super Eagle"),
        Tr("Pixelate"),
        Tr("Advance MAME Scale 2x"),
        Tr("Bilinear Plus"),
        Tr("Scanlines"),
        Tr("TV Mode"),
        QStringLiteral("HQ 4x"),
        QStringLiteral("LQ 2x"),
        Tr("Simple 4x"),
        QStringLiteral("xBRZ 2x"),
        QStringLiteral("xBRZ 6x"),
        QStringLiteral("xBRZ 9x"),
        QStringLiteral("ScaleFX 3x"),
        QStringLiteral("ScaleFX 9x"),
        Tr("Plugin"),
    };
}

QStringList InterframeLabels() {
    return {Tr("None"), Tr("Smart"), Tr("Motion Blur")};
}

QStringList RenderMethodLabels() {
    QStringList labels;
    for (size_t i = 0; i < config::kNbRenderMethods; i++) {
        switch (static_cast<config::RenderMethod>(i)) {
            case config::RenderMethod::kSimple:
                labels << Tr("Simple");
                break;
            case config::RenderMethod::kOpenGL:
                labels << QStringLiteral("OpenGL");
                break;
            case config::RenderMethod::kSDL:
                labels << QStringLiteral("SDL");
                break;
#if defined(_WIN32)
#if !defined(NO_D3D12)
            case config::RenderMethod::kDirect3d12:
                labels << QStringLiteral("Direct3D 12");
                break;
#endif
#if !defined(NO_D3D)
            case config::RenderMethod::kDirect3d:
                labels << QStringLiteral("Direct3D 9");
                break;
#endif
#elif defined(__APPLE__)
            case config::RenderMethod::kQuartz2d:
                labels << QStringLiteral("Quartz 2D");
                break;
#ifndef NO_METAL
            case config::RenderMethod::kMetal:
                labels << QStringLiteral("Metal");
                break;
#endif
#endif
#ifndef NO_VULKAN
            case config::RenderMethod::kVulkan:
                labels << QStringLiteral("Vulkan");
                break;
#endif
            case config::RenderMethod::kLast:
                break;
        }
    }
    return labels;
}

QStringList ColorCorrectionProfileLabels() {
    return {QStringLiteral("sRGB"), QStringLiteral("DCI"), QStringLiteral("Rec2020")};
}

QStringList AudioApiLabels() {
    QStringList labels;
    for (size_t i = 0; i < config::kNbAudioApis; i++) {
        const auto api = static_cast<config::AudioApi>(i);
        switch (api) {
#if defined(VBAM_ENABLE_OPENAL)
            case config::AudioApi::kOpenAL:
                labels << QStringLiteral("OpenAL");
                break;
#endif
            case config::AudioApi::kSDL:
                labels << QStringLiteral("SDL");
                break;
#if defined(_WIN32)
            case config::AudioApi::kDirectSound:
                labels << Tr("Direct Sound");
                break;
#endif
#if defined(VBAM_ENABLE_XAUDIO2)
            case config::AudioApi::kXAudio2:
                labels << QStringLiteral("XAudio2");
                break;
#endif
#if defined(VBAM_ENABLE_FAUDIO)
            case config::AudioApi::kFAudio:
                labels << QStringLiteral("FAudio");
                break;
#endif
#if defined(__APPLE__)
            case config::AudioApi::kCoreAudio:
                labels << QStringLiteral("CoreAudio");
                break;
#endif
#if defined(VBAM_ENABLE_AAUDIO)
            case config::AudioApi::kAAudio:
                labels << QStringLiteral("AAudio");
                break;
#endif
            case config::AudioApi::kNull:
                labels << Tr("No sound (timer paced)");
                break;
            case config::AudioApi::kLast:
                break;
        }
    }
    return labels;
}

QStringList AudioRateLabels() {
    return {Tr("48 KHz"), Tr("44.1 KHz"), Tr("22 KHz"), Tr("11 KHz")};
}

QStringList EnumLabelsForOption(config::OptionID id) {
    const config::Option* option = config::Option::ByID(id);
    switch (option->type()) {
        case config::Option::Type::kFilter:
            return FilterLabels();
        case config::Option::Type::kInterframe:
            return InterframeLabels();
        case config::Option::Type::kRenderMethod:
            return RenderMethodLabels();
        case config::Option::Type::kColorCorrectionProfile:
            return ColorCorrectionProfileLabels();
        case config::Option::Type::kAudioApi:
            return AudioApiLabels();
        case config::Option::Type::kAudioRate:
            return AudioRateLabels();
        default:
            return {};
    }
}

// Slider helpers -------------------------------------------------------------

QWidget* WrapSliderWithValueLabel(QSlider* slider,
                                  const QString& suffix,
                                  std::function<void()> reset_to_default) {
    auto* container = new QWidget(slider->parentWidget());
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    slider->setParent(container);
    layout->addWidget(slider, 1);

    auto* label = new QLabel(container);
    label->setMinimumWidth(label->fontMetrics().horizontalAdvance(QStringLiteral("10000%")));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto update_label = [label, slider, suffix](int value) {
        label->setText(QString::number(value) + suffix);
    };
    update_label(slider->value());
    QObject::connect(slider, &QSlider::valueChanged, container, update_label);
    layout->addWidget(label);

    if (reset_to_default) {
        auto* button = new QPushButton(Tr("Default"), container);
        button->setAutoDefault(false);
        QObject::connect(button, &QPushButton::clicked, container,
                         [reset_to_default] { reset_to_default(); });
        layout->addWidget(button);
    }
    return container;
}

void ResetSliderToOptionDefault(QSlider* slider, config::OptionID id) {
    const config::Option* option = config::Option::ByID(id);
    if (option->is_unsigned()) {
        slider->setValue(static_cast<int>(option->GetUnsignedDefault()));
    } else {
        slider->setValue(option->GetIntDefault());
    }
}

}  // namespace widgets

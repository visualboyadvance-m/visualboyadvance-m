#include "qt/opts.h"

#include <limits>
#include <memory>
#include <unordered_set>

#include <QCoreApplication>
#include <QSettings>
#include <QThread>

#include "core/base/check.h"
#include "qt/app.h"
#include "qt/config/bindings.h"
#include "qt/config/cmdtab.h"
#include "qt/config/command.h"
#include "qt/config/option-observer.h"
#include "qt/config/option-proxy.h"
#include "qt/config/option.h"
#include "qt/config/user-input.h"
#include "qt/log.h"

namespace {

QString Tr(const char* s) {
    return QCoreApplication::translate("vbam", s);
}

QSettings* Config() {
    return vbamApp().config();
}

void SaveOption(config::Option* option) {
    QSettings* cfg = Config();

    switch (option->type()) {
        case config::Option::Type::kNone:
            // Keyboard and Joypad are handled separately.
            break;
        case config::Option::Type::kBool:
            cfg->setValue(option->config_name(), option->GetBool());
            break;
        case config::Option::Type::kDouble:
            cfg->setValue(option->config_name(), option->GetDouble());
            break;
        case config::Option::Type::kInt:
            cfg->setValue(option->config_name(), option->GetInt());
            break;
        case config::Option::Type::kUnsigned:
            cfg->setValue(option->config_name(), option->GetUnsigned());
            break;
        case config::Option::Type::kString:
            cfg->setValue(option->config_name(), option->GetString());
            break;
        case config::Option::Type::kFilter:
        case config::Option::Type::kInterframe:
        case config::Option::Type::kRenderMethod:
        case config::Option::Type::kColorCorrectionProfile:
        case config::Option::Type::kAudioApi:
        case config::Option::Type::kAudioRate:
            cfg->setValue(option->config_name(), option->GetEnumString());
            break;
        case config::Option::Type::kGbPalette:
            cfg->setValue(option->config_name(), option->GetGbPaletteString());
            break;
    }
    cfg->sync();
}

// Intitialize global observers to overwrite the configuration option when the
// option has been modified.
void InitializeOptionObservers() {
    static std::unordered_set<std::unique_ptr<config::OptionsObserver>> g_observers;
    g_observers.reserve(config::kNbOptions);
    for (config::Option& option : config::Option::All()) {
        g_observers.emplace(std::make_unique<config::OptionsObserver>(option.id(), &SaveOption));
    }
}

// Reads an unsigned option, returning `default_value` for a missing or
// unparseable entry (QSettings stores INI values as strings).
uint32_t LoadUnsignedOption(QSettings* cfg, const QString& option_name, uint32_t default_value) {
    if (!cfg->contains(option_name)) {
        return default_value;
    }
    const QString temp = cfg->value(option_name).toString();
    bool ok = false;
    const qulonglong out = temp.toULongLong(&ok);
    if (!ok) {
        return default_value;
    }
    if (out > std::numeric_limits<uint32_t>::max()) {
        return default_value;
    }
    return static_cast<uint32_t>(out);
}

// Reads a boolean option. Accepts the wx spellings ("0"/"1") as well as
// QSettings' own ("true"/"false").
bool LoadBoolOption(QSettings* cfg, const QString& option_name, bool default_value) {
    if (!cfg->contains(option_name)) {
        return default_value;
    }
    const QString temp = cfg->value(option_name).toString().trimmed().toLower();
    if (temp == "1" || temp == "true" || temp == "yes" || temp == "on") {
        return true;
    }
    if (temp == "0" || temp == "false" || temp == "no" || temp == "off") {
        return false;
    }
    return default_value;
}

// Removes every entry of the INI that is not a known option, keyboard shortcut
// or joypad binding, mirroring the wx port's cleanup pass.
void RemoveUnknownEntries(QSettings* cfg) {
    QStringList item_del;
    QStringList grp_del;

    // Root-level keys are never valid.
    for (const QString& key : cfg->childKeys()) {
        item_del.push_back(key);
    }

    for (const QString& group : cfg->childGroups()) {
        // The Recent group holds the file history.
        if (group == "Recent") {
            continue;
        }
        // Per-dialog geometry saved by dialogs::BaseDialog.
        if (group == "DialogGeometry") {
            continue;
        }

        cfg->beginGroup(group);

        for (const QString& sub : cfg->childGroups()) {
            if (group == "Joypad" && sub.size() == 1 && sub[0] >= '1' && sub[0] <= '4') {
                cfg->beginGroup(sub);
                for (const QString& key_group : cfg->childGroups()) {
                    grp_del.push_back(group + '/' + sub + '/' + key_group);
                }
                for (const QString& entry : cfg->childKeys()) {
                    if (!config::StringToGameKey(entry)) {
                        item_del.push_back(group + '/' + sub + '/' + entry);
                    }
                }
                cfg->endGroup();
            } else {
                grp_del.push_back(group + '/' + sub);
            }
        }

        for (const QString& entry : cfg->childKeys()) {
            const QString full = group + '/' + entry;
            if (group == "Keyboard") {
                if (!config::CommandFromConfigString(entry).has_value()) {
                    item_del.push_back(full);
                }
            } else if (!config::Option::ByName(full) && full != "General/LastUpdated" &&
                       full != "General/LastUpdatedFileName") {
                item_del.push_back(full);
            }
        }

        cfg->endGroup();
    }

    for (const QString& item : item_del) {
        cfg->remove(item);
    }
    for (const QString& group : grp_del) {
        cfg->remove(group);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// RecentFiles

void RecentFiles::AddFileToHistory(const QString& path) {
    files_.removeAll(path);
    files_.prepend(path);
    while (files_.size() > kMaxFiles) {
        files_.removeLast();
    }
}

void RecentFiles::RemoveFileFromHistory(int index) {
    if (index >= 0 && index < files_.size()) {
        files_.removeAt(index);
    }
}

void RecentFiles::Clear() {
    files_.clear();
}

// ---------------------------------------------------------------------------

opts_t gopts;

// This constructor only works with globally allocated gopts.
opts_t::opts_t() {}

void load_opts(bool first_time_launch) {
    // just for sanity...
    static bool did_init = false;
    VBAM_CHECK(!did_init);
    did_init = true;

    // Make sure the command table is sorted before any shortcut lookup.
    config::SortCmdTab();

    // enumvals should not be translated, since they would cause config file
    // change after lang change
    // instead, translate when presented to user
    QSettings* cfg = Config();

    // Read the IniVersion now since the Option initialization code will reset
    // it to kIniLatestVersion if it is unset.
    uint32_t ini_version = 0;
    if (first_time_launch) {
        // Just go with the default values for the first time launch.
        ini_version = config::kIniLatestVersion;
    } else {
        // We want to default to 0 if the option is not set.
        ini_version = LoadUnsignedOption(cfg, "General/IniVersion", 0);
        if (ini_version > config::kIniLatestVersion) {
            vbam::LogWarning(
                Tr("The INI file was written for a more recent version of "
                   "VBA-M. Some INI option values may have been reset."));
            ini_version = config::kIniLatestVersion;
        }
    }

    // Ensure there are no unknown options present.
    RemoveUnknownEntries(cfg);

    // now read actual values and set to default if unset
    // config file will be updated with unset options
    for (config::Option& opt : config::Option::All()) {
        switch (opt.type()) {
        case config::Option::Type::kNone:
            // Keyboard or Joystick. Handled separately for now.
            break;
        case config::Option::Type::kBool: {
            const bool temp = LoadBoolOption(cfg, opt.config_name(), opt.GetBool());
            opt.SetBool(temp);
            cfg->setValue(opt.config_name(), opt.GetBool());
            break;
        }
        case config::Option::Type::kDouble: {
            bool ok = false;
            double temp = cfg->value(opt.config_name()).toString().toDouble(&ok);
            if (!cfg->contains(opt.config_name()) || !ok) {
                temp = opt.GetDouble();
            }
            opt.SetDouble(temp);
            cfg->setValue(opt.config_name(), opt.GetDouble());
            break;
        }
        case config::Option::Type::kInt: {
            bool ok = false;
            int32_t temp = cfg->value(opt.config_name()).toString().toInt(&ok);
            if (!cfg->contains(opt.config_name()) || !ok) {
                temp = opt.GetInt();
            }
            opt.SetInt(temp);
            cfg->setValue(opt.config_name(), opt.GetInt());
            break;
        }
        case config::Option::Type::kUnsigned: {
            const uint32_t temp = LoadUnsignedOption(cfg, opt.config_name(), opt.GetUnsigned());
            opt.SetUnsigned(temp);
            cfg->setValue(opt.config_name(), opt.GetUnsigned());
            break;
        }
        case config::Option::Type::kString: {
            const QString temp = cfg->value(opt.config_name(), opt.GetString()).toString();
            opt.SetString(temp);
            cfg->setValue(opt.config_name(), opt.GetString());
            break;
        }
        case config::Option::Type::kFilter:
        case config::Option::Type::kInterframe:
        case config::Option::Type::kRenderMethod:
        case config::Option::Type::kColorCorrectionProfile:
        case config::Option::Type::kAudioApi:
        case config::Option::Type::kAudioRate: {
            const QString temp = cfg->value(opt.config_name()).toString();
            if (!temp.isEmpty()) {
                opt.SetEnumString(temp.toLower());
            }
            // This is necessary, in case the option we loaded was invalid.
            cfg->setValue(opt.config_name(), opt.GetEnumString());
            break;
        }
        case config::Option::Type::kGbPalette: {
            const QString temp =
                cfg->value(opt.config_name(), opt.GetGbPaletteString()).toString();
            opt.SetGbPaletteString(temp);
            cfg->setValue(opt.config_name(), opt.GetGbPaletteString());
            break;
        }
        }
    }

    config::Bindings* const bindings = vbamApp().bindings();

    // Keyboard does not get written with defaults
    for (const cmditem& cmd_item : cmdtab) {
        const QString kbopt = QStringLiteral("Keyboard/") + cmd_item.cmd;
        const QString s = cfg->value(kbopt).toString();
        if (!s.isEmpty()) {
            auto inputs = config::UserInput::FromConfigString(s);
            if (inputs.empty()) {
                vbam::LogWarning(Tr("Invalid key binding %1 for %2").arg(s, kbopt));
            } else {
                for (const auto& input : inputs) {
                    bindings->AssignInputToCommand(input,
                                                   config::ShortcutCommand(cmd_item.cmd_id));
                }
            }
        }
    }

    // Force overwrite the default Joypad configuration.
    for (auto& iter : bindings->GetJoypadConfiguration()) {
        const QString optname = iter.first.ToConfigString();
        if (cfg->contains(optname)) {
            const QString s = cfg->value(optname).toString();
            const auto user_inputs = config::UserInput::FromConfigString(s);
            if (!s.isEmpty() && user_inputs.empty()) {
                vbam::LogWarning(Tr("Invalid key binding %1 for %2").arg(s, optname));
            }
            bindings->AssignInputsToCommand(user_inputs, iter.first);
        } else {
            cfg->setValue(optname, iter.second);
        }
    }

    // recent is special
    // Recent does not get written with defaults
    {
        QStringList files;
        cfg->beginGroup("Recent");
        for (int i = 1; i <= RecentFiles::kMaxFiles; i++) {
            const QString file = cfg->value(QStringLiteral("file%1").arg(i)).toString();
            if (!file.isEmpty()) {
                files.push_back(file);
            }
        }
        cfg->endGroup();
        gopts.recent.SetFiles(files);
    }
    cfg->sync();

    InitializeOptionObservers();

    // We default the MaxThreads option to 0, so set it to the CPU count here.
    config::OptionProxy<config::OptionID::kDispMaxThreads> max_threads;
    if (max_threads == 0) {
        // Handle erroneous thread count values appropriately.
        const int cpu_count = QThread::idealThreadCount();
        if (cpu_count > 256) {
            max_threads = 256;
        } else if (cpu_count < 1) {
            max_threads = 1;
        } else {
            max_threads = cpu_count;
        }
    }

    // Apply Option updates.
    while (ini_version < config::kIniLatestVersion) {
        // Update the ini version as we go in case we fail halfway through.
        OPTION(kGenIniVersion) = ini_version;
        switch (ini_version) {
            case 0: { // up to 2.1.5 included.
#ifndef NO_LINK
                // Previous default was 1.
                if (OPTION(kGBALinkTimeout) == 1) {
                    OPTION(kGBALinkTimeout) = 500;
                }
#endif
                [[fallthrough]];
            }
            case 1: { // up to 2.2.2 included.
                // Previous defaults were false.
                OPTION(kGBALCDFilter) = true;
                OPTION(kGBLCDFilter)  = true;
                [[fallthrough]];
            }
            case 2: { // new default for 2.3.0 and later.
                // Previous default was no filter.
                if (OPTION(kDispFilter) == config::Filter::kNone) {
                    OPTION(kDispFilter) = config::Filter::kXbrz2x;
                }
                // The wx port steers a saved OpenGL choice to a platform
                // renderer here; the Qt port only has Simple and OpenGL, so
                // there is nothing to do.
                [[fallthrough]];
            }
            case 3: { // new default for Android: the GLES renderer (wx only).
                [[fallthrough]];
            }
            case 4: { // new default for the sound buffer count.
                // Each buffer is a frame of audio, so the count is latency:
                // the previous default of 10 was 167ms. Only move a config that
                // still holds that value; an explicit choice is left alone.
                if (OPTION(kSoundBuffers) == 10) {
                    OPTION(kSoundBuffers) = 3;
                }
            }
        }
        ini_version++;
    }

    // Finally, overwrite the value to the current version.
    OPTION(kGenIniVersion) = config::kIniLatestVersion;
}

// Note: run load_opts() first to guarantee all config opts exist
void update_opts() {
    for (config::Option& opt : config::Option::All()) {
        SaveOption(&opt);
    }
}

void update_shortcut_opts() {
    QSettings* cfg = Config();

    // For keyboard shortcuts, it's easier to delete everything and start over.
    cfg->remove("Keyboard");
    for (const auto& iter : vbamApp().bindings()->GetKeyboardConfiguration()) {
        cfg->setValue(iter.first, iter.second);
    }

    update_joypad_opts();
}

void update_joypad_opts() {
    QSettings* cfg = Config();

    // For joypads, we just compare the strings.
    bool game_bindings_changed = false;
    for (const auto& iter : vbamApp().bindings()->GetJoypadConfiguration()) {
        const QString option_name = iter.first.ToConfigString();
        const QString saved_config = cfg->value(option_name, "").toString();
        if (saved_config != iter.second) {
            game_bindings_changed = true;
            cfg->setValue(option_name, iter.second);
        }
    }

    if (game_bindings_changed) {
        vbamApp().emulated_gamepad()->Reset();
    }

    cfg->sync();
}

void opt_set(const QString& name, const QString& val) {
    config::Option* opt = config::Option::ByName(name);

    // opt->is_none() means it is Keyboard or Joypad.
    if (opt && !opt->is_none()) {
        switch (opt->type()) {
        case config::Option::Type::kNone:
            VBAM_NOTREACHED();
            return;
        case config::Option::Type::kBool:
            if (val != "0" && val != "1") {
                vbam::LogWarning(Tr("Invalid value %1 for option %2").arg(val, name));
                return;
            }
            opt->SetBool(val == "1");
            return;
        case config::Option::Type::kDouble: {
            bool ok = false;
            const double value = val.toDouble(&ok);
            if (!ok) {
                vbam::LogWarning(Tr("Invalid value %1 for option %2").arg(val, name));
                return;
            }
            opt->SetDouble(value);
            return;
        }
        case config::Option::Type::kInt: {
            bool ok = false;
            const int value = val.toInt(&ok);
            if (!ok) {
                vbam::LogWarning(Tr("Invalid value %1 for option %2").arg(val, name));
                return;
            }
            opt->SetInt(static_cast<int32_t>(value));
            return;
        }
        case config::Option::Type::kUnsigned: {
            bool ok = false;
            const uint value = val.toUInt(&ok);
            if (!ok) {
                vbam::LogWarning(Tr("Invalid value %1 for option %2").arg(val, name));
                return;
            }
            opt->SetUnsigned(static_cast<uint32_t>(value));
            return;
        }
        case config::Option::Type::kString:
            opt->SetString(val);
            return;
        case config::Option::Type::kFilter:
        case config::Option::Type::kInterframe:
        case config::Option::Type::kRenderMethod:
        case config::Option::Type::kColorCorrectionProfile:
        case config::Option::Type::kAudioApi:
        case config::Option::Type::kAudioRate:
            opt->SetEnumString(val);
            return;
        case config::Option::Type::kGbPalette:
            opt->SetGbPaletteString(val);
            return;
        }
    }

    nonstd::optional<config::Command> command = config::Command::FromString(name);
    if (command) {
        config::Bindings* const bindings = vbamApp().bindings();
        const auto inputs = config::UserInput::FromConfigString(val);
        if (inputs.empty()) {
            vbam::LogWarning(Tr("Invalid key binding %1 for %2").arg(val, name));
        }
        bindings->AssignInputsToCommand(inputs, *command);
        return;
    }

    vbam::LogWarning(Tr("Unknown option %1 with value %2").arg(name, val));
}

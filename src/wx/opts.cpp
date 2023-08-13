#include "opts.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_set>

#include <wx/defs.h>
#include <wx/filehistory.h>
#include <wx/log.h>
#include <wx/xrc/xmlres.h>

#include "config/option-observer.h"
#include "config/option-proxy.h"
#include "config/option.h"
#include "config/user-input.h"
#include "strutils.h"
#include "wxhead.h"
#include "wxvbam.h"

/*
       disableSfx(F) -> cpuDisableSfx
       priority(2) -> threadPriority
       saveMoreCPU(F) -> Sm60FPS

     SDL:
      -p/--profile=hz
*/

namespace {

void SaveOption(config::Option* option) {
    wxConfigBase* cfg = wxConfigBase::Get();

    switch (option->type()) {
        case config::Option::Type::kNone:
            // Keyboard and Joypad are handled separately.
            break;
        case config::Option::Type::kBool:
            cfg->Write(option->config_name(), option->GetBool());
            break;
        case config::Option::Type::kDouble:
            cfg->Write(option->config_name(), option->GetDouble());
            break;
        case config::Option::Type::kInt:
            cfg->Write(option->config_name(), option->GetInt());
            break;
        case config::Option::Type::kUnsigned:
            cfg->Write(option->config_name(), option->GetUnsigned());
            break;
        case config::Option::Type::kString:
            cfg->Write(option->config_name(), option->GetString());
            break;
        case config::Option::Type::kFilter:
        case config::Option::Type::kInterframe:
        case config::Option::Type::kRenderMethod:
        case config::Option::Type::kAudioApi:
        case config::Option::Type::kSoundQuality:
            cfg->Write(option->config_name(), option->GetEnumString());
            break;
        case config::Option::Type::kGbPalette:
            cfg->Write(option->config_name(), option->GetGbPaletteString());
            break;
    }
    cfg->Flush();
}

// Intitialize global observers to overwrite the configuration option when the
// option has been modified.
void InitializeOptionObservers() {
    static std::unordered_set<std::unique_ptr<config::OptionsObserver>>
        g_observers;
    g_observers.reserve(config::kNbOptions);
    for (config::Option& option : config::Option::All()) {
        g_observers.emplace(std::make_unique<config::OptionsObserver>(
            option.id(), &SaveOption));
    }
}

// Helper function to work around wxWidgets limitations when converting string
// to unsigned int.
uint32_t LoadUnsignedOption(wxConfigBase* cfg,
                            const wxString& option_name,
                            uint32_t default_value) {
    wxString temp;
    if (!cfg->Read(option_name, &temp)) {
        return default_value;
    }
    if (!temp.IsNumber()) {
        return default_value;
    }

    // Go through ulonglong to get enough space to work with. Also, older
    // versions do not have a conversion function for unsigned int.
    wxULongLong_t out;
    if (!temp.ToULongLong(&out)) {
        return default_value;
    }

    if (out > std::numeric_limits<uint32_t>::max()) {
        return default_value;
    }

    return out;
}

}  // namespace

#define WJKB config::UserInput

opts_t gopts;

const std::map<config::GameControl, std::set<config::UserInput>> kDefaultBindings = {
    { config::GameControl(0, config::GameKey::Up), {
        WJKB('W'),
        WJKB(11, wxJoyControl::Button, 1),
        WJKB(1, wxJoyControl::AxisMinus, 1),
        WJKB(3, wxJoyControl::AxisMinus, 1),
    }},
    { config::GameControl(0, config::GameKey::Down), {
        WJKB('S'),
        WJKB(12, wxJoyControl::Button, 1),
        WJKB(1, wxJoyControl::AxisPlus, 1),
        WJKB(3, wxJoyControl::AxisPlus, 1),
    }},
    { config::GameControl(0, config::GameKey::Left), {
        WJKB('A'),
        WJKB(13, wxJoyControl::Button, 1),
        WJKB(0, wxJoyControl::AxisMinus, 1),
        WJKB(2, wxJoyControl::AxisMinus, 1),
    }},
    { config::GameControl(0, config::GameKey::Right), {
        WJKB('D'),
        WJKB(14, wxJoyControl::Button, 1),
        WJKB(0, wxJoyControl::AxisPlus, 1),
        WJKB(2, wxJoyControl::AxisPlus, 1),
    }},
    { config::GameControl(0, config::GameKey::A), {
        WJKB('L'),
        WJKB(0, wxJoyControl::Button, 1),
    }},
    { config::GameControl(0, config::GameKey::B), {
        WJKB('K'),
        WJKB(1, wxJoyControl::Button, 1),
    }},
    { config::GameControl(0, config::GameKey::L), {
        WJKB('I'),
        WJKB(2, wxJoyControl::Button, 1),
        WJKB(9, wxJoyControl::Button, 1),
        WJKB(4, wxJoyControl::AxisPlus, 1),
    }},
    { config::GameControl(0, config::GameKey::R), {
        WJKB('O'),
        WJKB(3, wxJoyControl::Button, 1),
        WJKB(10, wxJoyControl::Button, 1),
        WJKB(5, wxJoyControl::AxisPlus, 1),
    }},
    { config::GameControl(0, config::GameKey::Select), {
        WJKB(WXK_BACK),
        WJKB(4, wxJoyControl::Button, 1),
    }},
    { config::GameControl(0, config::GameKey::Start), {
        WJKB(WXK_RETURN),
        WJKB(6, wxJoyControl::Button, 1),
    }},
    { config::GameControl(0, config::GameKey::MotionUp), {}},
    { config::GameControl(0, config::GameKey::MotionDown), {}},
    { config::GameControl(0, config::GameKey::MotionLeft), {}},
    { config::GameControl(0, config::GameKey::MotionRight), {}},
    { config::GameControl(0, config::GameKey::MotionIn), {}},
    { config::GameControl(0, config::GameKey::MotionOut), {}},
    { config::GameControl(0, config::GameKey::AutoA), {}},
    { config::GameControl(0, config::GameKey::AutoB), {}},
    { config::GameControl(0, config::GameKey::Speed), {
            WJKB(WXK_SPACE),
    }},
    { config::GameControl(0, config::GameKey::Capture), {}},
    { config::GameControl(0, config::GameKey::Gameshark), {}},

    { config::GameControl(1, config::GameKey::Up), {}},
    { config::GameControl(1, config::GameKey::Down), {}},
    { config::GameControl(1, config::GameKey::Left), {}},
    { config::GameControl(1, config::GameKey::Right), {}},
    { config::GameControl(1, config::GameKey::A), {}},
    { config::GameControl(1, config::GameKey::B), {}},
    { config::GameControl(1, config::GameKey::L), {}},
    { config::GameControl(1, config::GameKey::R), {}},
    { config::GameControl(1, config::GameKey::Select), {}},
    { config::GameControl(1, config::GameKey::Start), {}},
    { config::GameControl(1, config::GameKey::MotionUp), {}},
    { config::GameControl(1, config::GameKey::MotionDown), {}},
    { config::GameControl(1, config::GameKey::MotionLeft), {}},
    { config::GameControl(1, config::GameKey::MotionRight), {}},
    { config::GameControl(1, config::GameKey::MotionIn), {}},
    { config::GameControl(1, config::GameKey::MotionOut), {}},
    { config::GameControl(1, config::GameKey::AutoA), {}},
    { config::GameControl(1, config::GameKey::AutoB), {}},
    { config::GameControl(1, config::GameKey::Speed), {}},
    { config::GameControl(1, config::GameKey::Capture), {}},
    { config::GameControl(1, config::GameKey::Gameshark), {}},

    { config::GameControl(2, config::GameKey::Up), {}},
    { config::GameControl(2, config::GameKey::Down), {}},
    { config::GameControl(2, config::GameKey::Left), {}},
    { config::GameControl(2, config::GameKey::Right), {}},
    { config::GameControl(2, config::GameKey::A), {}},
    { config::GameControl(2, config::GameKey::B), {}},
    { config::GameControl(2, config::GameKey::L), {}},
    { config::GameControl(2, config::GameKey::R), {}},
    { config::GameControl(2, config::GameKey::Select), {}},
    { config::GameControl(2, config::GameKey::Start), {}},
    { config::GameControl(2, config::GameKey::MotionUp), {}},
    { config::GameControl(2, config::GameKey::MotionDown), {}},
    { config::GameControl(2, config::GameKey::MotionLeft), {}},
    { config::GameControl(2, config::GameKey::MotionRight), {}},
    { config::GameControl(2, config::GameKey::MotionIn), {}},
    { config::GameControl(2, config::GameKey::MotionOut), {}},
    { config::GameControl(2, config::GameKey::AutoA), {}},
    { config::GameControl(2, config::GameKey::AutoB), {}},
    { config::GameControl(2, config::GameKey::Speed), {}},
    { config::GameControl(2, config::GameKey::Capture), {}},
    { config::GameControl(2, config::GameKey::Gameshark), {}},

    { config::GameControl(3, config::GameKey::Up), {}},
    { config::GameControl(3, config::GameKey::Down), {}},
    { config::GameControl(3, config::GameKey::Left), {}},
    { config::GameControl(3, config::GameKey::Right), {}},
    { config::GameControl(3, config::GameKey::A), {}},
    { config::GameControl(3, config::GameKey::B), {}},
    { config::GameControl(3, config::GameKey::L), {}},
    { config::GameControl(3, config::GameKey::R), {}},
    { config::GameControl(3, config::GameKey::Select), {}},
    { config::GameControl(3, config::GameKey::Start), {}},
    { config::GameControl(3, config::GameKey::MotionUp), {}},
    { config::GameControl(3, config::GameKey::MotionDown), {}},
    { config::GameControl(3, config::GameKey::MotionLeft), {}},
    { config::GameControl(3, config::GameKey::MotionRight), {}},
    { config::GameControl(3, config::GameKey::MotionIn), {}},
    { config::GameControl(3, config::GameKey::MotionOut), {}},
    { config::GameControl(3, config::GameKey::AutoA), {}},
    { config::GameControl(3, config::GameKey::AutoB), {}},
    { config::GameControl(3, config::GameKey::Speed), {}},
    { config::GameControl(3, config::GameKey::Capture), {}},
    { config::GameControl(3, config::GameKey::Gameshark), {}},
};

// This constructor only works with globally allocated gopts.
opts_t::opts_t()
{
    recent = new wxFileHistory(10);
}

// FIXME: simulate MakeInstanceFilename(vbam.ini) using subkeys (Slave%d/*)
void load_opts(bool first_time_launch) {
    // just for sanity...
    static bool did_init = false;
    assert(!did_init);
    did_init = true;

    // enumvals should not be translated, since they would cause config file
    // change after lang change
    // instead, translate when presented to user
    wxConfigBase* cfg = wxConfigBase::Get();
    cfg->SetPath("/");

    // enure there are no unknown options present
    // note that items cannot be deleted until after loop or loop will fail
    wxArrayString item_del, grp_del;
    long grp_idx;
    wxString s;
    bool cont;

    for (cont = cfg->GetFirstEntry(s, grp_idx); cont;
         cont = cfg->GetNextEntry(s, grp_idx)) {
        //wxLogWarning(_("Invalid option %s present; removing if possible"), s.c_str());
        item_del.push_back(s);
    }

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
            wxLogWarning(
                _("The INI file was written for a more recent version of "
                  "VBA-M. Some INI option values may have been reset."));
            ini_version = config::kIniLatestVersion;
        }
    }

    for (cont = cfg->GetFirstGroup(s, grp_idx); cont;
         cont = cfg->GetNextGroup(s, grp_idx)) {
        // ignore wxWidgets-managed global library settings
        if (s == wxT("wxWindows"))
            continue;

        // ignore file history
        if (s == wxT("Recent"))
            continue;

        cfg->SetPath(s);
        int poff = s.size();
        long entry_idx;
        wxString e;

        for (cont = cfg->GetFirstGroup(e, entry_idx); cont;
             cont = cfg->GetNextGroup(e, entry_idx)) {
            // the only one with subgroups
            if (s == wxT("Joypad") && e.size() == 1 && e[0] >= wxT('1') && e[0] <= wxT('4')) {
                s.append(wxT('/'));
                s.append(e);
                s.append(wxT('/'));
                int poff2 = s.size();
                cfg->SetPath(e);
                long key_idx;

                for (cont = cfg->GetFirstGroup(e, key_idx); cont;
                     cont = cfg->GetNextGroup(e, key_idx)) {
                    s.append(e);
                    //wxLogWarning(_("Invalid option group %s present; removing if possible"), s.c_str());
                    grp_del.push_back(s);
                    s.resize(poff2);
                }

                for (cont = cfg->GetFirstEntry(e, key_idx); cont;
                     cont = cfg->GetNextEntry(e, key_idx)) {
                    if (!config::StringToGameKey(e)) {
                        s.append(e);
                        //wxLogWarning(_("Invalid option %s present; removing if possible"), s.c_str());
                        item_del.push_back(s);
                        s.resize(poff2);
                    }
                }

                s.resize(poff);
                cfg->SetPath(wxT("/"));
                cfg->SetPath(s);
            } else {
                s.append(wxT('/'));
                s.append(e);
                //wxLogWarning(_("Invalid option group %s present; removing if possible"), s.c_str());
                grp_del.push_back(s);
                s.resize(poff);
            }
        }

        for (cont = cfg->GetFirstEntry(e, entry_idx); cont;
             cont = cfg->GetNextEntry(e, entry_idx)) {
            // kb options come from a different list
            if (s == wxT("Keyboard")) {
                const cmditem dummy = new_cmditem(e);

                if (!std::binary_search(&cmdtab[0], &cmdtab[ncmds], dummy, cmditem_lt)) {
                    s.append(wxT('/'));
                    s.append(e);
                    //wxLogWarning(_("Invalid option %s present; removing if possible"), s.c_str());
                    item_del.push_back(s);
                    s.resize(poff);
                }
            } else {
                s.append(wxT('/'));
                s.append(e);
                if (!config::Option::ByName(s) && s != "General/LastUpdated" &&
                    s != "General/LastUpdatedFileName") {
                    //wxLogWarning(_("Invalid option %s present; removing if possible"), s.c_str());
                    item_del.push_back(s);
                }
                s.resize(poff);
            }
        }

        cfg->SetPath(wxT("/"));
    }

    for (size_t i = 0; i < item_del.size(); i++)
        cfg->DeleteEntry(item_del[i]);

    for (size_t i = 0; i < grp_del.size(); i++)
        cfg->DeleteGroup(grp_del[i]);

    // now read actual values and set to default if unset
    // config file will be updated with unset options
    cfg->SetRecordDefaults();

    // First access here will also initialize translations.
    for (config::Option& opt : config::Option::All()) {
        switch (opt.type()) {
        case config::Option::Type::kNone:
            // Keyboard or Joystick. Handled separately for now.
            break;
        case config::Option::Type::kBool: {
            bool temp;
            cfg->Read(opt.config_name(), &temp, opt.GetBool());
            opt.SetBool(temp);
            break;
        }
        case config::Option::Type::kDouble: {
            double temp;
            cfg->Read(opt.config_name(), &temp, opt.GetDouble());
            opt.SetDouble(temp);
            break;
        }
        case config::Option::Type::kInt: {
            int32_t temp;
            cfg->Read(opt.config_name(), &temp, opt.GetInt());
            opt.SetInt(temp);
            break;
        }
        case config::Option::Type::kUnsigned: {
            uint32_t temp =
                LoadUnsignedOption(cfg, opt.config_name(), opt.GetUnsigned());
            opt.SetUnsigned(temp);
            break;
        }
        case config::Option::Type::kString: {
            wxString temp;
            cfg->Read(opt.config_name(),  &temp, opt.GetString());
            opt.SetString(temp);
            break;
        }
        case config::Option::Type::kFilter:
        case config::Option::Type::kInterframe:
        case config::Option::Type::kRenderMethod:
        case config::Option::Type::kAudioApi:
        case config::Option::Type::kSoundQuality: {
            wxString temp;
            if (cfg->Read(opt.config_name(), &temp) && !temp.empty()) {
                opt.SetEnumString(temp.MakeLower());
            }
            // This is necessary, in case the option we loaded was invalid.
            cfg->Write(opt.config_name(), opt.GetEnumString());
            break;
        }
        case config::Option::Type::kGbPalette: {
            wxString temp;
            cfg->Read(opt.config_name(), &temp, opt.GetGbPaletteString());
            opt.SetGbPaletteString(temp);
            break;
        }
        }
    }

    // Initialize game control bindings to populate the configuration map.
    gopts.game_control_bindings.insert(kDefaultBindings.begin(), kDefaultBindings.end());

    // joypad is special
    for (auto& iter : gopts.game_control_bindings) {
        const wxString optname = iter.first.ToString();
        if (cfg->Read(optname, &s)) {
            iter.second = config::UserInput::FromConfigString(s);
            if (!s.empty() && iter.second.empty()) {
                wxLogWarning(_("Invalid key binding %s for %s"), s.c_str(), optname.c_str());
            }
        } else {
            s = config::UserInput::SpanToConfigString(iter.second);
            cfg->Write(optname, s);
        }
    }

    // keyboard is special
    // Keyboard does not get written with defaults
    wxString kbopt(wxT("Keyboard/"));
    int kboff = kbopt.size();

    for (int i = 0; i < ncmds; i++) {
        kbopt.resize(kboff);
        kbopt.append(cmdtab[i].cmd);

        if (cfg->Read(kbopt, &s) && s.size()) {
            auto inputs = config::UserInput::FromConfigString(s);
            if (inputs.empty()) {
                wxLogWarning(_("Invalid key binding %s for %s"), s.c_str(), kbopt.c_str());
            } else {
                for (const auto& input : inputs) {
                    gopts.shortcuts.AssignInputToCommand(input, cmdtab[i].cmd_id);
                }
            }
        }
    }

    // recent is special
    // Recent does not get written with defaults
    cfg->SetPath(wxT("/Recent"));
    gopts.recent->Load(*cfg);
    cfg->SetPath(wxT("/"));
    cfg->Flush();

    InitializeOptionObservers();

    // We default the MaxThreads option to 0, so set it to the CPU count here.
    config::OptionProxy<config::OptionID::kDispMaxThreads> max_threads;
    if (max_threads == 0) {
        // Handle erroneous thread count values appropriately.
        const int cpu_count = wxThread::GetCPUCount();
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
                // Previous default was true.
                OPTION(kGBALCDFilter) = false;
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

void update_joypad_opts() {
    wxConfigBase* cfg = wxConfigBase::Get();

    // For joypad, compare the UserInput sets. Since UserInput guarantees a
    // certain ordering, it is possible that the user control in the panel shows
    // a different ordering than the one that will be eventually saved, but this
    // is nothing to worry about.
    bool game_bindings_changed = false;
    for (const auto &iter : gopts.game_control_bindings) {
        wxString option_name = iter.first.ToString();
        std::set<config::UserInput> saved_config =
            config::UserInput::FromConfigString(cfg->Read(option_name, ""));
        if (saved_config != iter.second) {
            game_bindings_changed = true;
            cfg->Write(option_name, config::UserInput::SpanToConfigString(iter.second));
        }
    }

    if (game_bindings_changed) {
        config::GameControlState::Instance().OnGameBindingsChanged();
    }

    cfg->SetPath("/");
    cfg->Flush();
}

void update_shortcut_opts() {
    wxConfigBase* cfg = wxConfigBase::Get();

    // For shortcuts, it's easier to delete everything and start over.
    cfg->DeleteGroup("/Keyboard");
    cfg->SetPath("/Keyboard");
    for (const auto& iter : gopts.shortcuts.GetConfiguration()) {
        int cmd = 0;
        for (cmd = 0; cmd < ncmds; cmd++)
            if (cmdtab[cmd].cmd_id == iter.first)
                break;
        if (cmd == ncmds) {
            // Command not found. This should never happen.
            assert(false);
            continue;
        }

        cfg->Write(cmdtab[cmd].cmd, iter.second);
    }

    cfg->SetPath("/");
    cfg->Flush();
}

void opt_set(const wxString& name, const wxString& val) {
    config::Option* opt = config::Option::ByName(name);

    // opt->is_none() means it is Keyboard or Joypad.
    if (opt && !opt->is_none()) {
        switch (opt->type()) {
        case config::Option::Type::kNone:
            // This should never happen.
            assert(false);
            return;
        case config::Option::Type::kBool:
            if (val != '0' && val != '1') {
                wxLogWarning(_("Invalid value %s for option %s"),
                    name.c_str(), val.c_str());
                return;
            }
            opt->SetBool(val == '1');
            return;
        case config::Option::Type::kDouble: {
            double value;
            if (!val.ToDouble(&value)) {
                wxLogWarning(_("Invalid value %s for option %s"), val, name.c_str());
                return;
            }
            opt->SetDouble(value);
            return;
        }
        case config::Option::Type::kInt: {
            long value;
            if (!val.ToLong(&value)) {
                wxLogWarning(_("Invalid value %s for option %s"), val, name.c_str());
                return;
            }
            opt->SetInt(static_cast<int32_t>(value));
            return;
        }
        case config::Option::Type::kUnsigned: {
            unsigned long value;
            if (!val.ToULong(&value)) {
                wxLogWarning(_("Invalid value %s for option %s"), val, name.c_str());
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
        case config::Option::Type::kAudioApi:
        case config::Option::Type::kSoundQuality:
            opt->SetEnumString(val);
            return;
        case config::Option::Type::kGbPalette:
            opt->SetGbPaletteString(val);
            return;
        }
    }

    if (name.Find(wxT('/')) == wxNOT_FOUND) {
        return;
    }

    auto parts = strutils::split(name, wxT("/"));
    if (parts[0] == wxT("Keyboard")) {
        cmditem* cmd = std::lower_bound(&cmdtab[0], &cmdtab[ncmds], cmditem{parts[1],wxString(),0,0,NULL}, cmditem_lt);

        if (cmd == &cmdtab[ncmds] || wxStrcmp(parts[1], cmd->cmd)) {
            return;
        }

        if (!val.empty()) {
            const auto inputs = config::UserInput::FromConfigString(val);
            if (inputs.empty()) {
                wxLogWarning(_("Invalid key binding %s for %s"), val.c_str(), name.c_str());
            }
            for (const auto& input : inputs) {
                gopts.shortcuts.AssignInputToCommand(input, cmd->cmd_id);
            }
        }

        return;
    }

    const nonstd::optional<config::GameControl> game_control =
        config::GameControl::FromString(name);
    if (game_control) {
        if (val.empty()) {
            gopts.game_control_bindings[game_control.value()].clear();
        } else {
            gopts.game_control_bindings[game_control.value()] =
                config::UserInput::FromConfigString(val);
        }
        return;
    }

    wxLogWarning(_("Unknown option %s with value %s"), name.c_str(), val.c_str());
}

#include "opts.h"

#include <algorithm>
#include <memory>
#include <unordered_set>

#include <wx/log.h>
#include <wx/display.h>

#include "config/option-observer.h"
#include "config/option.h"
#include "strutils.h"
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
    wxFileConfig* cfg = wxGetApp().cfg;

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

}  // namespace

#define WJKB config::UserInput

opts_t gopts;

// having the standard menu accels here means they will work even without menus
const wxAcceleratorEntryUnicode default_accels[] = {
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('C'), XRCID("CheatsList")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('N'), XRCID("NextFrame")),
    // some ports add ctrl-q anyway, so may as well make it official
    // maybe make alt-f4 universal as well...
    // FIXME: ctrl-Q does not work on wxMSW
    // FIXME: esc does not work on wxMSW

    // this was annoying people A LOT #334
    //wxAcceleratorEntry(wxMOD_NONE, WXK_ESCAPE, wxID_EXIT),

    // this was annoying people #298
    //wxAcceleratorEntry(wxMOD_CMD, wxT('X'), wxID_EXIT),

    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('Q'), wxID_EXIT),
    // FIXME: ctrl-W does not work on wxMSW
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('W'), wxID_CLOSE),
    // load most recent is more commonly used than load other
    //wxAcceleratorEntry(wxMOD_CMD, wxT('L'), XRCID("Load")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('L'), XRCID("LoadGameRecent")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F1, XRCID("LoadGame01")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F2, XRCID("LoadGame02")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F3, XRCID("LoadGame03")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F4, XRCID("LoadGame04")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F5, XRCID("LoadGame05")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F6, XRCID("LoadGame06")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F7, XRCID("LoadGame07")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F8, XRCID("LoadGame08")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F9, XRCID("LoadGame09")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_F10, XRCID("LoadGame10")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_PAUSE, XRCID("Pause")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('P'), XRCID("Pause")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('R'), XRCID("Reset")),
    // add shortcuts for original size multiplier #415
    wxAcceleratorEntryUnicode(wxMOD_NONE, wxT('1'), XRCID("SetSize1x")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, wxT('2'), XRCID("SetSize2x")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, wxT('3'), XRCID("SetSize3x")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, wxT('4'), XRCID("SetSize4x")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, wxT('5'), XRCID("SetSize5x")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, wxT('6'), XRCID("SetSize6x")),
    // save oldest is more commonly used than save other
    //wxAcceleratorEntry(wxMOD_CMD, wxT('S'), XRCID("Save")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('S'), XRCID("SaveGameOldest")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F1, XRCID("SaveGame01")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F2, XRCID("SaveGame02")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F3, XRCID("SaveGame03")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F4, XRCID("SaveGame04")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F5, XRCID("SaveGame05")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F6, XRCID("SaveGame06")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F7, XRCID("SaveGame07")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F8, XRCID("SaveGame08")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F9, XRCID("SaveGame09")),
    wxAcceleratorEntryUnicode(wxMOD_SHIFT, WXK_F10, XRCID("SaveGame10")),
    // I prefer the SDL ESC key binding
    //wxAcceleratorEntry(wxMOD_NONE, WXK_ESCAPE, XRCID("ToggleFullscreen"),
    // alt-enter is more standard anyway
    wxAcceleratorEntryUnicode(wxMOD_ALT, WXK_RETURN, XRCID("ToggleFullscreen")),
    wxAcceleratorEntryUnicode(wxMOD_ALT, wxT('1'), XRCID("JoypadAutofireA")),
    wxAcceleratorEntryUnicode(wxMOD_ALT, wxT('2'), XRCID("JoypadAutofireB")),
    wxAcceleratorEntryUnicode(wxMOD_ALT, wxT('3'), XRCID("JoypadAutofireL")),
    wxAcceleratorEntryUnicode(wxMOD_ALT, wxT('4'), XRCID("JoypadAutofireR")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('1'), XRCID("VideoLayersBG0")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('2'), XRCID("VideoLayersBG1")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('3'), XRCID("VideoLayersBG2")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('4'), XRCID("VideoLayersBG3")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('5'), XRCID("VideoLayersOBJ")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('6'), XRCID("VideoLayersWIN0")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('7'), XRCID("VideoLayersWIN1")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('8'), XRCID("VideoLayersOBJWIN")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('B'), XRCID("Rewind")),
    // following are not in standard menus
    // FILExx are filled in when recent menu is filled
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F1, wxID_FILE1),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F2, wxID_FILE2),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F3, wxID_FILE3),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F4, wxID_FILE4),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F5, wxID_FILE5),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F6, wxID_FILE6),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F7, wxID_FILE7),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F8, wxID_FILE8),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F9, wxID_FILE9),
    wxAcceleratorEntryUnicode(wxMOD_CMD, WXK_F10, wxID_FILE10),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('0'), XRCID("VideoLayersReset")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('G'), XRCID("ChangeFilter")),
    wxAcceleratorEntryUnicode(wxMOD_CMD, wxT('I'), XRCID("ChangeIFB")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_NUMPAD_ADD, XRCID("IncreaseVolume")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_NUMPAD_SUBTRACT, XRCID("DecreaseVolume")),
    wxAcceleratorEntryUnicode(wxMOD_NONE, WXK_NUMPAD_ENTER, XRCID("ToggleSound"))
};
const int num_def_accels = sizeof(default_accels) / sizeof(default_accels[0]);

const std::map<config::GameControl, std::set<config::UserInput>> kDefaultBindings = {
    { config::GameControl(0, config::GameKey::Up), {
        WJKB(wxT('W')),
        WJKB(11, wxJoyControl::Button, 1),
        WJKB(1, wxJoyControl::AxisMinus, 1),
        WJKB(3, wxJoyControl::AxisMinus, 1),
    }},
    { config::GameControl(0, config::GameKey::Down), {
        WJKB(wxT('S')),
        WJKB(12, wxJoyControl::Button, 1),
        WJKB(1, wxJoyControl::AxisPlus, 1),
        WJKB(3, wxJoyControl::AxisPlus, 1),
    }},
    { config::GameControl(0, config::GameKey::Left), {
        WJKB(wxT('A')),
        WJKB(13, wxJoyControl::Button, 1),
        WJKB(0, wxJoyControl::AxisMinus, 1),
        WJKB(2, wxJoyControl::AxisMinus, 1),
    }},
    { config::GameControl(0, config::GameKey::Right), {
        WJKB(wxT('D')),
        WJKB(14, wxJoyControl::Button, 1),
        WJKB(0, wxJoyControl::AxisPlus, 1),
        WJKB(2, wxJoyControl::AxisPlus, 1),
    }},
    { config::GameControl(0, config::GameKey::A), {
        WJKB(wxT('L')),
        WJKB(0, wxJoyControl::Button, 1),
    }},
    { config::GameControl(0, config::GameKey::B), {
        WJKB(wxT('K')),
        WJKB(1, wxJoyControl::Button, 1),
    }},
    { config::GameControl(0, config::GameKey::L), {
        WJKB(wxT('I')),
        WJKB(2, wxJoyControl::Button, 1),
        WJKB(9, wxJoyControl::Button, 1),
        WJKB(4, wxJoyControl::AxisPlus, 1),
    }},
    { config::GameControl(0, config::GameKey::R), {
        WJKB(wxT('O')),
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

wxAcceleratorEntry_v sys_accels;

// This constructor only works with globally allocated gopts.  It relies on
// the default value of every non-object to be 0.
opts_t::opts_t()
{
    frameSkip = -1;
    audio_api = AUD_SDL;

    retain_aspect = true;
    max_threads = wxThread::GetCPUCount();

    // handle erroneous thread count values appropriately
    if (max_threads > 256)
        max_threads = 256;

    if (max_threads < 1)
        max_threads = 1;

    // 10 fixes stuttering on mac with openal, as opposed to 5
    // also should be better for modern hardware in general
    audio_buffers = 10;

    sound_en = 0x30f;
    sound_vol = 100;
    sound_qual = 1;
    gb_echo = 20;
    gb_stereo = 15;
    gb_declick = true;
    gba_sound_filter = 50;
    bilinear = true;
    default_stick = 1;

    recent = new wxFileHistory(10);
    autofire_rate = 1;
    print_auto_page = true;
    autoPatch = true;
    // quick fix for issues #48 and #445
    link_host = "127.0.0.1";
    server_ip = "*";
    link_port = 5738;

    hide_menu_bar = true;

    skipSaveGameBattery = true;
}

// FIXME: simulate MakeInstanceFilename(vbam.ini) using subkeys (Slave%d/*)
void load_opts() {
    // just for sanity...
    static bool did_init = false;
    assert(!did_init);
    did_init = true;

    // enumvals should not be translated, since they would cause config file
    // change after lang change
    // instead, translate when presented to user
    wxFileConfig* cfg = wxGetApp().cfg;
    cfg->SetPath(wxT("/"));
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

    // Date of last online update check;
    gopts.last_update = cfg->Read(wxT("General/LastUpdated"), (long)0);
    cfg->Read(wxT("General/LastUpdatedFileName"), &gopts.last_updated_filename);

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
            int temp;
            cfg->Read(opt.config_name(), &temp, opt.GetUnsigned());
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
            opt.SetGbPalette(temp);
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
            iter.second = config::UserInput::FromString(s);
            if (!s.empty() && iter.second.empty()) {
                wxLogWarning(_("Invalid key binding %s for %s"), s.c_str(), optname.c_str());
            }
        } else {
            s = config::UserInput::SpanToString(iter.second);
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
            wxAcceleratorEntry_v val = wxJoyKeyTextCtrl::ToAccelFromString(s);

            if (!val.size())
                wxLogWarning(_("Invalid key binding %s for %s"), s.c_str(), kbopt.c_str());
            else {
                for (size_t j = 0; j < val.size(); j++)
                    val[j].Set(val[j].GetUkey(), val[j].GetJoystick(), val[j].GetFlags(), val[j].GetKeyCode(), cmdtab[i].cmd_id);

                gopts.accels.insert(gopts.accels.end(), val.begin(), val.end());
            }
        }
    }

    // Make sure linkTimeout is not set to 1, which was the previous default.
    if (linkTimeout <= 1)
        linkTimeout = 500;

    // recent is special
    // Recent does not get written with defaults
    cfg->SetPath(wxT("/Recent"));
    gopts.recent->Load(*cfg);
    cfg->SetPath(wxT("/"));
    cfg->Flush();

    InitializeOptionObservers();
}

// Note: run load_opts() first to guarantee all config opts exist
void update_opts()
{
    wxFileConfig* cfg = wxGetApp().cfg;

    for (config::Option& opt : config::Option::All()) {
        SaveOption(&opt);
    }

    // For joypad, compare the UserInput sets. Since UserInput guarantees a
    // certain ordering, it is possible that the user control in the panel shows
    // a different ordering than the one that will be eventually saved, but this
    // is nothing to worry about.
    bool game_bindings_changed = false;
    for (auto &iter : gopts.game_control_bindings) {
        wxString option_name = iter.first.ToString();
        std::set<config::UserInput> saved_config =
            config::UserInput::FromString(cfg->Read(option_name, ""));
        if (saved_config != iter.second) {
            game_bindings_changed = true;
            cfg->Write(option_name, config::UserInput::SpanToString(iter.second));
        }
    }
    if (game_bindings_changed) {
        config::GameControlState::Instance().OnGameBindingsChanged();
    }

    // for keyboard, first remove any commands that aren't bound at all
    if (cfg->HasGroup(wxT("/Keyboard"))) {
        cfg->SetPath(wxT("/Keyboard"));
        wxString s;
        long entry_idx;
        wxArrayString item_del;

        for (bool cont = cfg->GetFirstEntry(s, entry_idx); cont;
             cont = cfg->GetNextEntry(s, entry_idx)) {
            const cmditem dummy = new_cmditem(s);
            cmditem* cmd = std::lower_bound(&cmdtab[0], &cmdtab[ncmds], dummy, cmditem_lt);
            size_t i;

            for (i = 0; i < gopts.accels.size(); i++)
                if (gopts.accels[i].GetCommand() == cmd->cmd_id)
                    break;

            if (i == gopts.accels.size())
                item_del.push_back(s);
        }

        for (size_t i = 0; i < item_del.size(); i++)
            cfg->DeleteEntry(item_del[i]);
    }

    // then, add/update the commands that are bound
    // even if only ordering changed, a write will be triggered.
    // nothing to worry about...
    if (gopts.accels.size())
        cfg->SetPath(wxT("/Keyboard"));

    int cmd_id = -1;
    for (wxAcceleratorEntry_v::iterator i = gopts.accels.begin();
         i < gopts.accels.end(); ++i) {
        if (cmd_id == i->GetCommand()) continue;
        cmd_id = i->GetCommand();
        int cmd;

        for (cmd = 0; cmd < ncmds; cmd++)
            if (cmdtab[cmd].cmd_id == cmd_id)
                break;

        // NOOP overwrittes commands removed by the user
        wxString command = cmdtab[cmd].cmd;
        if (cmdtab[cmd].cmd_id == XRCID("NOOP"))
            command = wxT("NOOP");

        wxAcceleratorEntry_v::iterator j;

        for (j = i + 1; j < gopts.accels.end(); ++j)
            if (j->GetCommand() != cmd_id)
                break;

        wxAcceleratorEntry_v nv(i, j);
        wxString nvs = wxJoyKeyTextCtrl::FromAccelToString(nv, wxT(','), true);

        if (nvs != cfg->Read(command))
            cfg->Write(command, nvs);
    }

    cfg->SetPath(wxT("/"));
    // recent items are updated separately
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
            opt->SetGbPalette(val);
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

        for (auto i = gopts.accels.begin(); i < gopts.accels.end(); ++i)
            if (i->GetCommand() == cmd->cmd_id) {
                auto j = i;
                for (; j < gopts.accels.end(); ++j)
                    if (j->GetCommand() != cmd->cmd_id)
                        break;
                gopts.accels.erase(i, j);
                break;
            }

        if (!val.empty()) {
            auto aval = wxJoyKeyTextCtrl::ToAccelFromString(val);
            for (size_t i = 0; i < aval.size(); i++) {
                aval[i].Set(aval[i].GetUkey(), aval[i].GetJoystick(), aval[i].GetFlags(), aval[i].GetKeyCode(), cmd->cmd_id);
            }
            if (!aval.size()) {
                wxLogWarning(_("Invalid key binding %s for %s"), val.c_str(), name.c_str());
                return;
            }

            gopts.accels.insert(gopts.accels.end(), aval.begin(), aval.end());
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
                config::UserInput::FromString(val);
        }
        return;
    }

    wxLogWarning(_("Unknown option %s with value %s"), name.c_str(), val.c_str());
}

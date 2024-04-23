#include "wx/config/shortcuts.h"
#include "wx/config/user-input.h"

#include <wx/xrc/xmlres.h>

#define VBAM_SHORTCUTS_INTERNAL_INCLUDE
#include "wx/config/internal/shortcuts-internal.h"
#undef VBAM_SHORTCUTS_INTERNAL_INCLUDE

namespace config {
namespace internal {

const std::unordered_map<int, UserInput>& DefaultShortcuts() {
    static const std::unordered_map<int, UserInput> kDefaultShortcuts = {
        {XRCID("CheatsList"), KeyboardInput('C', wxMOD_CMD)},
        {XRCID("NextFrame"), KeyboardInput('N', wxMOD_CMD)},
        // this was annoying people A LOT #334
        //{wxID_EXIT, KeyboardInput(WXK_ESCAPE, wxMOD_NONE)},
        // this was annoying people #298
        //{wxID_EXIT, KeyboardInput('X', wxMOD_CMD)},

        {wxID_EXIT, KeyboardInput('Q', wxMOD_CMD)},
        {wxID_CLOSE, KeyboardInput('W', wxMOD_CMD)},
        // load most recent is more commonly used than load state
        // {XRCID("Load"), KeyboardInput('L', wxMOD_CMD)},
        {XRCID("LoadGameRecent"), KeyboardInput('L', wxMOD_CMD)},
        {XRCID("LoadGame01"), KeyboardInput(WXK_F1, wxMOD_NONE)},
        {XRCID("LoadGame02"), KeyboardInput(WXK_F2, wxMOD_NONE)},
        {XRCID("LoadGame03"), KeyboardInput(WXK_F3, wxMOD_NONE)},
        {XRCID("LoadGame04"), KeyboardInput(WXK_F4, wxMOD_NONE)},
        {XRCID("LoadGame05"), KeyboardInput(WXK_F5, wxMOD_NONE)},
        {XRCID("LoadGame06"), KeyboardInput(WXK_F6, wxMOD_NONE)},
        {XRCID("LoadGame07"), KeyboardInput(WXK_F7, wxMOD_NONE)},
        {XRCID("LoadGame08"), KeyboardInput(WXK_F8, wxMOD_NONE)},
        {XRCID("LoadGame09"), KeyboardInput(WXK_F9, wxMOD_NONE)},
        {XRCID("LoadGame10"), KeyboardInput(WXK_F10, wxMOD_NONE)},
        {XRCID("Pause"), KeyboardInput(WXK_PAUSE, wxMOD_NONE)},
        {XRCID("Pause"), KeyboardInput('P', wxMOD_CMD)},
        {XRCID("Reset"), KeyboardInput('R', wxMOD_CMD)},
        // add shortcuts for original size multiplier #415
        {XRCID("SetSize1x"), KeyboardInput('1', wxMOD_NONE)},
        {XRCID("SetSize2x"), KeyboardInput('2', wxMOD_NONE)},
        {XRCID("SetSize3x"), KeyboardInput('3', wxMOD_NONE)},
        {XRCID("SetSize4x"), KeyboardInput('4', wxMOD_NONE)},
        {XRCID("SetSize5x"), KeyboardInput('5', wxMOD_NONE)},
        {XRCID("SetSize6x"), KeyboardInput('6', wxMOD_NONE)},
        // save oldest is more commonly used than save other
        // {XRCID("Save"), KeyboardInput('S', wxMOD_CMD)},
        {XRCID("SaveGameOldest"), KeyboardInput('S', wxMOD_CMD)},
        {XRCID("SaveGame01"), KeyboardInput(WXK_F1, wxMOD_SHIFT)},
        {XRCID("SaveGame02"), KeyboardInput(WXK_F2, wxMOD_SHIFT)},
        {XRCID("SaveGame03"), KeyboardInput(WXK_F3, wxMOD_SHIFT)},
        {XRCID("SaveGame04"), KeyboardInput(WXK_F4, wxMOD_SHIFT)},
        {XRCID("SaveGame05"), KeyboardInput(WXK_F5, wxMOD_SHIFT)},
        {XRCID("SaveGame06"), KeyboardInput(WXK_F6, wxMOD_SHIFT)},
        {XRCID("SaveGame07"), KeyboardInput(WXK_F7, wxMOD_SHIFT)},
        {XRCID("SaveGame08"), KeyboardInput(WXK_F8, wxMOD_SHIFT)},
        {XRCID("SaveGame09"), KeyboardInput(WXK_F9, wxMOD_SHIFT)},
        {XRCID("SaveGame10"), KeyboardInput(WXK_F10, wxMOD_SHIFT)},
        // I prefer the SDL ESC key binding
        // {XRCID("ToggleFullscreen"), KeyboardInput(WXK_ESCAPE, wxMOD_NONE)},
        // alt-enter is more standard anyway
        {XRCID("ToggleFullscreen"), KeyboardInput(WXK_RETURN, wxMOD_ALT)},
        {XRCID("JoypadAutofireA"), KeyboardInput('1', wxMOD_ALT)},
        {XRCID("JoypadAutofireB"), KeyboardInput('2', wxMOD_ALT)},
        {XRCID("JoypadAutofireL"), KeyboardInput('3', wxMOD_ALT)},
        {XRCID("JoypadAutofireR"), KeyboardInput('4', wxMOD_ALT)},
        {XRCID("VideoLayersBG0"), KeyboardInput('1', wxMOD_CMD)},
        {XRCID("VideoLayersBG1"), KeyboardInput('2', wxMOD_CMD)},
        {XRCID("VideoLayersBG2"), KeyboardInput('3', wxMOD_CMD)},
        {XRCID("VideoLayersBG3"), KeyboardInput('4', wxMOD_CMD)},
        {XRCID("VideoLayersOBJ"), KeyboardInput('5', wxMOD_CMD)},
        {XRCID("VideoLayersWIN0"), KeyboardInput('6', wxMOD_CMD)},
        {XRCID("VideoLayersWIN1"), KeyboardInput('7', wxMOD_CMD)},
        {XRCID("VideoLayersOBJWIN"), KeyboardInput('8', wxMOD_CMD)},
        {XRCID("Rewind"), KeyboardInput('B', wxMOD_CMD)},
        // following are not in standard menus
        // FILExx are filled in when recent menu is filled
        {wxID_FILE1, KeyboardInput(WXK_F1, wxMOD_CMD)},
        {wxID_FILE2, KeyboardInput(WXK_F2, wxMOD_CMD)},
        {wxID_FILE3, KeyboardInput(WXK_F3, wxMOD_CMD)},
        {wxID_FILE4, KeyboardInput(WXK_F4, wxMOD_CMD)},
        {wxID_FILE5, KeyboardInput(WXK_F5, wxMOD_CMD)},
        {wxID_FILE6, KeyboardInput(WXK_F6, wxMOD_CMD)},
        {wxID_FILE7, KeyboardInput(WXK_F7, wxMOD_CMD)},
        {wxID_FILE8, KeyboardInput(WXK_F8, wxMOD_CMD)},
        {wxID_FILE9, KeyboardInput(WXK_F9, wxMOD_CMD)},
        {wxID_FILE10, KeyboardInput(WXK_F10, wxMOD_CMD)},
        {XRCID("VideoLayersReset"), KeyboardInput('0', wxMOD_CMD)},
        {XRCID("ChangeFilter"), KeyboardInput('G', wxMOD_CMD)},
        {XRCID("ChangeIFB"), KeyboardInput('I', wxMOD_CMD)},
        {XRCID("IncreaseVolume"), KeyboardInput(WXK_NUMPAD_ADD, wxMOD_NONE)},
        {XRCID("DecreaseVolume"), KeyboardInput(WXK_NUMPAD_SUBTRACT, wxMOD_NONE)},
        {XRCID("ToggleSound"), KeyboardInput(WXK_NUMPAD_ENTER, wxMOD_NONE)},
    };
    return kDefaultShortcuts;
}

UserInput DefaultShortcutForCommand(int command) {
    const auto& iter = DefaultShortcuts().find(command);
    if (iter != DefaultShortcuts().end()) {
        return iter->second;
    }
    return UserInput();
}

}  // namespace internal
}  // namespace config

#include "wx/config/shortcuts.h"

#include <wx/xrc/xmlres.h>

#define VBAM_SHORTCUTS_INTERNAL_INCLUDE
#include "wx/config/internal/shortcuts-internal.h"
#undef VBAM_SHORTCUTS_INTERNAL_INCLUDE

namespace config {
namespace internal {

const std::unordered_map<int, UserInput>& DefaultShortcuts() {
    static const std::unordered_map<int, UserInput> kDefaultShortcuts = {
        {XRCID("CheatsList"), UserInput('C', wxMOD_CMD)},
        {XRCID("NextFrame"), UserInput('N', wxMOD_CMD)},
        // this was annoying people A LOT #334
        //{wxID_EXIT, UserInput(WXK_ESCAPE, wxMOD_NONE)},
        // this was annoying people #298
        //{wxID_EXIT, UserInput('X', wxMOD_CMD)},

        {wxID_EXIT, UserInput('Q', wxMOD_CMD)},
        {wxID_CLOSE, UserInput('W', wxMOD_CMD)},
        // load most recent is more commonly used than load state
        // {XRCID("Load"), UserInput('L', wxMOD_CMD)},
        {XRCID("LoadGameRecent"), UserInput('L', wxMOD_CMD)},
        {XRCID("LoadGame01"), UserInput(WXK_F1, wxMOD_NONE)},
        {XRCID("LoadGame02"), UserInput(WXK_F2, wxMOD_NONE)},
        {XRCID("LoadGame03"), UserInput(WXK_F3, wxMOD_NONE)},
        {XRCID("LoadGame04"), UserInput(WXK_F4, wxMOD_NONE)},
        {XRCID("LoadGame05"), UserInput(WXK_F5, wxMOD_NONE)},
        {XRCID("LoadGame06"), UserInput(WXK_F6, wxMOD_NONE)},
        {XRCID("LoadGame07"), UserInput(WXK_F7, wxMOD_NONE)},
        {XRCID("LoadGame08"), UserInput(WXK_F8, wxMOD_NONE)},
        {XRCID("LoadGame09"), UserInput(WXK_F9, wxMOD_NONE)},
        {XRCID("LoadGame10"), UserInput(WXK_F10, wxMOD_NONE)},
        {XRCID("Pause"), UserInput(WXK_PAUSE, wxMOD_NONE)},
        {XRCID("Pause"), UserInput('P', wxMOD_CMD)},
        {XRCID("Reset"), UserInput('R', wxMOD_CMD)},
        // add shortcuts for original size multiplier #415
        {XRCID("SetSize1x"), UserInput('1', wxMOD_NONE)},
        {XRCID("SetSize2x"), UserInput('2', wxMOD_NONE)},
        {XRCID("SetSize3x"), UserInput('3', wxMOD_NONE)},
        {XRCID("SetSize4x"), UserInput('4', wxMOD_NONE)},
        {XRCID("SetSize5x"), UserInput('5', wxMOD_NONE)},
        {XRCID("SetSize6x"), UserInput('6', wxMOD_NONE)},
        // save oldest is more commonly used than save other
        // {XRCID("Save"), UserInput('S', wxMOD_CMD)},
        {XRCID("SaveGameOldest"), UserInput('S', wxMOD_CMD)},
        {XRCID("SaveGame01"), UserInput(WXK_F1, wxMOD_SHIFT)},
        {XRCID("SaveGame02"), UserInput(WXK_F2, wxMOD_SHIFT)},
        {XRCID("SaveGame03"), UserInput(WXK_F3, wxMOD_SHIFT)},
        {XRCID("SaveGame04"), UserInput(WXK_F4, wxMOD_SHIFT)},
        {XRCID("SaveGame05"), UserInput(WXK_F5, wxMOD_SHIFT)},
        {XRCID("SaveGame06"), UserInput(WXK_F6, wxMOD_SHIFT)},
        {XRCID("SaveGame07"), UserInput(WXK_F7, wxMOD_SHIFT)},
        {XRCID("SaveGame08"), UserInput(WXK_F8, wxMOD_SHIFT)},
        {XRCID("SaveGame09"), UserInput(WXK_F9, wxMOD_SHIFT)},
        {XRCID("SaveGame10"), UserInput(WXK_F10, wxMOD_SHIFT)},
        // I prefer the SDL ESC key binding
        // {XRCID("ToggleFullscreen"), UserInput(WXK_ESCAPE, wxMOD_NONE)},
        // alt-enter is more standard anyway
        {XRCID("ToggleFullscreen"), UserInput(WXK_RETURN, wxMOD_ALT)},
        {XRCID("JoypadAutofireA"), UserInput('1', wxMOD_ALT)},
        {XRCID("JoypadAutofireB"), UserInput('2', wxMOD_ALT)},
        {XRCID("JoypadAutofireL"), UserInput('3', wxMOD_ALT)},
        {XRCID("JoypadAutofireR"), UserInput('4', wxMOD_ALT)},
        {XRCID("VideoLayersBG0"), UserInput('1', wxMOD_CMD)},
        {XRCID("VideoLayersBG1"), UserInput('2', wxMOD_CMD)},
        {XRCID("VideoLayersBG2"), UserInput('3', wxMOD_CMD)},
        {XRCID("VideoLayersBG3"), UserInput('4', wxMOD_CMD)},
        {XRCID("VideoLayersOBJ"), UserInput('5', wxMOD_CMD)},
        {XRCID("VideoLayersWIN0"), UserInput('6', wxMOD_CMD)},
        {XRCID("VideoLayersWIN1"), UserInput('7', wxMOD_CMD)},
        {XRCID("VideoLayersOBJWIN"), UserInput('8', wxMOD_CMD)},
        {XRCID("Rewind"), UserInput('B', wxMOD_CMD)},
        // following are not in standard menus
        // FILExx are filled in when recent menu is filled
        {wxID_FILE1, UserInput(WXK_F1, wxMOD_CMD)},
        {wxID_FILE2, UserInput(WXK_F2, wxMOD_CMD)},
        {wxID_FILE3, UserInput(WXK_F3, wxMOD_CMD)},
        {wxID_FILE4, UserInput(WXK_F4, wxMOD_CMD)},
        {wxID_FILE5, UserInput(WXK_F5, wxMOD_CMD)},
        {wxID_FILE6, UserInput(WXK_F6, wxMOD_CMD)},
        {wxID_FILE7, UserInput(WXK_F7, wxMOD_CMD)},
        {wxID_FILE8, UserInput(WXK_F8, wxMOD_CMD)},
        {wxID_FILE9, UserInput(WXK_F9, wxMOD_CMD)},
        {wxID_FILE10, UserInput(WXK_F10, wxMOD_CMD)},
        {XRCID("VideoLayersReset"), UserInput('0', wxMOD_CMD)},
        {XRCID("ChangeFilter"), UserInput('G', wxMOD_CMD)},
        {XRCID("ChangeIFB"), UserInput('I', wxMOD_CMD)},
        {XRCID("IncreaseVolume"), UserInput(WXK_NUMPAD_ADD, wxMOD_NONE)},
        {XRCID("DecreaseVolume"), UserInput(WXK_NUMPAD_SUBTRACT, wxMOD_NONE)},
        {XRCID("ToggleSound"), UserInput(WXK_NUMPAD_ENTER, wxMOD_NONE)},
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

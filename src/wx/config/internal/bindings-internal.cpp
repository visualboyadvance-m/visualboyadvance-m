#include "wx/config/bindings.h"
#include "wx/config/command.h"

#include <unordered_set>

#include <wx/xrc/xmlres.h>

#define VBAM_BINDINGS_INTERNAL_INCLUDE
#include "wx/config/internal/bindings-internal.h"
#undef VBAM_BINDINGS_INTERNAL_INCLUDE

namespace config {
namespace internal {

const std::unordered_map<Command, std::unordered_set<UserInput>>& DefaultInputs() {
    // clang-format off
    static const std::unordered_map<Command, std::unordered_set<UserInput>> kDefaultInputs = {
        {ShortcutCommand(XRCID("CheatsList")),
            {
                KeyboardInput('C', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("NextFrame")),
            {
                KeyboardInput('N', wxMOD_CMD)
            }},
        // this was annoying people A LOT #334
        // {ShortcutCommand(wxID_EXIT),
        //     {
        //         KeyboardInput(WXK_ESCAPE)
        //     }},
        // this was annoying people #298
        // {ShortcutCommand(wxID_EXIT),
        //     {
        //         KeyboardInput('X', wxMOD_CMD)
        //     }},
        {ShortcutCommand(wxID_EXIT),
            {
                KeyboardInput('Q', wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_CLOSE),
            {
                KeyboardInput('W', wxMOD_CMD)
            }},
        // load most recent is more commonly used than load state
        // {ShortcutCommand(XRCID("Load")),
        //     {
        //         KeyboardInput('L', wxMOD_CMD)
        // }},
        {ShortcutCommand(XRCID("LoadGameRecent")),
            {
                KeyboardInput('L', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("LoadGame01")),
            {
                KeyboardInput(WXK_F1)
            }},
        {ShortcutCommand(XRCID("LoadGame02")),
            {
                KeyboardInput(WXK_F2)
            }},
        {ShortcutCommand(XRCID("LoadGame03")),
            {
                KeyboardInput(WXK_F3)
            }},
        {ShortcutCommand(XRCID("LoadGame04")),
            {
                KeyboardInput(WXK_F4)
            }},
        {ShortcutCommand(XRCID("LoadGame05")),
            {
                KeyboardInput(WXK_F5)
            }},
        {ShortcutCommand(XRCID("LoadGame06")),
            {
                KeyboardInput(WXK_F6)
            }},
        {ShortcutCommand(XRCID("LoadGame07")),
            {
                KeyboardInput(WXK_F7)
            }},
        {ShortcutCommand(XRCID("LoadGame08")),
            {
                KeyboardInput(WXK_F8)
            }},
        {ShortcutCommand(XRCID("LoadGame09")),
            {
                KeyboardInput(WXK_F9)
            }},
        {ShortcutCommand(XRCID("LoadGame10")),
            {
                KeyboardInput(WXK_F10)
            }},
        {ShortcutCommand(XRCID("Pause")),
         {KeyboardInput(WXK_PAUSE), KeyboardInput('P', wxMOD_CMD)}},
        {ShortcutCommand(XRCID("Reset")),
            {
                KeyboardInput('R', wxMOD_CMD)
            }},
        // add shortcuts for original size multiplier #415
        {ShortcutCommand(XRCID("SetSize1x")),
            {
                KeyboardInput('1')
            }},
        {ShortcutCommand(XRCID("SetSize2x")),
            {
                KeyboardInput('2')
            }},
        {ShortcutCommand(XRCID("SetSize3x")),
            {
                KeyboardInput('3')
            }},
        {ShortcutCommand(XRCID("SetSize4x")),
            {
                KeyboardInput('4')
            }},
        {ShortcutCommand(XRCID("SetSize5x")),
            {
                KeyboardInput('5')
            }},
        {ShortcutCommand(XRCID("SetSize6x")),
            {
                KeyboardInput('6')
            }},
        // save oldest is more commonly used than save other
        // {ShortcutCommand(XRCID("Save")),
        //     {
        //         KeyboardInput('S', wxMOD_CMD)
        //     }},
        {ShortcutCommand(XRCID("SaveGameOldest")),
            {
                KeyboardInput('S', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("SaveGame01")),
            {
                KeyboardInput(WXK_F1, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame02")),
            {
                KeyboardInput(WXK_F2, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame03")),
            {
                KeyboardInput(WXK_F3, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame04")),
            {
                KeyboardInput(WXK_F4, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame05")),
            {
                KeyboardInput(WXK_F5, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame06")),
            {
                KeyboardInput(WXK_F6, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame07")),
            {
                KeyboardInput(WXK_F7, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame08")),
            {
                KeyboardInput(WXK_F8, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame09")),
            {
                KeyboardInput(WXK_F9, wxMOD_SHIFT)
            }},
        {ShortcutCommand(XRCID("SaveGame10")),
            {
                KeyboardInput(WXK_F10, wxMOD_SHIFT)
            }},
        // I prefer the SDL ESC key binding
        // {ShortcutCommand(XRCID("ToggleFullscreen")),
        //     {
        //         KeyboardInput(WXK_ESCAPE)
        //     }},
        // alt-enter is more standard anyway
        {ShortcutCommand(XRCID("ToggleFullscreen")),
            {
                KeyboardInput(WXK_RETURN, wxMOD_ALT)
            }},
        {ShortcutCommand(XRCID("JoypadAutofireA")),
            {
                KeyboardInput('1', wxMOD_ALT)
            }},
        {ShortcutCommand(XRCID("JoypadAutofireB")),
            {
                KeyboardInput('2', wxMOD_ALT)
            }},
        {ShortcutCommand(XRCID("JoypadAutofireL")),
            {
                KeyboardInput('3', wxMOD_ALT)
            }},
        {ShortcutCommand(XRCID("JoypadAutofireR")),
            {
                KeyboardInput('4', wxMOD_ALT)
            }},
        {ShortcutCommand(XRCID("VideoLayersBG0")),
            {
                KeyboardInput('1', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("VideoLayersBG1")),
            {
                KeyboardInput('2', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("VideoLayersBG2")),
            {
                KeyboardInput('3', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("VideoLayersBG3")),
            {
                KeyboardInput('4', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("VideoLayersOBJ")),
            {
                KeyboardInput('5', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("VideoLayersWIN0")),
            {
                KeyboardInput('6', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("VideoLayersWIN1")),
            {
                KeyboardInput('7', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("VideoLayersOBJWIN")),
            {
                KeyboardInput('8', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("Rewind")),
            {
                KeyboardInput('B', wxMOD_CMD)
            }},
        // The following commands do not have the dafault wxWidgets shortcut.
        // The wxID_FILE1 shortcut is active when the first recent menu entry is populated.
        // The same goes for the others, wxID_FILE2 is active when the second recent menu entry is
        // populated, etc.
        {ShortcutCommand(wxID_FILE1),
            {
                KeyboardInput(WXK_F1, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE2),
            {
                KeyboardInput(WXK_F2, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE3),
            {
                KeyboardInput(WXK_F3, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE4),
            {
                KeyboardInput(WXK_F4, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE5),
            {
                KeyboardInput(WXK_F5, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE6),
            {
                KeyboardInput(WXK_F6, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE7),
            {
                KeyboardInput(WXK_F7, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE8),
            {
                KeyboardInput(WXK_F8, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE9),
            {
                KeyboardInput(WXK_F9, wxMOD_CMD)
            }},
        {ShortcutCommand(wxID_FILE10),
            {
                KeyboardInput(WXK_F10, wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("VideoLayersReset")),
            {
                KeyboardInput('0', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("ChangeFilter")),
            {
                KeyboardInput('G', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("ChangeIFB")),
            {
                KeyboardInput('I', wxMOD_CMD)
            }},
        {ShortcutCommand(XRCID("IncreaseVolume")),
            {
                KeyboardInput(WXK_NUMPAD_ADD)
            }},
        {ShortcutCommand(XRCID("DecreaseVolume")),
            {
                KeyboardInput(WXK_NUMPAD_SUBTRACT)
            }},
        {ShortcutCommand(XRCID("ToggleSound")),
            {
                KeyboardInput(WXK_NUMPAD_ENTER)
            }},

        // Player 1 controls.
        {GameCommand(GameJoy(0), config::GameKey::Up),
         {
             KeyboardInput('W'),
             JoyInput(JoyId(0), JoyControl::Button, 11),
             JoyInput(JoyId(0), JoyControl::AxisMinus, 1),
             JoyInput(JoyId(0), JoyControl::AxisMinus, 3),
             JoyInput(JoyId(0), JoyControl::HatNorth, 0),
         }},
        {GameCommand(GameJoy(0), config::GameKey::Down),
            {
                KeyboardInput('S'),
                JoyInput(JoyId(0), JoyControl::Button, 12),
                JoyInput(JoyId(0), JoyControl::AxisPlus, 1),
                JoyInput(JoyId(0), JoyControl::AxisPlus, 3),
                JoyInput(JoyId(0), JoyControl::HatSouth, 0),
            }},
        {GameCommand(GameJoy(0), config::GameKey::Left),
            {
                KeyboardInput('A'),
                JoyInput(JoyId(0), JoyControl::Button, 13),
                JoyInput(JoyId(0), JoyControl::AxisMinus, 0),
                JoyInput(JoyId(0), JoyControl::AxisMinus, 2),
                JoyInput(JoyId(0), JoyControl::HatWest, 0),
            }},
        {GameCommand(GameJoy(0), config::GameKey::Right),
            {
                KeyboardInput('D'),
                JoyInput(JoyId(0), JoyControl::Button, 14),
                JoyInput(JoyId(0), JoyControl::AxisPlus, 0),
                JoyInput(JoyId(0), JoyControl::AxisPlus, 2),
                JoyInput(JoyId(0), JoyControl::HatEast, 0),
            }},
        {GameCommand(GameJoy(0), config::GameKey::A),
            {
                KeyboardInput('L'),
                JoyInput(JoyId(0), JoyControl::Button, 1),
            }},
        {GameCommand(GameJoy(0), config::GameKey::B),
            {
                KeyboardInput('K'),
                JoyInput(JoyId(0), JoyControl::Button, 0),
            }},
        {GameCommand(GameJoy(0), config::GameKey::L),
            {
                KeyboardInput('I'),
                JoyInput(JoyId(0), JoyControl::Button, 2),
                JoyInput(JoyId(0), JoyControl::Button, 9),
                JoyInput(JoyId(0), JoyControl::AxisPlus, 4),
            }},
        {GameCommand(GameJoy(0), config::GameKey::R),
            {
                KeyboardInput('O'),
                JoyInput(JoyId(0), JoyControl::Button, 3),
                JoyInput(JoyId(0), JoyControl::Button, 10),
                JoyInput(JoyId(0), JoyControl::AxisPlus, 5),
            }},
        {GameCommand(GameJoy(0), config::GameKey::Select),
            {
                KeyboardInput(WXK_BACK),
                JoyInput(JoyId(0), JoyControl::Button, 4),
            }},
        {GameCommand(GameJoy(0), config::GameKey::Start),
            {
                KeyboardInput(WXK_RETURN),
                JoyInput(JoyId(0), JoyControl::Button, 6),
            }},
        {GameCommand(GameJoy(0), config::GameKey::MotionUp), {}},
        {GameCommand(GameJoy(0), config::GameKey::MotionDown), {}},
        {GameCommand(GameJoy(0), config::GameKey::MotionLeft), {}},
        {GameCommand(GameJoy(0), config::GameKey::MotionRight), {}},
        {GameCommand(GameJoy(0), config::GameKey::MotionIn), {}},
        {GameCommand(GameJoy(0), config::GameKey::MotionOut), {}},
        {GameCommand(GameJoy(0), config::GameKey::AutoA), {}},
        {GameCommand(GameJoy(0), config::GameKey::AutoB), {}},
        {GameCommand(GameJoy(0), config::GameKey::Speed),
            {
                KeyboardInput(WXK_SPACE),
            }},
        {GameCommand(GameJoy(0), config::GameKey::Capture), {}},
        {GameCommand(GameJoy(0), config::GameKey::Gameshark), {}},

        // Player 2 controls.
        {GameCommand(GameJoy(1), config::GameKey::Up),
            {
                JoyInput(JoyId(1), JoyControl::Button, 11),
                JoyInput(JoyId(1), JoyControl::AxisMinus, 1),
                JoyInput(JoyId(1), JoyControl::AxisMinus, 3),
                JoyInput(JoyId(1), JoyControl::HatNorth, 0),
            }},
        {GameCommand(GameJoy(1), config::GameKey::Down),
            {
                JoyInput(JoyId(1), JoyControl::Button, 12),
                JoyInput(JoyId(1), JoyControl::AxisPlus, 1),
                JoyInput(JoyId(1), JoyControl::AxisPlus, 3),
                JoyInput(JoyId(1), JoyControl::HatSouth, 0),
            }},
        {GameCommand(GameJoy(1), config::GameKey::Left),
            {
                JoyInput(JoyId(1), JoyControl::Button, 13),
                JoyInput(JoyId(1), JoyControl::AxisMinus, 0),
                JoyInput(JoyId(1), JoyControl::AxisMinus, 2),
                JoyInput(JoyId(1), JoyControl::HatWest, 0),
            }},
        {GameCommand(GameJoy(1), config::GameKey::Right),
            {
                JoyInput(JoyId(1), JoyControl::Button, 14),
                JoyInput(JoyId(1), JoyControl::AxisPlus, 0),
                JoyInput(JoyId(1), JoyControl::AxisPlus, 2),
                JoyInput(JoyId(1), JoyControl::HatEast, 0),
            }},
        {GameCommand(GameJoy(1), config::GameKey::A),
            {
                JoyInput(JoyId(1), JoyControl::Button, 1),
            }},
        {GameCommand(GameJoy(1), config::GameKey::B),
            {
                JoyInput(JoyId(1), JoyControl::Button, 0),
            }},
        {GameCommand(GameJoy(1), config::GameKey::L),
            {
                JoyInput(JoyId(1), JoyControl::Button, 2),
                JoyInput(JoyId(1), JoyControl::Button, 9),
                JoyInput(JoyId(1), JoyControl::AxisPlus, 4),
            }},
        {GameCommand(GameJoy(1), config::GameKey::R),
            {
                JoyInput(JoyId(1), JoyControl::Button, 3),
                JoyInput(JoyId(1), JoyControl::Button, 10),
                JoyInput(JoyId(1), JoyControl::AxisPlus, 5),
            }},
        {GameCommand(GameJoy(1), config::GameKey::Select),
            {
                JoyInput(JoyId(1), JoyControl::Button, 4),
            }},
        {GameCommand(GameJoy(1), config::GameKey::Start),
            {
                JoyInput(JoyId(1), JoyControl::Button, 6),
            }},
        {GameCommand(GameJoy(1), config::GameKey::MotionUp), {}},
        {GameCommand(GameJoy(1), config::GameKey::MotionDown), {}},
        {GameCommand(GameJoy(1), config::GameKey::MotionLeft), {}},
        {GameCommand(GameJoy(1), config::GameKey::MotionRight), {}},
        {GameCommand(GameJoy(1), config::GameKey::MotionIn), {}},
        {GameCommand(GameJoy(1), config::GameKey::MotionOut), {}},
        {GameCommand(GameJoy(1), config::GameKey::AutoA), {}},
        {GameCommand(GameJoy(1), config::GameKey::AutoB), {}},
        {GameCommand(GameJoy(1), config::GameKey::Speed), {}},
        {GameCommand(GameJoy(1), config::GameKey::Capture), {}},
        {GameCommand(GameJoy(1), config::GameKey::Gameshark), {}},

        // Player 3 controls.
        {GameCommand(GameJoy(2), config::GameKey::Up),
            {
                JoyInput(JoyId(2), JoyControl::Button, 11),
                JoyInput(JoyId(2), JoyControl::AxisMinus, 1),
                JoyInput(JoyId(2), JoyControl::AxisMinus, 3),
                JoyInput(JoyId(2), JoyControl::HatNorth, 0),
            }},
        {GameCommand(GameJoy(2), config::GameKey::Down),
            {
                JoyInput(JoyId(2), JoyControl::Button, 12),
                JoyInput(JoyId(2), JoyControl::AxisPlus, 1),
                JoyInput(JoyId(2), JoyControl::AxisPlus, 3),
                JoyInput(JoyId(2), JoyControl::HatSouth, 0),
            }},
        {GameCommand(GameJoy(2), config::GameKey::Left),
            {
                JoyInput(JoyId(2), JoyControl::Button, 13),
                JoyInput(JoyId(2), JoyControl::AxisMinus, 0),
                JoyInput(JoyId(2), JoyControl::AxisMinus, 2),
                JoyInput(JoyId(2), JoyControl::HatWest, 0),
            }},
        {GameCommand(GameJoy(2), config::GameKey::Right),
            {
                JoyInput(JoyId(2), JoyControl::Button, 14),
                JoyInput(JoyId(2), JoyControl::AxisPlus, 0),
                JoyInput(JoyId(2), JoyControl::AxisPlus, 2),
                JoyInput(JoyId(2), JoyControl::HatEast, 0),
            }},
        {GameCommand(GameJoy(2), config::GameKey::A),
            {
                JoyInput(JoyId(2), JoyControl::Button, 1),
            }},
        {GameCommand(GameJoy(2), config::GameKey::B),
            {
                JoyInput(JoyId(2), JoyControl::Button, 0),
            }},
        {GameCommand(GameJoy(2), config::GameKey::L),
            {
                JoyInput(JoyId(2), JoyControl::Button, 2),
                JoyInput(JoyId(2), JoyControl::Button, 9),
                JoyInput(JoyId(2), JoyControl::AxisPlus, 4),
            }},
        {GameCommand(GameJoy(2), config::GameKey::R),
            {
                JoyInput(JoyId(2), JoyControl::Button, 3),
                JoyInput(JoyId(2), JoyControl::Button, 10),
                JoyInput(JoyId(2), JoyControl::AxisPlus, 5),
            }},
        {GameCommand(GameJoy(2), config::GameKey::Select),
            {
                JoyInput(JoyId(2), JoyControl::Button, 4),
            }},
        {GameCommand(GameJoy(2), config::GameKey::Start),
            {
                JoyInput(JoyId(2), JoyControl::Button, 6),
            }},
        {GameCommand(GameJoy(2), config::GameKey::MotionUp), {}},
        {GameCommand(GameJoy(2), config::GameKey::MotionDown), {}},
        {GameCommand(GameJoy(2), config::GameKey::MotionLeft), {}},
        {GameCommand(GameJoy(2), config::GameKey::MotionRight), {}},
        {GameCommand(GameJoy(2), config::GameKey::MotionIn), {}},
        {GameCommand(GameJoy(2), config::GameKey::MotionOut), {}},
        {GameCommand(GameJoy(2), config::GameKey::AutoA), {}},
        {GameCommand(GameJoy(2), config::GameKey::AutoB), {}},
        {GameCommand(GameJoy(2), config::GameKey::Speed), {}},
        {GameCommand(GameJoy(2), config::GameKey::Capture), {}},
        {GameCommand(GameJoy(2), config::GameKey::Gameshark), {}},

        // Player 4 controls.
        {GameCommand(GameJoy(3), config::GameKey::Up),
            {
                JoyInput(JoyId(3), JoyControl::Button, 11),
                JoyInput(JoyId(3), JoyControl::AxisMinus, 1),
                JoyInput(JoyId(3), JoyControl::AxisMinus, 3),
                JoyInput(JoyId(3), JoyControl::HatNorth, 0),
            }},
        {GameCommand(GameJoy(3), config::GameKey::Down),
            {
                JoyInput(JoyId(3), JoyControl::Button, 12),
                JoyInput(JoyId(3), JoyControl::AxisPlus, 1),
                JoyInput(JoyId(3), JoyControl::AxisPlus, 3),
                JoyInput(JoyId(3), JoyControl::HatSouth, 0),
            }},
        {GameCommand(GameJoy(3), config::GameKey::Left),
            {
                JoyInput(JoyId(3), JoyControl::Button, 13),
                JoyInput(JoyId(3), JoyControl::AxisMinus, 0),
                JoyInput(JoyId(3), JoyControl::AxisMinus, 2),
                JoyInput(JoyId(3), JoyControl::HatWest, 0),
            }},
        {GameCommand(GameJoy(3), config::GameKey::Right),
            {
                JoyInput(JoyId(3), JoyControl::Button, 14),
                JoyInput(JoyId(3), JoyControl::AxisPlus, 0),
                JoyInput(JoyId(3), JoyControl::AxisPlus, 2),
                JoyInput(JoyId(3), JoyControl::HatEast, 0),
            }},
        {GameCommand(GameJoy(3), config::GameKey::A),
            {
                JoyInput(JoyId(3), JoyControl::Button, 1),
            }},
        {GameCommand(GameJoy(3), config::GameKey::B),
            {
                JoyInput(JoyId(3), JoyControl::Button, 0),
            }},
        {GameCommand(GameJoy(3), config::GameKey::L),
            {
                JoyInput(JoyId(3), JoyControl::Button, 2),
                JoyInput(JoyId(3), JoyControl::Button, 9),
                JoyInput(JoyId(3), JoyControl::AxisPlus, 4),
            }},
        {GameCommand(GameJoy(3), config::GameKey::R),
            {
                JoyInput(JoyId(3), JoyControl::Button, 3),
                JoyInput(JoyId(3), JoyControl::Button, 10),
                JoyInput(JoyId(3), JoyControl::AxisPlus, 5),
            }},
        {GameCommand(GameJoy(3), config::GameKey::Select),
            {
                JoyInput(JoyId(3), JoyControl::Button, 4),
            }},
        {GameCommand(GameJoy(3), config::GameKey::Start),
            {
                JoyInput(JoyId(3), JoyControl::Button, 6),
            }},
        {GameCommand(GameJoy(3), config::GameKey::MotionUp), {}},
        {GameCommand(GameJoy(3), config::GameKey::MotionDown), {}},
        {GameCommand(GameJoy(3), config::GameKey::MotionLeft), {}},
        {GameCommand(GameJoy(3), config::GameKey::MotionRight), {}},
        {GameCommand(GameJoy(3), config::GameKey::MotionIn), {}},
        {GameCommand(GameJoy(3), config::GameKey::MotionOut), {}},
        {GameCommand(GameJoy(3), config::GameKey::AutoA), {}},
        {GameCommand(GameJoy(3), config::GameKey::AutoB), {}},
        {GameCommand(GameJoy(3), config::GameKey::Speed), {}},
        {GameCommand(GameJoy(3), config::GameKey::Capture), {}},
        {GameCommand(GameJoy(3), config::GameKey::Gameshark), {}},
    };
    // clang-format on
    return kDefaultInputs;
}

const std::unordered_set<UserInput>& DefaultInputsForCommand(const Command& command) {
    const auto& iter = DefaultInputs().find(command);
    if (iter != DefaultInputs().end()) {
        return iter->second;
    }
    static const std::unordered_set<UserInput> kEmptySet;
    return kEmptySet;
}

bool IsDefaultInputForCommand(const Command& command, const UserInput& input) {
    const auto& iter = DefaultInputs().find(command);
    if (iter != DefaultInputs().end()) {
        return iter->second.find(input) != iter->second.end();
    }
    return false;
}

}  // namespace internal
}  // namespace config

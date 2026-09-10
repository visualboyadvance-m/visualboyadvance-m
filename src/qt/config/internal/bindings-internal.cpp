#include "qt/config/bindings.h"
#include "qt/config/command.h"

#include <unordered_set>
#include <utility>

#include <Qt>

#include "qt/cmd-ids.h"

#define VBAM_BINDINGS_INTERNAL_INCLUDE
#include "qt/config/internal/bindings-internal.h"
#undef VBAM_BINDINGS_INTERNAL_INCLUDE

namespace config {
namespace internal {

namespace {

// The "command" modifier: Ctrl everywhere, Command on macOS (Qt maps the
// Command key to ControlModifier there, like wxMOD_CMD).
constexpr uint32_t kMod = kKeyModControl;

// GameCommand has const members and no default constructor, so the array is
// spelled out as a nested initializer built at compile time.
template <size_t... Is>
constexpr std::array<GameCommand, sizeof...(Is)> MakeOrderedGameCommands(
    std::index_sequence<Is...>) {
    return {GameCommand(GameJoy(Is / kNbGameKeys), kAllGameKeys[Is % kNbGameKeys])...};
}

// Default joystick bindings shared by every player: SDL game controller
// layout (A/B/X/Y = 0..3, back = 4, start = 6, shoulders 9/10, d-pad 11..14,
// sticks on axes 0..3, triggers on axes 4/5).
void AddDefaultJoyInputs(std::unordered_map<Command, std::unordered_set<UserInput>>& map,
                         size_t player) {
    const GameJoy joy(player);
    const JoyId id(static_cast<int>(player));
    auto add = [&](GameKey key, std::unordered_set<UserInput> inputs) {
        auto& set = map[Command(GameCommand(joy, key))];
        for (const auto& input : inputs) {
            set.insert(input);
        }
    };
    add(GameKey::Up, {JoyInput(id, JoyControl::Button, 11), JoyInput(id, JoyControl::AxisMinus, 1),
                      JoyInput(id, JoyControl::AxisMinus, 3), JoyInput(id, JoyControl::HatNorth, 0)});
    add(GameKey::Down, {JoyInput(id, JoyControl::Button, 12), JoyInput(id, JoyControl::AxisPlus, 1),
                        JoyInput(id, JoyControl::AxisPlus, 3), JoyInput(id, JoyControl::HatSouth, 0)});
    add(GameKey::Left, {JoyInput(id, JoyControl::Button, 13), JoyInput(id, JoyControl::AxisMinus, 0),
                        JoyInput(id, JoyControl::AxisMinus, 2), JoyInput(id, JoyControl::HatWest, 0)});
    add(GameKey::Right, {JoyInput(id, JoyControl::Button, 14), JoyInput(id, JoyControl::AxisPlus, 0),
                         JoyInput(id, JoyControl::AxisPlus, 2), JoyInput(id, JoyControl::HatEast, 0)});
    add(GameKey::A, {JoyInput(id, JoyControl::Button, 1)});
    add(GameKey::B, {JoyInput(id, JoyControl::Button, 0)});
    add(GameKey::L, {JoyInput(id, JoyControl::Button, 2), JoyInput(id, JoyControl::Button, 9),
                     JoyInput(id, JoyControl::AxisPlus, 4)});
    add(GameKey::R, {JoyInput(id, JoyControl::Button, 3), JoyInput(id, JoyControl::Button, 10),
                     JoyInput(id, JoyControl::AxisPlus, 5)});
    add(GameKey::Select, {JoyInput(id, JoyControl::Button, 4)});
    add(GameKey::Start, {JoyInput(id, JoyControl::Button, 6)});
    // Entries without defaults still exist (empty sets) like in the wx port.
    add(GameKey::MotionUp, {});
    add(GameKey::MotionDown, {});
    add(GameKey::MotionLeft, {});
    add(GameKey::MotionRight, {});
    add(GameKey::MotionIn, {});
    add(GameKey::MotionOut, {});
    add(GameKey::AutoA, {});
    add(GameKey::AutoB, {});
    add(GameKey::Speed, {});
    add(GameKey::Capture, {});
    add(GameKey::Gameshark, {});
}

}  // namespace

const std::array<GameCommand, kNbGameKeys * kNbJoypads>& OrderedGameCommands() {
    static const std::array<GameCommand, kNbGameKeys * kNbJoypads> kOrdered =
        MakeOrderedGameCommands(std::make_index_sequence<kNbGameKeys * kNbJoypads>());
    return kOrdered;
}

const std::unordered_map<Command, std::unordered_set<UserInput>>& DefaultInputs() {
    static const std::unordered_map<Command, std::unordered_set<UserInput>> kDefaultInputs = [] {
        // clang-format off
        std::unordered_map<Command, std::unordered_set<UserInput>> map = {
            {ShortcutCommand(cmd::kCheatsList), {KeyboardInput('C', kMod)}},
            {ShortcutCommand(cmd::kNextFrame), {KeyboardInput('N', kMod)}},
            // Escape and Cmd+X for exit were annoying people (#334, #298).
            {ShortcutCommand(cmd::kExit), {KeyboardInput('Q', kMod)}},
            {ShortcutCommand(cmd::kClose), {KeyboardInput('W', kMod)}},
            // load most recent is more commonly used than load state
            {ShortcutCommand(cmd::kLoadGameRecent), {KeyboardInput('L', kMod)}},
            {ShortcutCommand(cmd::kLoadGame01), {KeyboardInput(Qt::Key_F1)}},
            {ShortcutCommand(cmd::kLoadGame02), {KeyboardInput(Qt::Key_F2)}},
            {ShortcutCommand(cmd::kLoadGame03), {KeyboardInput(Qt::Key_F3)}},
            {ShortcutCommand(cmd::kLoadGame04), {KeyboardInput(Qt::Key_F4)}},
            {ShortcutCommand(cmd::kLoadGame05), {KeyboardInput(Qt::Key_F5)}},
            {ShortcutCommand(cmd::kLoadGame06), {KeyboardInput(Qt::Key_F6)}},
            {ShortcutCommand(cmd::kLoadGame07), {KeyboardInput(Qt::Key_F7)}},
            {ShortcutCommand(cmd::kLoadGame08), {KeyboardInput(Qt::Key_F8)}},
            {ShortcutCommand(cmd::kLoadGame09), {KeyboardInput(Qt::Key_F9)}},
            {ShortcutCommand(cmd::kLoadGame10), {KeyboardInput(Qt::Key_F10)}},
            {ShortcutCommand(cmd::kPause),
                {KeyboardInput(Qt::Key_Pause), KeyboardInput('P', kMod)}},
            {ShortcutCommand(cmd::kReset), {KeyboardInput('R', kMod)}},
            // add shortcuts for original size multiplier #415
            {ShortcutCommand(cmd::kSetSize1x), {KeyboardInput('1')}},
            {ShortcutCommand(cmd::kSetSize2x), {KeyboardInput('2')}},
            {ShortcutCommand(cmd::kSetSize3x), {KeyboardInput('3')}},
            {ShortcutCommand(cmd::kSetSize4x), {KeyboardInput('4')}},
            {ShortcutCommand(cmd::kSetSize5x), {KeyboardInput('5')}},
            {ShortcutCommand(cmd::kSetSize6x), {KeyboardInput('6')}},
            // save oldest is more commonly used than save other
            {ShortcutCommand(cmd::kSaveGameOldest), {KeyboardInput('S', kMod)}},
            {ShortcutCommand(cmd::kSaveGame01), {KeyboardInput(Qt::Key_F1, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame02), {KeyboardInput(Qt::Key_F2, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame03), {KeyboardInput(Qt::Key_F3, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame04), {KeyboardInput(Qt::Key_F4, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame05), {KeyboardInput(Qt::Key_F5, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame06), {KeyboardInput(Qt::Key_F6, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame07), {KeyboardInput(Qt::Key_F7, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame08), {KeyboardInput(Qt::Key_F8, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame09), {KeyboardInput(Qt::Key_F9, kKeyModShift)}},
            {ShortcutCommand(cmd::kSaveGame10), {KeyboardInput(Qt::Key_F10, kKeyModShift)}},
            // alt-enter is the standard fullscreen toggle
            {ShortcutCommand(cmd::kToggleFullscreen), {KeyboardInput(Qt::Key_Return, kKeyModAlt)}},
            {ShortcutCommand(cmd::kJoypadAutofireA), {KeyboardInput('1', kKeyModAlt)}},
            {ShortcutCommand(cmd::kJoypadAutofireB), {KeyboardInput('2', kKeyModAlt)}},
            {ShortcutCommand(cmd::kJoypadAutofireL), {KeyboardInput('3', kKeyModAlt)}},
            {ShortcutCommand(cmd::kJoypadAutofireR), {KeyboardInput('4', kKeyModAlt)}},
            {ShortcutCommand(cmd::kVideoLayersBG0), {KeyboardInput('1', kMod)}},
            {ShortcutCommand(cmd::kVideoLayersBG1), {KeyboardInput('2', kMod)}},
            {ShortcutCommand(cmd::kVideoLayersBG2), {KeyboardInput('3', kMod)}},
            {ShortcutCommand(cmd::kVideoLayersBG3), {KeyboardInput('4', kMod)}},
            {ShortcutCommand(cmd::kVideoLayersOBJ), {KeyboardInput('5', kMod)}},
            {ShortcutCommand(cmd::kVideoLayersWIN0), {KeyboardInput('6', kMod)}},
            {ShortcutCommand(cmd::kVideoLayersWIN1), {KeyboardInput('7', kMod)}},
            {ShortcutCommand(cmd::kVideoLayersOBJWIN), {KeyboardInput('8', kMod)}},
            {ShortcutCommand(cmd::kRewind), {KeyboardInput('B', kMod)}},
            // The recent file shortcuts are active when the corresponding
            // recent menu entry is populated.
            {ShortcutCommand(cmd::kFile1), {KeyboardInput(Qt::Key_F1, kMod)}},
            {ShortcutCommand(cmd::kFile2), {KeyboardInput(Qt::Key_F2, kMod)}},
            {ShortcutCommand(cmd::kFile3), {KeyboardInput(Qt::Key_F3, kMod)}},
            {ShortcutCommand(cmd::kFile4), {KeyboardInput(Qt::Key_F4, kMod)}},
            {ShortcutCommand(cmd::kFile5), {KeyboardInput(Qt::Key_F5, kMod)}},
            {ShortcutCommand(cmd::kFile6), {KeyboardInput(Qt::Key_F6, kMod)}},
            {ShortcutCommand(cmd::kFile7), {KeyboardInput(Qt::Key_F7, kMod)}},
            {ShortcutCommand(cmd::kFile8), {KeyboardInput(Qt::Key_F8, kMod)}},
            {ShortcutCommand(cmd::kFile9), {KeyboardInput(Qt::Key_F9, kMod)}},
            {ShortcutCommand(cmd::kFile10), {KeyboardInput(Qt::Key_F10, kMod)}},
            {ShortcutCommand(cmd::kVideoLayersReset), {KeyboardInput('0', kMod)}},
            {ShortcutCommand(cmd::kChangeFilter), {KeyboardInput('G', kMod)}},
            {ShortcutCommand(cmd::kChangeIFB), {KeyboardInput('I', kMod)}},
            // Qt has no distinct key codes for the keypad +/-; the keyboard
            // handler strips the keypad modifier so these match either key.
            {ShortcutCommand(cmd::kIncreaseVolume), {KeyboardInput(Qt::Key_Plus)}},
            {ShortcutCommand(cmd::kDecreaseVolume), {KeyboardInput(Qt::Key_Minus)}},
            {ShortcutCommand(cmd::kToggleSound), {KeyboardInput(Qt::Key_Enter)}},

            // Player 1 keyboard controls.
            {GameCommand(GameJoy(0), GameKey::Up), {KeyboardInput('W')}},
            {GameCommand(GameJoy(0), GameKey::Down), {KeyboardInput('S')}},
            {GameCommand(GameJoy(0), GameKey::Left), {KeyboardInput('A')}},
            {GameCommand(GameJoy(0), GameKey::Right), {KeyboardInput('D')}},
            {GameCommand(GameJoy(0), GameKey::A), {KeyboardInput('L')}},
            {GameCommand(GameJoy(0), GameKey::B), {KeyboardInput('K')}},
            {GameCommand(GameJoy(0), GameKey::L), {KeyboardInput('I')}},
            {GameCommand(GameJoy(0), GameKey::R), {KeyboardInput('O')}},
            {GameCommand(GameJoy(0), GameKey::Select), {KeyboardInput(Qt::Key_Backspace)}},
            {GameCommand(GameJoy(0), GameKey::Start), {KeyboardInput(Qt::Key_Return)}},
            {GameCommand(GameJoy(0), GameKey::Speed), {KeyboardInput(Qt::Key_Space)}},
        };
        // clang-format on

        // Joystick defaults for every player.
        for (size_t player = 0; player < kNbJoypads; player++) {
            AddDefaultJoyInputs(map, player);
        }
        return map;
    }();
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

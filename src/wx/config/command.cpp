#include "wx/config/command.h"

#include <algorithm>
#include <map>

#include <wx/log.h>
#include <wx/translation.h>

#include "wx/config/cmdtab.h"
#include "wx/config/strutils.h"

namespace config {
namespace {

constexpr int GameKeyToInt(const GameKey& game_key) {
    return static_cast<int>(game_key);
}

// Returns true if `joypad` is in a valid joypad range.
constexpr bool JoypadInRange(const int& joypad) {
    constexpr size_t kMinJoypadIndex = 0;
    return static_cast<size_t>(joypad) >= kMinJoypadIndex &&
           static_cast<size_t>(joypad) < kNbJoypads;
}

wxString GameKeyToUxString(const GameKey& game_key) {
    // Note: this must match GUI widget names or GUI won't work
    // This array's order determines tab order as well
    static const std::array<wxString, kNbGameKeys> kGameKeyStrings = {
        _("Up"),         _("Down"),        _("Left"),        _("Right"),        _("A"),
        _("B"),          _("L"),           _("R"),           _("Select"),       _("Start"),
        _("Motion Up"),  _("Motion Down"), _("Motion Left"), _("Motion Right"), _("Motion In"),
        _("Motion Out"), _("Auto A"),      _("Auto B"),      _("Speed"),        _("Capture"),
        _("GameShark"),
    };
    return kGameKeyStrings[GameKeyToInt(game_key)];
}

}  // namespace

// clang-format off
wxString GameKeyToString(const GameKey& game_key) {
    // Note: this must match GUI widget names or GUI won't work
    // This array's order determines tab order as well
    static const std::array<wxString, kNbGameKeys> kGameKeyStrings = {
        "Up",
        "Down",
        "Left",
        "Right",
        "A",
        "B",
        "L",
        "R",
        "Select",
        "Start",
        "MotionUp",
        "MotionDown",
        "MotionLeft",
        "MotionRight",
        "MotionIn",
        "MotionOut",
        "AutoA",
        "AutoB",
        "Speed",
        "Capture",
        "GS",
    };
    return kGameKeyStrings[GameKeyToInt(game_key)];
}

nonstd::optional<GameKey> StringToGameKey(const wxString& input) {
    static const std::map<wxString, GameKey> kStringToGameKey = {
        { "Up",          GameKey::Up },
        { "Down",        GameKey::Down },
        { "Left",        GameKey::Left },
        { "Right",       GameKey::Right },
        { "A",           GameKey::A },
        { "B",           GameKey::B },
        { "L",           GameKey::L },
        { "R",           GameKey::R },
        { "Select",      GameKey::Select },
        { "Start",       GameKey::Start },
        { "MotionUp",    GameKey::MotionUp },
        { "MotionDown",  GameKey::MotionDown },
        { "MotionLeft",  GameKey::MotionLeft },
        { "MotionRight", GameKey::MotionRight },
        { "MotionIn",    GameKey::MotionIn },
        { "MotionOut",   GameKey::MotionOut },
        { "AutoA",       GameKey::AutoA },
        { "AutoB",       GameKey::AutoB },
        { "Speed",       GameKey::Speed },
        { "Capture",     GameKey::Capture },
        { "GS",          GameKey::Gameshark },
    };

    const auto iter = kStringToGameKey.find(input);
    if (iter == kStringToGameKey.end()) {
        return nonstd::nullopt;
    }
    return iter->second;
}
// clang-format on

wxString GameCommand::ToConfigString() const {
    return wxString::Format("Joypad/%zu/%s", joypad_.ux_index(), GameKeyToString(game_key_));
}
wxString GameCommand::ToUXString() const {
    return wxString::Format(_("Joypad %zu %s"), joypad_.ux_index(), GameKeyToUxString(game_key()));
}

wxString ShortcutCommand::ToConfigString() const {
    for (const cmditem& cmd_item : cmdtab) {
        if (cmd_item.cmd_id == id_) {
            return wxString::Format("Keyboard/%s", cmd_item.cmd);
        }
    }

    // Command not found. This should never happen.
    assert(false);
    return wxEmptyString;
}

// static
nonstd::optional<Command> Command::FromString(const wxString& name) {
    static const wxString kKeyboard("Keyboard");
    static const wxString kJoypad("Joypad");

    bool is_keyboard = !wxStrncmp(name, kKeyboard, kKeyboard.size());
    bool is_joypad = !wxStrncmp(name, kJoypad, kJoypad.size());
    if (!is_keyboard && !is_joypad) {
        wxLogDebug("Doesn't start with joypad or keyboard");
        return nonstd::nullopt;
    }

    auto parts = config::str_split(name, "/");
    if (is_joypad) {
        if (parts.size() != 3) {
            wxLogDebug("Wrong split size: %d", parts.size());
            return nonstd::nullopt;
        }

        const int joypad = parts[1][0] - '1';
        if (!JoypadInRange(joypad)) {
            wxLogDebug("Wrong joypad index: %d", joypad);
            return nonstd::nullopt;
        }

        const nonstd::optional<GameKey> game_key = StringToGameKey(parts[2]);
        if (!game_key) {
            wxLogDebug("Failed to parse game_key: %s", parts[2]);
            return nonstd::nullopt;
        }

        return Command(GameCommand(GameJoy(joypad), *game_key));
    } else {
        if (parts.size() != 2) {
            wxLogDebug("Wrong split size: %d", parts.size());
            return nonstd::nullopt;
        }

        const auto iter = std::lower_bound(cmdtab.begin(), cmdtab.end(),
                                           cmditem{parts[1], wxString(), 0, 0, NULL}, cmditem_lt);
        if (iter == cmdtab.end() || iter->cmd != parts[1]) {
            wxLogDebug("Command ID %s not found", parts[1]);
            return nonstd::nullopt;
        }

        return Command(ShortcutCommand(iter->cmd_id));
    }
}

}  // namespace config

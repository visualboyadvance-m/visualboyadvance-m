#include "qt/config/command.h"

#include <map>

#include <QCoreApplication>

#include "qt/config/cmdtab.h"
#include "qt/config/strutils.h"
#include "qt/log.h"

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

QString GameKeyToUxString(const GameKey& game_key) {
    // This array's order determines tab order as well
    static const std::array<const char*, kNbGameKeys> kGameKeyStrings = {
        "Up",         "Down",        "Left",        "Right",        "A",
        "B",          "L",           "R",           "Select",       "Start",
        "Motion Up",  "Motion Down", "Motion Left", "Motion Right", "Motion In",
        "Motion Out", "Auto A",      "Auto B",      "Speed",        "Capture",
        "GameShark",
    };
    return QCoreApplication::translate("vbam", kGameKeyStrings[GameKeyToInt(game_key)]);
}

}  // namespace

// clang-format off
QString GameKeyToString(const GameKey& game_key) {
    // Note: this must match GUI widget names or GUI won't work
    // This array's order determines tab order as well
    static const std::array<QString, kNbGameKeys> kGameKeyStrings = {
        QStringLiteral("Up"),
        QStringLiteral("Down"),
        QStringLiteral("Left"),
        QStringLiteral("Right"),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("L"),
        QStringLiteral("R"),
        QStringLiteral("Select"),
        QStringLiteral("Start"),
        QStringLiteral("MotionUp"),
        QStringLiteral("MotionDown"),
        QStringLiteral("MotionLeft"),
        QStringLiteral("MotionRight"),
        QStringLiteral("MotionIn"),
        QStringLiteral("MotionOut"),
        QStringLiteral("AutoA"),
        QStringLiteral("AutoB"),
        QStringLiteral("Speed"),
        QStringLiteral("Capture"),
        QStringLiteral("GS"),
    };
    return kGameKeyStrings[GameKeyToInt(game_key)];
}

nonstd::optional<GameKey> StringToGameKey(const QString& input) {
    static const std::map<QString, GameKey> kStringToGameKey = {
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

QString GameCommand::ToConfigString() const {
    return QStringLiteral("Joypad/%1/%2")
        .arg(static_cast<qulonglong>(joypad_.ux_index()))
        .arg(GameKeyToString(game_key_));
}

QString GameCommand::ToUXString() const {
    return QCoreApplication::translate("vbam", "Joypad %1 %2")
        .arg(static_cast<qulonglong>(joypad_.ux_index()))
        .arg(GameKeyToUxString(game_key()));
}

QString ShortcutCommand::ToConfigString() const {
    return GetCommandINIEntry(id_);
}

// static
nonstd::optional<Command> Command::FromString(const QString& name) {
    static const QString kKeyboard("Keyboard");
    static const QString kJoypad("Joypad");

    const bool is_keyboard = name.startsWith(kKeyboard);
    const bool is_joypad = name.startsWith(kJoypad);
    if (!is_keyboard && !is_joypad) {
        vbam::LogDebug("Doesn't start with joypad or keyboard");
        return nonstd::nullopt;
    }

    const QStringList parts = config::str_split(name, "/");
    if (is_joypad) {
        if (parts.size() != 3) {
            vbam::LogDebug(QStringLiteral("Wrong split size: %1").arg(parts.size()));
            return nonstd::nullopt;
        }

        if (parts[1].isEmpty()) {
            return nonstd::nullopt;
        }
        const int joypad = parts[1][0].toLatin1() - '1';
        if (!JoypadInRange(joypad)) {
            vbam::LogDebug(QStringLiteral("Wrong joypad index: %1").arg(joypad));
            return nonstd::nullopt;
        }

        const nonstd::optional<GameKey> game_key = StringToGameKey(parts[2]);
        if (!game_key) {
            vbam::LogDebug(QStringLiteral("Failed to parse game_key: %1").arg(parts[2]));
            return nonstd::nullopt;
        }

        return Command(GameCommand(GameJoy(joypad), *game_key));
    } else {
        if (parts.size() != 2) {
            vbam::LogDebug(QStringLiteral("Wrong split size: %1").arg(parts.size()));
            return nonstd::nullopt;
        }

        const auto cmd_id = CommandFromConfigString(parts[1]);
        if (!cmd_id.has_value()) {
            vbam::LogDebug(QStringLiteral("Command ID %1 not found").arg(parts[1]));
            return nonstd::nullopt;
        }

        return Command(ShortcutCommand(cmd_id.value()));
    }
}

}  // namespace config

#include "wx/config/command.h"

#include <gtest/gtest.h>

#include <wx/log.h>

TEST(GameCommandTest, Basic) {
    config::GameCommand command(config::GameJoy(0), config::GameKey::Up);

    EXPECT_EQ(command.joypad(), config::GameJoy(0));
    EXPECT_EQ(command.game_key(), config::GameKey::Up);
    EXPECT_EQ(command.ToConfigString(), "Joypad/1/Up");
}

TEST(ShortcutCommandTest, Basic) {
    config::ShortcutCommand command(wxID_OPEN);

    EXPECT_EQ(command.id(), wxID_OPEN);
    EXPECT_EQ(command.ToConfigString(), "Keyboard/OPEN");
}

TEST(CommandTest, FromString) {
    const auto game_command = config::Command::FromString("Joypad/1/Up");
    ASSERT_TRUE(game_command.has_value());
    ASSERT_TRUE(game_command->is_game());

    const config::GameCommand& game = game_command->game();
    EXPECT_EQ(game.joypad(), config::GameJoy(0));
    EXPECT_EQ(game.game_key(), config::GameKey::Up);

    const auto shortcut_command = config::Command::FromString("Keyboard/OPEN");
    ASSERT_TRUE(shortcut_command.has_value());
    ASSERT_TRUE(shortcut_command->is_shortcut());

    const config::ShortcutCommand& shortcut = shortcut_command->shortcut();
    EXPECT_EQ(shortcut.id(), wxID_OPEN);
}

TEST(CommandTest, FromStringInvalid) {
    // Need to disable logging to test for errors.
    const wxLogNull disable_logging;

    const auto game_command = config::Command::FromString("Joypad/1/Invalid");
    EXPECT_FALSE(game_command.has_value());

    const auto shortcut_command = config::Command::FromString("Keyboard/INVALID");
    EXPECT_FALSE(shortcut_command.has_value());

    const auto invalid_command = config::Command::FromString("INVALID");
    EXPECT_FALSE(invalid_command.has_value());
}

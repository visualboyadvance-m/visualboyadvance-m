#include "wx/config/bindings.h"

#include <gtest/gtest.h>

#include <wx/xrc/xmlres.h>
#include "wx/config/command.h"
#include "wx/config/user-input.h"

TEST(BindingsTest, Default) {
    const config::Bindings bindings;

    // Check that the default bindings are set up correctly.
    auto inputs =
        bindings.InputsForCommand(config::GameCommand(config::GameJoy(0), config::GameKey::Up));
    EXPECT_TRUE(inputs.find(config::KeyboardInput('W')) != inputs.end());
    EXPECT_TRUE(inputs.find(config::JoyInput(config::JoyId(0), config::JoyControl::HatNorth, 0)) !=
                inputs.end());

    inputs = bindings.InputsForCommand(config::ShortcutCommand(wxID_CLOSE));
    EXPECT_TRUE(inputs.find(config::KeyboardInput('W', wxMOD_CMD)) != inputs.end());

    inputs = bindings.InputsForCommand(config::ShortcutCommand(XRCID("LoadGame01")));
    EXPECT_TRUE(inputs.find(config::KeyboardInput(WXK_F1)) != inputs.end());

    // Check that the INI configuration for the keyboard is empty.
    const auto config = bindings.GetKeyboardConfiguration();
    EXPECT_TRUE(config.empty());
}

// Tests that assigning a default input to another command generates the right
// configuration.
TEST(BindingsTest, AssignDefault) {
    config::Bindings bindings;

    // Assign F1 to the "Close" command.
    bindings.AssignInputToCommand(config::KeyboardInput(WXK_F1),
                                  config::ShortcutCommand(wxID_CLOSE));

    // The INI configuration should have NOOP set to F1, and Close set to F1.
    const auto config = bindings.GetKeyboardConfiguration();
    EXPECT_EQ(config.size(), 2);
    EXPECT_EQ(config[0].first, "Keyboard/NOOP");
    EXPECT_EQ(config[0].second, "F1");
    EXPECT_EQ(config[1].first, "Keyboard/CLOSE");
    EXPECT_EQ(config[1].second, "F1");
}

// Tests that unassigning a default input generates the right configuration.
TEST(BindingsTest, UnassignDefault) {
    config::Bindings bindings;

    // Unassign F1.
    bindings.UnassignInput(config::KeyboardInput(WXK_F1));

    // The INI configuration should have NOOP set to F1.
    const auto config = bindings.GetKeyboardConfiguration();
    EXPECT_EQ(config.size(), 1);
    EXPECT_EQ(config[0].first, "Keyboard/NOOP");
    EXPECT_EQ(config[0].second, "F1");
}

// Tests that re-assigning a default input to its default command generates the
// right configuration.
TEST(BindingsTest, ReassignDefault) {
    config::Bindings bindings;

    // Assign F1 to the "Close" command.
    bindings.AssignInputToCommand(config::KeyboardInput(WXK_F1),
                                  config::ShortcutCommand(wxID_CLOSE));

    // Re-assign F1 to the "LoadGame01" command.
    bindings.AssignInputToCommand(config::KeyboardInput(WXK_F1),
                                  config::ShortcutCommand(XRCID("LoadGame01")));

    // The INI configuration should be empty.
    const auto config = bindings.GetKeyboardConfiguration();
    EXPECT_TRUE(config.empty());
}

// Tests that assigning an input to "NOOP" properly disables the default input.
TEST(BindingsTest, AssignToNoop) {
    config::Bindings bindings;

    // Assign F1 to the "NOOP" command.
    bindings.AssignInputToCommand(config::KeyboardInput(WXK_F1),
                                  config::ShortcutCommand(XRCID("NOOP")));

    const auto command = bindings.CommandForInput(config::KeyboardInput(WXK_F1));
    EXPECT_FALSE(command.has_value());

    // The INI configuration should have NOOP set to F1 and nothing more.
    const auto config = bindings.GetKeyboardConfiguration();
    EXPECT_EQ(config.size(), 1);
    EXPECT_EQ(config[0].first, "Keyboard/NOOP");
    EXPECT_EQ(config[0].second, "F1");
}
 
// Tests that assigning an input not used as a default shortcut to "NOOP" does
// nothing.
TEST(BindingsTest, AssignUnusedToNoop) {
    config::Bindings bindings;

    // Assign "T" to the "NOOP" command.
    bindings.AssignInputToCommand(config::KeyboardInput('T'), config::ShortcutCommand(XRCID("NOOP")));

    // The INI configuration should be empty.
    const auto config = bindings.GetKeyboardConfiguration();
    EXPECT_TRUE(config.empty());

    // "T" should have no assignment.
    const auto command = bindings.CommandForInput(config::KeyboardInput('T'));
    EXPECT_FALSE(command.has_value());
}

// Tests that assigning a default input to a Game command works as expected.
TEST(BindingsTest, AssignDefaultToGame) {
    config::Bindings bindings;

    // Assign F1 to the "Up" command and clear all of the default input for the
    // "Up" command.
    bindings.AssignInputsToCommand({config::KeyboardInput(WXK_F1)},
                                   config::GameCommand(config::GameJoy(0), config::GameKey::Up));

    // The INI configuration should have NOOP set to F1.
    const auto config = bindings.GetKeyboardConfiguration();
    EXPECT_EQ(config.size(), 1);
    EXPECT_EQ(config[0].first, "Keyboard/NOOP");
    EXPECT_EQ(config[0].second, "F1");

    EXPECT_EQ(
        bindings.InputsForCommand(config::GameCommand(config::GameJoy(0), config::GameKey::Up)),
        std::unordered_set<config::UserInput>{config::KeyboardInput(WXK_F1)});

    EXPECT_EQ(bindings.CommandForInput(config::KeyboardInput(WXK_F1)),
              config::Command(config::GameCommand(config::GameJoy(0), config::GameKey::Up)));
}

// Tests the "ClearCommandAssignments" method.
TEST(BindingsTest, ClearCommand) {
    config::Bindings bindings;

    // Clear "CLOSE" assignments.
    bindings.ClearCommandAssignments(config::ShortcutCommand(wxID_CLOSE));

    // The INI configuration should only have the NOOP assignment.
    const auto config = bindings.GetKeyboardConfiguration();
    EXPECT_EQ(config.size(), 1);
    EXPECT_EQ(config[0].first, "Keyboard/NOOP");
    EXPECT_EQ(config[0].second, "CTRL+W");

    EXPECT_TRUE(bindings.InputsForCommand(config::ShortcutCommand(wxID_CLOSE)).empty());
}

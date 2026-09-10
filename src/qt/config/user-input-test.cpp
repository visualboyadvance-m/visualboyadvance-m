#include "qt/config/user-input.h"

#include <gtest/gtest.h>

#include <QString>

TEST(KeyboardInputTest, Basic) {
    config::KeyboardInput input(Qt::Key_F1);

    EXPECT_EQ(input.key(), Qt::Key_F1);
    EXPECT_EQ(input.mod(), config::kKeyModNone);
    EXPECT_EQ(input.ToConfigString(), "F1");
}

TEST(KeyboardInputTest, Modifiers) {
    config::KeyboardInput input(Qt::Key_F1, config::kKeyModShift);
    EXPECT_EQ(input.key(), Qt::Key_F1);
    EXPECT_EQ(input.mod(), config::kKeyModShift);
    EXPECT_EQ(input.ToConfigString(), "SHIFT+F1");
}

TEST(KeyboardInputTest, Char) {
    config::KeyboardInput input('A');

    EXPECT_EQ(input.key(), 'A');
    EXPECT_EQ(input.mod(), config::kKeyModNone);
    EXPECT_EQ(input.ToConfigString(), "A");
}

TEST(KeyboardInputTest, MultipleModifiers) {
    config::KeyboardInput input('A', config::kKeyModShift | config::kKeyModControl);

    EXPECT_EQ(input.key(), 'A');
    EXPECT_EQ(input.mod(), config::kKeyModShift | config::kKeyModControl);
    EXPECT_EQ(input.ToConfigString(), "CTRL+SHIFT+A");
}

TEST(KeyboardInputTest, ModOnly) {
    config::KeyboardInput input(Qt::Key_Shift, config::kKeyModShift);

    EXPECT_EQ(input.key(), Qt::Key_Shift);
    EXPECT_EQ(input.mod(), config::kKeyModShift);
    EXPECT_EQ(input.ToConfigString(), "SHIFT");
}

TEST(KeyboardInputTest, ExtendedModifiers) {
    config::KeyboardInput input('A', config::kKeyModLeftControl);
    EXPECT_TRUE(input.has_extended_modifiers());
    EXPECT_EQ(input.mod(), config::kKeyModControl);
    EXPECT_EQ(input.ToConfigString(), "LCTRL+A");

    // LCTRL+A matches CTRL+A but not RCTRL+A.
    EXPECT_EQ(input, config::KeyboardInput('A', config::kKeyModControl));
    EXPECT_NE(input, config::KeyboardInput('A', config::kKeyModRightControl));

    config::KeyboardInput lshift(Qt::Key_Shift, config::kKeyModLeftShift);
    EXPECT_EQ(lshift.ToConfigString(), "LSHIFT");
}

TEST(KeyboardInputTest, NamedKeys) {
    EXPECT_EQ(config::KeyboardInput(Qt::Key_Return).ToConfigString(), "RETURN");
    EXPECT_EQ(config::KeyboardInput(Qt::Key_Enter).ToConfigString(), "KP_ENTER");
    EXPECT_EQ(config::KeyboardInput(Qt::Key_Backspace).ToConfigString(), "BACK");
    EXPECT_EQ(config::KeyboardInput(Qt::Key_Space).ToConfigString(), "SPACE");
    EXPECT_EQ(config::KeyboardInput(Qt::Key_PageUp, config::kKeyModAlt).ToConfigString(),
              "ALT+PAGEUP");
}

TEST(KeyboardInputTest, FromBasicConfigString) {
    const std::unordered_set<config::UserInput> inputs = config::UserInput::FromConfigString("F1");
    ASSERT_EQ(inputs.size(), 1u);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_keyboard());

    EXPECT_EQ(input.keyboard_input().key(), Qt::Key_F1);
    EXPECT_EQ(input.keyboard_input().mod(), config::kKeyModNone);
}

TEST(KeyboardInputTest, FromConfigStringWithModifiers) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("SHIFT+F1");
    ASSERT_EQ(inputs.size(), 1u);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_keyboard());

    EXPECT_EQ(input.keyboard_input().key(), Qt::Key_F1);
    EXPECT_EQ(input.keyboard_input().mod(), config::kKeyModShift);
}

TEST(KeyboardInputTest, FromConfigStringLegacyNames) {
    // wx / legacy spellings must still parse.
    auto inputs = config::UserInput::FromConfigString("ESCAPE");
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(inputs.begin()->keyboard_input().key(), Qt::Key_Escape);

    inputs = config::UserInput::FromConfigString("KP_ENTER");
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(inputs.begin()->keyboard_input().key(), Qt::Key_Enter);

    inputs = config::UserInput::FromConfigString("lctrl+shift+return");
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(inputs.begin()->keyboard_input().key(), Qt::Key_Return);
    EXPECT_EQ(inputs.begin()->keyboard_input().mod_extended(),
              config::kKeyModLeftControl | config::kKeyModShift);
}

TEST(KeyboardInputTest, FromCommaConfigString) {
    // A standalone comma should parse as a keyboard input.
    const std::unordered_set<config::UserInput> inputs = config::UserInput::FromConfigString(",");
    ASSERT_EQ(inputs.size(), 1u);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_keyboard());

    EXPECT_EQ(input.keyboard_input().key(), ',');
    EXPECT_EQ(input.keyboard_input().mod(), config::kKeyModNone);
}

TEST(KeyboardInputTest, CommaConfigString) {
    const config::KeyboardInput input(',');
    EXPECT_EQ(input.key(), ',');
    EXPECT_EQ(input.mod(), config::kKeyModNone);
    EXPECT_EQ(input.ToConfigString(), "44:0");

    // Parse the config string back.
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("44:0,44:1");
    ASSERT_EQ(inputs.size(), 2u);

    EXPECT_TRUE(inputs.find(config::KeyboardInput(',', config::kKeyModNone)) != inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput(',', config::kKeyModAlt)) != inputs.end());
}

TEST(KeyboardInputTest, FromColonConfigString) {
    // A standalone colon should parse as a keyboard input.
    const std::unordered_set<config::UserInput> inputs = config::UserInput::FromConfigString(":");
    ASSERT_EQ(inputs.size(), 1u);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_keyboard());

    EXPECT_EQ(input.keyboard_input().key(), ':');
    EXPECT_EQ(input.keyboard_input().mod(), config::kKeyModNone);
}

TEST(KeyboardInputTest, ColonConfigString) {
    const config::KeyboardInput input(':');
    EXPECT_EQ(input.key(), ':');
    EXPECT_EQ(input.mod(), config::kKeyModNone);
    EXPECT_EQ(input.ToConfigString(), "58:0");

    // Parse the config string back.
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("58:0,58:1");
    ASSERT_EQ(inputs.size(), 2u);

    EXPECT_TRUE(inputs.find(config::KeyboardInput(':', config::kKeyModNone)) != inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput(':', config::kKeyModAlt)) != inputs.end());
}

TEST(KeyboardInputTest, ColonCommaConfigString) {
    const std::unordered_set<config::UserInput> inputs = config::UserInput::FromConfigString(",,:");
    ASSERT_EQ(inputs.size(), 2u);

    EXPECT_TRUE(inputs.find(config::KeyboardInput(',', config::kKeyModNone)) != inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput(':', config::kKeyModNone)) != inputs.end());
}

// Every config string must parse back to the input it came from.
TEST(KeyboardInputTest, RoundTrip) {
    const std::vector<config::KeyboardInput> inputs = {
        config::KeyboardInput('A'),
        config::KeyboardInput('Z', config::kKeyModControl | config::kKeyModAlt),
        config::KeyboardInput('5', config::kKeyModLeftShift),
        config::KeyboardInput(Qt::Key_F12, config::kKeyModMeta),
        config::KeyboardInput(Qt::Key_Return, config::kKeyModAlt),
        config::KeyboardInput(Qt::Key_Enter),
        config::KeyboardInput(Qt::Key_Escape),
        config::KeyboardInput(Qt::Key_Backspace),
        config::KeyboardInput(Qt::Key_Pause),
        config::KeyboardInput(Qt::Key_Plus),
        config::KeyboardInput(Qt::Key_Minus),
        config::KeyboardInput(Qt::Key_Comma, config::kKeyModShift),
        config::KeyboardInput(Qt::Key_Shift, config::kKeyModShift),
        config::KeyboardInput(Qt::Key_Control, config::kKeyModRightControl),
        config::KeyboardInput(Qt::Key_Meta, config::kKeyModMeta),
        config::KeyboardInput(Qt::Key_VolumeUp),
        config::KeyboardInput(Qt::Key_Eacute),  // unnamed: numeric form
    };
    for (const auto& input : inputs) {
        const QString config = input.ToConfigString();
        const auto parsed = config::UserInput::FromConfigString(config);
        ASSERT_EQ(parsed.size(), 1u) << config.toStdString();
        ASSERT_TRUE(parsed.begin()->is_keyboard()) << config.toStdString();
        EXPECT_EQ(parsed.begin()->keyboard_input(), input) << config.toStdString();
        EXPECT_EQ(parsed.begin()->keyboard_input().mod_extended(), input.mod_extended())
            << config.toStdString();
    }
}

TEST(JoyInputTest, Basic) {
    config::JoyInput input(config::JoyId(0), config::JoyControl::AxisPlus, 2);

    EXPECT_EQ(input.joy(), config::JoyId(0));
    EXPECT_EQ(input.control(), config::JoyControl::AxisPlus);
    EXPECT_EQ(input.control_index(), 2);
    EXPECT_EQ(input.ToConfigString(), "Joy1-Axis2+");
}

TEST(JoyInputTest, FromAxisConfigString) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("Joy1-Axis2+");
    ASSERT_EQ(inputs.size(), 1u);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_joystick());

    EXPECT_EQ(input.joy_input().joy(), config::JoyId(0));
    EXPECT_EQ(input.joy_input().control(), config::JoyControl::AxisPlus);
    EXPECT_EQ(input.joy_input().control_index(), 2);
}

TEST(JoyInputTest, FromHatConfigString) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("Joy1-Hat0N");
    ASSERT_EQ(inputs.size(), 1u);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_joystick());

    EXPECT_EQ(input.joy_input().joy(), config::JoyId(0));
    EXPECT_EQ(input.joy_input().control(), config::JoyControl::HatNorth);
    EXPECT_EQ(input.joy_input().control_index(), 0);
}

TEST(JoyInputTest, FromButtonConfigString) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("Joy10-Button20");
    ASSERT_EQ(inputs.size(), 1u);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_joystick());

    EXPECT_EQ(input.joy_input().joy(), config::JoyId(9));
    EXPECT_EQ(input.joy_input().control(), config::JoyControl::Button);
    EXPECT_EQ(input.joy_input().control_index(), 20);
}

TEST(JoyInputTest, RoundTrip) {
    const std::vector<config::JoyInput> inputs = {
        config::JoyInput(config::JoyId(0), config::JoyControl::AxisPlus, 2),
        config::JoyInput(config::JoyId(3), config::JoyControl::AxisMinus, 0),
        config::JoyInput(config::JoyId(1), config::JoyControl::Button, 14),
        config::JoyInput(config::JoyId(2), config::JoyControl::HatNorth, 1),
        config::JoyInput(config::JoyId(2), config::JoyControl::HatSouth, 1),
        config::JoyInput(config::JoyId(2), config::JoyControl::HatEast, 0),
        config::JoyInput(config::JoyId(2), config::JoyControl::HatWest, 0),
    };
    for (const auto& input : inputs) {
        const auto parsed = config::UserInput::FromConfigString(input.ToConfigString());
        ASSERT_EQ(parsed.size(), 1u);
        ASSERT_TRUE(parsed.begin()->is_joystick());
        EXPECT_EQ(parsed.begin()->joy_input(), input);
    }
}

TEST(UserInputTest, FromMixedConfigString) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("CTRL+SHIFT+F1,Joy1-Axis2+,A,SHIFT");
    ASSERT_EQ(inputs.size(), 4u);

    EXPECT_TRUE(inputs.find(config::KeyboardInput(
                    Qt::Key_F1, config::kKeyModShift | config::kKeyModControl)) != inputs.end());
    EXPECT_TRUE(inputs.find(config::JoyInput(config::JoyId(0), config::JoyControl::AxisPlus, 2)) !=
                inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput('A')) != inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput(Qt::Key_Shift, config::kKeyModShift)) !=
                inputs.end());
}

TEST(UserInputTest, SpanToConfigString) {
    const std::unordered_set<config::UserInput> inputs = {
        config::KeyboardInput(Qt::Key_F1),
        config::JoyInput(config::JoyId(0), config::JoyControl::Button, 3),
    };
    const QString config = config::UserInput::SpanToConfigString(inputs);
    EXPECT_EQ(config::UserInput::FromConfigString(config), inputs);
    EXPECT_TRUE(config::UserInput::SpanToConfigString({}).isEmpty());
}

TEST(UserInputTest, InvalidConfigString) {
    EXPECT_TRUE(config::UserInput::FromConfigString("").empty());
    // While this should parse as "SHIFT+," and ",", the comma handling can't parse this.
    EXPECT_TRUE(config::UserInput::FromConfigString("SHIFT+,,,").empty());
    EXPECT_TRUE(config::UserInput::FromConfigString("CTRL+SHIFT+F1,asdf").empty());
    EXPECT_TRUE(config::UserInput::FromConfigString("abc").empty());
    EXPECT_TRUE(config::UserInput::FromConfigString("Joy1-Axis2").empty());
    EXPECT_TRUE(config::UserInput::FromConfigString("Joy1-Button").empty());
    EXPECT_TRUE(config::UserInput::FromConfigString("Joy1-HatN").empty());
    EXPECT_TRUE(config::UserInput::FromConfigString("Joy1-Hat0").empty());
}

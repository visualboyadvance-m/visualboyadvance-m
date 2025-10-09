#include "wx/config/user-input.h"

#include <gtest/gtest.h>
#include <wx/string.h>

TEST(KeyboardInputTest, Basic) {
    config::KeyboardInput input(wxKeyCode::WXK_F1);

    EXPECT_EQ(input.key(), wxKeyCode::WXK_F1);
    EXPECT_EQ(input.mod(), wxKeyModifier::wxMOD_NONE);
    EXPECT_EQ(input.ToConfigString(), "F1");
}

TEST(KeyboardInputTest, Modifiers) {
    config::KeyboardInput input(wxKeyCode::WXK_F1, wxMOD_SHIFT);
    EXPECT_EQ(input.key(), wxKeyCode::WXK_F1);
    EXPECT_EQ(input.mod(), wxMOD_SHIFT);
    EXPECT_EQ(input.ToConfigString(), "SHIFT+F1");
}

TEST(KeyboardInputTest, Char) {
    config::KeyboardInput input('A');

    EXPECT_EQ(input.key(), 'A');
    EXPECT_EQ(input.mod(), wxKeyModifier::wxMOD_NONE);
    EXPECT_EQ(input.ToConfigString(), "A");
}

TEST(KeyboardInputTest, MultipleModifiers) {
    config::KeyboardInput input('A', static_cast<wxKeyModifier>(wxMOD_SHIFT | wxMOD_CONTROL));

    EXPECT_EQ(input.key(), 'A');
    EXPECT_EQ(input.mod(), static_cast<wxKeyModifier>(wxMOD_SHIFT | wxMOD_CONTROL));
    EXPECT_EQ(input.ToConfigString(), "CTRL+SHIFT+A");
}

TEST(KeyboardInputTest, ModOnly) {
    config::KeyboardInput input(wxKeyCode::WXK_SHIFT, wxMOD_SHIFT);

    EXPECT_EQ(input.key(), wxKeyCode::WXK_SHIFT);
    EXPECT_EQ(input.mod(), wxMOD_SHIFT);
    EXPECT_EQ(input.ToConfigString(), "SHIFT");
}

TEST(KeyboardInputTest, FromBasicConfigString) {
    const std::unordered_set<config::UserInput> inputs = config::UserInput::FromConfigString("F1");
    ASSERT_EQ(inputs.size(), 1);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_keyboard());

    EXPECT_EQ(input.keyboard_input().key(), wxKeyCode::WXK_F1);
    EXPECT_EQ(input.keyboard_input().mod(), wxKeyModifier::wxMOD_NONE);
}

TEST(KeyboardInputTest, FromConfigStringWithModifiers) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("SHIFT+F1");
    ASSERT_EQ(inputs.size(), 1);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_keyboard());

    EXPECT_EQ(input.keyboard_input().key(), wxKeyCode::WXK_F1);
    EXPECT_EQ(input.keyboard_input().mod(), wxMOD_SHIFT);
}

TEST(KeyboardInputTest, FromCommaConfigString) {
    // A standalone comma should parse as a keyboard input.
    const std::unordered_set<config::UserInput> inputs = config::UserInput::FromConfigString(",");
    ASSERT_EQ(inputs.size(), 1);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_keyboard());

    EXPECT_EQ(input.keyboard_input().key(), ',');
    EXPECT_EQ(input.keyboard_input().mod(), wxMOD_NONE);
}

TEST(KeyboardInputTest, CommaConfigString) {
    const config::KeyboardInput input(',');
    EXPECT_EQ(input.key(), ',');
    EXPECT_EQ(input.mod(), wxMOD_NONE);
    EXPECT_EQ(input.ToConfigString(), "44:0");

    // Parse the config string back.
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("44:0,44:1");
    ASSERT_EQ(inputs.size(), 2);

    EXPECT_TRUE(inputs.find(config::KeyboardInput(',', wxMOD_NONE)) != inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput(',', wxMOD_ALT)) != inputs.end());
}

TEST(KeyboardInputTest, FromColonConfigString) {
    // A standalone colon should parse as a keyboard input.
    const std::unordered_set<config::UserInput> inputs = config::UserInput::FromConfigString(":");
    ASSERT_EQ(inputs.size(), 1);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_keyboard());

    EXPECT_EQ(input.keyboard_input().key(), ':');
    EXPECT_EQ(input.keyboard_input().mod(), wxMOD_NONE);
}

TEST(KeyboardInputTest, ColonConfigString) {
    const config::KeyboardInput input(':');
    EXPECT_EQ(input.key(), ':');
    EXPECT_EQ(input.mod(), wxMOD_NONE);
    EXPECT_EQ(input.ToConfigString(), "58:0");

    // Parse the config string back.
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("58:0,58:1");
    ASSERT_EQ(inputs.size(), 2);

    EXPECT_TRUE(inputs.find(config::KeyboardInput(':', wxMOD_NONE)) != inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput(':', wxMOD_ALT)) != inputs.end());
}

TEST(KeyboardInputTest, ColonCommaConfigString) {
    const std::unordered_set<config::UserInput> inputs = config::UserInput::FromConfigString(",,:");
    ASSERT_EQ(inputs.size(), 2);

    EXPECT_TRUE(inputs.find(config::KeyboardInput(',', wxMOD_NONE)) != inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput(':', wxMOD_NONE)) != inputs.end());
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
    ASSERT_EQ(inputs.size(), 1);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_joystick());

    EXPECT_EQ(input.joy_input().joy(), config::JoyId(0));
    EXPECT_EQ(input.joy_input().control(), config::JoyControl::AxisPlus);
    EXPECT_EQ(input.joy_input().control_index(), 2);
}

TEST(JoyInputTest, FromHatConfigString) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("Joy1-Hat0N");
    ASSERT_EQ(inputs.size(), 1);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_joystick());

    EXPECT_EQ(input.joy_input().joy(), config::JoyId(0));
    EXPECT_EQ(input.joy_input().control(), config::JoyControl::HatNorth);
    EXPECT_EQ(input.joy_input().control_index(), 0);
}

TEST(JoyInputTest, FromButtonConfigString) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("Joy10-Button20");
    ASSERT_EQ(inputs.size(), 1);

    const config::UserInput input = *inputs.begin();
    ASSERT_TRUE(input.is_joystick());

    EXPECT_EQ(input.joy_input().joy(), config::JoyId(9));
    EXPECT_EQ(input.joy_input().control(), config::JoyControl::Button);
    EXPECT_EQ(input.joy_input().control_index(), 20);
}

TEST(UserInputTest, FromMixedConfigString) {
    const std::unordered_set<config::UserInput> inputs =
        config::UserInput::FromConfigString("CTRL+SHIFT+F1,Joy1-Axis2+,A,SHIFT");
    ASSERT_EQ(inputs.size(), 4);

    EXPECT_TRUE(inputs.find(config::KeyboardInput(
                    wxKeyCode::WXK_F1, static_cast<wxKeyModifier>(wxMOD_SHIFT | wxMOD_CONTROL))) !=
                inputs.end());
    EXPECT_TRUE(inputs.find(config::JoyInput(config::JoyId(0), config::JoyControl::AxisPlus, 2)) !=
                inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput('A')) != inputs.end());
    EXPECT_TRUE(inputs.find(config::KeyboardInput(wxKeyCode::WXK_SHIFT, wxMOD_SHIFT)) !=
                inputs.end());
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

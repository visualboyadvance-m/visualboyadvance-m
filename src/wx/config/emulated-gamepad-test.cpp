#include "wx/config/emulated-gamepad.h"

#include <gtest/gtest.h>
#include <wx/string.h>
#include <memory>
#include "wx/config/bindings.h"
#include "wx/config/command.h"
#include "wx/config/user-input.h"

#define KEYM_UP 1 << 6

// Test fixture to set up the EmulatedGamepad with default bindings.
class GamepadTest : public ::testing::Test {
public:
    GamepadTest() = default;
    ~GamepadTest() override = default;

protected:
    void SetUp() override {
        gamepad_ =
            std::make_unique<config::EmulatedGamepad>(std::bind(&GamepadTest::bindings, this));
    }

    void TearDown() override { gamepad_.reset(); }

    config::Bindings* bindings() { return &bindings_; }
    config::EmulatedGamepad* gamepad() { return gamepad_.get(); }

private:
    config::Bindings bindings_;
    std::unique_ptr<config::EmulatedGamepad> gamepad_;
};

TEST_F(GamepadTest, Basic) {
    // No input should be pressed initially.
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);

    const config::JoyInput up(config::JoyId(0), config::JoyControl::HatNorth, 0);

    // Press up, the up key should be pressed.
    EXPECT_TRUE(gamepad()->OnInputPressed(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Release up, the up key should be released.
    EXPECT_TRUE(gamepad()->OnInputReleased(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);
}

// Tests that pressing multiple bindings at the same time keeps the keys pressed
// until all corresponding bindings are released.
TEST_F(GamepadTest, ManyBindingsPressed) {
    const config::JoyInput up(config::JoyId(0), config::JoyControl::HatNorth, 0);
    const config::KeyboardInput w('W');

    // Press up, the up key should be pressed.
    EXPECT_TRUE(gamepad()->OnInputPressed(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Press W, the up key should still be pressed.
    EXPECT_TRUE(gamepad()->OnInputPressed(w));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Release up, the up key should still be pressed.
    EXPECT_TRUE(gamepad()->OnInputReleased(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Release w, the up key should be released.
    EXPECT_TRUE(gamepad()->OnInputReleased(w));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);
}

// Tests that pressing the same binding twice is a noop.
TEST_F(GamepadTest, DoublePress) {
    const config::JoyInput up(config::JoyId(0), config::JoyControl::HatNorth, 0);

    // Press up, the up key should be pressed.
    EXPECT_TRUE(gamepad()->OnInputPressed(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Press up again, the up key should still be pressed.
    EXPECT_TRUE(gamepad()->OnInputPressed(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Release up, the up key should be released.
    EXPECT_TRUE(gamepad()->OnInputReleased(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);
}

// Tests that releasing the same biniding twice is a noop.
TEST_F(GamepadTest, DoubleRelease) {
    const config::JoyInput up(config::JoyId(0), config::JoyControl::HatNorth, 0);

    // Press up, the up key should be pressed.
    EXPECT_TRUE(gamepad()->OnInputPressed(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Release up, the up key should be released.
    EXPECT_TRUE(gamepad()->OnInputReleased(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);

    // Release up again, the up key should still be released.
    EXPECT_TRUE(gamepad()->OnInputReleased(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);
}

// Tests that pressing an unbound input is a noop.
TEST_F(GamepadTest, UnassignedInput) {
    const config::KeyboardInput f1(WXK_F1);
    const config::JoyInput up(config::JoyId(0), config::JoyControl::HatNorth, 0);

    // Press F1, nothing should happen.
    EXPECT_FALSE(gamepad()->OnInputPressed(f1));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);

    // Press up, the up key should be pressed.
    EXPECT_TRUE(gamepad()->OnInputPressed(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Release F1, nothing should happen.
    EXPECT_FALSE(gamepad()->OnInputReleased(f1));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Release up, the up key should be released.
    EXPECT_TRUE(gamepad()->OnInputReleased(up));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);

    // Release F1 again, nothing should happen.
    EXPECT_FALSE(gamepad()->OnInputReleased(f1));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);
}

// Tests that assigning an input to a game command properly triggers the game
// command.
TEST_F(GamepadTest, NonDefaultInput) {
    const config::KeyboardInput f1(WXK_F1);

    // Assign F1 to "Up".
    bindings()->AssignInputToCommand(f1,
                                     config::GameCommand(config::GameJoy(0), config::GameKey::Up));

    // Press F1, the up key should be pressed.
    EXPECT_TRUE(gamepad()->OnInputPressed(f1));
    EXPECT_EQ(gamepad()->GetJoypad(0), KEYM_UP);

    // Release F1, the up key should be released.
    EXPECT_TRUE(gamepad()->OnInputReleased(f1));
    EXPECT_EQ(gamepad()->GetJoypad(0), 0);
}

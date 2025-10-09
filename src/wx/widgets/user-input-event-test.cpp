#include "wx/widgets/user-input-event.h"

#include <gtest/gtest.h>

#include <wx/eventfilter.h>

#include "wx/config/user-input.h"

namespace widgets {

namespace {

static constexpr config::KeyboardInput kF1(wxKeyCode::WXK_F1);
static constexpr config::KeyboardInput kCtrlF1(wxKeyCode::WXK_F1, wxMOD_CONTROL);
static constexpr config::JoyInput kButton0(config::JoyId(0), config::JoyControl::Button, 0);

}  // namespace

TEST(UserInputEventTest, KeyboardInputEvent) {
    // Press Ctrl+F1.
    UserInputEvent pressed_event({{kCtrlF1, true}});
    EXPECT_EQ(pressed_event.FirstReleasedInput(), nonstd::nullopt);

    // Process Ctrl+F1.
    EXPECT_EQ(pressed_event.FilterProcessedInput(kCtrlF1), wxEventFilter::Event_Processed);
    EXPECT_EQ(pressed_event.data().size(), 0);
}

TEST(UserInputEventTest, JoystickInputEvent) {
    // Press button 0.
    UserInputEvent pressed_event({{kButton0, true}});
    EXPECT_EQ(pressed_event.FirstReleasedInput(), nonstd::nullopt);

    // Process button 0.
    EXPECT_EQ(pressed_event.FilterProcessedInput(kButton0), wxEventFilter::Event_Processed);
    EXPECT_EQ(pressed_event.data().size(), 0);
}

TEST(UserInputeventTest, MultipleInput) {
    // Release F1 and Ctrl+F1.
    UserInputEvent pressed_event({{kCtrlF1, false}, {kF1, false}});
    EXPECT_EQ(pressed_event.FirstReleasedInput(), kCtrlF1);

    // Process Ctrl+F1.
    EXPECT_EQ(pressed_event.FilterProcessedInput(kCtrlF1), wxEventFilter::Event_Skip);
    EXPECT_EQ(pressed_event.data().size(), 1);

    // Process button 0.
    EXPECT_EQ(pressed_event.FilterProcessedInput(kF1), wxEventFilter::Event_Processed);
    EXPECT_EQ(pressed_event.data().size(), 0);
}

}  // namespace widgets

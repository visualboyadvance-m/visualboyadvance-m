#include "wx/widgets/keyboard-input-handler.h"

#include <algorithm>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <wx/event.h>

#include "wx/config/user-input.h"
#include "wx/widgets/event-handler-provider.h"
#include "wx/widgets/test/widgets-test.h"
#include "wx/widgets/user-input-event.h"

namespace widgets {

namespace {

class KeyboardInputHandlerTest : public WidgetsTest,
                                 public wxEvtHandler,
                                 public EventHandlerProvider {
public:
    KeyboardInputHandlerTest() : handler_(this) {
        Bind(VBAM_EVT_USER_INPUT, &KeyboardInputHandlerTest::OnUserInputEvent, this);
    }

protected:
    // Processes `key_event` and returns the data from the generated events.
    std::vector<UserInputEvent::Data> ProcessKeyEvent(wxKeyEvent key_event) {
        handler_.ProcessKeyEvent(key_event);
        ProcessPendingEvents();
        std::vector<UserInputEvent::Data> data;
        std::swap(data, pending_data_);
        return data;
    }

    // Track modifier state for creating realistic test events
    void SetModifierState(wxKeyCode modifier, bool pressed) {
        if (pressed) {
            active_test_modifiers_.insert(modifier);
        } else {
            active_test_modifiers_.erase(modifier);
        }
    }

    // Apply current modifier state to an event
    void ApplyModifierState(wxKeyEvent& event) {
        event.m_controlDown = active_test_modifiers_.count(WXK_CONTROL) > 0;
        event.m_shiftDown = active_test_modifiers_.count(WXK_SHIFT) > 0;
        event.m_altDown = active_test_modifiers_.count(WXK_ALT) > 0;
    }

private:
    void OnUserInputEvent(UserInputEvent& event) {
        // Do not let the event propagate.
        event.Skip(false);
        for (const auto& data : event.data()) {
            pending_data_.emplace_back(data);
        }
    }

    // EventHandlerProvider implementation.
    wxEvtHandler* event_handler() override { return this; }

    KeyboardInputHandler handler_;
    std::vector<UserInputEvent::Data> pending_data_;
    std::set<wxKeyCode> active_test_modifiers_;
};

static constexpr config::KeyboardInput kF1(wxKeyCode::WXK_F1);
#ifdef __WXMAC__
// On macOS, Command key (WXK_CONTROL) maps to Meta, not Control
static constexpr config::KeyboardInput kCtrlF1(wxKeyCode::WXK_F1, wxMOD_META);
static constexpr config::KeyboardInput kCtrlShiftF1(wxKeyCode::WXK_F1,
                                                    static_cast<wxKeyModifier>(wxMOD_META |
                                                                               wxMOD_SHIFT));
static constexpr config::KeyboardInput kCtrl(wxKeyCode::WXK_CONTROL, wxMOD_META);
#else
static constexpr config::KeyboardInput kCtrlF1(wxKeyCode::WXK_F1, wxMOD_CONTROL);
static constexpr config::KeyboardInput kCtrlShiftF1(wxKeyCode::WXK_F1,
                                                    static_cast<wxKeyModifier>(wxMOD_CONTROL |
                                                                               wxMOD_SHIFT));
static constexpr config::KeyboardInput kCtrl(wxKeyCode::WXK_CONTROL, wxMOD_CONTROL);
#endif
static constexpr config::KeyboardInput kShift(wxKeyCode::WXK_SHIFT, wxMOD_SHIFT);
static const UserInputEvent::Data kF1Pressed(kF1, true);
static const UserInputEvent::Data kF1Released(kF1, false);
static const UserInputEvent::Data kCtrlF1Pressed(kCtrlF1, true);
static const UserInputEvent::Data kCtrlF1Released(kCtrlF1, false);
static const UserInputEvent::Data kCtrlShiftF1Pressed(kCtrlShiftF1, true);
static const UserInputEvent::Data kCtrlShiftF1Released(kCtrlShiftF1, false);
static const UserInputEvent::Data kCtrlPressed(kCtrl, true);
static const UserInputEvent::Data kCtrlReleased(kCtrl, false);
static const UserInputEvent::Data kShiftPressed(kShift, true);
static const UserInputEvent::Data kShiftReleased(kShift, false);

wxKeyEvent F1DownEvent() {
    wxKeyEvent event(wxEVT_KEY_DOWN);
    event.m_keyCode = WXK_F1;
    return event;
}

wxKeyEvent F1UpEvent() {
    wxKeyEvent event(wxEVT_KEY_UP);
    event.m_keyCode = WXK_F1;
    return event;
}

wxKeyEvent CtrlF1DownEvent() {
    wxKeyEvent event(wxEVT_KEY_DOWN);
    event.m_keyCode = WXK_F1;
    event.m_controlDown = true;
    return event;
}

wxKeyEvent CtrlF1UpEvent() {
    wxKeyEvent event(wxEVT_KEY_UP);
    event.m_keyCode = WXK_F1;
    event.m_controlDown = true;
    return event;
}

wxKeyEvent CtrlShiftF1DownEvent() {
    wxKeyEvent event(wxEVT_KEY_DOWN);
    event.m_keyCode = WXK_F1;
    event.m_controlDown = true;
    event.m_shiftDown = true;
    return event;
}

wxKeyEvent CtrlShiftF1UpEvent() {
    wxKeyEvent event(wxEVT_KEY_UP);
    event.m_keyCode = WXK_F1;
    event.m_controlDown = true;
    event.m_shiftDown = true;
    return event;
}

wxKeyEvent CtrlDownEvent() {
    wxKeyEvent event(wxEVT_KEY_DOWN);
    event.m_keyCode = WXK_CONTROL;
    return event;
}

wxKeyEvent CtrlUpEvent() {
    wxKeyEvent event(wxEVT_KEY_UP);
    event.m_keyCode = WXK_CONTROL;
    return event;
}

wxKeyEvent ShiftDownEvent() {
    wxKeyEvent event(wxEVT_KEY_DOWN);
    event.m_keyCode = WXK_SHIFT;
    return event;
}

wxKeyEvent ShiftUpEvent() {
    wxKeyEvent event(wxEVT_KEY_UP);
    event.m_keyCode = WXK_SHIFT;
    return event;
}

}  // namespace

TEST_F(KeyboardInputHandlerTest, SimpleKeyDownUp) {
    // Send F1 down event.
    ASSERT_THAT(ProcessKeyEvent(F1DownEvent()), testing::ElementsAre(kF1Pressed));

    // Send F1 up event.
    ASSERT_THAT(ProcessKeyEvent(F1UpEvent()), testing::ElementsAre(kF1Released));
}

TEST_F(KeyboardInputHandlerTest, ModifierThenKey) {
    // Ctrl Down -> F1 Down -> F1 Up -> Ctrl Up
    ASSERT_THAT(ProcessKeyEvent(CtrlDownEvent()), testing::ElementsAre(kCtrlPressed));
    ASSERT_THAT(ProcessKeyEvent(CtrlF1DownEvent()),
                testing::ElementsAre(kCtrlF1Pressed, kF1Pressed));
    ASSERT_THAT(ProcessKeyEvent(CtrlF1UpEvent()),
                testing::ElementsAre(kCtrlF1Released, kF1Released));
    ASSERT_THAT(ProcessKeyEvent(CtrlUpEvent()), testing::ElementsAre(kCtrlReleased));
}

TEST_F(KeyboardInputHandlerTest, KeyThenModifier) {
    // F1 Down -> Ctrl Down -> Ctrl Up -> F1 Up
    // In this case, no Ctrl+F1 event should be generated.
    ASSERT_THAT(ProcessKeyEvent(F1DownEvent()), testing::ElementsAre(kF1Pressed));
    ASSERT_THAT(ProcessKeyEvent(CtrlDownEvent()), testing::ElementsAre(kCtrlPressed));
    ASSERT_THAT(ProcessKeyEvent(CtrlUpEvent()), testing::ElementsAre(kCtrlReleased));
    ASSERT_THAT(ProcessKeyEvent(F1UpEvent()), testing::ElementsAre(kF1Released));

    // F1 Down -> Ctrl Down -> F1 Up -> Ctrl Up
    // In this case, a Ctrl+F1 event should be generated when F1 is released.
    ASSERT_THAT(ProcessKeyEvent(F1DownEvent()), testing::ElementsAre(kF1Pressed));
    ASSERT_THAT(ProcessKeyEvent(CtrlDownEvent()), testing::ElementsAre(kCtrlPressed));
    ASSERT_THAT(ProcessKeyEvent(F1UpEvent()),
                testing::ElementsAre(kCtrlF1Pressed, kCtrlF1Released, kF1Released));
    ASSERT_THAT(ProcessKeyEvent(CtrlUpEvent()), testing::ElementsAre(kCtrlReleased));
}

TEST_F(KeyboardInputHandlerTest, Multiplemodifiers) {
    // F1 Down -> Ctrl Down -> Shift Down -> F1 Up -> Ctrl Up -> Shift Up
    // In this case, a Ctrl+Shift+F1 event should be generated when F1 is released.
    ASSERT_THAT(ProcessKeyEvent(F1DownEvent()), testing::ElementsAre(kF1Pressed));

    SetModifierState(WXK_CONTROL, true);
    wxKeyEvent ctrl_down = CtrlDownEvent();
    ApplyModifierState(ctrl_down);
    ASSERT_THAT(ProcessKeyEvent(ctrl_down), testing::ElementsAre(kCtrlPressed));

    SetModifierState(WXK_SHIFT, true);
    wxKeyEvent shift_down = ShiftDownEvent();
    ApplyModifierState(shift_down);
    ASSERT_THAT(ProcessKeyEvent(shift_down), testing::ElementsAre(kShiftPressed));

    wxKeyEvent f1_up = F1UpEvent();
    ApplyModifierState(f1_up);
    ASSERT_THAT(ProcessKeyEvent(f1_up),
                testing::ElementsAre(kCtrlShiftF1Pressed, kCtrlShiftF1Released, kF1Released));

    SetModifierState(WXK_CONTROL, false);
    wxKeyEvent ctrl_up = CtrlUpEvent();
    ApplyModifierState(ctrl_up);
    ASSERT_THAT(ProcessKeyEvent(ctrl_up), testing::ElementsAre(kCtrlReleased));

    SetModifierState(WXK_SHIFT, false);
    wxKeyEvent shift_up = ShiftUpEvent();
    ApplyModifierState(shift_up);
    ASSERT_THAT(ProcessKeyEvent(shift_up), testing::ElementsAre(kShiftReleased));

    // Ctrl Down -> Shift Down -> F1 Down -> F1 Up -> Shift Up -> Ctrl Up
    // In this case, a Ctrl+Shift+F1 event should be generated when F1 is pressed.
    SetModifierState(WXK_CONTROL, true);
    wxKeyEvent ctrl_down2 = CtrlDownEvent();
    ApplyModifierState(ctrl_down2);
    ASSERT_THAT(ProcessKeyEvent(ctrl_down2), testing::ElementsAre(kCtrlPressed));

    SetModifierState(WXK_SHIFT, true);
    wxKeyEvent shift_down2 = ShiftDownEvent();
    ApplyModifierState(shift_down2);
    ASSERT_THAT(ProcessKeyEvent(shift_down2), testing::ElementsAre(kShiftPressed));

    ASSERT_THAT(ProcessKeyEvent(CtrlShiftF1DownEvent()),
                testing::ElementsAre(kCtrlShiftF1Pressed, kF1Pressed));
    ASSERT_THAT(ProcessKeyEvent(CtrlShiftF1UpEvent()),
                testing::ElementsAre(kCtrlShiftF1Released, kF1Released));

    SetModifierState(WXK_SHIFT, false);
    wxKeyEvent shift_up2 = ShiftUpEvent();
    ApplyModifierState(shift_up2);
    ASSERT_THAT(ProcessKeyEvent(shift_up2), testing::ElementsAre(kShiftReleased));

    SetModifierState(WXK_CONTROL, false);
    wxKeyEvent ctrl_up2 = CtrlUpEvent();
    ApplyModifierState(ctrl_up2);
    ASSERT_THAT(ProcessKeyEvent(ctrl_up2), testing::ElementsAre(kCtrlReleased));
}

}  // namespace widgets

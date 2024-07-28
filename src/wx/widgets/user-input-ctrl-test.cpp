#include "wx/widgets/user-input-ctrl.h"

#include <unordered_set>

#include "wx/config/user-input.h"
#include "wx/widgets/test/widgets-test.h"

namespace widgets {
TEST_F(WidgetsTest, UserInputCtrlTest) {
    // Add a UserInputCtrl to the frame, `frame()` takes ownership here.
    UserInputCtrl* user_input_ctrl = new UserInputCtrl(frame(), XRCID("UserInputCtrl"));

    // Check the UserInputCtrl is empty.
    EXPECT_TRUE(user_input_ctrl->IsEmpty());

    // Send a EVT_CHAR event to the UserInputCtrl.
    wxKeyEvent key_event(wxEVT_CHAR);
    key_event.m_keyCode = 'a';
    user_input_ctrl->GetEventHandler()->ProcessEvent(key_event);

    // Check the UserInputCtrl is empty.
    EXPECT_TRUE(user_input_ctrl->IsEmpty());

    // Send a EVT_KEY_DOWN event to the UserInputCtrl.
    wxKeyEvent key_down_event(wxEVT_KEY_DOWN);
    key_down_event.m_keyCode = 'a';
    user_input_ctrl->GetEventHandler()->ProcessEvent(key_down_event);

    // Check the UserInputCtrl is empty.
    EXPECT_TRUE(user_input_ctrl->IsEmpty());

    // Send a EVT_USER_INPUT event to the UserInputCtrl.
    UserInputEvent user_input_event1({UserInputEvent::Data(config::KeyboardInput('A'), false)});
    user_input_ctrl->GetEventHandler()->ProcessEvent(user_input_event1);

    // Check the UserInputCtrl is not empty.
    EXPECT_FALSE(user_input_ctrl->IsEmpty());
    EXPECT_EQ(user_input_ctrl->SingleInput(), config::KeyboardInput('A'));

    // Send another EVT_USER_INPUT event to the UserInputCtrl.
    UserInputEvent user_input_event2({UserInputEvent::Data(config::KeyboardInput('B'), false)});
    user_input_ctrl->GetEventHandler()->ProcessEvent(user_input_event2);

    // Check the UserInputCtrl is not empty and contains a single input.
    EXPECT_FALSE(user_input_ctrl->IsEmpty());
    EXPECT_EQ(user_input_ctrl->SingleInput(), config::KeyboardInput('B'));
    EXPECT_EQ(user_input_ctrl->inputs().size(), 1);
}

TEST_F(WidgetsTest, UserInputCtrlMultiKeyTest) {
    // Add a UserInputCtrl to the frame, `frame()` takes ownership here.
    UserInputCtrl* user_input_ctrl = new UserInputCtrl(frame(), XRCID("UserInputCtrl"));
    user_input_ctrl->SetMultiKey(true);

    // Check the UserInputCtrl is empty.
    EXPECT_TRUE(user_input_ctrl->IsEmpty());

    // Send a EVT_USER_INPUT event to the UserInputCtrl.
    UserInputEvent user_input_event1({UserInputEvent::Data(config::KeyboardInput('A'), false)});
    user_input_ctrl->GetEventHandler()->ProcessEvent(user_input_event1);

    // Check the UserInputCtrl is not empty.
    EXPECT_FALSE(user_input_ctrl->IsEmpty());
    EXPECT_EQ(user_input_ctrl->inputs(),
              std::unordered_set<config::UserInput>({config::KeyboardInput('A')}));

    // Send another EVT_USER_INPUT event to the UserInputCtrl.
    UserInputEvent user_input_event2({UserInputEvent::Data(config::KeyboardInput('B'), false)});
    user_input_ctrl->GetEventHandler()->ProcessEvent(user_input_event2);

    // Check the UserInputCtrl is not empty and contains two inputs.
    EXPECT_FALSE(user_input_ctrl->IsEmpty());
    EXPECT_EQ(user_input_ctrl->inputs(),
              std::unordered_set<config::UserInput>(
                  {config::KeyboardInput('A'), config::KeyboardInput('B')}));
}

}  // namespace widgets
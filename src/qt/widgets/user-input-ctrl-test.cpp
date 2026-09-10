#include "qt/widgets/user-input-ctrl.h"

#include <unordered_set>

#include <QApplication>
#include <QKeyEvent>

#include <gtest/gtest.h>

#include "qt/config/user-input.h"

namespace widgets {

namespace {

// A QApplication is needed to create widgets.
class UserInputCtrlTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "user-input-ctrl-test";
            static char* argv[] = {arg0, nullptr};
            app_ = new QApplication(argc, argv);
        }
    }

    static QApplication* app_;
};

QApplication* UserInputCtrlTest::app_ = nullptr;

}  // namespace

TEST_F(UserInputCtrlTest, UserInputCtrlTest) {
    UserInputCtrl user_input_ctrl;

    // Check the UserInputCtrl is empty.
    EXPECT_TRUE(user_input_ctrl.IsEmpty());

    // Key events must not modify the control.
    QKeyEvent key_event(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    QCoreApplication::sendEvent(&user_input_ctrl, &key_event);
    EXPECT_TRUE(user_input_ctrl.IsEmpty());
    EXPECT_TRUE(user_input_ctrl.text().isEmpty());

    // Feed a batch with a released input.
    UserInputBatch batch1;
    batch1.data.emplace_back(config::KeyboardInput(Qt::Key_A), false);
    user_input_ctrl.OnInputBatch(batch1);

    // Check the UserInputCtrl is not empty.
    EXPECT_FALSE(user_input_ctrl.IsEmpty());
    EXPECT_EQ(user_input_ctrl.SingleInput(), config::UserInput(config::KeyboardInput(Qt::Key_A)));

    // Feed another batch.
    UserInputBatch batch2;
    batch2.data.emplace_back(config::KeyboardInput(Qt::Key_B), false);
    user_input_ctrl.OnInputBatch(batch2);

    // Check the UserInputCtrl is not empty and contains a single input.
    EXPECT_FALSE(user_input_ctrl.IsEmpty());
    EXPECT_EQ(user_input_ctrl.SingleInput(), config::UserInput(config::KeyboardInput(Qt::Key_B)));
    EXPECT_EQ(user_input_ctrl.inputs().size(), 1u);
}

TEST_F(UserInputCtrlTest, UserInputCtrlMultiKeyTest) {
    UserInputCtrl user_input_ctrl;
    user_input_ctrl.SetMultiKey(true);

    // Check the UserInputCtrl is empty.
    EXPECT_TRUE(user_input_ctrl.IsEmpty());

    UserInputBatch batch1;
    batch1.data.emplace_back(config::KeyboardInput(Qt::Key_A), false);
    user_input_ctrl.OnInputBatch(batch1);

    // Check the UserInputCtrl is not empty.
    EXPECT_FALSE(user_input_ctrl.IsEmpty());
    EXPECT_EQ(user_input_ctrl.inputs(),
              std::unordered_set<config::UserInput>({config::KeyboardInput(Qt::Key_A)}));

    UserInputBatch batch2;
    batch2.data.emplace_back(config::KeyboardInput(Qt::Key_B), false);
    user_input_ctrl.OnInputBatch(batch2);

    // Check the UserInputCtrl is not empty and contains two inputs.
    EXPECT_FALSE(user_input_ctrl.IsEmpty());
    EXPECT_EQ(user_input_ctrl.inputs(),
              std::unordered_set<config::UserInput>(
                  {config::KeyboardInput(Qt::Key_A), config::KeyboardInput(Qt::Key_B)}));
}

TEST_F(UserInputCtrlTest, PressedInputsAreIgnored) {
    UserInputCtrl user_input_ctrl;

    UserInputBatch batch;
    batch.data.emplace_back(config::KeyboardInput(Qt::Key_A), true);
    user_input_ctrl.OnInputBatch(batch);

    EXPECT_TRUE(user_input_ctrl.IsEmpty());
}

}  // namespace widgets

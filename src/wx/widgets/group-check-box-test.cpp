#include "wx/widgets/group-check-box.h"

#include <gtest/gtest.h>

#include <wx/frame.h>
#include <wx/sizer.h>
#include <wx/xrc/xmlres.h>

#include "wx/widgets/test/widgets-test.h"

namespace widgets {

TEST_F(WidgetsTest, GroupCheckBoxTest) {
    // Add 2 GroupCheckBoxes to the frame, `frame()` takes ownership here.
    GroupCheckBox* check_box_1 = new GroupCheckBox(frame(), XRCID("One"), "One");
    GroupCheckBox* check_box_2 = new GroupCheckBox(frame(), XRCID("Two"), "Two");

    // Tick one checkbox and check the other is unticked.
    check_box_1->SetValue(true);
    EXPECT_FALSE(check_box_2->GetValue());

    // And vice-versa.
    check_box_2->SetValue(true);
    EXPECT_FALSE(check_box_1->GetValue());
}

}  // namespace widgets

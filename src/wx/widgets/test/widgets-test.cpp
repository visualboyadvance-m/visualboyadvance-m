#include "wx/widgets/test/widgets-test.h"

#include <wx/xrc/xmlres.h>

namespace widgets {

WidgetsTest::WidgetsTest() {
    // wxWidgets can run multi-threaded so set this up for death tests.
    GTEST_FLAG_SET(death_test_style, "threadsafe");
}

WidgetsTest::~WidgetsTest() = default;

void WidgetsTest::SetUp() {
    // Give the wxFrame a unique name and initialize it.
    const char* test_name(testing::UnitTest::GetInstance()->current_test_info()->name());
    frame_ = std::make_unique<wxFrame>(0, wxXmlResource::DoGetXRCID(test_name), test_name);
}

void WidgetsTest::TearDown() {
    ASSERT_FALSE(frame_->IsShown());
    ASSERT_FALSE(frame_->IsBeingDeleted());

    // Ensure the frame is destroyed before the app.
    frame_->Destroy();
    frame_.reset();
}

}  // namespace widgets

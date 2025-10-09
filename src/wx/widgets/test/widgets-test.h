#ifndef VBAM_WX_WIDGETS_TEST_WIDGETS_TEST_H_
#define VBAM_WX_WIDGETS_TEST_WIDGETS_TEST_H_

#include <memory>

#include <gtest/gtest.h>

#include <wx/frame.h>

#include "wx/widgets/test/test-app.h"

namespace widgets {

// A base fixture for tests that use wxWidgets widgets. An app and a wxFrame are
// provided for convenience. They will be deleted on test teardown.
class WidgetsTest : public testing::Test {
public:
    WidgetsTest();
    ~WidgetsTest() override;

    // testing::Test implementation.
    void SetUp() override;
    void TearDown() override;

protected:
    wxFrame* frame() { return frame_.get(); }

private:
    TestApp test_app_;
    std::unique_ptr<wxFrame> frame_;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_TEST_WIDGETS_TEST_H_

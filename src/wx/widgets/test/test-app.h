#ifndef VBAM_WX_WIDGETS_TEST_TEST_APP_H_
#define VBAM_WX_WIDGETS_TEST_TEST_APP_H_

#include <wx/app.h>

namespace widgets {

// While this object is in scope, wxApp::GetInstance() will return this object.
class TestApp : public wxApp {
public:
    TestApp();
    ~TestApp() override;

    bool OnInit() override { return true; }

private:
    // The previous wxApp instance, active before we set this one.
    wxAppConsole* const previous_app_;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_TEST_TEST_APP_H_

#include "wx/widgets/test/test-app.h"

namespace widgets {

TestApp::TestApp() : previous_app_(wxApp::GetInstance()) {
    // Initialize the wxWidgets app.
    int argc = 0;
    wxApp::Initialize(argc, nullptr);

    // Set the wxApp instance to this object.
    wxApp::SetInstance(this);
}

TestApp::~TestApp() {
    // Clean up the wxWidgets app.
    wxApp::CleanUp();

    // Restore the previous wxApp instance.
    wxApp::SetInstance(previous_app_);
}

}  // namespace widgets

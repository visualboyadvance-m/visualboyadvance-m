#include "wx/widgets/keep-on-top-styler.h"

#include <gtest/gtest.h>

#include <wx/frame.h>
#include <wx/xrc/xmlres.h>

#include "wx/config/option-proxy.h"
#include "wx/widgets/test/widgets-test.h"

namespace widgets {

TEST_F(WidgetsTest, KeepOnTopStyler) {
    // Enable the keep on top option.
    OPTION(kDispKeepOnTop) = true;

    // Create a Frame, it should not have the wxSTAY_ON_TOP style.
    EXPECT_EQ(frame()->GetWindowStyle() & wxSTAY_ON_TOP, 0);

    // Add a KeepOnTopStyler to the frame.
    KeepOnTopStyler styler(frame());

    // Send the Show event to the frame.
    wxShowEvent show_event(wxEVT_SHOW, true);
    frame()->GetEventHandler()->ProcessEvent(show_event);

    // The frame should have the wxSTAY_ON_TOP style.
    EXPECT_EQ(frame()->GetWindowStyle() & wxSTAY_ON_TOP, wxSTAY_ON_TOP);

    // Disable the keep on top option.
    OPTION(kDispKeepOnTop) = false;

    // The frame should no longer have the wxSTAY_ON_TOP style.
    EXPECT_EQ(frame()->GetWindowStyle() & wxSTAY_ON_TOP, 0);
}

}  // namespace widgets

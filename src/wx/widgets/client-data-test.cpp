#include "wx/widgets/client-data.h"

#include <memory>

#include <gtest/gtest.h>

#include <wx/window.h>

#include "wx/config/user-input.h"

namespace widgets {

// Test the ClientData class with a simple type.
TEST(ClientDataTest, Int) {
    std::unique_ptr<wxWindow> window(std::make_unique<wxWindow>());
    window->SetClientObject(new ClientData<int>(42));

    EXPECT_EQ(ClientData<int>::From(window.get()), 42);
}

// Test the ClientData class with a complex type.
TEST(ClientDataTest, Object) {
    std::unique_ptr<wxWindow> window(std::make_unique<wxWindow>());
    window->SetClientObject(new ClientData<config::UserInput>(config::KeyboardInput('4')));

    EXPECT_EQ(ClientData<config::UserInput>::From(window.get()).device(),
              config::UserInput::Device::Keyboard);
}

}  // namespace widgets

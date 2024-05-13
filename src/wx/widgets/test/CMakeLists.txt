# This defines the `vbam-wx-widgets-test-fixture` library, which is used for
# providing a base fixture for tests using wxWidgets.

if(NOT BUILD_TESTING)
    return()
endif()

add_library(vbam-wx-widgets-test-fixture OBJECT)
target_sources(vbam-wx-widgets-test-fixture
    PRIVATE
    test-app.cpp
    widgets-test.cpp

    PUBLIC
    test-app.h
    widgets-test.h
)
configure_wx_target(vbam-wx-widgets-test-fixture)
target_link_libraries(vbam-wx-widgets-test-fixture PUBLIC GTest::gtest)

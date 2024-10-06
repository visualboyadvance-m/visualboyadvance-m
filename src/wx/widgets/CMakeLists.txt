# This defines the vbam-wx-widgets target.
# This does not export any localizable file as there is no localized string
# in this target.

add_library(vbam-wx-widgets OBJECT)

target_sources(vbam-wx-widgets
    PRIVATE
    # from external source with minor modifications
    checkedlistctrl.cpp
    $<IF:$<BOOL:${APPLE}>,dpi-support-mac.mm,dpi-support.cpp>
    group-check-box.cpp
    keep-on-top-styler.cpp
    keyboard-input-handler.cpp
    option-validator.cpp
    render-plugin.cpp
    user-input-ctrl.cpp
    user-input-event.cpp
    sdl-poller.cpp
    shortcut-menu-bar.cpp
    utils.cpp
    wxmisc.cpp

    PUBLIC
    # from external source with minor modifications
    checkedlistctrl.h
    client-data.h
    dpi-support.h
    event-handler-provider.h
    group-check-box.h
    keep-on-top-styler.h
    keyboard-input-handler.h
    option-validator.h
    render-plugin.h
    user-input-ctrl.h
    user-input-event.h
    sdl-poller.h
    shortcut-menu-bar.h
    utils.h
    wxmisc.h
)

target_link_libraries(vbam-wx-widgets PUBLIC vbam-wx-config)
configure_wx_target(vbam-wx-widgets)

if(BUILD_TESTING)
    add_executable(vbam-wx-widgets-tests
        client-data-test.cpp
        group-check-box-test.cpp
        keep-on-top-styler-test.cpp
        keyboard-input-handler-test.cpp
        option-validator-test.cpp
        user-input-ctrl-test.cpp
        user-input-event-test.cpp
    )

    target_link_libraries(vbam-wx-widgets-tests
        # Test deps.
        vbam-core-base-test
        vbam-core-fake
        vbam-wx-fake-opts
        vbam-wx-widgets-test-fixture

        # Target deps.
        vbam-wx-config
        vbam-wx-widgets
        GTest::gtest_main
        GTest::gmock_main
    )

    configure_wx_target(vbam-wx-widgets-tests)
    if (NOT CMAKE_CROSSCOMPILING)
        gtest_discover_tests(vbam-wx-widgets-tests)
    endif()
endif()

add_subdirectory(test)

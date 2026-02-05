# This defines the vbam-wx-config target and the
# `VBAM_LOCALIZABLE_WX_CONFIG_FILES` variable, containing the list of
# localizable files in the vbam-wx-config target.

# I don't like duplicating/triplicating code, so I only declare
# event handlers once, and copy them in other places they are needed
# all using portable cmake code
add_custom_command(
    OUTPUT
        ${VBAM_GENERATED_DIR}/wx/config/internal/cmdtab.cpp
        ${VBAM_GENERATED_DIR}/wx/cmdhandlers.h
        ${VBAM_GENERATED_DIR}/wx/cmd-evtable.h
    COMMAND
        ${CMAKE_COMMAND} -D OUTDIR=${VBAM_GENERATED_DIR}/wx/ -P copy-events.cmake
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ../cmdevents.cpp
        copy-events.cmake
)

add_library(vbam-wx-config OBJECT)

# Export the localizable files to the parent scope.
set(VBAM_LOCALIZABLE_WX_CONFIG_FILES
    ${CMAKE_CURRENT_LIST_DIR}/bindings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/command.cpp
    ${CMAKE_CURRENT_LIST_DIR}/cmdtab.cpp
    ${CMAKE_CURRENT_LIST_DIR}/emulated-gamepad.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/bindings-internal.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/bindings-internal.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/option-internal.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/option-internal.h
    ${CMAKE_CURRENT_LIST_DIR}/option-observer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/option.cpp
    ${CMAKE_CURRENT_LIST_DIR}/strutils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/user-input.cpp
    # Generated file.
    ${VBAM_GENERATED_DIR}/wx/config/internal/cmdtab.cpp
)

set(VBAM_LOCALIZABLE_WX_CONFIG_FILES
    ${VBAM_LOCALIZABLE_WX_CONFIG_FILES}
    PARENT_SCOPE
)

target_sources(vbam-wx-config
    PRIVATE
    ${VBAM_LOCALIZABLE_WX_CONFIG_FILES}

    PUBLIC
    bindings.h
    command.h
    cmdtab.h
    emulated-gamepad.h
    option-id.h
    option-observer.h
    option-proxy.h
    option.h
    strutils.h
    user-input.h
)

configure_wx_target(vbam-wx-config)

if(BUILD_TESTING AND NOT COMPILE_ONLY)
    add_executable(vbam-wx-config-tests
        bindings-test.cpp
        command-test.cpp
        emulated-gamepad-test.cpp
        option-test.cpp
        strutils-test.cpp
        user-input-test.cpp
    )
    target_link_libraries(vbam-wx-config-tests
        # Test deps.
        vbam-core-fake
        vbam-wx-fake-opts

        # Target deps.
        vbam-wx-config
        GTest::gtest_main
    )

    configure_wx_target(vbam-wx-config-tests)

    # Workaround for wxWidgets 3.3 Debug build linker issue with wxFormatString::Validate
    # This is a known issue where the symbol is defined in both the wxWidgets library and user code
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(MSVC)
            target_link_options(vbam-wx-config-tests PRIVATE /FORCE:MULTIPLE)
        elseif(MINGW OR (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
            target_link_options(vbam-wx-config-tests PRIVATE -Wl,--allow-multiple-definition)
        endif()
    endif()

    if (NOT CMAKE_CROSSCOMPILING)
        vbam_gtest_discover_tests(vbam-wx-config-tests)
    endif()
endif()

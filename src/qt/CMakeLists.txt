if(NOT ENABLE_QT)
    return()
endif()

# The Qt 6 frontend. A separate desktop port with the wx port's feature set,
# built on Qt Widgets: same core, same components (filters, interframe blending,
# A/V recording), same INI option names.
#
# Do not use this file directly. Always use the top level CMakeLists.txt file.

add_compile_definitions(${VBAM_FRONTEND_DEFINITIONS})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(vbam_qt_components Core Gui Widgets OpenGL OpenGLWidgets Network PrintSupport)
find_package(Qt6 REQUIRED COMPONENTS ${vbam_qt_components} OPTIONAL_COMPONENTS LinguistTools)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

set(VBAM_QT_CONFIG_SOURCES
    config/bindings.cpp
    config/bindings.h
    config/cmdtab-data.cpp
    config/cmdtab.cpp
    config/cmdtab.h
    config/command.cpp
    config/command.h
    config/emulated-gamepad.cpp
    config/emulated-gamepad.h
    config/internal/bindings-internal.cpp
    config/internal/bindings-internal.h
    config/internal/option-internal.cpp
    config/internal/option-internal.h
    config/option-id.h
    config/option-observer.cpp
    config/option-observer.h
    config/option-proxy.h
    config/option.cpp
    config/option.h
    config/strutils.cpp
    config/strutils.h
    config/user-input.cpp
    config/user-input.h
)

set(VBAM_QT_WIDGETS_SOURCES
    widgets/input-dispatcher.h
    widgets/keyboard-input-handler.cpp
    widgets/keyboard-input-handler.h
    widgets/on-screen-controller.cpp
    widgets/on-screen-controller.h
    widgets/option-binding.cpp
    widgets/option-binding.h
    widgets/render-plugin.cpp
    widgets/render-plugin.h
    widgets/sdl-poller.cpp
    widgets/sdl-poller.h
    widgets/user-input-ctrl.cpp
    widgets/user-input-ctrl.h
)
if(ANDROID)
    # Controllers come from the activity over JNI on Android (see the header).
    list(APPEND VBAM_QT_WIDGETS_SOURCES widgets/android-gamepad.cpp widgets/android-gamepad.h)
endif()

set(VBAM_QT_AUDIO_SOURCES
    audio/audio.cpp
    audio/audio.h
    audio/internal/sdl.cpp
    audio/internal/sdl.h
)

if(APPLE)
    list(APPEND VBAM_QT_AUDIO_SOURCES audio/internal/coreaudio.cpp audio/internal/coreaudio.h)
    # The Quartz 2D renderer (CoreGraphics / CoreAnimation, Objective-C++).
    list(APPEND VBAM_QT_MAIN_SOURCES_APPLE drawing-panel-quartz.mm)
endif()
if(ENABLE_OPENAL)
    list(APPEND VBAM_QT_AUDIO_SOURCES audio/internal/openal.cpp audio/internal/openal.h)
endif()
if(ENABLE_FAUDIO)
    list(APPEND VBAM_QT_AUDIO_SOURCES audio/internal/faudio.cpp audio/internal/faudio.h)
endif()
if(ENABLE_AAUDIO)
    list(APPEND VBAM_QT_AUDIO_SOURCES audio/internal/aaudio.cpp audio/internal/aaudio.h)
endif()
if(WIN32)
    list(APPEND VBAM_QT_AUDIO_SOURCES audio/internal/dsound.cpp audio/internal/dsound.h)
    if(ENABLE_XAUDIO2)
        list(APPEND VBAM_QT_AUDIO_SOURCES
            audio/internal/xaudio2.cpp
            audio/internal/xaudio2.h
            audio/internal/xaudio2_7.cpp
            audio/internal/xaudio2_7.h
            audio/internal/xaudio2_9.cpp
            audio/internal/xaudio2_9.h
        )
    endif()
endif()

set(VBAM_QT_DIALOGS_SOURCES
    dialogs/accel-config.cpp
    dialogs/accel-config.h
    dialogs/base-dialog.cpp
    dialogs/base-dialog.h
    dialogs/cheat-add.cpp
    dialogs/cheat-add.h
    dialogs/cheat-edit.cpp
    dialogs/cheat-edit.h
    dialogs/cheat-list.cpp
    dialogs/cheat-list.h
    dialogs/cheat-search.cpp
    dialogs/cheat-search.h
    dialogs/code-select.cpp
    dialogs/code-select.h
    dialogs/directories-config.cpp
    dialogs/directories-config.h
    dialogs/display-config.cpp
    dialogs/display-config.h
    dialogs/export-sps.cpp
    dialogs/export-sps.h
    dialogs/game-boy-advance-config.cpp
    dialogs/game-boy-advance-config.h
    dialogs/game-boy-config.cpp
    dialogs/game-boy-config.h
    dialogs/game-maker.cpp
    dialogs/game-maker.h
    dialogs/gb-rom-info.cpp
    dialogs/gb-rom-info.h
    dialogs/gba-rom-info.cpp
    dialogs/gba-rom-info.h
    dialogs/general-config.cpp
    dialogs/general-config.h
    dialogs/joypad-config.cpp
    dialogs/joypad-config.h
    dialogs/sound-config.cpp
    dialogs/sound-config.h
    dialogs/speedup-config.cpp
    dialogs/speedup-config.h
)

if(ENABLE_LINK)
    list(APPEND VBAM_QT_DIALOGS_SOURCES
        dialogs/link-config.cpp
        dialogs/link-config.h
        dialogs/net-link.cpp
        dialogs/net-link.h
    )
endif()

set(VBAM_QT_VIEWERS_SOURCES
    viewers/viewers.cpp
    viewers/gfxviewers.cpp
    viewers/ioregs.h
    viewers/log-dialog.cpp
    viewers/log-dialog.h
    viewers/viewer-openers.cpp
    viewers/viewer.cpp
    viewers/viewer.h
)

set(VBAM_QT_RENDERER_SOURCES
    renderers/sdl-panel.cpp
    renderers/sdl-panel.h
)
# On Android the Vulkan panel presents through VK_KHR_android_surface on an
# overlay SurfaceView (see renderers/vulkan-panel.cpp); the headers come from
# the NDK sysroot and the platform loader (libvulkan.so) is loaded at run time,
# so nothing beyond ENABLE_VULKAN is needed there.
set(VBAM_QT_VULKAN ${ENABLE_VULKAN})
if(VBAM_QT_VULKAN)
    list(APPEND VBAM_QT_RENDERER_SOURCES renderers/vulkan-panel.cpp renderers/vulkan-panel.h)
endif()
if(ENABLE_GLES)
    list(APPEND VBAM_QT_RENDERER_SOURCES renderers/gles-panel.cpp renderers/gles-panel.h)
endif()
if(APPLE)
    list(APPEND VBAM_QT_RENDERER_SOURCES
        renderers/mac-support.h
        renderers/mac-support.mm
        renderers/metal-panel.h
        renderers/metal-panel.mm
    )
    # The Objective-C++ renderer code is written for ARC.
    set_source_files_properties(renderers/mac-support.mm renderers/metal-panel.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()
if(WIN32)
    list(APPEND VBAM_QT_RENDERER_SOURCES renderers/d3d-panel.cpp renderers/d3d-panel.h)
endif()

set(VBAM_QT_MAIN_SOURCES
    android-compat.cpp
    android-compat.h
    app.cpp
    app.h
    cmd-handlers.cpp
    cmd-handlers.h
    cmd-handlers-cheats.cpp
    cmd-handlers-lua.cpp
    cmd-ids.h
    drawing-panel.cpp
    drawing-panel.h
    game-area.cpp
    game-area.h
    log.cpp
    log.h
    main-window.cpp
    main-window.h
    main.cpp
    menu-def.cpp
    opts.cpp
    opts.h
    rpi.h
    sys.cpp
    sys.h
)

set(VBAM_QT_LUA_SOURCES)
if(ENABLE_LUA)
    set(VBAM_QT_LUA_SOURCES
        lua/lua_api_bit.cpp
        lua/lua_api_emu.cpp
        lua/lua_api_gui.cpp
        lua/lua_api_joypad.cpp
        lua/lua_api_memory.cpp
        lua/lua_api_rom.cpp
        lua/lua_api_savestate.cpp
        lua/lua_console.cpp
        lua/lua_console.h
        lua/lua_editor.cpp
        lua/lua_editor.h
        lua/lua_engine.cpp
        lua/lua_engine.h
        lua/lua_internal.h
        lua/lua_overlay.cpp
    )
endif()

# ---------------------------------------------------------------------------
# Common configuration shared by the executable and the test binaries.
# ---------------------------------------------------------------------------

function(configure_qt_target target)
    target_link_libraries(${target} PRIVATE
        vbam-core
        nonstd-lib
        ghc-filesystem
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Network
        Qt6::PrintSupport
        ${VBAM_SDL_LIBS}
    )

    if(ENABLE_SDL3)
        target_compile_definitions(${target} PRIVATE ENABLE_SDL3)
    endif()

    # OpenGL.
    if(OpenGL_FOUND AND NOT DISABLE_OPENGL)
        target_link_libraries(${target} PRIVATE ${OPENGL_LIBRARIES})
    else()
        target_compile_definitions(${target} PRIVATE NO_OGL)
    endif()

    # Vulkan: headers only, the loader (or MoltenVK) is loaded at run time.
    if(VBAM_QT_VULKAN)
        if(TARGET Vulkan::Headers)
            target_link_libraries(${target} PRIVATE Vulkan::Headers)
        elseif(Vulkan_INCLUDE_DIRS)
            target_include_directories(${target} PRIVATE ${Vulkan_INCLUDE_DIRS})
        elseif(VULKAN_INCLUDE_DIR)
            target_include_directories(${target} PRIVATE ${VULKAN_INCLUDE_DIR})
        endif()
        # Where the loader was found at configure time: on macOS that is usually
        # a vcpkg / SDK / Homebrew tree that is not on the dyld search path.
        unset(_vbam_vk_lib)
        if(Vulkan_LIBRARY AND NOT Vulkan_LIBRARY MATCHES "NOTFOUND")
            set(_vbam_vk_lib "${Vulkan_LIBRARY}")
        elseif(MOLTENVK AND NOT MOLTENVK MATCHES "NOTFOUND")
            set(_vbam_vk_lib "${MOLTENVK}")
        endif()
        if(_vbam_vk_lib)
            get_filename_component(_vbam_vk_dir "${_vbam_vk_lib}" DIRECTORY)
            target_compile_definitions(${target} PRIVATE
                "VBAM_VULKAN_LIBRARY_DIR=\"${_vbam_vk_dir}\"")
        endif()
    else()
        target_compile_definitions(${target} PRIVATE NO_VULKAN)
    endif()

    # OpenGL ES 2 renderer (the Android output module; buildable elsewhere for
    # testing with -DENABLE_GLES=ON).
    if(ENABLE_GLES)
        target_compile_definitions(${target} PRIVATE VBAM_ENABLE_GLES)
    endif()

    # Direct3D (d3d9.dll, d3d12.dll and dxgi.dll are loaded at run time; the
    # import libraries only supply the interface GUIDs).
    if(WIN32)
        if(NOT ENABLE_DIRECT3D)
            target_compile_definitions(${target} PRIVATE NO_D3D)
        endif()
        if(ENABLE_DIRECT3D12)
            target_link_libraries(${target} PRIVATE dxguid)
        else()
            target_compile_definitions(${target} PRIVATE NO_D3D12)
        endif()
    endif()

    # Metal (shaders are compiled from source at run time, no metallib).
    if(APPLE)
        find_library(QT_METAL Metal)
        find_library(QT_QUARTZCORE QuartzCore)
        target_link_libraries(${target} PRIVATE ${QT_METAL} ${QT_QUARTZCORE})
    endif()


    # OpenAL.
    if(ENABLE_OPENAL)
        target_compile_definitions(${target} PRIVATE VBAM_ENABLE_OPENAL)
        if(OPENAL_STATIC)
            target_compile_definitions(${target} PRIVATE AL_LIBTYPE_STATIC)
        endif()
        target_include_directories(${target} PRIVATE ${OPENAL_INCLUDE_DIR})
        target_link_libraries(${target} PRIVATE ${OPENAL_LIBRARY})
        # A static OpenAL-Soft (vcpkg) needs the fmt library it was built with.
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            find_library(FMT_LIBRARY NAMES fmtd fmt)
        else()
            find_library(FMT_LIBRARY fmt)
        endif()
        if(FMT_LIBRARY)
            target_link_libraries(${target} PRIVATE ${FMT_LIBRARY})
        endif()
        if(WIN32)
            target_link_libraries(${target} PRIVATE avrt)
        endif()
    endif()

    # XAudio2.
    if(ENABLE_XAUDIO2)
        target_compile_definitions(${target} PRIVATE VBAM_ENABLE_XAUDIO2)
    endif()

    # FAudio.
    # AAudio (Android only). The driver TU and the kAAudio option value are both
    # compiled out when this is off; the sound dialog hides its radio button.
    if(ENABLE_AAUDIO)
        target_compile_definitions(${target} PRIVATE VBAM_ENABLE_AAUDIO)
        target_link_libraries(${target} PRIVATE aaudio)
    endif()

    if(ENABLE_FAUDIO)
        target_compile_definitions(${target} PRIVATE VBAM_ENABLE_FAUDIO)
        if(APPLE AND NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
            target_link_libraries(${target} PRIVATE FAudio)
        elseif(VBAM_STATIC AND TARGET FAudio::FAudio-static)
            target_link_libraries(${target} PRIVATE FAudio::FAudio-static)
        else()
            target_link_libraries(${target} PRIVATE FAudio::FAudio)
        endif()
        if(WIN32)
            target_link_libraries(${target} PRIVATE dxguid uuid winmm ole32 advapi32 user32 mfplat mfreadwrite mfuuid propsys)
        endif()
    endif()

    # Lua.
    if(ENABLE_LUA)
        target_compile_definitions(${target} PRIVATE VBAM_ENABLE_LUA)
        target_include_directories(${target} PRIVATE ${LUA_INCLUDE_DIR})
        target_link_libraries(${target} PRIVATE ${LUA_LIBRARIES})
    endif()

    if(WIN32)
        target_link_libraries(${target} PRIVATE wsock32 ws2_32 winmm version imm32 shlwapi)
        if(ENABLE_XAUDIO2 OR NOT ENABLE_FAUDIO)
            target_link_libraries(${target} PRIVATE dsound dxguid)
        endif()
    endif()

    if(APPLE)
        find_library(QT_COREAUDIO CoreAudio)
        find_library(QT_AUDIOTOOLBOX AudioToolbox)
        find_library(QT_IOKIT IOKit)
        find_library(QT_COREFOUNDATION CoreFoundation)
        find_library(QT_QUARTZCORE QuartzCore)
        find_library(QT_COREGRAPHICS CoreGraphics)
        find_library(QT_APPKIT AppKit)
        target_link_libraries(${target} PRIVATE
            ${QT_COREAUDIO} ${QT_AUDIOTOOLBOX} ${QT_IOKIT} ${QT_COREFOUNDATION}
            ${QT_QUARTZCORE} ${QT_COREGRAPHICS} ${QT_APPKIT})
    endif()

    if(ENABLE_LINK)
        target_link_libraries(${target} PRIVATE ${SFML_LIBRARIES})
    endif()

    if(ANDROID)
        # JNI (android-compat.cpp, the gamepad bridge) and __android_log_print.
        target_link_libraries(${target} PRIVATE android log)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# The application.
# ---------------------------------------------------------------------------

set(VBAM_QT_ICON_PNG ${CMAKE_SOURCE_DIR}/src/wx/icons/visualboyadvance-m.png)
set(VBAM_QT_ICON_ICNS ${CMAKE_SOURCE_DIR}/src/wx/icons/visualboyadvance-m.icns)
set(VBAM_QT_ICON_ICO ${CMAKE_SOURCE_DIR}/src/wx/icons/visualboyadvance-m.ico)

if(COMPILE_ONLY)
    add_library(visualboyadvance-m-qt OBJECT)
else()
    if(ANDROID AND QT_KNOWN_POLICY_QTP0002)
        # Take the QT_ANDROID_* path properties below as they are (plain POSIX
        # paths, no generator expressions); settles the policy and silences
        # the QTP0002 warning.
        qt_policy(SET QTP0002 NEW)
    endif()
    # On Android this is a shared module (lib<name>_<abi>.so) loaded by the Java
    # activity, and qt_add_executable wires up androiddeployqt for the APK.
    qt_add_executable(visualboyadvance-m-qt WIN32 MACOSX_BUNDLE)
endif()

target_sources(visualboyadvance-m-qt PRIVATE
    ${VBAM_QT_MAIN_SOURCES}
    ${VBAM_QT_CONFIG_SOURCES}
    ${VBAM_QT_WIDGETS_SOURCES}
    ${VBAM_QT_AUDIO_SOURCES}
    ${VBAM_QT_DIALOGS_SOURCES}
    ${VBAM_QT_VIEWERS_SOURCES}
    ${VBAM_QT_LUA_SOURCES}
    ${VBAM_QT_RENDERER_SOURCES}
    ${VBAM_QT_MAIN_SOURCES_APPLE}
)

# The application icon, embedded as a Qt resource so the window icon works on
# every platform, plus the native bundle / exe icons.
qt_add_resources(visualboyadvance-m-qt "vbam-qt-icons"
    PREFIX "/icons"
    BASE ${CMAKE_SOURCE_DIR}/src/wx/icons
    FILES ${VBAM_QT_ICON_PNG}
)

if(APPLE AND NOT COMPILE_ONLY)
    set_source_files_properties(${VBAM_QT_ICON_ICNS} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(visualboyadvance-m-qt PRIVATE ${VBAM_QT_ICON_ICNS})
    set_target_properties(visualboyadvance-m-qt PROPERTIES
        OUTPUT_NAME "visualboyadvance-m-qt"
        MACOSX_BUNDLE_BUNDLE_NAME "VisualBoyAdvance-M Qt"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.visualboyadvance-m.visualboyadvance-m-qt"
        MACOSX_BUNDLE_ICON_FILE "visualboyadvance-m.icns"
        MACOSX_BUNDLE_BUNDLE_VERSION "${VBAM_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${VBAM_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
    set_source_files_properties(audio/internal/coreaudio.cpp PROPERTIES
        COMPILE_OPTIONS "-Wno-elaborated-enum-base;-Wno-availability")
endif()

if(WIN32 AND NOT COMPILE_ONLY)
    target_sources(visualboyadvance-m-qt PRIVATE visualboyadvance-m-qt.rc)
endif()

if(ANDROID AND NOT COMPILE_ONLY)
    # The package overlay (manifest, Java activity + gamepad/wake-lock/menu-bar
    # helpers shared with the wx port, SDL's Java glue, launcher icons).
    # androiddeployqt bundles any extra shared libraries the app dlopen()s: when
    # SDL3 comes from the NDK sysroot it must travel in the APK; a static SDL3
    # is linked into our own module .so and VBAM_SDL3_ANDROID_LIB is empty.
    set_target_properties(visualboyadvance-m-qt PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 34)

    if(VBAM_SDL3_ANDROID_LIB)
        set_property(TARGET visualboyadvance-m-qt PROPERTY
            QT_ANDROID_EXTRA_LIBS "${VBAM_SDL3_ANDROID_LIB}")
    endif()

    # androiddeployqt leaves the package at src/qt/android-build/<target>.apk.
    # Put it at the top of the build directory under a name that says which
    # ABI it holds. The dependency is on the .apk file, a known OUTPUT here,
    # since Qt's apk target is created by a deferred finalizer.
    set(vbam_qt_apk_built "${CMAKE_CURRENT_BINARY_DIR}/android-build/visualboyadvance-m-qt.apk")
    set(vbam_qt_apk_named "${CMAKE_BINARY_DIR}/visualboyadvance-m-qt-${ARCH_NAME}.apk")

    # Signing, same policy as the wx port: a package meant to be handed to
    # someone (UPSTREAM_RELEASE) is signed with the developer keystore, any
    # other build is copied unsigned and told how to sign it.
    if(NOT DEFINED ENV{HOME})
        set(vbam_home "$ENV{USERPROFILE}")
    else()
        set(vbam_home "$ENV{HOME}")
    endif()

    set(VBAM_ANDROID_KEYSTORE "${vbam_home}/.codesign/android_debug.keystore"
        CACHE FILEPATH "Keystore the Android APK is signed with")
    set(VBAM_ANDROID_KEY_ALIAS "androiddebugkey"
        CACHE STRING "Key in VBAM_ANDROID_KEYSTORE the APK is signed with")
    set(VBAM_ANDROID_KEYSTORE_PASS "android"
        CACHE STRING "Password for VBAM_ANDROID_KEYSTORE and its key")

    file(GLOB vbam_build_tools LIST_DIRECTORIES true "${ANDROID_SDK_ROOT}/build-tools/*")
    list(SORT vbam_build_tools COMPARE NATURAL ORDER DESCENDING)

    find_program(VBAM_APKSIGNER
        NAMES apksigner apksigner.bat
        HINTS ${vbam_build_tools}
    )

    if(VBAM_APKSIGNER AND NOT DEFINED VBAM_APKSIGNER_JVM_OPTS)
        # Silences a recent JDK's conscrypt native-access warnings; only used
        # when this JDK accepts the flag.
        set(vbam_native_access -J-enable-native-access=ALL-UNNAMED)
        execute_process(
            COMMAND "${VBAM_APKSIGNER}" ${vbam_native_access} version
            RESULT_VARIABLE vbam_native_access_status
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT vbam_native_access_status EQUAL 0)
            set(vbam_native_access "")
        endif()
        set(VBAM_APKSIGNER_JVM_OPTS "${vbam_native_access}" CACHE INTERNAL
            "JVM options apksigner is invoked with")
    endif()

    set(vbam_qt_sign_apk FALSE)
    if(UPSTREAM_RELEASE)
        if(NOT VBAM_APKSIGNER)
            message(WARNING "No apksigner in ${ANDROID_SDK_ROOT}/build-tools or on the PATH; "
                            "the Qt APK will be left unsigned.")
        elseif(NOT EXISTS "${VBAM_ANDROID_KEYSTORE}")
            message(WARNING "No keystore at ${VBAM_ANDROID_KEYSTORE}; the Qt APK will be left "
                            "unsigned. See src/wx/CMakeLists.txt for how to make one.")
        else()
            set(vbam_qt_sign_apk TRUE)
        endif()
    endif()

    if(vbam_qt_sign_apk)
        set(vbam_qt_apk_commands
            COMMAND "${VBAM_APKSIGNER}" ${VBAM_APKSIGNER_JVM_OPTS} sign
                    --ks       "${VBAM_ANDROID_KEYSTORE}"
                    --ks-pass  "pass:${VBAM_ANDROID_KEYSTORE_PASS}"
                    --key-pass "pass:${VBAM_ANDROID_KEYSTORE_PASS}"
                    --ks-key-alias "${VBAM_ANDROID_KEY_ALIAS}"
                    --v4-signing-enabled false
                    --out "${vbam_qt_apk_named}"
                    "${vbam_qt_apk_built}"
        )
        set(vbam_qt_apk_comment "Signing APK to ${vbam_qt_apk_named}")
    else()
        set(vbam_qt_apk_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${vbam_qt_apk_built}" "${vbam_qt_apk_named}"
            COMMAND "${CMAKE_COMMAND}" -E echo
                    "  ${vbam_qt_apk_named} is unsigned and Android will not install it."
            COMMAND "${CMAKE_COMMAND}" -E echo
                    "  Sign it with: apksigner ${VBAM_APKSIGNER_JVM_OPTS} sign --ks ${VBAM_ANDROID_KEYSTORE} --ks-key-alias ${VBAM_ANDROID_KEY_ALIAS} ${vbam_qt_apk_named}"
            COMMAND "${CMAKE_COMMAND}" -E echo
                    "  or configure with -DUPSTREAM_RELEASE=TRUE to have the build sign it."
        )
        set(vbam_qt_apk_comment "Copying APK to ${vbam_qt_apk_named}")
    endif()

    add_custom_command(
        OUTPUT  "${vbam_qt_apk_named}"
        ${vbam_qt_apk_commands}
        DEPENDS "${vbam_qt_apk_built}"
        COMMENT "${vbam_qt_apk_comment}"
        VERBATIM
    )
    add_custom_target(visualboyadvance-m-qt-apk-named ALL DEPENDS "${vbam_qt_apk_named}")
endif()

target_link_libraries(visualboyadvance-m-qt PRIVATE
    vbam-components-draw-text
    vbam-components-filters
    vbam-components-filters-agb
    vbam-components-filters-cgb
    vbam-components-filters-interframe
    vbam-components-filters-scalefx
    vbam-components-user-config
    vbam-sdl-motion
)

if(ENABLE_FFMPEG)
    target_link_libraries(visualboyadvance-m-qt PRIVATE vbam-components-av-recording)
    target_link_options(visualboyadvance-m-qt PRIVATE ${FFMPEG_LDFLAGS})
    if(ANDROID)
        # vcpkg's ffmpeg for Android uses the MediaCodec NDK wrapper.
        target_link_libraries(visualboyadvance-m-qt PRIVATE mediandk)
    endif()
endif()

configure_qt_target(visualboyadvance-m-qt)

# Translations: every tr() string is collected into a .ts per language under
# po/qt (lupdate) and compiled to .qm files shipped next to the binary /
# in the bundle (lrelease). Only when Qt's linguist tools are available.
if(TARGET Qt6::lrelease AND EXISTS ${CMAKE_SOURCE_DIR}/po/qt)
    file(GLOB VBAM_QT_TS_FILES ${CMAKE_SOURCE_DIR}/po/qt/*.ts)
    if(VBAM_QT_TS_FILES)
        qt_add_translations(visualboyadvance-m-qt
            TS_FILES ${VBAM_QT_TS_FILES}
            QM_FILES_OUTPUT_VARIABLE VBAM_QT_QM_FILES
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(BUILD_TESTING AND NOT COMPILE_ONLY)
    add_executable(vbam-qt-config-tests
        ${VBAM_QT_CONFIG_SOURCES}
        config/bindings-test.cpp
        config/command-test.cpp
        config/emulated-gamepad-test.cpp
        config/option-test.cpp
        config/strutils-test.cpp
        config/user-input-test.cpp
        config/test-main.cpp
        test/fake-opts.cpp
        log.cpp
    )
    # The option tests read core globals (gbEmulatorType, gbPaletteOption...),
    # so the real core is linked; vbam-core-fake supplies the system callbacks.
    target_link_libraries(vbam-qt-config-tests PRIVATE
        vbam-core-fake
        GTest::gtest
    )
    configure_qt_target(vbam-qt-config-tests)

    if(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
        vbam_gtest_discover_tests(vbam-qt-config-tests)
    endif()
endif()

if(BUILD_TESTING AND NOT COMPILE_ONLY AND ENABLE_LUA)
    add_executable(vbam-qt-lua-test-runner
        lua/test/lua_test_runner.cpp
        lua/lua_engine.cpp
        lua/lua_api_emu.cpp
        lua/lua_api_memory.cpp
        lua/lua_api_joypad.cpp
        lua/lua_api_gui.cpp
        lua/lua_api_savestate.cpp
        lua/lua_api_rom.cpp
        lua/lua_api_bit.cpp
        lua/lua_overlay.cpp
    )
    target_compile_definitions(vbam-qt-lua-test-runner PRIVATE VBAM_ENABLE_LUA)
    target_include_directories(vbam-qt-lua-test-runner PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(vbam-qt-lua-test-runner PRIVATE
        ${LUA_LIBRARIES} vbam-core vbam-core-fake Qt6::Core)
    if(APPLE)
        if(NOT LIBICONV_LIB)
            find_library(LIBICONV_LIB NAMES libiconv iconv)
        endif()
        target_link_libraries(vbam-qt-lua-test-runner PRIVATE
            "-framework CoreFoundation" ${LIBICONV_LIB})
    endif()
    if(NOT CMAKE_CROSSCOMPILING)
        add_test(NAME qt.lua.api
                 COMMAND vbam-qt-lua-test-runner
                     "${CMAKE_CURRENT_SOURCE_DIR}/lua/test/lua_api_test.lua")
    endif()
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

if(NOT COMPILE_ONLY AND NOT ANDROID)
    if(APPLE)
        install(TARGETS visualboyadvance-m-qt BUNDLE DESTINATION .)
    else()
        install(TARGETS visualboyadvance-m-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    if(UNIX AND NOT APPLE)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/visualboyadvance-m-qt.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(FILES ${VBAM_QT_ICON_PNG}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
            RENAME visualboyadvance-m-qt.png)
    endif()
endif()

include(VbamFunctions)

set(VBAM_LIBS ${VBAMCORE_LIBS})

if(ENABLE_OPENAL)
    find_package(OpenAL REQUIRED)

    include_directories(${OPENAL_INCLUDE_DIR})

    if(OPENAL_STATIC OR (WIN32 AND ((NOT (MINGW AND MSYS)) OR CMAKE_TOOLCHAIN_FILE MATCHES mxe)))
        add_definitions(-DAL_LIBTYPE_STATIC)
    endif()

        list(APPEND VBAM_LIBS ${OPENAL_LIBRARY})
else()
    add_definitions(-DNO_OAL)
endif()

if(APPLE)
    add_definitions(-DwxMAC_USE_CORE_GRAPHICS)
endif()

if(NOT ENABLE_XAUDIO2)
    add_definitions(-DNO_XAUDIO2)
endif()

if(NOT ENABLE_DIRECT3D)
    add_definitions(-DNO_D3D)
endif()

if(ENABLE_FAUDIO)
    find_package(FAudio REQUIRED)
    list(APPEND VBAM_LIBS FAudio)
else()
    add_definitions(-DNO_FAUDIO)
endif()

# on unix we have to check for X11 before we overwrite all the compile/link
# flags with the wx tests
if(NOT WIN32 AND NOT APPLE)
    find_package(X11)

    if(X11_X11_LIB)
        include_directories(${X11_INCLUDE_DIR})
        list(APPEND VBAM_LIBS ${X11_X11_LIB})
    endif()
    if(X11_Xscreensaver_LIB)
        list(APPEND VBAM_LIBS ${X11_Xscreensaver_LIB})
        add_definitions(-DHAVE_XSS)
    endif()

    find_library(EGL_LIBRARY EGL)

    if(EGL_LIBRARY)
        list(APPEND VBAM_LIBS ${EGL_LIBRARY})
        add_definitions(-DHAVE_EGL)
    endif()

    find_library(WAYLAND_LIBRARY wayland-client)

    if(WAYLAND_LIBRARY)
        list(APPEND VBAM_LIBS ${WAYLAND_LIBRARY})
    endif()
endif()

# Win32 definitions common to all toolchains.
if (WIN32)
    add_definitions(-D_UNICODE -DUNICODE -DwxUSE_GUI=1 -D__WXMSW__ -DWINVER=0x0A00 -DNTDDI_VERSION=0x0A000007)
endif()

find_package(wxWidgets CONFIG REQUIRED)

if(ENABLE_NLS)
    find_package(Gettext REQUIRED)
    find_program(XGETTEXT xgettext)
    find_program(MSGINIT  msginit)
    if(NOT XGETTEXT OR NOT MSGINIT)
        message(SEND_ERROR "Cannot find gettext xgettext:'${XGETTEXT}' msginit:'${MSGINIT}'")
    endif()
endif()

# contrib widgets
include_directories(widgets)

# for out-of-tree builds, grab includes from both target and source dirs
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

find_program(ZIP_PROGRAM zip DOC "zip compressor executable")

if(NOT ZIP_PROGRAM)
    message(FATAL_ERROR "The zip compressor program is required for building.")
endif()

set(
    XRC_SOURCES
    xrc/AccelConfig.xrc
    xrc/CheatAdd.xrc
    xrc/CheatCreate.xrc
    xrc/CheatEdit.xrc
    xrc/CheatList.xrc
    xrc/CodeSelect.xrc
    xrc/DirectoriesConfig.xrc
    xrc/Disassemble.xrc
    xrc/DisplayConfig.xrc
    xrc/ExportSPS.xrc
    xrc/GBAROMInfo.xrc
    xrc/GBColorPrefPanel.xrc
    xrc/GBDisassemble.xrc
    xrc/GBMapViewer.xrc
    xrc/GBOAMViewer.xrc
    xrc/GBPaletteViewer.xrc
    xrc/GBPrinter.xrc
    xrc/GBROMInfo.xrc
    xrc/GBTileViewer.xrc
    xrc/GameBoyAdvanceConfig.xrc
    xrc/GameBoyConfig.xrc
    xrc/GeneralConfig.xrc
    xrc/IOViewer.xrc
    xrc/JoyPanel.xrc
    xrc/JoypadConfig.xrc
    xrc/LinkConfig.xrc
    xrc/Logging.xrc
    xrc/MainFrame.xrc
    xrc/MainIcon.xrc
    xrc/MainMenu.xrc
    xrc/MapViewer.xrc
    xrc/MemSelRegion.xrc
    xrc/MemViewer.xrc
    xrc/NetLink.xrc
    xrc/OAMViewer.xrc
    xrc/PaletteViewer.xrc
    xrc/SoundConfig.xrc
    xrc/TileViewer.xrc
    xrc/SpeedupConfig.xrc
    xrc/UIConfig.xrc
)

# wxrc does not support xrs files in -c output (> 10x compression)
# we do it using the bin2c.c utility

find_program(BIN2C NAMES bin2c PATHS "${CMAKE_BINARY_DIR}")
if(NOT BIN2C)
    include(HostCompile)
    host_compile(${CMAKE_CURRENT_SOURCE_DIR}/bin2c.c ${BIN2C})
    set(BIN2C bin2c)
endif()

if(CMAKE_HOST_WIN32 AND CMAKE_CROSSCOMPILING)
    find_program(WXRC NAMES wxrc)
endif()

if(WXRC)
    separate_arguments(WXRC UNIX_COMMAND ${WXRC})
elseif(DEFINED ENV{WXRC})
    separate_arguments(WXRC UNIX_COMMAND $ENV{WXRC})
elseif(wxWidgets_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND ${wxWidgets_CONFIG_EXECUTABLE} --utility=wxrc
        OUTPUT_VARIABLE wxrc
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # this is necessary on msys2
    if(NOT wxrc)
        execute_process(
            COMMAND sh -c "${wxWidgets_CONFIG_EXECUTABLE} --utility=wxrc"
            OUTPUT_VARIABLE wxrc
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    # check if the path from wx-config is good
    # and not e.g. an incompatible binary when cross-compiling
    if(EXISTS ${wxrc})
        check_clean_exit(exit_status ${wxrc} --help)

        if(exit_status EQUAL 0)
            set(WXRC ${wxrc})
        endif()
    elseif(wxrc)
        # this is necessary on msys2
        cygpath(cyg_path ${wxrc})

        if(EXISTS ${cyg_path})
            check_clean_exit(exit_status ${cyg_path} --help)

            if(exit_status EQUAL 0)
                set(WXRC ${cyg_path})
            endif()
        endif()
    endif()
endif()

if(NOT WXRC)
    find_wx_util(WXRC wxrc)
endif()

if(NOT WXRC)
    message(WARNING "could not find your wxrc executable")
    set(WXRC wxrc)
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wxvbam.xrs
    COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{PATH}" ${WXRC} ${XRC_SOURCES} -o ${CMAKE_CURRENT_BINARY_DIR}/wxvbam.xrs
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${XRC_SOURCES}
)

add_custom_command(
    OUTPUT builtin-xrc.h
    COMMAND ${BIN2C} wxvbam.xrs builtin-xrc.h builtin_xrs
    DEPENDS wxvbam.xrs ${BIN2C}
)

# use a built-in vba-over.ini if no config file present
add_custom_command(
    OUTPUT builtin-over.h
    COMMAND ${BIN2C} ${CMAKE_CURRENT_SOURCE_DIR}/../vba-over.ini builtin-over.h builtin_over
    DEPENDS ../vba-over.ini ${BIN2C}
)

# I don't like duplicating/triplicating code, so I only declare
# event handlers once, and copy them in other places they are needed
# all using portable cmake code
add_custom_command(
    OUTPUT cmdtab.cpp cmdhandlers.h cmd-evtable.h
    COMMAND ${CMAKE_COMMAND} -D OUTDIR=${CMAKE_CURRENT_BINARY_DIR} -P copy-events.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS cmdevents.cpp
)


# the following should be in the main file for consistency with
# other front ends, but can't due to cmake issues
# then again, the main file should be split up into separate dirs anyway

set(
    SRC_WX
    wxvbam.cpp
    background-input.cpp
    guiinit.cpp
    viewers.cpp
    gfxviewers.cpp
    cmdevents.cpp
    extra-translations.cpp
    opts.cpp
    sys.cpp
    panel.cpp
    viewsupt.cpp
    wayland.cpp
    strutils.cpp
    wxutil.cpp
    config/game-control.cpp
    config/internal/option-internal.cpp
    config/option-observer.cpp
    config/option.cpp
    config/user-input.cpp
    dialogs/display-config.cpp
    widgets/keyedit.cpp
    widgets/joyedit.cpp
    widgets/option-validator.cpp
    widgets/render-plugin.cpp
    widgets/sdljoy.cpp
    widgets/wxmisc.cpp
    # probably ought to be in common
    ../sdl/text.cpp
    # from external source with minor modifications
    widgets/checkedlistctrl.cpp
    ../common/version.cpp
)

set(ALL_SRC_WX ${SRC_WX})

list(APPEND ALL_SRC_WX ${CMAKE_CURRENT_SOURCE_DIR}/macsupport.mm)
list(APPEND ALL_SRC_WX ${CMAKE_CURRENT_SOURCE_DIR}/widgets/dpi-support-mac.mm)
list(APPEND ALL_SRC_WX ${CMAKE_CURRENT_SOURCE_DIR}/widgets/dpi-support.cpp)

if(APPLE)
    list(APPEND SRC_WX macsupport.mm)
    list(APPEND SRC_WX widgets/dpi-support-mac.mm)
else()
    list(APPEND SRC_WX widgets/dpi-support.cpp)
endif()

set(
    HDR_WX
    wxvbam.h
    background-input.h
    wxlogdebug.h
    drawing.h
    filters.h
    ioregs.h
    opts.h
    viewsupt.h
    wxhead.h
    wayland.h
    wxutil.h
    config/game-control.h
    config/internal/option-internal.h
    config/option-id.h
    config/option-observer.h
    config/option.h
    config/user-input.h
    dialogs/display-config.h
    widgets/dpi-support.h
    widgets/option-validator.h
    widgets/render-plugin.h
    widgets/wx/keyedit.h
    widgets/wx/joyedit.h
    widgets/wx/sdljoy.h
    widgets/wx/webupdatedef.h
    widgets/wx/wxmisc.h
    # probably ought to be in common
    ../sdl/text.h
    # from external source with minor modifications
    widgets/wx/checkedlistctrl.h
    ../common/version_cpp.h
    ../common/range.hpp
    ../common/contains.h
)

set(ALL_HDR_WX ${HDR_WX})
# make files included for gettext pot generation
list(APPEND ALL_SRC_WX autoupdater/wxmsw/autoupdater.cpp)
list(APPEND ALL_HDR_WX autoupdater/autoupdater.h)
list(APPEND ALL_SRC_WX autoupdater/wxmsw/winsparkle-wrapper.cpp)
list(APPEND ALL_HDR_WX autoupdater/wxmsw/winsparkle-wrapper.h)
list(APPEND ALL_HDR_WX autoupdater/wxmsw/winsparkle-rc.h)

if(WIN32 AND (X86_64 OR X86_32) AND ENABLE_ONLINEUPDATES)
    list(APPEND SRC_WX autoupdater/wxmsw/autoupdater.cpp)
    list(APPEND HDR_WX autoupdater/autoupdater.h)
    list(APPEND SRC_WX autoupdater/wxmsw/winsparkle-wrapper.cpp)
    list(APPEND HDR_WX autoupdater/wxmsw/winsparkle-wrapper.h)
    list(APPEND HDR_WX autoupdater/wxmsw/winsparkle-rc.h)
endif()

# make files included for gettext pot generation
list(APPEND ALL_HDR_WX autoupdater/autoupdater.h)
list(APPEND ALL_SRC_WX autoupdater/macos/autoupdater.cpp)
list(APPEND ALL_SRC_WX autoupdater/macos/sparkle-wrapper.mm)
list(APPEND ALL_HDR_WX autoupdater/macos/sparkle-wrapper.h)

if(APPLE AND ENABLE_ONLINEUPDATES)
    list(APPEND HDR_WX autoupdater/autoupdater.h)
    list(APPEND SRC_WX autoupdater/macos/autoupdater.cpp)
    list(APPEND SRC_WX autoupdater/macos/sparkle-wrapper.mm)
    list(APPEND HDR_WX autoupdater/macos/sparkle-wrapper.h)
endif()

set(
    RES_WX
    ${XRC_SOURCES}
    ../vba-over.ini
    # icon File
    xrc/visualboyadvance-m.xpm
    # generated includes must be explicitly listed
    ${CMAKE_CURRENT_BINARY_DIR}/builtin-xrc.h
    ${CMAKE_CURRENT_BINARY_DIR}/builtin-over.h
    ${CMAKE_CURRENT_BINARY_DIR}/cmdhandlers.h
    ${CMAKE_CURRENT_BINARY_DIR}/cmd-evtable.h
    # generated
    ${CMAKE_CURRENT_BINARY_DIR}/cmdtab.cpp
)

set(
    CM_STUFF
    copy-events.cmake
)

list(APPEND ALL_SRC_WX openal.cpp)
list(APPEND ALL_HDR_WX openal.h)

if(ENABLE_OPENAL)
   list(APPEND SRC_WX openal.cpp)
   list(APPEND HDR_WX openal.h)
endif()

list(APPEND ALL_SRC_WX xaudio2.cpp)

if(ENABLE_XAUDIO2)
   list(APPEND SRC_WX xaudio2.cpp)
   find_package(xaudio2redist CONFIG REQUIRED)
endif()

list(APPEND ALL_SRC_WX faudio.cpp)

if(ENABLE_FAUDIO)
   list(APPEND SRC_WX faudio.cpp)
endif()

list(APPEND ALL_SRC_WX dsound.cpp)

if(WIN32)
    list(APPEND SRC_WX dsound.cpp)

    list(APPEND RES_WX wxvbam.rc)

    list(APPEND VBAM_LIBS dxguid dsound wsock32 ws2_32 imm32 version)
endif()

link_directories(${CMAKE_BINARY_DIR})

set(VBAM_ICON visualboyadvance-m.icns)

set(VBAM_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/icons/${VBAM_ICON})

if(APPLE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
endif()

if(NOT TRANSLATIONS_ONLY)
    add_executable(
        visualboyadvance-m
        WIN32
        MACOSX_BUNDLE
        ${SRC_WX}
        ${HDR_WX}
        ${RES_WX}
        ${VBAM_ICON_PATH}
        ${CM_STUFF}
    )
    target_compile_features(visualboyadvance-m PRIVATE cxx_std_17)

    target_link_libraries(visualboyadvance-m "${GETOPT_LIB}")

    if(ENABLE_XAUDIO2)
        target_link_libraries(visualboyadvance-m Microsoft::XAudio2Redist)
    endif()
    if(WIN32 AND (X86_64 OR X86_32) AND ENABLE_ONLINEUPDATES)
        find_file(WINSPARKLE_DLL NAMES WinSparkle.dll PATH_SUFFIXES bin)
        set(WINSPARKLE_DLL WinSparkle.dll)
        configure_file(autoupdater/wxmsw/winsparkle-path.h.in ${CMAKE_BINARY_DIR}/winsparkle-path.h)
    endif()

    if(APPLE AND ENABLE_ONLINEUPDATES)
        include(FetchContent)
        FetchContent_Declare(Sparkle
            URL "https://github.com/sparkle-project/Sparkle/releases/download/2.3.0-beta.2/Sparkle-2.3.0-beta.2.tar.xz"
            URL_HASH SHA256=6875388aae23c1705c3956c62a9c967f4b788bc4f1dad93ab5645bc6096ef13b
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(Sparkle)

        find_library(SPARKLE_FRAMEWORK
          NAMES Sparkle
          HINTS ${sparkle_SOURCE_DIR}
        )
        find_path(SPARKLE_INCLUDE_DIR Sparkle.h HINTS ${SPARKLE_FRAMEWORK}/Headers)
        target_include_directories(
            visualboyadvance-m
            PRIVATE ${SPARKLE_INCLUDE_DIR}
        )

        set(APPCAST_URL "https://data.vba-m.com/appcast.xml")
        set(CCS1 rm -rf ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks/Sparkle.framework)
        set(CCS2 mkdir -p ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks)
        # The following commands *should* be run to make sure Sparkle is not going
        # to bug randomly.
        set(CCS3 cp -a ${SPARKLE_FRAMEWORK} ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks/Sparkle.framework)
        set(CCS4 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist CFBundleVersion            -string "${VERSION}")
        set(CCS5 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist CFBundleShortVersionString -string "${VERSION}")
        set(CCS6 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUEnableAutomaticChecks -bool YES)
        #set(CCS7 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUPublicEDKey -string "${PUBLIC_KEY}")
        set(CCS8 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUFeedURL -string "${APPCAST_URL}")
        add_custom_command(TARGET visualboyadvance-m POST_BUILD
          COMMAND ${CCS1}
          COMMAND ${CCS2}
          COMMAND ${CCS3}
          COMMAND ${CCS4}
          COMMAND ${CCS5}
          COMMAND ${CCS6}
          #COMMAND ${CCS7}
          COMMAND ${CCS8}
        )

        TARGET_LINK_LIBRARIES(visualboyadvance-m ${SPARKLE_FRAMEWORK})
    endif()

    target_link_libraries(
        visualboyadvance-m
        wx::core wx::base wx::net wx::wxgl wx::wxxml wx::wxxrc wx::wxhtml
        ${VBAM_LIBS}
        sfml-network
    )

    if(ENABLE_FFMPEG)
        target_link_libraries(
            visualboyadvance-m
            ${FFMPEG_LIBRARIES}
        )

        if(FFMPEG_LDFLAGS)
            join("${FFMPEG_LDFLAGS}" " " FFMPEG_LDFLAGS_STR)

            set_target_properties(
                visualboyadvance-m
                PROPERTIES LINK_FLAGS ${FFMPEG_LDFLAGS_STR}
            )
        endif()
    endif()

    if(WIN32)
        # Force a re-link when the manifest file is modified.
        set_target_properties(visualboyadvance-m
            PROPERTIES
            LINK_DEPENDS
            "${CMAKE_CURRENT_LIST_DIR}/visualboyadvance-m.manifest")

        if(MSVC)
            # Disable the auto-generated manifest from CMake.
            target_link_options(visualboyadvance-m PRIVATE "/MANIFEST:NO")
        endif()
    endif()

    if(NOT WIN32 AND NOT APPLE)
        install(FILES     "${CMAKE_CURRENT_SOURCE_DIR}/visualboyadvance-m.desktop"     DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
        install(FILES     "${CMAKE_CURRENT_SOURCE_DIR}/visualboyadvance-m.appdata.xml" DESTINATION ${CMAKE_INSTALL_PREFIX}/share/appdata)
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/icons/sizes/"                   DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor)
    endif()
endif()

# Make the translations.zip
if(ENABLE_NLS)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/translations.zip
        COMMAND ${CMAKE_COMMAND} -D "ZIP_PROGRAM=${ZIP_PROGRAM}" -P ${CMAKE_CURRENT_SOURCE_DIR}/make-translations-zip.cmake
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS translations make-translations-zip.cmake
    )

    add_custom_target(
        translations-zip
        SOURCES ${CMAKE_BINARY_DIR}/translations.zip
    )

    add_dependencies(visualboyadvance-m translations-zip)

# Update the gettext pot source.
# Do this automatically instead of manually to make sure we don't forget to update.
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
        COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{PATH}" ${WXRC} -g ${XRC_SOURCES} -o ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${XRC_SOURCES}
    )

    add_custom_target(xrc-strings DEPENDS ${CMAKE_BINARY_DIR}/wx-xrc-strings.h)

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/wxvbam.pot
        COMMAND ${XGETTEXT} -k_ -kN_ -o ${CMAKE_BINARY_DIR}/wxvbam.pot ${ALL_SRC_WX} ${ALL_HDR_WX} ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
        DEPENDS ${ALL_SRC_WX} ${ALL_HDR_WX} ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_custom_target(gettext-pot ALL DEPENDS ${CMAKE_BINARY_DIR}/wxvbam.pot)

    add_dependencies(gettext-pot xrc-strings)

    add_custom_command(
        TARGET gettext-pot
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -D SRC_DIR=${CMAKE_SOURCE_DIR}/po/wxvbam -D BIN_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/check-pot-updated.cmake
    )
endif()

if(NOT TRANSLATIONS_ONLY)
    find_program(GPG_PROGRAM gpg)

    if(GPG_PROGRAM)
        execute_process(
            COMMAND ${GPG_PROGRAM} -k
            OUTPUT_VARIABLE GPG_KEYS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    if(NOT ZIP_SUFFIX)
        set(ZIP_SUFFIX "")
    endif()

    if(UPSTREAM_RELEASE AND WIN32)
        set(home "$ENV{HOME}")

        if(NOT CMAKE_CROSSCOMPILING AND NOT DEFINED ENV{MSYSTEM_PREFIX})
            set(home "$ENV{USERPROFILE}")
        endif()

        # rewrite backslashes to slashes, needed for msys osslsigncode
        string(REGEX REPLACE "\\\\" "/" home "${home}")

        set(cert "${home}/.codesign/windows_comodo.pkcs12")

        if(EXISTS "${cert}")
            find_program(OSSLSIGNCODE_PROGRAM osslsigncode)
            find_program(SIGNTOOL_PROGRAM     signtool)

            if(OSSLSIGNCODE_PROGRAM)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E rename visualboyadvance-m.exe visualboyadvance-m-unsigned.exe
                    COMMAND ${OSSLSIGNCODE_PROGRAM} sign -pkcs12 ${cert} -pass "vbam3!13" -t http://timestamp.digicert.com -n visualboyadvance-m -i https://github.com/visualboyadvance-m/visualboyadvance-m -in visualboyadvance-m-unsigned.exe -out visualboyadvance-m.exe
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    )
            elseif(SIGNTOOL_PROGRAM)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy visualboyadvance-m.exe visualboyadvance-m-unsigned.exe
                    COMMAND ${SIGNTOOL_PROGRAM} sign /f ${cert} /p "vbam3!13" /tr http://timestamp.digicert.com /du https://github.com/visualboyadvance-m/visualboyadvance-m /a visualboyadvance-m.exe
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    )
            endif()
        endif()

        if(CMAKE_BUILD_TYPE STREQUAL Debug)
            set(exezip visualboyadvance-m-Win-${ARCH_NAME}-debug${ZIP_SUFFIX}.zip)
        else()
            set(exezip visualboyadvance-m-Win-${ARCH_NAME}${ZIP_SUFFIX}.zip)
        endif()

        unset(pdb_file)

        if(MSVC AND CMAKE_BUILD_TYPE STREQUAL Debug)
            set(pdb_file visualboyadvance-m.pdb)
        endif()

        add_custom_command(
            TARGET visualboyadvance-m
            POST_BUILD
            COMMAND ${ZIP_PROGRAM} -9 ${exezip} visualboyadvance-m.exe ${pdb_file}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        if(GPG_KEYS)
            add_custom_command(
                OUTPUT ${CMAKE_BINARY_DIR}/translations.zip.asc
                COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/translations.zip.asc
    #            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/interactive-pause.cmake
                COMMAND ${GPG_PROGRAM} --detach-sign -a ${CMAKE_BINARY_DIR}/translations.zip
                DEPENDS ${CMAKE_BINARY_DIR}/translations.zip
            )

            add_custom_target(translations-zip-sig DEPENDS ${CMAKE_BINARY_DIR}/translations.zip.asc)

            add_dependencies(translations-zip translations-zip-sig)

            add_custom_command(
                TARGET visualboyadvance-m
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove ${exezip}.asc
    #            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/interactive-pause.cmake
                COMMAND ${GPG_PROGRAM} --detach-sign -a ${exezip}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()

        if(NOT MSVC AND CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
            find_program(STRIP_PROGRAM strip)

            if(STRIP_PROGRAM)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${STRIP_PROGRAM} visualboyadvance-m.exe
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endif()
        endif()
    endif()

    if(APPLE)
        # this should set ROM file types correctly
        set_property(TARGET visualboyadvance-m APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
        set(MACOSX_BUNDLE_ICON_FILE ${VBAM_ICON})
        set_source_files_properties(${VBAM_ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

        # bundle dylibs and relink them for releasing .app
        # also install translations into the .app
        # but only in Release mode
        if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
            add_custom_command(
                TARGET visualboyadvance-m POST_BUILD
                COMMAND ${CMAKE_SOURCE_DIR}/tools/macOS/third_party_libs_tool ./visualboyadvance-m.app
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            )

            add_custom_command(
                TARGET visualboyadvance-m POST_BUILD
                COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/mac-translations.cmake
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()
    endif()

    if(APPLE AND (UPSTREAM_RELEASE OR ENABLE_ONLINEUPDATES))
        if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
            find_program(STRIP_PROGRAM strip)

            if(STRIP_PROGRAM)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${STRIP_PROGRAM} visualboyadvance-m.app/Contents/MacOS/visualboyadvance-m
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endif()
        endif()

        add_custom_command(
            TARGET visualboyadvance-m
            POST_BUILD
            VERBATIM COMMAND sh -c [=[codesign --sign "Developer ID Application" --force --deep ./visualboyadvance-m.app || :]=]
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        if(EXISTS ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks)
            # Sign frameworks individually, like Xcode.
            file(GLOB frameworks ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks/*)
            foreach(framework ${frameworks})
                message(STATUS "Signing framework: " ${framework})
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    VERBATIM COMMAND sh -c "codesign --sign 'Developer ID Application' --force ${framework} || :"
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endforeach()
        endif()

        if(UPSTREAM_RELEASE)
           if(CMAKE_BUILD_TYPE STREQUAL Debug)
              set(appzip visualboyadvance-m-Mac-${ARCH_NAME}-debug${ZIP_SUFFIX}.zip)
           else()
              set(appzip visualboyadvance-m-Mac-${ARCH_NAME}${ZIP_SUFFIX}.zip)
           endif()

            add_custom_command(
                TARGET visualboyadvance-m
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove ${appzip}
                COMMAND ${ZIP_PROGRAM} -9yr ${appzip} ./visualboyadvance-m.app
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )

            if(GPG_KEYS)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E remove ${appzip}.asc
    #                COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/interactive-pause.cmake
                    COMMAND ${GPG_PROGRAM} --detach-sign -a ${appzip}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endif()
        endif()
    endif()

    if(UPSTREAM_RELEASE AND NOT WIN32 AND NOT APPLE AND CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
        find_program(STRIP_PROGRAM strip)

        if(STRIP_PROGRAM)
            add_custom_command(
                TARGET visualboyadvance-m
                POST_BUILD
                COMMAND ${STRIP_PROGRAM} visualboyadvance-m
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()
    endif()

    install(
        TARGETS visualboyadvance-m
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    if(BUILD_TESTING AND (NOT CMAKE_CROSSCOMPILING))
        add_subdirectory(tests)
    endif()
endif()

# vim:sw=4 et tw=0:

#Do not use this file directly.  Always use the top level CMakeLists.txt file
SET( CMAKE_CXX_FLAGS -std=gnu++11 )

#GTK dependencies
FIND_PACKAGE ( PkgConfig REQUIRED )
FIND_PACKAGE ( Gettext REQUIRED )

# These dependencies require pkg-config to be found
PKG_CHECK_MODULES ( GLIBMM REQUIRED glibmm-2.4 )
PKG_CHECK_MODULES ( GIOMM REQUIRED giomm-2.4 )

if(ENABLE_GTK)
    PKG_CHECK_MODULES ( GTKMM REQUIRED gtkmm-2.4 )
    PKG_CHECK_MODULES ( GDKMM REQUIRED gdkmm-2.4 )
    PKG_CHECK_MODULES ( GTKGLMM REQUIRED gtkglextmm-x11-1.2 )
endif(ENABLE_GTK)

if(ENABLE_GTK3)
    PKG_CHECK_MODULES ( GTKMM REQUIRED gtkmm-3.0 )
    PKG_CHECK_MODULES ( GDKMM REQUIRED gdkmm-3.0 )
endif(ENABLE_GTK3)

if(NOT APPLE AND NOT WIN32)
    FIND_PACKAGE ( X11 REQUIRED )
endif(NOT APPLE AND NOT WIN32)

#Make sure the gtk ui elements are available for out of tree builds
#See window.cpp:1544 (the sGetUiFilePath function) for more details
#file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/ui/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ui/)

SET(SRC_GTK
    configfile.cpp
    main.cpp
    system.cpp
    windowcallbacks.cpp
    filters.cpp
    generalconfig.cpp
    gameboyconfig.cpp
    gameboyadvanceconfig.cpp
    cheatlist.cpp
    cheatedit.cpp
    gameboyadvancecheatlist.cpp
    gameboycheatlist.cpp
    joypadconfig.cpp
    directoriesconfig.cpp
    displayconfig.cpp
    soundconfig.cpp
    screenarea.cpp
    screenarea-cairo.cpp
    #screenarea-opengl.cpp
    tools.cpp
    window.cpp
    ../sdl/inputSDL.cpp
)

INCLUDE_DIRECTORIES(
    ${GLIBMM_INCLUDE_DIRS}
    ${GIOMM_INCLUDE_DIRS}
    ${GTKMM_INCLUDE_DIRS}
    ${GDKMM_INCLUDE_DIRS}
    ${GTKGLMM_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
)

LINK_DIRECTORIES(
    ${GLIBMM_LIBRARY_DIRS}
    ${GIOMM_LIBRARY_DIRS}
    ${GTKMM_LIBRARY_DIRS}
    ${GTKGLMM_LIBRARY_DIRS}
)

ADD_EXECUTABLE (
    gvbam
    WIN32
    MACOSX_BUNDLE
    ${SRC_GTK}
    ../common/SoundSDL.cpp
)

TARGET_LINK_LIBRARIES (
    gvbam
    ${VBAMCORE_LIBS}
    ${GTKMM_LIBRARIES}
    ${GTKGLMM_LIBRARIES}
    ${X11_LIBRARIES}
)

INSTALL(PROGRAMS ${PROJECT_BINARY_DIR}/gvbam DESTINATION bin)
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ui DESTINATION ${DATA_INSTALL_DIR})
IF(NOT WIN32 AND NOT APPLE)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gvbam.desktop DESTINATION share/applications)
ENDIF(NOT WIN32 AND NOT APPLE)

if(APPLE)
    #Probably don't need this
    SET(CMAKE_EXECUTABLE_SUFFIX .app)
    # this should set ROM file types correctly
    SET_PROPERTY(TARGET wxvbam APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
endif(APPLE)

SET(WX_EXE_NAME gvbam${CMAKE_EXECUTABLE_SUFFIX})

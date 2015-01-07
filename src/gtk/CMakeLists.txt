#Do not use this file directly.  Always use the top level CMakeLists.txt file

#Make sure the gtk ui elements are available for out of tree builds
#See window.cpp:1544 (the sGetUiFilePath function) for more details
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/ui/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ui/)

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
    screenarea-opengl.cpp
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
)

TARGET_LINK_LIBRARIES (
    gvbam
    ${VBAMCORE_LIBS}
    ${GTKMM_LIBRARIES}
    ${GTKGLMM_LIBRARIES}
)

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/gvbam DESTINATION bin)
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

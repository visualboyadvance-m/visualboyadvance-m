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
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gvbam.desktop DESTINATION share/applications)
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/icons/ DESTINATION share/icons/hicolor PATTERN ".svn" EXCLUDE)
INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ui DESTINATION ${DATA_INSTALL_DIR} PATTERN ".svn" EXCLUDE)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../vba-over.ini DESTINATION ${DATA_INSTALL_DIR})

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
IF(NOT WIN32 AND NOT APPLE)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/gvbam.desktop DESTINATION share/applications)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/icons/ DESTINATION share/icons/hicolor PATTERN ".svn" EXCLUDE)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ui DESTINATION ${DATA_INSTALL_DIR} PATTERN ".svn" EXCLUDE)
ENDIF(NOT WIN32 AND NOT APPLE)

# for consistency with others, copy exe to top-level dir
if(WIN32)
    SET(GTK_EXE_NAME gvbam${CMAKE_EXECUTABLE_SUFFIX})
    ADD_CUSTOM_COMMAND(TARGET gvbam POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy ${GTK_EXE_NAME} ../../${GTK_EXE_NAME})
else(WIN32)
if(APPLE)
    SET(GTK_EXE_NAME gvbam.app)
    # this should set ROM file types correctly
    SET_PROPERTY(TARGET gvbam APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
else(APPLE)
    SET(GTK_EXE_NAME gvbam${CMAKE_EXECUTABLE_SUFFIX})
endif(APPLE)
    ADD_CUSTOM_COMMAND(TARGET gvbam POST_BUILD
               # I'd rather make this link relative, but it's too hard
                       COMMAND rm -rf ../../${GTK_EXE_NAME}
                       COMMAND ln -s ${CMAKE_CURRENT_BINARY_DIR}/${GTK_EXE_NAME} ../../${GTK_EXE_NAME})
endif(WIN32)

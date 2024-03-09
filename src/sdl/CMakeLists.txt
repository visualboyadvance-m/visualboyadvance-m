if(NOT ENABLE_SDL)
    return()
endif()

# Define the vbam executable.
add_executable(vbam WIN32)

target_sources(vbam
    PRIVATE
    ConfigManager.cpp
    ConfigManager.h
    dictionary.c
    dictionary.h
    expr.cpp
    expr.cpp.h
    exprNode.cpp
    exprNode.h
    expr-lex.cpp
    filters.cpp
    filters.h
    iniparser.c
    iniparser.h
    inputSDL.cpp
    inputSDL.h
    SDL.cpp
)

if(MSVC)
    # We should probably use an external library for this.
    target_sources(vbam
        PRIVATE
        ${CMAKE_SOURCE_DIR}/dependencies/msvc/getopt.c
        ${CMAKE_SOURCE_DIR}/dependencies/msvc/getopt.h
    )
endif()

target_link_libraries(vbam
    vbam-core
    vbam-components-audio-sdl
    vbam-components-draw-text
    vbam-components-filters
    vbam-components-filters-agb
    vbam-components-filters-interframe
    vbam-components-user-config
    ${OPENGL_LIBRARIES}
)

if(WIN32)
    target_link_libraries(vbam ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY})
endif()

if(ENABLE_LIRC)
    target_link_libraries(vbam lirc_client)
    target_compile_definitions(vbam VBAM_ENABLE_LIRC)
endif()

if(WIN32)
    target_link_libraries(vbam wsock32 ws2_32 winmm version imm32 ${SDL2MAIN_LIBRARY})
endif()

# Installation scripts.
install(
    PROGRAMS ${PROJECT_BINARY_DIR}/vbam${CMAKE_EXECUTABLE_SUFFIX}
    DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
)
install(
    FILES ${CMAKE_CURRENT_LIST_DIR}/vbam.cfg-example
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}
    RENAME vbam.cfg
)

if(UNIX)
    # man pages.
    install(FILES ${CMAKE_SOURCE_DIR}/src/debian/vbam.6 DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man6)
endif()

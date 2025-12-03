if(NOT ENABLE_SDL)
    return()
endif()

# Define the vbam executable (or object library in COMPILE_ONLY mode).
if(COMPILE_ONLY)
    add_library(vbam OBJECT)
else()
    add_executable(vbam WIN32)
endif()

target_sources(vbam
    PRIVATE
    audio_sdl.cpp
    audio_sdl.h
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

target_compile_definitions(vbam PRIVATE YY_NO_INPUT)

if(MSVC)
    # We should probably use an external library for this.
    target_sources(vbam
        PRIVATE
        ${CMAKE_SOURCE_DIR}/win32-deps/msvc/getopt.c
        ${CMAKE_SOURCE_DIR}/win32-deps/msvc/getopt.h
    )
endif()

if(ENABLE_LIRC)
    set(LIRC_CLIENT_LIBRARY lirc_client)
endif()

if(ENABLE_SDL3)
    target_include_directories(vbam
        PRIVATE ${SDL3_INCLUDE_DIRS}
    )
else()
    target_include_directories(vbam
        PRIVATE ${SDL2_INCLUDE_DIRS}
    )
endif()

if(COMPILE_ONLY)
    target_link_libraries(vbam PUBLIC nonstd-lib)
else()
    target_link_libraries(vbam nonstd-lib)
endif()

if(NOT COMPILE_ONLY)
    target_link_libraries(vbam
        vbam-core
        vbam-components-draw-text
        vbam-components-filters
        vbam-components-filters-agb
        vbam-components-filters-cgb
        vbam-components-filters-interframe
        vbam-components-user-config
        ${OPENGL_LIBRARIES}
        ${VBAM_SDL_LIBS}
    )

    if(WIN32)
        if(ENABLE_SDL3)
            target_link_libraries(vbam ${SDL3_LIBRARY})
        else()
            target_link_libraries(vbam ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY})
        endif()
    endif()

    if(ENABLE_LIRC)
        target_link_libraries(vbam lirc_client)
    endif()

    if(WIN32)
        target_link_libraries(vbam wsock32 ws2_32 winmm version imm32)
    endif()
endif()

if(ENABLE_LIRC)
    target_compile_definitions(vbam PUBLIC VBAM_ENABLE_LIRC)
endif()

# Installation scripts.
if(NOT COMPILE_ONLY)
    install(
        PROGRAMS ${PROJECT_BINARY_DIR}/vbam${CMAKE_EXECUTABLE_SUFFIX}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(
        FILES ${CMAKE_CURRENT_LIST_DIR}/vbam.cfg-example
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
        RENAME vbam.cfg
    )

    if(UNIX)
        # man pages.
        install(FILES ${CMAKE_SOURCE_DIR}/src/debian/vbam.6 DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
    endif()
endif()

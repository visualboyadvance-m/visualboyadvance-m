cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19...3.28.3)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(WIN32)
    include(RemoveStrawberryPerlFromPATH)
endif()

if(UPDATE_APPCAST)
    include(UpdateAppcast)
endif()

if(TAG_RELEASE)
    include(MakeReleaseCommitAndTag)
endif()

set(VCPKG_DEPS pkgconf zlib pthreads "sdl2[samplerate]" gettext wxwidgets)

set(VCPKG_DEPS_OPTIONAL
    sfml   ENABLE_LINK
    ffmpeg ENABLE_FFMPEG
)

if(WIN32)
    find_program(POWERSHELL
        NAMES powershell.exe pwsh.exe
        HINTS "/Windows/System32/WindowsPowerShell/v1.0"
        REQUIRED)
else()
    find_program(POWERSHELL pwsh)
endif()

include(Set-Toolchain-vcpkg)

# Use ccache if available and not already enabled on the command line.
# This has to be done before the project() call.
if(NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(CCACHE_EXECUTABLE ccache)
    if(CCACHE_EXECUTABLE)
        message(STATUS "Enabling ccache")

        set(CMAKE_C_COMPILER_LAUNCHER        ${CCACHE_EXECUTABLE} CACHE STRING "C compiler launcher"     FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER      ${CCACHE_EXECUTABLE} CACHE STRING "C++ compiler launcher"   FORCE)
        set(CMAKE_ASM_NASM_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE} CACHE STRING "nasm assembler launcher" FORCE)
    endif()
endif()

find_package(Git)

# Make sure we pull in the submodules on windows and mingw.
if(GIT_FOUND AND (WIN32 OR MINGW))
    # Win32 deps submodule
    set(SUBMODULE_MANUAL_UPDATE FALSE)

    if(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
        set(SUBMODULE_MANUAL_UPDATE TRUE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" submodule update --init --remote --recursive
            RESULT_VARIABLE SUBMODULE_UPDATE_STATUS
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
        if(NOT (SUBMODULE_MANUAL_UPDATE AND SUBMODULE_UPDATE_STATUS EQUAL 0))
            message(FATAL_ERROR "Please pull in git submodules, e.g.\nrun: git submodule update --init --remote --recursive")
        endif()
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

project(VBA-M C CXX)

find_package(PkgConfig)

if(NOT CMAKE_PREFIX_PATH AND (NOT ("$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")))
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|Debug|RelWithDebInfo|MinSizeRel)$")
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}', must be one of: 'Release', 'Debug', 'RelWithDebInfo' or 'MinSizeRel'")
endif()

# Link debug libs for RelWithDebInfo
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Debug")
endif()

include(CTest)
if(BUILD_TESTING)
    enable_testing()
endif()

include(GNUInstallDirs)
include(Options)
include(Architecture)
include(Toolchain)

#Output all binaries at top level
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

if(NOT HTTPS)
    add_compile_definitions(NO_HTTPS)
endif()

if(ENABLE_GBA_LOGGING)
    add_compile_definitions(GBA_LOGGING )
endif()

if(ENABLE_MMX)
    add_compile_definitions(MMX)
endif()

# The SDL port can't be built without debugging support
if(NOT ENABLE_DEBUGGER AND ENABLE_SDL)
    message(SEND_ERROR "The SDL port can't be built without debugging support")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    include(GitTagVersion)
    git_version(VBAM_VERSION VBAM_REVISION VBAM_VERSION_RELEASE)

    # only use the plugin to tie the configure state to the sha to force rebuilds
    # of files that depend on version.h
    include(GetGitRevisionDescription)
    get_git_head_revision(REFSPEC COMMITHASH)

    # Make sure old tags are gone from all clones.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} tag -l
        OUTPUT_VARIABLE git_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    string(REGEX REPLACE ";"     "\\\\;" git_tags_lines "${git_tags}")
    string(REGEX REPLACE "\r?\n" ";"     git_tags_lines "${git_tags_lines}")

    set(found_old_tags FALSE)

    foreach(tag ${git_tags_lines})
        if(NOT tag MATCHES "^v[0-9]")
            set(found_old_tags TRUE)
            break()
        endif()
    endforeach()

    if(found_old_tags)
        # Delete all tags and fetch them from the origin.

        foreach(tag ${git_tags_lines})
            execute_process(
                COMMAND ${GIT_EXECUTABLE} tag -d ${tag}
                OUTPUT_QUIET
                ERROR_QUIET
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        endforeach()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} fetch --tags origin
            OUTPUT_QUIET
            ERROR_QUIET
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()
endif()

# no git or no tags, use ChangeLog
if(NOT VBAM_VERSION)
    include(ChangeLogVersion)
    changelog_version(VBAM_VERSION VBAM_REVISION VBAM_VERSION_RELEASE)
endif()

# We do not support amd64 asm yet
if(X86_64 AND (ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS OR ENABLE_MMX))
    message(FATAL_ERROR "The options ASM_CORE, ASM_SCALERS and MMX are not supported on X86_64 yet.")
endif()

if(NOT WIN32)
    find_library(PTHREAD_LIB pthread)
    if(PTHREAD_LIB)
       set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${PTHREAD_LIB})
       set(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${PTHREAD_LIB})
    endif()
elseif(MINGW)
    if(NOT VBAM_STATIC)
        set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} -lpthread)
        set(VBAMCORE_LIBS ${VBAMCORE_LIBS} -lpthread)
    else()
        set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} "-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
        set(VBAMCORE_LIBS ${VBAMCORE_LIBS} "-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
    endif()
else()
    find_package(PThreads4W)
    if(PThreads4W_FOUND)
        set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} PThreads4W::PThreads4W)
        set(VBAMCORE_LIBS ${VBAMCORE_LIBS} PThreads4W::PThreads4W)
    endif()
endif()

# Look for some dependencies using CMake scripts
find_package(ZLIB REQUIRED)

set(OpenGL_GL_PREFERENCE GLVND)

if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    set(OpenGL_GL_PREFERENCE LEGACY)
endif()

find_package(OpenGL REQUIRED)
find_package(SDL2   REQUIRED)

# Add libsamplerate to SDL2 with vcpkg
unset(SDL2_LIBRARY_TEMP)
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    if(WIN32)
        unset(arch_suffix)
        unset(path_prefix)
        if(VCPKG_TARGET_TRIPLET MATCHES -static)
            set(arch_suffix  -static)
        endif()
        if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
            set(path_prefix debug)
        endif()
        set(installed_prefix ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows${arch_suffix}/${path_prefix})

        SET(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} ${installed_prefix}/lib/samplerate.lib)
    else()
        SET(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} -lsamplerate)
    endif()
endif()

set(VBAM_SDL2_LIBS SDL2::SDL2 ${SDL2_LIBRARY_TEMP})

# set the standard libraries all ports use
set(
    VBAMCORE_LIBS
    vbam-core
    ${VBAM_SDL2_LIBS}
    ${OPENGL_LIBRARIES}
)

set(VBAM_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)

# Set up "src" as a global include directory.
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${VBAM_GENERATED_DIR}
)

if(ENABLE_FFMPEG)
    if(NOT FFMPEG_LIBRARIES)
        message(FATAL_ERROR "ENABLE_FFMPEG was specified, but required versions of ffmpeg libraries cannot be found!")
    endif()

    if(APPLE)
        set(FFMPEG_LDFLAGS ${FFMPEG_LDFLAGS} -framework CoreText -framework ApplicationServices)

        if(UPSTREAM_RELEASE)
            set(FFMPEG_LDFLAGS ${FFMPEG_LDFLAGS} -lbz2 -ltiff -framework DiskArbitration -lfreetype -lfontconfig -llzma -lxml2 -lharfbuzz)
        endif()
    elseif(WIN32)
        set(WIN32_MEDIA_FOUNDATION_LIBS dxva2 evr mf mfplat mfplay mfreadwrite mfuuid amstrmid)
        list(APPEND FFMPEG_LIBRARIES secur32 bcrypt ${WIN32_MEDIA_FOUNDATION_LIBS})

        if(MSYS AND VBAM_STATIC)
            foreach(lib tiff jbig lzma)
                cygpath(lib "$ENV{MSYSTEM_PREFIX}/lib/lib${lib}.a")

                list(APPEND FFMPEG_LIBRARIES "${lib}")
            endforeach()
        endif()
    endif()
else()
    add_compile_definitions(NO_FFMPEG)
endif()

if(NOT ENABLE_ONLINEUPDATES)
  add_compile_definitions(NO_ONLINEUPDATES)
endif()

if(ENABLE_LIRC)
    set(WITHLIRC 1)
else()
    set(WITHLIRC 0)
endif()

# C defines
add_compile_definitions(HAVE_NETINET_IN_H HAVE_ARPA_INET_H HAVE_ZLIB_H FINAL_VERSION SDL USE_OPENGL SYSCONF_INSTALL_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}" WITH_LIRC=${WITHLIRC})
add_compile_definitions(PKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/vbam")
add_compile_definitions(__STDC_FORMAT_MACROS)

if(ENABLE_LINK)
    # IPC linking code needs sem_timedwait which can be either in librt or pthreads
    if(NOT WIN32)
        find_library(RT_LIB rt)
        if(RT_LIB)
           set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${RT_LIB})
           set(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${RT_LIB})
        endif()
    endif()

    include(CheckFunctionExists)
    check_function_exists(sem_timedwait SEM_TIMEDWAIT)
    if(SEM_TIMEDWAIT)
        add_compile_definitions(HAVE_SEM_TIMEDWAIT)
    endif()
else()
    add_compile_definitions(NO_LINK)
endif()

# The debugger is enabled by default
if(ENABLE_DEBUGGER)
    add_compile_definitions(VBAM_ENABLE_DEBUGGER)
endif()

# The ASM core is disabled by default because we don't know on which platform we are
if(NOT ENABLE_ASM_CORE)
    add_compile_definitions(C_CORE)
endif()

# Enable internationalization
if(ENABLE_NLS)
    set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
    add_compile_definitions(ENABLE_NLS)
    add_compile_definitions(LOCALEDIR="${LOCALEDIR}")
    # for now, only GBALink.cpp uses gettext() directly
    if(APPLE)
        # use Homebrew gettext if available
        if(EXISTS "/usr/local/opt/gettext")
            set(CMAKE_INCLUDE_PATH "${CMAKE_INCLUDE_PATH};/usr/local/opt/gettext/include")
            set(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH};/usr/local/opt/gettext/lib")
            set(CMAKE_PROGRAM_PATH "${CMAKE_PROGRAM_PATH};/usr/local/opt/gettext/bin")
        endif()
    endif()
    if(ENABLE_LINK OR ENABLE_WX)
        find_path(LIBINTL_INC libintl.h)
        find_library(LIBINTL_LIB    NAMES libintl    intl)
        find_library(LIBICONV_LIB   NAMES libiconv   iconv)
        find_library(LIBCHARSET_LIB NAMES libcharset charset)
        if(LIBINTL_LIB)
            list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBINTL_LIB})
            list(APPEND NLS_LIBS                 ${LIBINTL_LIB})
        endif()
        if(LIBICONV_LIB)
            list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBICONV_LIB})
            list(APPEND NLS_LIBS                 ${LIBICONV_LIB})
        endif()
        if(LIBCHARSET_LIB)
            list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBCHARSET_LIB})
            list(APPEND NLS_LIBS                 ${LIBCHARSET_LIB})
        endif()
        include(CheckFunctionExists)
        check_function_exists(gettext GETTEXT_FN)
        if(NOT (LIBINTL_INC OR GETTEXT_FN))
            message(FATAL_ERROR "NLS requires libintl/gettext")
        endif()
    endif()
endif()

if(NOT TRANSLATIONS_ONLY)
    add_subdirectory(third_party/include/nonstd)
    add_subdirectory(third_party/include/stb)
    add_subdirectory(src/core)
    add_subdirectory(src/components)
endif()

set(
    SRC_SDL
    src/sdl/ConfigManager.cpp
    src/sdl/dictionary.c
    src/sdl/expr.cpp
    src/sdl/exprNode.cpp
    src/sdl/expr-lex.cpp
    src/sdl/filters.cpp
    src/sdl/iniparser.c
    src/sdl/inputSDL.cpp
    src/sdl/SDL.cpp
)
if(MSVC)
    list(APPEND SRC_SDL "dependencies/msvc/getopt.c")
endif()

set(
    HDR_SDL
    src/sdl/ConfigManager.h
    src/sdl/dictionary.h
    src/sdl/expr.cpp.h
    src/sdl/exprNode.h
    src/sdl/filters.h
    src/sdl/iniparser.h
    src/sdl/inputSDL.h
)
if(MSVC)
    list(APPEND HDR_SDL "dependencies/msvc/getopt.h")
endif()

if((NOT TRANSLATIONS_ONLY) AND ENABLE_SDL)
    add_executable(
        vbam
        WIN32
        ${SRC_SDL}
        ${HDR_SDL}
    )

    if(WIN32)
        set(WIN32_LIBRARIES wsock32 ws2_32 winmm version imm32)
    endif()

    if(MSVC)
        # the debug lib libcmtd is linked in debug mode, so don't link the normal version
        set_target_properties(vbam PROPERTIES LINK_FLAGS_DEBUG "/nodefaultlib:libcmt")
    endif()

    if(ENABLE_LIRC)
        set(LIRC_CLIENT_LIBRARY lirc_client)
    endif()

    target_link_libraries(vbam
        vbam-components-audio-sdl
        vbam-components-draw-text
        vbam-components-filters
        vbam-components-filters-agb
        vbam-components-filters-interframe
        vbam-components-user-config
        ${VBAMCORE_LIBS}
        ${WIN32_LIBRARIES}
        ${LIRC_CLIENT_LIBRARY}
    )

    if(WIN32)
        target_link_libraries(vbam ${SDL2MAIN_LIBRARY})
    endif()

    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vbam${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

    if(WIN32)
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
            DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}
            RENAME vbam.cfg
        )
    else()
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
            DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}
            RENAME vbam.cfg
        )
    endif()
endif()

if(ENABLE_WX)
    add_subdirectory(src/wx)
endif()

if(ENABLE_WX)
    # Native Language Support
    if(ENABLE_NLS)
        add_subdirectory(po)
    endif()
endif()
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/vba-over.ini DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/vbam)

# manual pages
if(UNIX)
    if(ENABLE_SDL)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/vbam.6 DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man6)
    endif()
    if(ENABLE_WX)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/visualboyadvance-m.6 DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man6)
    endif()
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0-Git-${COMMITHASH}")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
include(CPack)

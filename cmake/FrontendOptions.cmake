# Frontend selection, and the predicates used to gate dependency probes on it.
#
# This file is included twice: once from the top-level CMakeLists.txt before
# Set-Toolchain-vcpkg.cmake, and once from Options.cmake. include_guard()
# makes the second include a no-op.
#
# The frontend options cannot live in Options.cmake with the rest of them: the
# vcpkg dependency lists are built - and vcpkg is bootstrapped and told what to
# install - before project(), which is long before Options.cmake runs. Since
# every dependency should only be installed and probed for when a frontend that
# actually uses it is being built, the selection has to be known this early.

include_guard(GLOBAL)

option(TRANSLATIONS_ONLY "Build only the translations.zip" OFF)

if(TRANSLATIONS_ONLY)
    set(VBAM_BUILD_DEFAULT OFF)
else()
    set(VBAM_BUILD_DEFAULT ON)
endif()

# On the first include we run before project(), so the usual platform
# variables are either unset or describe the host rather than the target:
# CMake pre-defines WIN32/APPLE from the host, and ANDROID comes from the NDK
# toolchain file, which has not been loaded yet. The vcpkg triplet, the
# toolchain file name and an explicitly passed CMAKE_SYSTEM_NAME are the only
# things that name the target this early, so consult those too. This only
# affects the *defaults* of the options below; both includes see the same
# values because option() takes the cached value on the second pass.
set(VBAM_TARGET_ANDROID OFF)
set(VBAM_TARGET_WIN32   ${WIN32})
set(VBAM_TARGET_APPLE   ${APPLE})

if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android"
        OR VCPKG_TARGET_TRIPLET MATCHES "-android$")
    set(VBAM_TARGET_ANDROID ON)
    set(VBAM_TARGET_WIN32   OFF)
    set(VBAM_TARGET_APPLE   OFF)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows"
        OR VCPKG_TARGET_TRIPLET MATCHES "-(windows|mingw|uwp)"
        OR CMAKE_TOOLCHAIN_FILE MATCHES "[Mm]in[Gg][Ww]|mxe")
    set(VBAM_TARGET_WIN32 ON)
    set(VBAM_TARGET_APPLE OFF)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin|iOS"
        OR VCPKG_TARGET_TRIPLET MATCHES "-(osx|ios)")
    set(VBAM_TARGET_WIN32 OFF)
    set(VBAM_TARGET_APPLE ON)
endif()

# The SDL port is a secondary frontend: on Windows and macOS the wx port is
# the deliverable, and on Android it is the wxQt app.
set(ENABLE_SDL_DEFAULT ${VBAM_BUILD_DEFAULT})

if(VBAM_TARGET_WIN32 OR VBAM_TARGET_APPLE OR VBAM_TARGET_ANDROID)
    set(ENABLE_SDL_DEFAULT OFF)
endif()

option(ENABLE_WX       "Build the wxWidgets port" ${VBAM_BUILD_DEFAULT})
option(ENABLE_SDL      "Build the SDL port"       ${ENABLE_SDL_DEFAULT})
option(ENABLE_LIBRETRO "Build the libretro core"  ${VBAM_BUILD_DEFAULT})

# GBA cable link is a core feature rather than a frontend, but it is the only
# consumer of gettext/libintl besides the wx port, so its value is needed here
# to decide whether libintl is a dependency at all.
#
# The bundled SFML it builds on has no Android backend vendored here (and cable
# link is not meaningful on a phone), so it is off by default there.
if(TRANSLATIONS_ONLY OR VBAM_TARGET_ANDROID)
    set(ENABLE_LINK_DEFAULT OFF)
else()
    set(ENABLE_LINK_DEFAULT ON)
endif()

option(ENABLE_LINK "Enable GBA linking functionality" ${ENABLE_LINK_DEFAULT})

# --- Dependency predicates -------------------------------------------------
#
# Which third-party libraries are worth looking for follows from the frontend
# selection. Everything not listed here is either a core dependency (zlib,
# bzip2, liblzma) or already scoped to one frontend's own CMakeLists.txt.
#
# The libretro core deliberately appears in none of them: it compiles its own
# copy of the emulator sources and links no external library.

# SDL is shared. The SDL port is built on it; the wx port uses it for audio
# output and game controller input, as does vbam-sdl-motion in the core.
set(VBAM_NEED_SDL OFF)
if(ENABLE_WX OR ENABLE_SDL)
    set(VBAM_NEED_SDL ON)
endif()

# OpenGL is a renderer backend in both desktop frontends.
set(VBAM_NEED_OPENGL OFF)
if(ENABLE_WX OR ENABLE_SDL)
    set(VBAM_NEED_OPENGL ON)
endif()

# gettext/libintl: the wx port's translations and the link code's messages.
set(VBAM_NEED_NLS OFF)
if(ENABLE_WX OR ENABLE_LINK)
    set(VBAM_NEED_NLS ON)
endif()

# Everything else - wxWidgets, Qt, nanosvg, Lua, Vulkan/MoltenVK, ffmpeg,
# OpenAL, FAudio, Wayland, Direct3D, XAudio2 - belongs to the wx port alone.
set(VBAM_NEED_WX_DEPS ${ENABLE_WX})

# Force a wx-only feature off when the wx port is not being built, and say so
# if it was asked for explicitly. Used for the options whose defaults come
# from a dependency probe we are about to skip.
function(vbam_disable_option_without_wx name)
    if(NOT ${name})
        return()
    endif()

    message(WARNING
        "${name} is only used by the wxWidgets frontend, which is not being "
        "built (ENABLE_WX=OFF). Disabling it.")

    set(${name} OFF PARENT_SCOPE)
endfunction()

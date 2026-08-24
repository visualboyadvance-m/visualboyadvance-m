option(BUILD_TESTING "Build testing" ON)
option(BUILD_SHARED_LIBS "Build dynamic libraries" OFF)
option(COMPILE_ONLY "Compile source files only, skip linking executables" OFF)

# Detect CI environment or allow explicit setting
if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS} OR DEFINED ENV{GITLAB_CI})
    set(VBAM_CI_DEFAULT ON)
else()
    set(VBAM_CI_DEFAULT OFF)
endif()
option(ENABLE_WERROR "Treat warnings as errors (enabled in CI)" ${VBAM_CI_DEFAULT})

# TRANSLATIONS_ONLY, ENABLE_WX, ENABLE_SDL, ENABLE_LIBRETRO, ENABLE_LINK and
# the VBAM_NEED_* dependency predicates. Already included (before project())
# by the top-level CMakeLists.txt, so this is normally a no-op; it is repeated
# here to keep Options.cmake readable on its own.
include(FrontendOptions)

# Wayland is a wx port display backend.
if(ENABLE_WX AND NOT WIN32 AND NOT APPLE AND NOT ANDROID)
    # Detect whether the installed GDK supports Wayland.
    #
    # We try, in order:
    #   1. pkg-config gdk-wayland-3.0 -- the canonical probe on modern GTK.
    #      libgtk-3-dev and equivalents ship gdk-wayland-3.0.pc whenever
    #      Wayland support is built in.
    #   2. check_include_file("gdk/gdkwayland.h"), but with GTK's include
    #      paths pulled in via pkg-config gtk+-3.0 first. This catches GDK
    #      installs that ship the header but no separate .pc file.
    set(_have_gdk_wayland OFF)

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GDK_WAYLAND_PC QUIET gdk-wayland-3.0)
        if(GDK_WAYLAND_PC_FOUND)
            set(_have_gdk_wayland ON)
        endif()
    endif()

    if(NOT _have_gdk_wayland)
        # Fallback for systems without gdk-wayland-3.0.pc: probe for the
        # header directly, but make sure GTK's include paths are visible to
        # the probe so it can actually find /usr/include/gtk-3.0/gdk/...
        if(PkgConfig_FOUND)
            pkg_check_modules(GTK3_PC QUIET gtk+-3.0)
        endif()

        include(CheckIncludeFile)
        set(_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
        if(GTK3_PC_INCLUDE_DIRS)
            list(APPEND CMAKE_REQUIRED_INCLUDES ${GTK3_PC_INCLUDE_DIRS})
        endif()
        check_include_file("gdk/gdkwayland.h" _have_gdkwayland_header)
        set(CMAKE_REQUIRED_INCLUDES "${_saved_required_includes}")

        if(_have_gdkwayland_header)
            set(_have_gdk_wayland ON)
        endif()
    endif()

    if(_have_gdk_wayland)
        set(no_wayland_default OFF)
    else()
        set(no_wayland_default ON)
    endif()

    option(NO_WAYLAND "Force Wayland disabled" ${no_wayland_default})

    unset(_have_gdk_wayland)
    unset(_have_gdkwayland_header CACHE)
    unset(_saved_required_includes)

    # Wayland protocol client glue is a SEPARATE, independently auto-detected
    # feature from the gdk/gdkwayland.h backend detection above. Generating it
    # needs the wayland-scanner code generator, the wayland-protocols data
    # directory, and libwayland-client -- none of which GDK provides. It drives
    # the HDR (color-management-v1) and software-scaling (viewporter) paths; the
    # C++ code falls back when a protocol is missing. Auto-enabled when the
    # toolchain is present, regardless of whether the GDK Wayland backend was
    # found. The found tools are reused by src/wx/CMakeLists.txt.
    find_program(WAYLAND_SCANNER wayland-scanner)
    find_library(WAYLAND_LIBRARY wayland-client)
    if(PkgConfig_FOUND)
        pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
    endif()

    if(WAYLAND_SCANNER AND WAYLAND_PROTOCOLS_DIR AND WAYLAND_LIBRARY)
        set(_wayland_protocols_default ON)
    else()
        set(_wayland_protocols_default OFF)
    endif()

    option(ENABLE_WAYLAND_PROTOCOLS
        "Generate Wayland protocol client glue (HDR color management, viewporter)"
        ${_wayland_protocols_default})

    unset(_wayland_protocols_default)
endif()

# Static linking
set(VBAM_STATIC_DEFAULT OFF)
if(VCPKG_TARGET_TRIPLET MATCHES -static OR CMAKE_TOOLCHAIN_FILE MATCHES "mxe|-static")
    set(VBAM_STATIC_DEFAULT ON)
elseif(MINGW OR MSYS2)
    # Default to static builds on MinGW and all MSYS2 envs.
    set(VBAM_STATIC_DEFAULT ON)
endif()

option(VBAM_STATIC "Try to link all libraries statically" ${VBAM_STATIC_DEFAULT})

# This is a commonly used CMake option.
if(DEFINED ENABLE_SHARED)
   if(NOT ENABLE_SHARED)
      set(VBAM_STATIC ON)
   else()
      set(VBAM_STATIC OFF)
   endif()
endif()

if(VBAM_STATIC)
    set(SDL2_STATIC ON)
    set(SDL3_STATIC ON)
    set(SFML_STATIC_LIBRARIES ON)
    set(FFMPEG_STATIC ON)
    set(OPENAL_STATIC ON)
    set_property(GLOBAL PROPERTY LINK_SEARCH_START_STATIC ON)
    set_property(GLOBAL PROPERTY LINK_SEARCH_END_STATIC   ON)

    if(MSVC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES .lib)
    else()
        list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .a)
    endif()
endif()

if(WIN32 AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
   if(ARM64 AND NOT CMAKE_CROSSCOMPILING)
      set(PKG_CONFIG_EXECUTABLE "$ENV{VCPKG_ROOT}/installed/arm64-windows/tools/pkgconf/pkgconf.exe")
   else()
      set(PKG_CONFIG_EXECUTABLE "$ENV{VCPKG_ROOT}/installed/x64-windows/tools/pkgconf/pkgconf.exe")
   endif()
endif()

find_package(PkgConfig QUIET)

# Add support for Homebrew, MacPorts and Fink on macOS
option(DISABLE_MACOS_PACKAGE_MANAGERS "Set to TRUE to disable support for macOS Homebrew, MacPorts and Fink." FALSE)
if(APPLE AND NOT DISABLE_MACOS_PACKAGE_MANAGERS)
    include(MacPackageManagers)
endif()

# SDL. The SDL port is built on it, and the wx port uses it for audio output
# and game controller input; the libretro core does not use it at all.
if(NOT VBAM_NEED_SDL)
    set(SDL3_FOUND  OFF)
    set(SDL2_FOUND  OFF)
    set(ENABLE_SDL3 OFF)
elseif(ANDROID)
    # SDL3 for Android is resolved from, in order of precedence:
    #
    #   1. VBAM_ANDROID_SDL3_PREFIX, a custom/patched SDL3 install -- this is
    #      how external-native-window video support is supplied (see
    #      SDL_SetAndroidExternalWindow).
    #   2. A working CMake package config, which is what the vcpkg sdl3 port
    #      installs for the *-android triplets. This is the normal case.
    #   3. The NDK sysroot. NDK r29 vendored SDL3 in a multi-arch sysroot, but
    #      its package config points at a non-existent single-arch
    #      "<prefix>/lib/libSDL3.so" and so fails find_package()'s import check;
    #      the target has to be built by hand from the real per-ABI paths.
    #      Later NDKs ship no SDL3 at all, so this step usually finds nothing.
    set(VBAM_ANDROID_SDL3_PREFIX "" CACHE PATH
        "Prefix of a custom/patched SDL3 install for Android (lib/libSDL3.so + include)")

    unset(_vbam_sdl3_lib)
    unset(_vbam_sdl3_inc)

    if(VBAM_ANDROID_SDL3_PREFIX AND NOT TARGET SDL3::SDL3)
        foreach(_vbam_sdl3_suffix .so .a)
            if(EXISTS "${VBAM_ANDROID_SDL3_PREFIX}/lib/libSDL3${_vbam_sdl3_suffix}")
                set(_vbam_sdl3_lib "${VBAM_ANDROID_SDL3_PREFIX}/lib/libSDL3${_vbam_sdl3_suffix}")
                set(_vbam_sdl3_inc "${VBAM_ANDROID_SDL3_PREFIX}/include")
                break()
            endif()
        endforeach()

        if(_vbam_sdl3_lib)
            message(STATUS "Using custom Android SDL3 from ${VBAM_ANDROID_SDL3_PREFIX}")
        else()
            message(FATAL_ERROR
                "VBAM_ANDROID_SDL3_PREFIX is set to ${VBAM_ANDROID_SDL3_PREFIX}, which "
                "has neither lib/libSDL3.so nor lib/libSDL3.a")
        endif()
    endif()

    if(NOT _vbam_sdl3_lib AND NOT TARGET SDL3::SDL3)
        find_package(SDL3 QUIET)
    endif()

    if(NOT _vbam_sdl3_lib AND NOT TARGET SDL3::SDL3)
        if(DEFINED CMAKE_LIBRARY_ARCHITECTURE AND CMAKE_LIBRARY_ARCHITECTURE)
            set(_vbam_android_triple "${CMAKE_LIBRARY_ARCHITECTURE}")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "riscv64")
            set(_vbam_android_triple "riscv64-linux-android")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
            set(_vbam_android_triple "aarch64-linux-android")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
            set(_vbam_android_triple "arm-linux-androideabi")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
            set(_vbam_android_triple "x86_64-linux-android")
        elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
            set(_vbam_android_triple "i686-linux-android")
        else()
            set(_vbam_android_triple "")
        endif()

        if(_vbam_android_triple AND
                EXISTS "${CMAKE_SYSROOT}/usr/lib/${_vbam_android_triple}/libSDL3.so")
            set(_vbam_sdl3_lib "${CMAKE_SYSROOT}/usr/lib/${_vbam_android_triple}/libSDL3.so")
            set(_vbam_sdl3_inc "${CMAKE_SYSROOT}/usr/include")
            message(STATUS "Using the SDL3 vendored in the NDK sysroot: ${_vbam_sdl3_lib}")
        endif()
    endif()

    if(_vbam_sdl3_lib)
        # Import as STATIC or SHARED according to what was actually resolved; a
        # static archive imported as SHARED makes CMake treat it as a runtime
        # dependency (and confuses install/deploy logic).
        if(_vbam_sdl3_lib MATCHES "\\.a$")
            add_library(SDL3::SDL3 STATIC IMPORTED)
        else()
            add_library(SDL3::SDL3 SHARED IMPORTED)
        endif()
        set_target_properties(SDL3::SDL3 PROPERTIES
            IMPORTED_LOCATION "${_vbam_sdl3_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${_vbam_sdl3_inc}")
    endif()

    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR
            "SDL3 not found for Android ABI ${CMAKE_ANDROID_ARCH_ABI}. Install the "
            "vcpkg sdl3 port for this triplet, or set VBAM_ANDROID_SDL3_PREFIX to an "
            "SDL3 install providing lib/libSDL3.so (or .a) and include/SDL3.")
    endif()

    # Stash the SDL3 .so the APK has to carry, so the wx target can hand it to
    # androiddeployqt (QT_ANDROID_EXTRA_LIBS) without a $<TARGET_FILE> genex,
    # whose "::" target name breaks Qt's deployment-settings parser.
    # androiddeployqt only accepts "lib*.so" there and hard-errors on anything
    # else, so a statically linked SDL3 must not be listed: it is already inside
    # the app's own module .so and needs no bundling.
    unset(_vbam_sdl3_bundle)

    if(TARGET SDL3::SDL3-shared)
        get_target_property(_vbam_sdl3_bundle SDL3::SDL3-shared IMPORTED_LOCATION)

        if(NOT _vbam_sdl3_bundle)
            get_target_property(_vbam_sdl3_bundle SDL3::SDL3-shared IMPORTED_LOCATION_RELEASE)
        endif()
    elseif(_vbam_sdl3_lib AND NOT _vbam_sdl3_lib MATCHES "\\.a$")
        set(_vbam_sdl3_bundle "${_vbam_sdl3_lib}")
    endif()

    if(NOT _vbam_sdl3_bundle)
        set(_vbam_sdl3_bundle "")
    endif()

    set(VBAM_SDL3_ANDROID_LIB "${_vbam_sdl3_bundle}" CACHE INTERNAL
        "SDL3 .so to bundle in the APK")

    set(SDL3_FOUND TRUE)
else()
    find_package(SDL3 QUIET)
endif()

if(VBAM_NEED_SDL)
    option(ENABLE_SDL3 "Use SDL3" "${SDL3_FOUND}")

    if(NOT TRANSLATIONS_ONLY)
        if(NOT ENABLE_SDL3)
            find_package(SDL2 QUIET)
        endif()

        if(NOT SDL3_FOUND AND NOT SDL2_FOUND)
            message(FATAL_ERROR "SDL2 or SDL3 is required, preferred SDL3")
        endif()
    endif()
endif()

# Lua scripting is a wx port feature.
if(ENABLE_WX)
    set(lua_default OFF)

    find_package(Lua)

    if(Lua_FOUND)
        # A located Lua is not necessarily a usable one. On macOS a package
        # manager install for the host architecture is happily found for a
        # cross build, and the mismatch then only shows up as undefined
        # symbols at link time, at the very end of the build. Link a probe
        # against it instead of trusting the find.
        include(CMakePushCheckState)
        include(CheckCSourceCompiles)

        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_INCLUDES  ${LUA_INCLUDE_DIR})
        set(CMAKE_REQUIRED_LIBRARIES ${LUA_LIBRARIES})
        check_c_source_compiles("
            #include <lua.h>
            #include <lauxlib.h>
            int main(void) { lua_close(luaL_newstate()); return 0; }
        " LUA_LINKS)
        cmake_pop_check_state()

        if(LUA_LINKS)
            set(lua_default ON)
        else()
            message(WARNING "Lua ${LUA_VERSION_STRING} was found at ${LUA_LIBRARY} but a test program does not link against it, it is probably for a different architecture")
        endif()
    endif()

    option(ENABLE_LUA "Enable Lua scripting (wx frontend)" ${lua_default})

    if(ENABLE_LUA AND NOT lua_default)
        message(FATAL_ERROR "ENABLE_LUA is set, but no usable Lua was found")
    endif()
else()
    vbam_disable_option_without_wx(ENABLE_LUA)
endif()

option(ENABLE_GENERIC_FILE_DIALOGS "Use generic file dialogs" OFF)
option(DISABLE_OPENGL "Disable OpenGL" OFF)
# The debugger's remote (GDB stub) transport is built on wxSocket, and the wx
# build for Android has wxUSE_SOCKETS=0 -- Android has no wxSocket backend, so
# the vcpkg wxwidgets port configures it out. tools/android/build-android.sh
# passes -DENABLE_DEBUGGER=OFF for the same reason; make that the default so a
# plain Android configure works, while leaving it overridable.
set(ENABLE_DEBUGGER_DEFAULT ON)
if(ANDROID)
    set(ENABLE_DEBUGGER_DEFAULT OFF)
endif()

option(ENABLE_DEBUGGER "Enable the debugger" ${ENABLE_DEBUGGER_DEFAULT})
option(ENABLE_ASAN "Enable -fsanitize=address by default. Requires debug build with GCC/Clang" OFF)
option(ENABLE_BZ2 "Enable BZ2 archive support" ON)
option(ENABLE_LZMA "Enable LZMA archive support" ON)

# Vulkan is a wx port renderer backend.
#
# Supports SDK installs (via VULKAN_SDK) and vcpkg (vulkan-headers + vulkan-loader).
# Both produce the Vulkan::Vulkan imported target used downstream.
if(ENABLE_WX AND NOT (X86 AND WIN32))
    find_package(Vulkan)

    option(ENABLE_VULKAN "Enable Vulkan" ${Vulkan_FOUND})

    # Catch stale cache or explicit -DENABLE_VULKAN=ON without the SDK present.
    if(ENABLE_VULKAN AND NOT Vulkan_FOUND)
        message(WARNING
            "ENABLE_VULKAN=ON but Vulkan was not found. "
            "For an SDK install, set the VULKAN_SDK environment variable. "
            "For vcpkg, ensure the vulkan-headers and vulkan-loader ports are "
            "installed for your triplet. Disabling Vulkan."
        )
        set(ENABLE_VULKAN OFF CACHE BOOL "Enable Vulkan" FORCE)
    endif()
else()
    set(ENABLE_VULKAN OFF)
endif()

option(ENABLE_MOLTENVK "Enable MoltenVK" OFF)

if(ENABLE_WX AND APPLE)
   # Prefer a linkable libMoltenVK.dylib over the .xcframework that Homebrew also
   # ships under Frameworks/: an .xcframework is a directory and cannot be linked
   # (CMake drops it). Searching frameworks last makes the dylib win.
   set(_vbam_save_find_framework ${CMAKE_FIND_FRAMEWORK})
   set(CMAKE_FIND_FRAMEWORK LAST)
   find_library(MOLTENVK NAMES MoltenVK)
   set(CMAKE_FIND_FRAMEWORK ${_vbam_save_find_framework})
   unset(_vbam_save_find_framework)

   if(MOLTENVK)
      # check_include_file() only searches the compiler's default include paths,
      # which miss Homebrew/MacPorts. find_path() honors CMAKE_INCLUDE_PATH (set
      # by MacPackageManagers), so it locates vulkan-headers installed via brew.
      find_path(VULKAN_INCLUDE_DIR vulkan/vulkan.h)
      if(VULKAN_INCLUDE_DIR)
         set(ENABLE_VULKAN ON)
         set(ENABLE_MOLTENVK ON)
      endif()
   else()
      unset(MOLTENVK)
   endif()
endif()

if(ENABLE_SDL3)
   set(CMAKE_C_FLAGS      "-DENABLE_SDL3 ${CMAKE_C_FLAGS}")
   set(CMAKE_CXX_FLAGS    "-DENABLE_SDL3 ${CMAKE_CXX_FLAGS}")
   set(CMAKE_OBJC_FLAGS   "-DENABLE_SDL3 ${CMAKE_OBJC_FLAGS}")
   set(CMAKE_OBJCXX_FLAGS "-DENABLE_SDL3 ${CMAKE_OBJCXX_FLAGS}")
#
#   include(CheckSourceCompiles)
#   check_source_compiles(CXX
#"#include <SDL3/SDL.h>
#
#int main() { return SDL_SCALEMODE_PIXELART; }
#"       HAVE_SDL_SCALEMODE_PIXELART)
#
#   if(HAVE_SDL_SCALEMODE_PIXELART)
#      set(CMAKE_C_FLAGS      "-DHAVE_SDL3_PIXELART ${CMAKE_C_FLAGS}")
#      set(CMAKE_CXX_FLAGS    "-DHAVE_SDL3_PIXELART ${CMAKE_CXX_FLAGS}")
#      set(CMAKE_OBJC_FLAGS   "-DHAVE_SDL3_PIXELART ${CMAKE_OBJC_FLAGS}")
#      set(CMAKE_OBJCXX_FLAGS "-DHAVE_SDL3_PIXELART ${CMAKE_OBJCXX_FLAGS}")
#   endif()
endif()

set(enable_asm_default OFF)
if(WIN32 AND X86_32 AND UPSTREAM_RELEASE)
   set(enable_asm_default ON)
endif()

option(ENABLE_ASM "Enable x86 ASM related options" ${enable_asm_default})

# The ARM ASM core seems to be very buggy, see #98 and #54. Default to it being
# OFF for the time being, until it is either fixed or replaced.
option(ENABLE_ASM_CORE "Enable x86 ASM CPU cores (EXPERIMENTAL)" OFF)

set(ASM_SCALERS_DEFAULT ${ENABLE_ASM})
set(MMX_DEFAULT ${ENABLE_ASM})

option(ENABLE_ASM_SCALERS "Enable x86 ASM graphic filters" ${ASM_SCALERS_DEFAULT})

include(CMakeDependentOption)
cmake_dependent_option(ENABLE_MMX "Enable MMX" ${MMX_DEFAULT} "ENABLE_ASM_SCALERS" OFF)

# LIRC remote control support is an SDL port feature.
if(ENABLE_SDL)
    option(ENABLE_LIRC "Enable LIRC support" OFF)
else()
    set(ENABLE_LIRC OFF)
endif()

# ENABLE_LINK (GBA cable link, built on the bundled SFML) is declared in
# FrontendOptions.cmake, since libintl is a dependency of the link code as
# well as of the wx port.

# ffmpeg A/V recording is a wx port feature; the SDL port and the libretro
# core have no recording UI.
set(FFMPEG_DEFAULT OFF)
set(FFMPEG_COMPONENTS         AVFORMAT            AVCODEC            SWSCALE          AVUTIL            SWRESAMPLE          X264    X265)
set(FFMPEG_COMPONENT_VERSIONS AVFORMAT>=58.12.100 AVCODEC>=58.18.100 SWSCALE>=5.1.100 AVUTIL>=56.14.100 SWRESAMPLE>=3.1.100 X264>=0 X265>=0)

if(ENABLE_WX AND NOT TRANSLATIONS_ONLY AND (NOT DEFINED ENABLE_FFMPEG OR ENABLE_FFMPEG))
    set(FFMPEG_DEFAULT ON)

    find_package(FFmpeg COMPONENTS ${FFMPEG_COMPONENTS})

    # check versions, but only if pkgconfig is available
    if(FFmpeg_FOUND AND PKG_CONFIG_FOUND)
        foreach(component ${FFMPEG_COMPONENT_VERSIONS})
            string(REPLACE ">=" ";" parts ${component})
            list(GET parts 0 name)
            list(GET parts 1 version)

            if((NOT DEFINED ${name}_VERSION) OR ${name}_VERSION VERSION_LESS ${version})
                set(FFmpeg_FOUND OFF)
            endif()
        endforeach()
    endif()

    if(NOT FFmpeg_FOUND)
        set(FFMPEG_DEFAULT OFF)
    endif()
endif()
if(ENABLE_WX)
    option(ENABLE_FFMPEG "Enable ffmpeg A/V recording" ${FFMPEG_DEFAULT})
else()
    vbam_disable_option_without_wx(ENABLE_FFMPEG)
endif()

# Online Updates (WinSparkle/Sparkle), a wx port feature.
set(ONLINEUPDATES_DEFAULT OFF)
if(DEFINED(UPSTREAM_RELEASE) AND UPSTREAM_RELEASE)
    set(ONLINEUPDATES_DEFAULT ON)
endif()
if(ENABLE_WX)
    option(ENABLE_ONLINEUPDATES "Enable online update checks" ${ONLINEUPDATES_DEFAULT})
else()
    set(ENABLE_ONLINEUPDATES OFF)
endif()
option(HTTPS "Use https URL for winsparkle" ON)

# We generally don't want LTO when debugging because it makes linking slow
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LTO_DEFAULT OFF)
else()
    set(LTO_DEFAULT ON)
endif()
option(ENABLE_LTO "Compile with Link Time Optimization" ${LTO_DEFAULT})

option(ENABLE_GBA_LOGGING "Enable extended GBA logging" ON)

option(UPSTREAM_RELEASE "do some optimizations and release automation tasks" OFF)

if(APPLE)
    set(bundle_dylibs_default OFF)

    if(UPSTREAM_RELEASE)
        set(bundle_dylibs_default ON)
    endif()

    option(BUNDLE_DYLIBS "Bundle dylibs into .app" ${bundle_dylibs_default})
endif()

# Direct3D renderers and the XAudio2 sound backend are wx port only.
if(ENABLE_WX AND WIN32)
    option(ENABLE_DIRECT3D "Enable Direct3D 9 rendering for the wxWidgets port" ON)

    if(NOT WINXP)
        option(ENABLE_DIRECT3D12 "Enable Direct3D 12 rendering for the wxWidgets port" ON)
        # D3D11 backs the software "Simple" renderer's HDR path on Windows.
        option(ENABLE_DIRECT3D11 "Enable Direct3D 11 rendering for the wxWidgets port" ON)
    else()
        set(ENABLE_DIRECT3D12 OFF)
        set(ENABLE_DIRECT3D11 OFF)
    endif()

    set(XAUDIO2_DEFAULT ON)
    if ((MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        # TODO: We should update the XAudio headers to build with clang-cl. See
        # https://github.com/visualboyadvance-m/visualboyadvance-m/issues/1021
        set(XAUDIO2_DEFAULT OFF)
    endif()
    option(ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" ${XAUDIO2_DEFAULT})
elseif(WIN32)
    set(ENABLE_DIRECT3D   OFF)
    set(ENABLE_DIRECT3D12 OFF)
    set(ENABLE_DIRECT3D11 OFF)
    set(ENABLE_XAUDIO2    OFF)
endif()

# OpenAL-Soft, a wx port sound backend.
if(ENABLE_WX)
    find_package(OpenAL QUIET)

    set(OPENAL_DEFAULT ${OpenAL_FOUND})

    if(APPLE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
       set(OPENAL_DEFAULT OFF)
    endif()

    if(MINGW AND X86)
        # OpenAL-Soft uses avrt.dll which is not available on Windows XP.
        set(OPENAL_DEFAULT OFF)
    endif()

    option(ENABLE_OPENAL "Enable OpenAL-Soft sound output for the wxWidgets port" ${OPENAL_DEFAULT})
else()
    vbam_disable_option_without_wx(ENABLE_OPENAL)
endif()

# FAudio, a wx port sound backend.
set(ENABLE_FAUDIO_DEFAULT OFF)

if(ENABLE_WX)
    find_package(FAudio QUIET)
endif()

if(FAudio_FOUND)
    # Check that FAudio links to the same SDL version we're using.
    # FAudio built with SDL2 won't work with SDL3 and vice versa.
    set(_faudio_sdl_mismatch OFF)

    # Check INTERFACE_LINK_LIBRARIES on FAudio targets for SDL version.
    foreach(_faudio_target FAudio::FAudio FAudio::FAudio-shared FAudio::FAudio-static)
        if(TARGET ${_faudio_target})
            get_target_property(_faudio_link_libs ${_faudio_target} INTERFACE_LINK_LIBRARIES)
            if(_faudio_link_libs)
                if(ENABLE_SDL3)
                    # We're using SDL3, FAudio must link to SDL3
                    if(_faudio_link_libs MATCHES "SDL2")
                        set(_faudio_sdl_mismatch ON)
                        message(STATUS "FAudio was built with SDL2, but we're using SDL3 - disabling FAudio")
                    endif()
                else()
                    # We're using SDL2, FAudio must link to SDL2
                    if(_faudio_link_libs MATCHES "SDL3")
                        set(_faudio_sdl_mismatch ON)
                        message(STATUS "FAudio was built with SDL3, but we're using SDL2 - disabling FAudio")
                    endif()
                endif()
                break()
            endif()
        endif()
    endforeach()

    # For static libraries without INTERFACE_LINK_LIBRARIES, check symbols.
    if(NOT WIN32 AND NOT _faudio_sdl_mismatch AND VBAM_STATIC)
        foreach(_faudio_target FAudio::FAudio-static FAudio::FAudio)
            if(TARGET ${_faudio_target})
                get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION)
                if(NOT _faudio_location)
                    get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION_RELEASE)
                endif()
                if(NOT _faudio_location)
                    get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION_RELWITHDEBINFO)
                endif()
                if(_faudio_location AND EXISTS "${_faudio_location}")
                    # SDL_GetNumAudioDevices exists in SDL2 but not SDL3
                    # SDL_GetAudioPlaybackDevices exists in SDL3 but not SDL2
                    execute_process(
                        COMMAND nm -g "${_faudio_location}"
                        OUTPUT_VARIABLE _faudio_symbols
                        ERROR_QUIET
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                    )
                    if(_faudio_symbols)
                        string(FIND "${_faudio_symbols}" "SDL_GetNumAudioDevices" _has_sdl2_symbol)
                        string(FIND "${_faudio_symbols}" "SDL_GetAudioPlaybackDevices" _has_sdl3_symbol)

                        if(ENABLE_SDL3 AND _has_sdl2_symbol GREATER -1 AND _has_sdl3_symbol EQUAL -1)
                            set(_faudio_sdl_mismatch ON)
                            message(STATUS "Static FAudio uses SDL2 symbols, but we're using SDL3 - disabling FAudio")
                        elseif(NOT ENABLE_SDL3 AND _has_sdl3_symbol GREATER -1 AND _has_sdl2_symbol EQUAL -1)
                            set(_faudio_sdl_mismatch ON)
                            message(STATUS "Static FAudio uses SDL3 symbols, but we're using SDL2 - disabling FAudio")
                        endif()
                    endif()
                    break()
                endif()
            endif()
        endforeach()
    endif()

    # For dynamic libraries, also check with ldd/otool if target properties didn't help.
    if(NOT WIN32 AND NOT _faudio_sdl_mismatch AND NOT VBAM_STATIC)
        foreach(_faudio_target FAudio::FAudio-shared FAudio::FAudio)
            if(TARGET ${_faudio_target})
                get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION)
                if(NOT _faudio_location)
                    get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION_RELEASE)
                endif()
                if(NOT _faudio_location)
                    get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION_RELWITHDEBINFO)
                endif()
                if(_faudio_location AND EXISTS "${_faudio_location}")
                    if(APPLE)
                        execute_process(
                            COMMAND otool -L "${_faudio_location}"
                            OUTPUT_VARIABLE _faudio_deps
                            ERROR_QUIET
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                        )
                    elseif(UNIX)
                        execute_process(
                            COMMAND ldd "${_faudio_location}"
                            OUTPUT_VARIABLE _faudio_deps
                            ERROR_QUIET
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                        )
                    endif()
                    if(_faudio_deps)
                        string(FIND "${_faudio_deps}" "libSDL2" _links_sdl2)
                        string(FIND "${_faudio_deps}" "libSDL3" _links_sdl3)

                        if(ENABLE_SDL3 AND _links_sdl2 GREATER -1)
                            set(_faudio_sdl_mismatch ON)
                            message(STATUS "FAudio links to libSDL2, but we're using SDL3 - disabling FAudio")
                        elseif(NOT ENABLE_SDL3 AND _links_sdl3 GREATER -1)
                            set(_faudio_sdl_mismatch ON)
                            message(STATUS "FAudio links to libSDL3, but we're using SDL2 - disabling FAudio")
                        endif()
                    endif()
                    break()
                endif()
            endif()
        endforeach()
    endif()

    if(NOT _faudio_sdl_mismatch)
        set(ENABLE_FAUDIO_DEFAULT ON)
    endif()

    unset(_faudio_sdl_mismatch)
    unset(_faudio_link_libs)
    unset(_faudio_target)
    unset(_faudio_location)
    unset(_faudio_symbols)
    unset(_faudio_deps)
    unset(_has_sdl2_symbol)
    unset(_has_sdl3_symbol)
    unset(_links_sdl2)
    unset(_links_sdl3)
endif()

if(ENABLE_WX)
    option(ENABLE_FAUDIO "Enable FAudio sound output for the wxWidgets port" ${ENABLE_FAUDIO_DEFAULT})
else()
    vbam_disable_option_without_wx(ENABLE_FAUDIO)
endif()

# Android-only backends. Both are native NDK/Qt paths with no counterpart on any
# other platform, so they are hard-off elsewhere rather than merely defaulted off
# -- an explicit -DENABLE_AAUDIO=ON on a desktop build would not compile.
if(ENABLE_WX AND ANDROID)
    # AAudio is the NDK's low-latency audio output (API 26+). Turning it off
    # leaves SDL as the Android sound backend.
    option(ENABLE_AAUDIO "Enable AAudio sound output for the wxWidgets port (Android only)" ON)

    # The in-tree GLES2 renderer (a QOpenGLWidget hosted in Qt's scene graph).
    # Turning it off leaves the software Simple renderer as the only output
    # module, since neither desktop OpenGL nor SDL video works on wxQt/Android.
    option(ENABLE_GLES "Enable the OpenGL ES 2 renderer for the wxWidgets port (Android only)" ON)
else()
    set(ENABLE_AAUDIO OFF)
    set(ENABLE_GLES OFF)
endif()

option(ZIP_SUFFIX [=[suffix for release zip files, e.g.  "-somebranch".zip]=] OFF)

# The SDL port can't be built without debugging support
if(NOT ENABLE_DEBUGGER AND ENABLE_SDL)
    message(FATAL_ERROR "The SDL port can't be built without debugging support")
endif()

if(TRANSLATIONS_ONLY AND (ENABLE_SDL OR ENABLE_WX))
    message(FATAL_ERROR "The SDL and wxWidgets ports can't be built when TRANSLATIONS_ONLY is enabled")
endif()

option(GPG_SIGNATURES "Create GPG signatures for release files" OFF)

if(ENABLE_WX AND APPLE)
   set(wx_mac_patched_default OFF)

   if(UPSTREAM_RELEASE)
      set(wx_mac_patched_default ON)
   endif()

   option(WX_MAC_PATCHED "A build of wxWidgets that is patched for the alert sound bug is being used" ${wx_mac_patched_default})
endif()

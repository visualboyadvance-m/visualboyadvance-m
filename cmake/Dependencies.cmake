include(VbamFunctions)

if(TRANSLATIONS_ONLY)
    return()
endif()

# Look for some dependencies using CMake scripts.
#
# Everything here is gated on the frontends that actually use it: the
# VBAM_NEED_* predicates come from FrontendOptions.cmake. Only zlib and the
# archive libraries used by the file extractors are unconditional, since
# vbam-core is built for every frontend selection.
find_package(ZLIB REQUIRED)

# OpenGL is a renderer backend in the wx and SDL ports.
if(VBAM_NEED_OPENGL)
    set(OpenGL_GL_PREFERENCE GLVND)

    if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
        set(OpenGL_GL_PREFERENCE LEGACY)
    endif()

    if(NOT DISABLE_OPENGL)
        find_package(OpenGL)

        if(NOT OpenGL_FOUND)
            set(CMAKE_C_FLAGS      "-DNO_OPENGL -DNO_OGL ${CMAKE_C_FLAGS}")
            set(CMAKE_CXX_FLAGS    "-DNO_OPENGL -DNO_OGL ${CMAKE_CXX_FLAGS}")
            set(CMAKE_OBJC_FLAGS   "-DNO_OPENGL -DNO_OGL ${CMAKE_OBJC_FLAGS}")
            set(CMAKE_OBJCXX_FLAGS "-DNO_OPENGL -DNO_OGL ${CMAKE_OBJCXX_FLAGS}")
        endif()
    endif()
endif()

# SDL, shared by the SDL port and the wx port's audio/game controller code.
unset(VBAM_SDL_LIBS)

if(VBAM_NEED_SDL)
    # Add libsamplerate to SDL2 with vcpkg
    unset(SDL_LIBRARY_TEMP)
    if((NOT ENABLE_SDL3) AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
        if(WIN32)
            unset(arch_suffix)
            unset(path_prefix)
            if(VCPKG_TARGET_TRIPLET MATCHES -static)
                set(arch_suffix  -static)
            endif()
            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(path_prefix debug)
            endif()

            set(samplerate_lib_name samplerate)
            set(installed_prefix    ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows${arch_suffix}/${path_prefix})

            if(MINGW)
                set(installed_prefix    ${_VCPKG_INSTALLED_DIR}/${WINARCH}-mingw${arch_suffix}/${path_prefix})
                set(samplerate_lib_name lib${samplerate_lib_name})
            endif()

            SET(SDL_LIBRARY_TEMP ${SDL_LIBRARY_TEMP} "${installed_prefix}/lib/${samplerate_lib_name}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        else()
            SET(SDL_LIBRARY_TEMP ${SDL_LIBRARY_TEMP} -lsamplerate)
        endif()
    endif()

    if(ENABLE_SDL3)
        if(VBAM_STATIC)
            set(VBAM_SDL_LIBS SDL3::SDL3-static ${SDL_LIBRARY_TEMP})
        else()
            set(VBAM_SDL_LIBS SDL3::SDL3        ${SDL_LIBRARY_TEMP})
        endif()
    else()
        if(VBAM_STATIC)
            set(VBAM_SDL_LIBS SDL2::SDL2-static ${SDL_LIBRARY_TEMP})
        else()
            set(VBAM_SDL_LIBS SDL2::SDL2        ${SDL_LIBRARY_TEMP})
        endif()
    endif()

    if(APPLE)
        list(APPEND VBAM_SDL_LIBS "-framework System")
    endif()
endif()

if(ENABLE_FFMPEG)
    if(NOT FFMPEG_LIBRARIES)
        message(FATAL_ERROR "ENABLE_FFMPEG was specified, but required versions of ffmpeg libraries cannot be found!")
    endif()

    if(ANDROID)
        # vcpkg's ffmpeg CMake wrapper reads the codec dependency lists out of
        # the .pc files with pkg_check_modules(... IMPORTED_TARGET ...), and
        # x265's "Libs: -L${libdir} -lx265 -lc++ -lm -pthread" sends
        # find_library() after a bare "c++". The NDK's per-ABI sysroot has no
        # libc++.so -- its C++ runtime is libc++_shared.so/libc++_static.a -- so
        # the search falls through to the host toolchain's own lib directory and
        # records <ndk>/toolchains/llvm/prebuilt/<host>/lib/libc++.so, an x86-64
        # library on an aarch64 link line:
        #
        #   ld.lld: error: .../prebuilt/linux-x86_64/lib/libc++.so is
        #   incompatible with aarch64linux
        #
        # No .pc file needs to name the C++ runtime here: the clang driver links
        # it according to ANDROID_STL (this build uses -static-libstdc++). Drop
        # every link item that resolved into the host toolchain, both from the
        # list and from the imported targets whose interfaces carry them.
        get_filename_component(vbam_ndk_host_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(vbam_ndk_host_dir "${vbam_ndk_host_dir}"   DIRECTORY)
        set(vbam_ndk_host_lib "${vbam_ndk_host_dir}/lib/")

        # Plain prefix matching, not a regex: these are filesystem paths and a
        # path is not a pattern.
        macro(vbam_strip_host_libs list_var context)
            set(vbam_kept_libs "")

            foreach(vbam_lib_item IN LISTS ${list_var})
                string(FIND "${vbam_lib_item}" "${vbam_ndk_host_lib}" vbam_host_pos)

                if(vbam_host_pos EQUAL 0)
                    message(STATUS
                        "ffmpeg: dropping host-toolchain link item from ${context}: ${vbam_lib_item}")
                else()
                    list(APPEND vbam_kept_libs "${vbam_lib_item}")
                endif()
            endforeach()

            set(${list_var} "${vbam_kept_libs}")
        endmacro()

        vbam_strip_host_libs(FFMPEG_LIBRARIES "FFMPEG_LIBRARIES")

        foreach(vbam_ffmpeg_lib IN LISTS FFMPEG_LIBRARIES)
            if(NOT TARGET "${vbam_ffmpeg_lib}")
                continue()
            endif()

            get_target_property(vbam_ffmpeg_iface "${vbam_ffmpeg_lib}" INTERFACE_LINK_LIBRARIES)

            if(NOT vbam_ffmpeg_iface)
                continue()
            endif()

            vbam_strip_host_libs(vbam_ffmpeg_iface "${vbam_ffmpeg_lib}")
            set_target_properties("${vbam_ffmpeg_lib}" PROPERTIES
                INTERFACE_LINK_LIBRARIES "${vbam_ffmpeg_iface}")
        endforeach()

        unset(vbam_ndk_host_dir)
        unset(vbam_ndk_host_lib)
        unset(vbam_kept_libs)
        unset(vbam_lib_item)
        unset(vbam_host_pos)
        unset(vbam_ffmpeg_lib)
        unset(vbam_ffmpeg_iface)
    endif()

    if(APPLE)
        list(APPEND FFMPEG_LDFLAGS "SHELL:-framework CoreText" "SHELL:-framework ApplicationServices")

        if(UPSTREAM_RELEASE)
            list(APPEND FFMPEG_LDFLAGS "SHELL:-framework VideoToolbox")

            if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
                list(APPEND FFMPEG_LDFLAGS -ltiff -llzma -lbz2 "SHELL:-framework DiskArbitration")
            endif()
        else()
            list(APPEND FFMPEG_LDFLAGS "SHELL:-framework VideoToolbox")
        endif()
    elseif(WIN32)
        set(WIN32_MEDIA_FOUNDATION_LIBS dxva2 evr mf mfplat mfplay mfreadwrite mfuuid amstrmid strmiids)
        list(APPEND FFMPEG_LIBRARIES secur32 bcrypt ncrypt ${WIN32_MEDIA_FOUNDATION_LIBS} dwrite msimg32 ntdll crypt32 ole32)

        if(MSYS2 AND VBAM_STATIC AND NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
            # Use --start-group/--end-group to handle circular dependencies
            # between the many transitive static libraries.
            list(APPEND FFMPEG_LIBRARIES "-Wl,--start-group")

            # gomp (OpenMP) is only available with GCC, not Clang.
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                set(_msys2_omp_lib gomp)
            else()
                set(_msys2_omp_lib omp)
            endif()

            foreach(lib tiff jbig lzma aom gsm jxl jxl_cms jxl_threads hwy lcms2 mp3lame lc3 opencore-amrnb opencore-amrwb opus openjp2 rav1e speex theora SvtAv1Enc vorbis vorbisenc vpx webp webpmux sharpyuv xvidcore va vpl dav1d zvbi rsvg-2 gdk_pixbuf-2.0 cairo cairo-gobject pixman-1 soxr ${_msys2_omp_lib} xml2 modplug gme gnutls bluray srt rtmp ssh shaderc shaderc_combined SPIRV-Tools-opt SPIRV-Tools glib-2.0 gmodule-2.0 gobject-2.0 gio-2.0 brotlicommon brotlienc brotlidec ogg png tasn1 nettle gmp pango-1.0 pangocairo-1.0 pangowin32-1.0 pangoft2-1.0 fontconfig fribidi harfbuzz graphite2 freetype thai datrie mincore zstd crypto hogweed glslang unistring ffi pcre2-8 va_win32 idn2 ntdll z)
                cygpath(lib "$ENV{MSYSTEM_PREFIX}/lib/lib${lib}.a")

                list(APPEND FFMPEG_LIBRARIES "${lib}")
            endforeach()

            # UCRT64's ffmpeg static archives were compiled with
            # __declspec(dllimport) for transitive deps like cairo/glib/hwy.
            # The __imp_* references can only be resolved by DLL import libs.
            # lld (CLANG64) doesn't need this and treats it as an error.
            if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                foreach(lib cairo glib-2.0 gobject-2.0 gio-2.0 hwy)
                    set(_dll_a "$ENV{MSYSTEM_PREFIX}/lib/lib${lib}.dll.a")
                    if(EXISTS "${_dll_a}")
                        cygpath(_dll_a "${_dll_a}")
                        list(APPEND FFMPEG_LIBRARIES "${_dll_a}")
                    endif()
                endforeach()
            endif()

            list(APPEND FFMPEG_LIBRARIES "-Wl,--end-group")

            add_link_options("-Wl,--allow-multiple-definition")

            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                add_link_options("-Wl,--error-limit=0")
            endif()

            # Create a wrapper script to suppress linker warnings but show output on error
            file(WRITE "${CMAKE_BINARY_DIR}/link_wrapper.ps1" [=[
$output = & $args[0] $args[1..($args.Length-1)] 2>&1 | Out-String
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    Write-Host $output
}
exit $exitCode
]=])
            set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "\"${POWERSHELL}\" -NoProfile -ExecutionPolicy Bypass -File \"${CMAKE_BINARY_DIR}/link_wrapper.ps1\"")
        endif()
    endif()
else()
    add_compile_definitions(NO_FFMPEG)
endif()

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

# gettext/libintl: the wx port's translations and the link code's messages.
if(VBAM_NEED_NLS)
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
    # Static libintl uses CFLocale/CFPreferences to detect the user's
    # preferred languages, so it needs CoreFoundation at link time.
    if(APPLE AND LIBINTL_LIB)
        list(APPEND CMAKE_REQUIRED_LIBRARIES "-framework CoreFoundation")
        list(APPEND NLS_LIBS                 "-framework CoreFoundation")
    endif()
    include(CheckFunctionExists)
    check_function_exists(gettext GETTEXT_FN)
    if(NOT (LIBINTL_INC OR GETTEXT_FN))
        if(ANDROID)
            # The NDK has no gettext/libintl. wxWidgets supplies its own _()
            # (wxGetTranslation), so the wx frontend still localizes; only the
            # core's gettext-based strings fall back to English.
            message(STATUS "gettext/libintl not available in the NDK; building without NLS")
        else()
            message(FATAL_ERROR "NLS requires libintl/gettext")
        endif()
    endif()
endif()

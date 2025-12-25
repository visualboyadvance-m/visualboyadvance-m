include(VbamFunctions)

if(TRANSLATIONS_ONLY)
    return()
endif()

# Look for some dependencies using CMake scripts
find_package(ZLIB REQUIRED)

set(OpenGL_GL_PREFERENCE GLVND)

if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    set(OpenGL_GL_PREFERENCE LEGACY)
endif()

if(NOT DISABLE_OPENGL)
    find_package(OpenGL)

    if(NOT OpenGL_FOUND)
        set(CMAKE_C_FLAGS "-DNO_OPENGL -DNO_OGL ${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "-DNO_OPENGL -DNO_OGL ${CMAKE_CXX_FLAGS}")
    endif()
endif()

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

if(ENABLE_FFMPEG)
    if(NOT FFMPEG_LIBRARIES)
        message(FATAL_ERROR "ENABLE_FFMPEG was specified, but required versions of ffmpeg libraries cannot be found!")
    endif()

    if(APPLE)
        list(APPEND FFMPEG_LDFLAGS "SHELL:-framework CoreText" "SHELL:-framework ApplicationServices")

        if(UPSTREAM_RELEASE)
            list(APPEND FFMPEG_LDFLAGS -lbz2 -ltiff "SHELL:-framework DiskArbitration" -lfreetype -lfontconfig -llzma -lxml2 -lharfbuzz -lcrypto -lssl)
        endif()
    elseif(WIN32)
        set(WIN32_MEDIA_FOUNDATION_LIBS dxva2 evr mf mfplat mfplay mfreadwrite mfuuid amstrmid strmiids)
        list(APPEND FFMPEG_LIBRARIES secur32 bcrypt ${WIN32_MEDIA_FOUNDATION_LIBS} dwrite msimg32 ntdll crypt32 ole32 idn2)

        if(MSYS2 AND VBAM_STATIC AND NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
            foreach(lib tiff jbig lzma aom gsm jxl jxl_cms jxl_threads hwy lcms2 mp3lame lc3 opencore-amrnb opencore-amrwb opus openjp2 rav1e speex theora SvtAv1Enc vorbis vorbisenc vpx webp webpmux sharpyuv xvidcore va vpl dav1d zvbi rsvg-2 gdk_pixbuf-2.0 cairo cairo-gobject pixman-1 soxr xml2 modplug gme gnutls bluray srt rtmp ssh shaderc shaderc_combined SPIRV-Tools-opt SPIRV-Tools glib-2.0 gmodule-2.0 gobject-2.0 gio-2.0 brotlicommon brotlienc brotlidec ogg png tasn1 nettle gmp pango-1.0 pangocairo-1.0 pangowin32-1.0 pangoft2-1.0 fontconfig fribidi harfbuzz graphite2 freetype thai datrie mincore zstd crypto hogweed glslang unistring ffi pcre2-8 va_win32 z)
                cygpath(lib "$ENV{MSYSTEM_PREFIX}/lib/lib${lib}.a")

                list(APPEND FFMPEG_LIBRARIES "${lib}")
            endforeach()

            add_link_options("-Wl,--allow-multiple-definition")
            add_link_options("-Wl,--error-limit=0")

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

if(TRANSLATIONS_ONLY)
    return()
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR)
    if(NOT CMAKE_TOOLCHAIN_FILE AND CMAKE_HOST_SYSTEM_PROCESSOR)
        set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
    elseif(CMAKE_TOOLCHAIN_FILE MATCHES mxe)
        if(CMAKE_TOOLCHAIN_FILE MATCHES "i[3-9]86")
            set(CMAKE_SYSTEM_PROCESSOR i686)
        else()
            set(CMAKE_SYSTEM_PROCESSOR x86_64)
        endif()
    elseif(CROSS_ARCH STREQUAL x86_64)
        set(CMAKE_SYSTEM_PROCESSOR x86_64)
    elseif(CROSS_ARCH STREQUAL i686)
        set(CMAKE_SYSTEM_PROCESSOR i686)
    endif()
endif()

# The processor may not be set, but set BITS regardless.
if(CMAKE_C_SIZEOF_DATA_PTR EQUAL 4)
    set(BITS 32)
elseif(CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
    set(BITS 64)
endif()

if(VCPKG_TARGET_TRIPLET MATCHES "^[aA][rR][mM]64")
    set(CMAKE_SYSTEM_PROCESSOR ARM64)
elseif(VCPKG_TARGET_TRIPLET MATCHES "^[aA][rR][mM]-")
    set(CMAKE_SYSTEM_PROCESSOR ARM)
endif()

if(APPLE AND
  (CMAKE_OSX_ARCHITECTURES MATCHES "[xX]86_64") OR
  (ENV{CFLAGS}             MATCHES "[xX]86_64") OR
  (ENV{CXXFLAGS}           MATCHES "[xX]86_64") OR
  (ENV{LDFLAGS}            MATCHES "[xX]86_64"))

  set(CMAKE_SYSTEM_PROCESSOR "x86_64")
endif()

# Turn asm on by default on 32bit x86 and set WINARCH for windows stuff.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "[xX]86|i[3-9]86|[aA][mM][dD]64")
    if(CMAKE_C_SIZEOF_DATA_PTR EQUAL 4) # 32 bit
        set(ASM_DEFAULT ON)
        set(X86_32 ON)
        set(X86 ON)
        set(WINARCH x86)
        set(ARCH_NAME x86_32)
    elseif(CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
        set(AMD64 ON)
        set(X64 ON)
        set(X86_64 ON)
        set(WINARCH x64)
        set(ARCH_NAME x86_64)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "[aA][aA][rR][cC][hH]|[aA][rR][mM]")
    if(CMAKE_C_SIZEOF_DATA_PTR EQUAL 4) # 32 bit
        set(ARM32 ON)
        set(ARCH_NAME ARM32)
        set(WINARCH arm)
    elseif(CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
        set(ARM64 ON)
        set(ARCH_NAME ARM64)
        set(WINARCH arm64)
    endif()

    if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "[aA][aA][rR][cC][hH]|[aA][rR][mM]")
        set(CMAKE_CROSSCOMPILING TRUE)
    endif()
endif()

if(DEFINED VCPKG_TARGET_TRIPLET)
    string(REGEX MATCH "^[^-]+" target_arch ${VCPKG_TARGET_TRIPLET})

    if(NOT WINARCH STREQUAL target_arch)
        message(FATAL_ERROR "Wrong build environment architecture for VCPKG_TARGET_TRIPLET, you specified ${target_arch} but your compiler is for ${WINARCH}")
    endif()
endif()

# We do not support amd64 asm yet
if(X86_64 AND (ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS OR ENABLE_MMX))
    message(FATAL_ERROR "The options ASM_CORE, ASM_SCALERS and MMX are not supported on X86_64 yet.")
endif()

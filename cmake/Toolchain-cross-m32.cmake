SET(CMAKE_C_FLAGS             "-m32" CACHE STRING "C compiler flags"   FORCE)
SET(CMAKE_CXX_FLAGS           "-m32" CACHE STRING "C++ compiler flags" FORCE)

SET(LIB32 /usr/lib) # Fedora

IF(EXISTS /usr/lib32)
    SET(LIB32 /usr/lib32) # Arch, Solus
ENDIF()

SET(CMAKE_SYSTEM_LIBRARY_PATH ${LIB32} CACHE STRING "system library search path" FORCE)
SET(CMAKE_LIBRARY_PATH        ${LIB32} CACHE STRING "library search path"        FORCE)

# this is probably unlikely to be needed, but just in case
SET(CMAKE_EXE_LINKER_FLAGS    "-m32 -L${LIB32}" CACHE STRING "executable linker flags"     FORCE)
SET(CMAKE_SHARED_LINKER_FLAGS "-m32 -L${LIB32}" CACHE STRING "shared library linker flags" FORCE)
SET(CMAKE_MODULE_LINKER_FLAGS "-m32 -L${LIB32}" CACHE STRING "module linker flags"         FORCE)

# find Wx config script on Fedora for the highest version of 32 bit Wx installed
IF(EXISTS ${LIB32}/wx/config)
    FILE(GLOB WX_INSTALLS ${LIB32}/wx/config/*)

    SET(MAX_WX_VERSION 0.0)
    FOREACH(WX_INSTALL ${WX_INSTALLS})
        STRING(REGEX MATCH "[0-9]+(\\.[0-9]+)+" WX_VERSION ${WX_INSTALL})

        IF(WX_VERSION VERSION_GREATER MAX_WX_VERSION)
            SET(MAX_WX_VERSION ${WX_VERSION})
        ENDIF()
    ENDFOREACH()

    FILE(GLOB WX_INSTALL_CONFIGS "${LIB32}/wx/config/*${MAX_WX_VERSION}*")
    LIST(GET WX_INSTALL_CONFIGS 0 WX_INSTALL_CONFIG)

    SET(WX_CONFIG_TRANSFORM_SCRIPT_LINES
        ""
    )
    FILE(WRITE ${CMAKE_BINARY_DIR}/wx-config-wrapper
"#!/bin/sh
${WX_INSTALL_CONFIG} \"\$@\" | sed 's!/emul32/!/usr/!g'
")
    EXECUTE_PROCESS(COMMAND chmod +x ${CMAKE_BINARY_DIR}/wx-config-wrapper)

    SET(wxWidgets_CONFIG_EXECUTABLE ${CMAKE_BINARY_DIR}/wx-config-wrapper)
ENDIF()

# on Fedora and Arch and similar, point pkgconfig at 32 bit .pc files. We have
# to include the regular system .pc files as well (at the end), because some
# are not always present in the 32 bit directory
IF(EXISTS ${LIB32}/pkgconfig)
    SET(ENV{PKG_CONFIG_LIBDIR} ${LIB32}/pkgconfig:/usr/share/pkgconfig:/usr/lib/pkgconfig:/usr/lib64/pkgconfig)
ENDIF()

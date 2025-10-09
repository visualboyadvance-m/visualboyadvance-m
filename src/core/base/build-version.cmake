# Create "core/base/version.h"

if(NOT DEFINED VBAM_GENERATED_VERSION_H)
    message(FATAL_ERROR "VBAM_GENERATED_VERSION_H not defined")
endif()
if(NOT DEFINED VBAM_VERSION)
    message(FATAL_ERROR "VBAM_VERSION not defined")
endif()
if(NOT DEFINED VBAM_REVISION)
    message(FATAL_ERROR "VBAM_REVISION not defined")
endif()
if(NOT DEFINED VBAM_WIN_VERSION)
    message(FATAL_ERROR "VBAM_WIN_VERSION not defined")
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/version_gen.h.in" "${VBAM_GENERATED_VERSION_H}" @ONLY)

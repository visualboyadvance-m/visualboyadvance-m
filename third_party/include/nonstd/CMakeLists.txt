# Defines the `NONSTD_INCLUDE_DIR` variable and `nonstd-lib` target.

# nonstd library dependency (header-only).
set(NONSTD_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR} PARENT_SCOPE)
add_library(nonstd-lib INTERFACE)
target_sources(nonstd-lib
    INTERFACE
    optional.hpp
    variant.hpp
)
# For C++, default to nonstd::optional and nonstd::variant for now due to mac
# build issues.
if(APPLE)
    target_compile_definitions(nonstd-lib
        INTERFACE
        optional_CONFIG_SELECT_OPTIONAL=optional_OPTIONAL_NONSTD
        variant_CONFIG_SELECT_VARIANT=variant_VARIANT_NONSTD
    )
else()
    target_compile_definitions(nonstd-lib
        INTERFACE
        optional_CONFIG_SELECT_OPTIONAL=optional_OPTIONAL_STD
        variant_CONFIG_SELECT_VARIANT=variant_VARIANT_STD
    )
endif()

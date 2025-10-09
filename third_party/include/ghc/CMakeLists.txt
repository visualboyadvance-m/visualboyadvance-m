add_library(ghc-filesystem INTERFACE)

target_include_directories(ghc-filesystem INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(ghc-filesystem INTERFACE
    filesystem.hpp
    fs_fwd.hpp
    fs_impl.hpp
    fs_std_fwd.hpp
    fs_std.hpp
    fs_std_impl.hpp
)

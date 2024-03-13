# this has to run after the toolchain is initialized.
include_directories("${CMAKE_SOURCE_DIR}/dependencies/mingw-include")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")

# Win32 deps submodule
set(git_checkout FALSE)
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    set(git_checkout TRUE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --remote --recursive
        RESULT_VARIABLE git_status
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
    if(NOT (git_checkout AND git_status EQUAL 0))
        message(FATAL_ERROR "Please pull in git submodules, e.g.\nrun: git submodule update --init --remote --recursive")
    endif()
endif()


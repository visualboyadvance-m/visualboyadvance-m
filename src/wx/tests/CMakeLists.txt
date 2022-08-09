# TODO: Does not link on CLANG64.
if(MSYS AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    return()
endif()

include(doctest)

function(add_doctest_test test_src)
    string(REGEX REPLACE ".cpp$" "" test_name "${test_src}")

    add_executable("${test_name}" "${ARGV}")

    target_link_libraries("${test_name}" ${wxWidgets_LIBRARIES})

    target_include_directories("${test_name}" PRIVATE ${wxWidgets_INCLUDE_DIRS})

    set_target_properties("${test_name}"
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    doctest_discover_tests("${test_name}")
endfunction()

add_doctest_test(strutils.cpp ../strutils.h ../strutils.cpp)

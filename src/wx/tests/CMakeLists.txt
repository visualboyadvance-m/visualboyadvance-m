include(Catch)

include_directories("${CMAKE_SOURCE_DIR}/third_party/include/catch2")

function(add_catch2_test test_src)
    string(REGEX REPLACE ".cpp$" "" test_name "${test_src}")

    add_executable("${test_name}" "${ARGV}")

    target_link_libraries("${test_name}" ${wxWidgets_LIBRARIES})

    target_include_directories("${test_name}" PRIVATE ${wxWidgets_INCLUDE_DIRS})

    set_target_properties("${test_name}"
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    catch_discover_tests("${test_name}")
endfunction()

add_catch2_test(strutils.cpp ../strutils.h ../strutils.cpp)

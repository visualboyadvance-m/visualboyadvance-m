# Interface for iterating over a text file by line.
#
# Example usage:
#
# fi_open_file(${some_file})
#
# while(NOT fi_done)
#     fi_get_next_line()
#
#     message(STATUS "read line: ${fi_line}")
# endwhile()

macro(fi_check_done)
    string(LENGTH "${fi_file_contents}" len)

    set(fi_done FALSE PARENT_SCOPE)

    if(len EQUAL 0)
        set(fi_done TRUE PARENT_SCOPE)
    endif()
endmacro()

function(fi_open_file file)
    file(READ "${file}" fi_file_contents)

    set(fi_file_contents "${fi_file_contents}" PARENT_SCOPE)

    fi_check_done()
endfunction()

function(fi_get_next_line)
    string(FIND "${fi_file_contents}" "\n" pos)

    string(SUBSTRING "${fi_file_contents}" 0 ${pos} fi_line)

    math(EXPR pos "${pos} + 1")

    string(SUBSTRING "${fi_file_contents}" ${pos} -1 fi_file_contents)

    fi_check_done()

    set(fi_line          "${fi_line}"          PARENT_SCOPE)
    set(fi_file_contents "${fi_file_contents}" PARENT_SCOPE)
endfunction()

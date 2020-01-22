# Interface for iterating over a text file by line.

function(fi_open_file file)
    file(READ "${file}" fi_file_contents)
    set(fi_file_contents "${fi_file_contents}" PARENT_SCOPE)
endfunction()

function(fi_get_next_line)
    string(FIND "${fi_file_contents}" "\n" pos)

    string(SUBSTRING "${fi_file_contents}" 0 ${pos} fi_line)

    math(EXPR pos "${pos} + 1")

    string(SUBSTRING "${fi_file_contents}" ${pos} -1 fi_file_contents)

    set(fi_line          "${fi_line}"          PARENT_SCOPE)
    set(fi_file_contents "${fi_file_contents}" PARENT_SCOPE)
endfunction()

function(fi_check_done)
    string(LENGTH "${fi_file_contents}" len)

    set(fi_done FALSE PARENT_SCOPE)

    if(len EQUAL 0)
        set(fi_done TRUE PARENT_SCOPE)
    endif()
endfunction()

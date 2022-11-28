# Remove StrawberryPerl from the PATH environment variable due to various build
# pollution.
unset(new_path)

foreach(p $ENV{PATH})
    if(NOT p MATCHES "(^|\\\\)[Ss]trawberry\\\\([Pp]erl|[Cc])\\\\(.*\\\\)?[Bb]in$")
        list(APPEND new_path ${p})
    endif()
endforeach()

set(ENV{PATH} "${new_path}")

# vim:sw=4 et sts=4 ts=8:

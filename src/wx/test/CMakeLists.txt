# This defines the `vbam-wx-fake-opts` library, which is used for providing
# a fake implementation of the gopts object, for testing purposes.

if(NOT BUILD_TESTING)
    return()
endif()

add_library(vbam-wx-fake-opts OBJECT)

target_sources(vbam-wx-fake-opts
    PRIVATE
    fake_opts.cpp
)

configure_wx_target(vbam-wx-fake-opts)

add_library(vbam-components-user-config OBJECT)

target_sources(vbam-components-user-config
    PRIVATE user_config.cpp
    PUBLIC user_config.h
)

# This defines the `vbam-core-apu` target.
# `vbam-core-apu` contains the core APU emulation code.

add_library(vbam-core-apu OBJECT)

target_sources(vbam-core-apu
    PRIVATE
    Blip_Buffer.cpp
    Effects_Buffer.cpp
    Gb_Apu.cpp
    Gb_Apu_State.cpp
    Gb_Oscs.cpp
    Multi_Buffer.cpp

    PUBLIC
    blargg_common.h
    blargg_config.h
    blargg_source.h
    Blip_Buffer.h
    Effects_Buffer.h
    Gb_Apu.h
    Gb_Oscs.h
    Multi_Buffer.h
)

configure_vbam_target(vbam-core-apu)

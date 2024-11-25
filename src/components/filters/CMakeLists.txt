add_library(vbam-components-filters OBJECT)

target_sources(vbam-components-filters
    PRIVATE
    internal/2xSaI.cpp
    internal/admame.cpp
    internal/bilinear.cpp
    internal/hq2x.cpp
    internal/hq2x.h
    internal/interp.h
    internal/lq2x.h
    internal/pixel.cpp
    internal/scanline.cpp
    internal/simpleFilter.cpp
    internal/xBRZ/xbrz.cpp
    internal/xBRZ/xbrz.h
    internal/xBRZ/xbrz_config.h
    internal/xBRZ/xbrz_tools.h
    internal/xbrzfilter.cpp

    PUBLIC
    filters.h
)

if(ENABLE_ASM_SCALERS)
    target_sources(vbam-components-filters
        PRIVATE
        internal/hq/asm/hq3x_16.asm
        internal/hq/asm/hq3x_32.asm
        internal/hq/asm/hq4x_16.asm
        internal/hq/asm/hq4x_32.asm
        internal/hq/asm/hq3x32.cpp
        internal/hq/asm/macros.mac
    )
    if(ENABLE_MMX)
        set(SRC_FILTERS ${SRC_FILTERS} internal/2xSaImmx.asm)
    endif()
else()
    target_sources(vbam-components-filters
        PRIVATE
        internal/hq/c/hq3x_pattern.h
        internal/hq/c/hq4x_pattern.h
        internal/hq/c/hq_base.h
        internal/hq/c/hq_implementation.cpp
        internal/hq/c/hq_shared.h
    )
endif()

configure_vbam_target(vbam-components-filters)

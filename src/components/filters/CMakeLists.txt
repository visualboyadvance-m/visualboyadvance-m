add_library(vbam-components-filters OBJECT)

target_sources(vbam-components-filters
    PRIVATE
    internal/2xSaI.cpp
    internal/admame.cpp
    internal/bilinear.cpp
    internal/hq2x.cpp
    internal/hq2x.h
    internal/hq_masks.h
    internal/interp.h
    internal/lq2x.h
    internal/pixel.cpp
    internal/scanline.cpp
    internal/simpleFilter.cpp
    internal/xBRZ/xbrz.cpp
    internal/xBRZ/xbrz.h
    internal/xBRZ/xbrz_config.h
    internal/xBRZ/xbrz_simd.h
    internal/xBRZ/xbrz_tools.h
    internal/xbrzfilter.cpp

    PUBLIC
    filters.h
    simd128.h
)

# xBRZ is third-party code; suppress compiler warnings.
#
# The scaler's SSE2, NEON and scalar backends (internal/xBRZ/xbrz_simd.h)
# are meant to produce identical pixels, and the unit test under test/ holds
# the SIMD build to the scalar one bit for bit. That only works when the
# compiler evaluates each float operation as written: the release flags'
# -ffast-math would let it reassociate the distance sums and fuse multiplies
# and adds differently in the vector and scalar code, so this file is built
# with strict floating point. The hot loops are hand-vectorized and lose
# nothing to it.
set(XBRZ_COMPILE_OPTIONS "")
if(MSVC)
    set(XBRZ_COMPILE_OPTIONS /wd5051 /fp:precise)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(XBRZ_COMPILE_OPTIONS -Wno-unused-function -fno-fast-math -ffp-contract=off)
endif()
set_source_files_properties(internal/xBRZ/xbrz.cpp PROPERTIES COMPILE_OPTIONS "${XBRZ_COMPILE_OPTIONS}")

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
        target_sources(vbam-components-filters PRIVATE internal/2xSaImmx.asm)
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

if(BUILD_TESTING AND NOT COMPILE_ONLY)
    add_subdirectory(test)
endif()

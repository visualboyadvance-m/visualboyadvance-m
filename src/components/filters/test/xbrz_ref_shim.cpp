// Plain-int entry point into the scalar reference build of the xBRZ scaler.
//
// This file is compiled with -Dxbrz=xbrz_scalar_ref, so the header below
// declares namespace xbrz_scalar_ref and the call goes to the copy of
// xbrz.cpp built with XBRZ_FORCE_SCALAR. The test cannot name that
// namespace's types itself (it includes the real xbrz.h), hence ints here.

#include <cstddef>
#include <cstdint>

#include "../internal/xBRZ/xbrz.h"

void xbrzScalarRefScale(size_t factor, const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight,
                        int colFmt, int srcPitch, int trgPitch, int yFirst, int yLast)
{
    xbrz::scale(factor, src, trg, srcWidth, srcHeight, static_cast<xbrz::ColorFormat>(colFmt),
                srcPitch, trgPitch, xbrz::ScalerCfg(), yFirst, yLast);
}

bool xbrzScalarRefEqualColorTest2(uint32_t col1, uint32_t col2, int colFmt, double equalColorTolerance)
{
    return xbrz::equalColorTest2(col1, col2, static_cast<xbrz::ColorFormat>(colFmt), equalColorTolerance, 0);
}

// ****************************************************************************
// * This file is part of the xBRZ project. It is distributed under           *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0         *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved          *
// *                                                                          *
// * xBRZ 9x scaler - extends xBRZ algorithm to 9x scaling factor             *
// ****************************************************************************

#ifndef XBRZ9X_H
#define XBRZ9X_H

#include <cstdint>

namespace xbrz9x
{
    // Scale image by 9x using xBRZ algorithm
    // This follows the same pattern as the standard xBRZ scalers (2x-6x)
    // but extended to 9x scaling factor.
    //
    // Parameters:
    //   src       - source image pixels (32-bit ARGB or RGB)
    //   trg       - target buffer (must be 9x width and 9x height of source)
    //   srcWidth  - width of source image in pixels
    //   srcHeight - height of source image in pixels
    //   srcPitch  - source row pitch in bytes
    //   trgPitch  - target row pitch in bytes
    //   useAlpha  - if true, process alpha channel; if false, treat as RGB
    void scale9x(const uint32_t* src, uint32_t* trg,
                 int srcWidth, int srcHeight,
                 int srcPitch, int trgPitch,
                 bool useAlpha = false);
}

#endif // XBRZ9X_H

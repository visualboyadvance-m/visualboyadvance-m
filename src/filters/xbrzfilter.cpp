#include "../System.h"

#include "xBRZ/xbrz.h"

void xbrz2x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
    xbrz::scale(2, (const uint32_t *)srcPtr, (uint32_t *)dstPtr, width, height, xbrz::ColorFormat::RGB, srcPitch, dstPitch);
}

void xbrz3x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
    xbrz::scale(3, (const uint32_t *)srcPtr, (uint32_t *)dstPtr, width, height, xbrz::ColorFormat::RGB, srcPitch, dstPitch);
}

void xbrz4x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
    xbrz::scale(4, (const uint32_t *)srcPtr, (uint32_t *)dstPtr, width, height, xbrz::ColorFormat::RGB, srcPitch, dstPitch);
}

void xbrz5x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
    xbrz::scale(5, (const uint32_t *)srcPtr, (uint32_t *)dstPtr, width, height, xbrz::ColorFormat::RGB, srcPitch, dstPitch);
}

void xbrz6x32(uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */, uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
    xbrz::scale(6, (const uint32_t *)srcPtr, (uint32_t *)dstPtr, width, height, xbrz::ColorFormat::RGB, srcPitch, dstPitch);
}

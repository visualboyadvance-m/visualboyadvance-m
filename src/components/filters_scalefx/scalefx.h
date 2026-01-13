// ScaleFX Filter - Public API
// CPU implementation of the ScaleFX shader algorithm
// Supports 3x and 9x scaling with optional reverse-AA

#ifndef SCALEFX_H
#define SCALEFX_H

#include <cstdint>

// Initialize ScaleFX buffers for given source dimensions
// Must be called before using any ScaleFX filter functions
void scalefx_init(int srcWidth, int srcHeight);

// Clean up ScaleFX buffers
void scalefx_cleanup();

// ScaleFX 3x filter (native scale)
// Produces 3x3 output pixels for each input pixel
void scalefx3x32(uint8_t* src, uint32_t srcPitch, uint8_t* delta,
                 uint8_t* dst, uint32_t dstPitch, int width, int height);

// ScaleFX 9x filter (3x applied twice)
// Produces 9x9 output pixels for each input pixel
void scalefx9x32(uint8_t* src, uint32_t srcPitch, uint8_t* delta,
                 uint8_t* dst, uint32_t dstPitch, int width, int height);

#endif // SCALEFX_H

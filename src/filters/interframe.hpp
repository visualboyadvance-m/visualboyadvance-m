/// Interframe blending filters

#ifndef INTERFRAME_HPP
#define INTERFRAME_HPP

extern int RGB_LOW_BITS_MASK;

void InterframeFilterInit();

// call ifc to ignore previous frame / when starting new
void InterframeCleanup();

// all 4 are MMX-accelerated if enabled
void SmartIB(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height);
void SmartIB32(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height);
void MotionBlurIB(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height);
void MotionBlurIB32(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height);

#ifdef MMX
static void SmartIB_MMX(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height);
static void SmartIB32_MMX(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height);
static void MotionBlurIB_MMX(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height);
static void MotionBlurIB32_MMX(uint8_t *srcPtr, uint32_t srcPitch, int width, int starty, int height);
#endif

//Options for if start is 0
void SmartIB(uint8_t *srcPtr, uint32_t srcPitch, int width, int height);
void SmartIB32(uint8_t *srcPtr, uint32_t srcPitch, int width, int height);
void MotionBlurIB(uint8_t *srcPtr, uint32_t srcPitch, int width, int height);
void MotionBlurIB32(uint8_t *srcPtr, uint32_t srcPitch, int width, int height);
#endif  //INTERFRAME_HPP

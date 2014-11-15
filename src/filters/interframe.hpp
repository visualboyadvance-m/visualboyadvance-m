/// Interframe blending filters

#ifndef INTERFRAME_HPP
#define INTERFRAME_HPP

extern int RGB_LOW_BITS_MASK;

static void Init();

// call ifc to ignore previous frame / when starting new
void InterframeCleanup();

// all 4 are MMX-accelerated if enabled
void SmartIB(u8 *srcPtr, u32 srcPitch, int width, int starty, int height);
void SmartIB32(u8 *srcPtr, u32 srcPitch, int width, int starty, int height);
void MotionBlurIB(u8 *srcPtr, u32 srcPitch, int width, int starty, int height);
void MotionBlurIB32(u8 *srcPtr, u32 srcPitch, int width, int starty, int height);

#ifdef MMX
static void SmartIB_MMX(u8 *srcPtr, u32 srcPitch, int width, int starty, int height);
static void SmartIB32_MMX(u8 *srcPtr, u32 srcPitch, int width, int starty, int height);
static void MotionBlurIB_MMX(u8 *srcPtr, u32 srcPitch, int width, int starty, int height);
static void MotionBlurIB32_MMX(u8 *srcPtr, u32 srcPitch, int width, int starty, int height);
#endif

//Options for if starty is 0
void SmartIB(u8 *srcPtr, u32 srcPitch, int width, int height);
void SmartIB32(u8 *srcPtr, u32 srcPitch, int width, int height);
void MotionBlurIB(u8 *srcPtr, u32 srcPitch, int width, int height);
void MotionBlurIB32(u8 *srcPtr, u32 srcPitch, int width, int height);
#endif  //INTERFRAME_HPP

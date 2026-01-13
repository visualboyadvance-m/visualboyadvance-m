#include "core/base/system.h"

extern int RGB_LOW_BITS_MASK;

extern "C"
{

#ifdef MMX
  void _2xSaILine (uint8_t *srcPtr, uint8_t *deltaPtr, uint32_t srcPitch,
                   uint32_t width, uint8_t *dstPtr, uint32_t dstPitch);
  void _2xSaISuperEagleLine (uint8_t *srcPtr, uint8_t *deltaPtr,
                             uint32_t srcPitch, uint32_t width,
                             uint8_t *dstPtr, uint32_t dstPitch);
  void _2xSaISuper2xSaILine (uint8_t *srcPtr, uint8_t *deltaPtr,
                             uint32_t srcPitch, uint32_t width,
                             uint8_t *dstPtr, uint32_t dstPitch);
    void Init_2xSaIMMX (uint32_t BitFormat);
  void BilinearMMX (uint16_t * A, uint16_t * B, uint16_t * C, uint16_t * D,
                    uint16_t * dx, uint16_t * dy, uint8_t *dP);
  void BilinearMMXGrid0 (uint16_t * A, uint16_t * B, uint16_t * C, uint16_t * D,
                         uint16_t * dx, uint16_t * dy, uint8_t *dP);
  void BilinearMMXGrid1 (uint16_t * A, uint16_t * B, uint16_t * C, uint16_t * D,
                         uint16_t * dx, uint16_t * dy, uint8_t *dP);
  void EndMMX ();

  bool cpu_mmx = 1;
#endif
}
static uint32_t redblueMask = 0xF81F;
static uint32_t greenMask = 0x7E0;

uint32_t qRGB_COLOR_MASK[2] = { 0xF7DEF7DE, 0xF7DEF7DE };

extern void hq2x_init(unsigned);

int Init_2xSaI(uint32_t BitFormat)
{
  if(systemColorDepth == 16) {
    if (BitFormat == 565) {
      redblueMask = 0xF81F;
      greenMask = 0x7E0;
      qRGB_COLOR_MASK[0] = qRGB_COLOR_MASK[1] = 0xF7DEF7DE;
      hq2x_init(16);
      RGB_LOW_BITS_MASK = 0x0821;
    } else if (BitFormat == 555) {
      redblueMask = 0x7C1F;
      greenMask = 0x3E0;
      qRGB_COLOR_MASK[0] = qRGB_COLOR_MASK[1] = 0x7BDE7BDE;
      hq2x_init(15);
      RGB_LOW_BITS_MASK = 0x0421;
    } else {
      return 0;
    }
  } else if(systemColorDepth == 32) {
    qRGB_COLOR_MASK[0] = qRGB_COLOR_MASK[1] = 0xfefefe;
    hq2x_init(32);
    RGB_LOW_BITS_MASK = 0x010101;
  } else
    return 0;

#ifdef MMX
    Init_2xSaIMMX (BitFormat);
#endif

  return 1;
}

static inline int GetResult1 (uint32_t A, uint32_t B, uint32_t C, uint32_t D,
                              uint32_t /* E */)
{
    int x = 0;
    int y = 0;
    int r = 0;

    if (A == C)
      x += 1;
    else if (B == C)
      y += 1;
    if (A == D)
      x += 1;
    else if (B == D)
      y += 1;
    if (x <= 1)
      r += 1;
    if (y <= 1)
      r -= 1;
    return r;
}

static inline int GetResult2 (uint32_t A, uint32_t B, uint32_t C, uint32_t D,
                              uint32_t /* E */)
{
  int x = 0;
  int y = 0;
  int r = 0;

  if (A == C)
    x += 1;
  else if (B == C)
    y += 1;
  if (A == D)
    x += 1;
  else if (B == D)
    y += 1;
  if (x <= 1)
    r -= 1;
  if (y <= 1)
    r += 1;
  return r;
}

static inline int GetResult (uint32_t A, uint32_t B, uint32_t C, uint32_t D)
{
  int x = 0;
  int y = 0;
  int r = 0;

  if (A == C)
    x += 1;
  else if (B == C)
    y += 1;
  if (A == D)
    x += 1;
  else if (B == D)
    y += 1;
  if (x <= 1)
    r += 1;
  if (y <= 1)
    r -= 1;
  return r;
}

static inline uint32_t INTERPOLATE(uint32_t A, uint32_t B) {
    if (A == B) {
        return A;
    }
    
    // Extract each 8-bit channel by reading the full byte at each color position
    uint32_t r_a = (A >> 16) & 0xFF;
    uint32_t g_a = (A >> 8) & 0xFF;
    uint32_t b_a = A & 0xFF;
    
    uint32_t r_b = (B >> 16) & 0xFF;
    uint32_t g_b = (B >> 8) & 0xFF;
    uint32_t b_b = B & 0xFF;
    
    // Average properly
    uint32_t r_avg = (r_a + r_b) >> 1;
    uint32_t g_avg = (g_a + g_b) >> 1;
    uint32_t b_avg = (b_a + b_b) >> 1;
    
    // Repack
    return (r_avg << 16) | (g_avg << 8) | b_avg;
}

static inline uint32_t Q_INTERPOLATE(uint32_t A, uint32_t B, uint32_t C, uint32_t D) {
    // Extract each 8-bit channel
    uint32_t r_a = (A >> 16) & 0xFF;
    uint32_t g_a = (A >> 8) & 0xFF;
    uint32_t b_a = A & 0xFF;
    
    uint32_t r_b = (B >> 16) & 0xFF;
    uint32_t g_b = (B >> 8) & 0xFF;
    uint32_t b_b = B & 0xFF;
    
    uint32_t r_c = (C >> 16) & 0xFF;
    uint32_t g_c = (C >> 8) & 0xFF;
    uint32_t b_c = C & 0xFF;
    
    uint32_t r_d = (D >> 16) & 0xFF;
    uint32_t g_d = (D >> 8) & 0xFF;
    uint32_t b_d = D & 0xFF;
    
    // Average across 4 pixels
    uint32_t r_avg = (r_a + r_b + r_c + r_d) >> 2;
    uint32_t g_avg = (g_a + g_b + g_c + g_d) >> 2;
    uint32_t b_avg = (b_a + b_b + b_c + b_d) >> 2;
    
    // Repack
    return (r_avg << 16) | (g_avg << 8) | b_avg;
}

[[maybe_unused]] static inline int GetResult1_32 (uint32_t A, uint32_t B, uint32_t C, uint32_t D,
                                 uint32_t /* E */)
{
    int x = 0;
    int y = 0;
    int r = 0;

    if (A == C)
      x += 1;
    else if (B == C)
      y += 1;
    if (A == D)
      x += 1;
    else if (B == D)
      y += 1;
    if (x <= 1)
      r += 1;
    if (y <= 1)
      r -= 1;
    return r;
}

[[maybe_unused]] static inline int GetResult2_32 (uint32_t A, uint32_t B, uint32_t C, uint32_t D,
                                 uint32_t /* E */)
{
  int x = 0;
  int y = 0;
  int r = 0;

  if (A == C)
    x += 1;
  else if (B == C)
    y += 1;
  if (A == D)
    x += 1;
  else if (B == D)
    y += 1;
  if (x <= 1)
    r -= 1;
  if (y <= 1)
    r += 1;
  return r;
}

#define BLUE_MASK565 0x001F001F
#define RED_MASK565 0xF800F800
#define GREEN_MASK565 0x07E007E0

#define BLUE_MASK555 0x001F001F
#define RED_MASK555 0x7C007C00
#define GREEN_MASK555 0x03E003E0

// Helper functions for safe pixel access with boundary clamping
static inline int ClampX(int x, int width) {
    return (x < 0) ? 0 : (x >= width) ? width - 1 : x;
}

static inline int ClampY(int y, int height) {
    return (y < 0) ? 0 : (y >= height) ? height - 1 : y;
}

// Safe 16-bit pixel access
static inline uint16_t GetPixel16(const uint16_t* src, int srcPitch16, int x, int y, int width, int height) {
    x = ClampX(x, width);
    y = ClampY(y, height);
    return src[y * srcPitch16 + x];
}

// Safe 32-bit pixel access
static inline uint32_t GetPixel32(const uint32_t* src, int srcPitch32, int x, int y, int width, int height) {
    x = ClampX(x, width);
    y = ClampY(y, height);
    return src[y * srcPitch32 + x];
}

void Super2xSaI (uint8_t *srcPtr, uint32_t srcPitch,
                 [[maybe_unused]] uint8_t *deltaPtr, uint8_t *dstPtr, uint32_t dstPitch,
                 int width, int height)
{
  uint8_t  *dP;
  uint32_t Nextline = srcPitch >> 1;
#ifdef MMX
  if (cpu_mmx) {
    for (int row = 0; row < height; row++) {
      _2xSaISuper2xSaILine (srcPtr + row * srcPitch, deltaPtr + row * srcPitch,
                            srcPitch, width, dstPtr + row * dstPitch * 2, dstPitch);
    }
  } else
#endif
    {

      const uint16_t* src = reinterpret_cast<const uint16_t*>(srcPtr);
      int srcPitch16 = static_cast<int>(Nextline);
      int originalHeight = height;

      for (int y = 0; y < originalHeight; y++) {
        dP = dstPtr + y * (dstPitch << 1);

        for (int x = 0; x < width; x++) {
          uint32_t color4, color5, color6;
          uint32_t color1, color2, color3;
          uint32_t colorA0, colorA1, colorA2, colorA3,
            colorB0, colorB1, colorB2, colorB3, colorS1, colorS2;
          uint32_t product1a, product1b, product2a, product2b;

          //---------------------------------------    B1 B2
          //                                         4  5  6 S2
          //                                         1  2  3 S1
          //                                           A1 A2

          colorB0 = GetPixel16(src, srcPitch16, x - 1, y - 1, width, originalHeight);
          colorB1 = GetPixel16(src, srcPitch16, x,     y - 1, width, originalHeight);
          colorB2 = GetPixel16(src, srcPitch16, x + 1, y - 1, width, originalHeight);
          colorB3 = GetPixel16(src, srcPitch16, x + 2, y - 1, width, originalHeight);

          color4 = GetPixel16(src, srcPitch16, x - 1, y, width, originalHeight);
          color5 = GetPixel16(src, srcPitch16, x,     y, width, originalHeight);
          color6 = GetPixel16(src, srcPitch16, x + 1, y, width, originalHeight);
          colorS2 = GetPixel16(src, srcPitch16, x + 2, y, width, originalHeight);

          color1 = GetPixel16(src, srcPitch16, x - 1, y + 1, width, originalHeight);
          color2 = GetPixel16(src, srcPitch16, x,     y + 1, width, originalHeight);
          color3 = GetPixel16(src, srcPitch16, x + 1, y + 1, width, originalHeight);
          colorS1 = GetPixel16(src, srcPitch16, x + 2, y + 1, width, originalHeight);

          colorA0 = GetPixel16(src, srcPitch16, x - 1, y + 2, width, originalHeight);
          colorA1 = GetPixel16(src, srcPitch16, x,     y + 2, width, originalHeight);
          colorA2 = GetPixel16(src, srcPitch16, x + 1, y + 2, width, originalHeight);
          colorA3 = GetPixel16(src, srcPitch16, x + 2, y + 2, width, originalHeight);

          //--------------------------------------
          if (color2 == color6 && color5 != color3) {
            product2b = product1b = color2;
          } else if (color5 == color3 && color2 != color6) {
            product2b = product1b = color5;
          } else if (color5 == color3 && color2 == color6) {
            int r = 0;

            r += GetResult (color6, color5, color1, colorA1);
            r += GetResult (color6, color5, color4, colorB1);
            r += GetResult (color6, color5, colorA2, colorS1);
            r += GetResult (color6, color5, colorB2, colorS2);

            if (r > 0)
              product2b = product1b = color6;
            else if (r < 0)
              product2b = product1b = color5;
            else {
              product2b = product1b = INTERPOLATE (color5, color6);
            }
          } else {
            if (color6 == color3 && color3 == colorA1
                && color2 != colorA2 && color3 != colorA0)
              product2b =
                Q_INTERPOLATE (color3, color3, color3, color2);
            else if (color5 == color2 && color2 == colorA2
                     && colorA1 != color3 && color2 != colorA3)
              product2b =
                Q_INTERPOLATE (color2, color2, color2, color3);
            else
              product2b = INTERPOLATE (color2, color3);

            if (color6 == color3 && color6 == colorB1
                && color5 != colorB2 && color6 != colorB0)
              product1b =
                Q_INTERPOLATE (color6, color6, color6, color5);
            else if (color5 == color2 && color5 == colorB2
                     && colorB1 != color6 && color5 != colorB3)
              product1b =
                Q_INTERPOLATE (color6, color5, color5, color5);
            else
              product1b = INTERPOLATE (color5, color6);
          }

          if (color5 == color3 && color2 != color6 && color4 == color5
              && color5 != colorA2)
            product2a = INTERPOLATE (color2, color5);
          else
            if (color5 == color1 && color6 == color5
                && color4 != color2 && color5 != colorA0)
              product2a = INTERPOLATE (color2, color5);
            else
              product2a = color2;

          if (color2 == color6 && color5 != color3 && color1 == color2
              && color2 != colorB2)
            product1a = INTERPOLATE (color2, color5);
          else
            if (color4 == color2 && color3 == color2
                && color1 != color5 && color2 != colorB0)
              product1a = INTERPOLATE (color2, color5);
            else
              product1a = color5;

#ifdef WORDS_BIGENDIAN
          product1a = (product1a << 16) | product1b;
          product2a = (product2a << 16) | product2b;
#else
          product1a = product1a | (product1b << 16);
          product2a = product2a | (product2b << 16);
#endif

          *((uint32_t *) dP) = product1a;
          *((uint32_t *) (dP + dstPitch)) = product2a;

          dP += sizeof (uint32_t);
        }                       // end of for ( x < width )
      }                 // endof: for (y < originalHeight)
    }
}

void Super2xSaI32 (uint8_t *srcPtr, uint32_t srcPitch,
                   uint8_t * /* deltaPtr */, uint8_t *dstPtr, uint32_t dstPitch,
                   int width, int height)
{
  uint32_t *dP;
  uint32_t Nextline = srcPitch >> 2;

  const uint32_t* src = reinterpret_cast<const uint32_t*>(srcPtr);
  int srcPitch32 = static_cast<int>(Nextline);

  for (int y = 0; y < height; y++) {
    dP = reinterpret_cast<uint32_t*>(dstPtr + y * (dstPitch << 1));

    for (int x = 0; x < width; x++) {
      uint32_t color4, color5, color6;
      uint32_t color1, color2, color3;
      uint32_t colorA0, colorA1, colorA2, colorA3,
        colorB0, colorB1, colorB2, colorB3, colorS1, colorS2;
      uint32_t product1a, product1b, product2a, product2b;

      //---------------------------------------    B1 B2
      //                                         4  5  6 S2
      //                                         1  2  3 S1
      //                                           A1 A2

      colorB0 = GetPixel32(src, srcPitch32, x - 1, y - 1, width, height);
      colorB1 = GetPixel32(src, srcPitch32, x,     y - 1, width, height);
      colorB2 = GetPixel32(src, srcPitch32, x + 1, y - 1, width, height);
      colorB3 = GetPixel32(src, srcPitch32, x + 2, y - 1, width, height);

      color4 = GetPixel32(src, srcPitch32, x - 1, y, width, height);
      color5 = GetPixel32(src, srcPitch32, x,     y, width, height);
      color6 = GetPixel32(src, srcPitch32, x + 1, y, width, height);
      colorS2 = GetPixel32(src, srcPitch32, x + 2, y, width, height);

      color1 = GetPixel32(src, srcPitch32, x - 1, y + 1, width, height);
      color2 = GetPixel32(src, srcPitch32, x,     y + 1, width, height);
      color3 = GetPixel32(src, srcPitch32, x + 1, y + 1, width, height);
      colorS1 = GetPixel32(src, srcPitch32, x + 2, y + 1, width, height);

      colorA0 = GetPixel32(src, srcPitch32, x - 1, y + 2, width, height);
      colorA1 = GetPixel32(src, srcPitch32, x,     y + 2, width, height);
      colorA2 = GetPixel32(src, srcPitch32, x + 1, y + 2, width, height);
      colorA3 = GetPixel32(src, srcPitch32, x + 2, y + 2, width, height);

      //--------------------------------------
      if (color2 == color6 && color5 != color3) {
        product2b = product1b = color2;
      } else if (color5 == color3 && color2 != color6) {
        product2b = product1b = color5;
      } else if (color5 == color3 && color2 == color6) {
        int r = 0;

        r += GetResult (color6, color5, color1, colorA1);
        r += GetResult (color6, color5, color4, colorB1);
        r += GetResult (color6, color5, colorA2, colorS1);
        r += GetResult (color6, color5, colorB2, colorS2);

        if (r > 0)
          product2b = product1b = color6;
        else if (r < 0)
          product2b = product1b = color5;
        else {
          product2b = product1b = INTERPOLATE (color5, color6);
        }
      } else {
        if (color6 == color3 && color3 == colorA1
            && color2 != colorA2 && color3 != colorA0)
          product2b =
            Q_INTERPOLATE (color3, color3, color3, color2);
        else if (color5 == color2 && color2 == colorA2
                 && colorA1 != color3 && color2 != colorA3)
          product2b =
            Q_INTERPOLATE (color2, color2, color2, color3);
        else
          product2b = INTERPOLATE (color2, color3);

        if (color6 == color3 && color6 == colorB1
            && color5 != colorB2 && color6 != colorB0)
          product1b =
            Q_INTERPOLATE (color6, color6, color6, color5);
        else if (color5 == color2 && color5 == colorB2
                 && colorB1 != color6 && color5 != colorB3)
          product1b =
            Q_INTERPOLATE (color6, color5, color5, color5);
        else
          product1b = INTERPOLATE (color5, color6);
      }

      if (color5 == color3 && color2 != color6 && color4 == color5
          && color5 != colorA2)
        product2a = INTERPOLATE (color2, color5);
      else
        if (color5 == color1 && color6 == color5
            && color4 != color2 && color5 != colorA0)
          product2a = INTERPOLATE (color2, color5);
        else
          product2a = color2;

      if (color2 == color6 && color5 != color3 && color1 == color2
          && color2 != colorB2)
        product1a = INTERPOLATE (color2, color5);
      else
        if (color4 == color2 && color3 == color2
            && color1 != color5 && color2 != colorB0)
          product1a = INTERPOLATE (color2, color5);
        else
          product1a = color5;
      *(dP) = product1a;
      *(dP+1) = product1b;
      *(dP + (dstPitch >> 2)) = product2a;
      *(dP + (dstPitch >> 2) + 1) = product2b;

      dP += 2;
    }                       // end of for ( x < width )
  }                 // endof: for (y < height)
}

void SuperEagle (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *deltaPtr,
                 uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t  *dP;
  uint16_t *xP;
  bool hasDelta = (deltaPtr != nullptr);

#ifdef MMX
  if (cpu_mmx && hasDelta) {
    for (int row = 0; row < height; row++) {
      _2xSaISuperEagleLine (srcPtr + row * srcPitch, deltaPtr + row * srcPitch,
                            srcPitch, width, dstPtr + row * dstPitch * 2, dstPitch);
    }
  } else
#endif
  {
    uint32_t Nextline = srcPitch >> 1;
    const uint16_t* src = reinterpret_cast<const uint16_t*>(srcPtr);
    int srcPitch16 = static_cast<int>(Nextline);

    for (int y = 0; y < height; y++) {
      xP = hasDelta ? reinterpret_cast<uint16_t*>(deltaPtr + y * srcPitch) : nullptr;
      dP = dstPtr + y * (dstPitch << 1);

      for (int x = 0; x < width; x++) {
        uint32_t color4, color5, color6;
        uint32_t color1, color2, color3;
        uint32_t colorA1, colorA2, colorB1, colorB2, colorS1, colorS2;
        uint32_t product1a, product1b, product2a, product2b;

        // Safe boundary-clamped pixel access
        colorB1 = GetPixel16(src, srcPitch16, x,     y - 1, width, height);
        colorB2 = GetPixel16(src, srcPitch16, x + 1, y - 1, width, height);

        color4 = GetPixel16(src, srcPitch16, x - 1, y, width, height);
        color5 = GetPixel16(src, srcPitch16, x,     y, width, height);
        color6 = GetPixel16(src, srcPitch16, x + 1, y, width, height);
        colorS2 = GetPixel16(src, srcPitch16, x + 2, y, width, height);

        color1 = GetPixel16(src, srcPitch16, x - 1, y + 1, width, height);
        color2 = GetPixel16(src, srcPitch16, x,     y + 1, width, height);
        color3 = GetPixel16(src, srcPitch16, x + 1, y + 1, width, height);
        colorS1 = GetPixel16(src, srcPitch16, x + 2, y + 1, width, height);

        colorA1 = GetPixel16(src, srcPitch16, x,     y + 2, width, height);
        colorA2 = GetPixel16(src, srcPitch16, x + 1, y + 2, width, height);

        // --------------------------------------
        if (color2 == color6 && color5 != color3) {
          product1b = product2a = color2;
          if ((color1 == color2) || (color6 == colorB2)) {
            product1a = INTERPOLATE (color2, color5);
            product1a = INTERPOLATE (color2, product1a);
            //                       product1a = color2;
          } else {
            product1a = INTERPOLATE (color5, color6);
          }

          if ((color6 == colorS2) || (color2 == colorA1)) {
            product2b = INTERPOLATE (color2, color3);
            product2b = INTERPOLATE (color2, product2b);
            //                       product2b = color2;
          } else {
            product2b = INTERPOLATE (color2, color3);
          }
        } else if (color5 == color3 && color2 != color6) {
          product2b = product1a = color5;

          if ((colorB1 == color5) || (color3 == colorS1)) {
            product1b = INTERPOLATE (color5, color6);
            product1b = INTERPOLATE (color5, product1b);
            //                       product1b = color5;
          } else {
            product1b = INTERPOLATE (color5, color6);
          }

          if ((color3 == colorA2) || (color4 == color5)) {
            product2a = INTERPOLATE (color5, color2);
            product2a = INTERPOLATE (color5, product2a);
            //                       product2a = color5;
          } else {
            product2a = INTERPOLATE (color2, color3);
          }

        } else if (color5 == color3 && color2 == color6) {
          int r = 0;

          r += GetResult (color6, color5, color1, colorA1);
          r += GetResult (color6, color5, color4, colorB1);
          r += GetResult (color6, color5, colorA2, colorS1);
          r += GetResult (color6, color5, colorB2, colorS2);

          if (r > 0) {
            product1b = product2a = color2;
            product1a = product2b = INTERPOLATE (color5, color6);
          } else if (r < 0) {
            product2b = product1a = color5;
            product1b = product2a = INTERPOLATE (color5, color6);
          } else {
            product2b = product1a = color5;
            product1b = product2a = color2;
          }
        } else {
          product2b = product1a = INTERPOLATE (color2, color6);
          product2b =
            Q_INTERPOLATE (color3, color3, color3, product2b);
          product1a =
            Q_INTERPOLATE (color5, color5, color5, product1a);

          product2a = product1b = INTERPOLATE (color5, color3);
          product2a =
            Q_INTERPOLATE (color2, color2, color2, product2a);
          product1b =
            Q_INTERPOLATE (color6, color6, color6, product1b);

          //                    product1a = color5;
          //                    product1b = color6;
          //                    product2a = color2;
          //                    product2b = color3;
        }
#ifdef WORDS_BIGENDIAN
        product1a = (product1a << 16) | product1b;
        product2a = (product2a << 16) | product2b;
#else
        product1a = product1a | (product1b << 16);
        product2a = product2a | (product2b << 16);
#endif

        *((uint32_t *) dP) = product1a;
        *((uint32_t *) (dP + dstPitch)) = product2a;
        if (xP) { *xP = (uint16_t)color5; xP++; }

        dP += sizeof (uint32_t);
      }                 // end of for ( x < width )
    }                   // endof: for (y < height)
  }
}

void SuperEagle32 (uint8_t *srcPtr, uint32_t srcPitch, uint8_t *deltaPtr,
                   uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint32_t  *dP;
  uint32_t *xP;
  bool hasDelta = (deltaPtr != nullptr);

  uint32_t Nextline = srcPitch >> 2;
  const uint32_t* src = reinterpret_cast<const uint32_t*>(srcPtr);
  int srcPitch32 = static_cast<int>(Nextline);

  for (int y = 0; y < height; y++) {
    xP = hasDelta ? reinterpret_cast<uint32_t*>(deltaPtr + y * srcPitch) : nullptr;
    dP = reinterpret_cast<uint32_t*>(dstPtr + y * (dstPitch << 1));

    for (int x = 0; x < width; x++) {
      uint32_t color4, color5, color6;
      uint32_t color1, color2, color3;
      uint32_t colorA1, colorA2, colorB1, colorB2, colorS1, colorS2;
      uint32_t product1a, product1b, product2a, product2b;

      // Safe boundary-clamped pixel access
      colorB1 = GetPixel32(src, srcPitch32, x,     y - 1, width, height);
      colorB2 = GetPixel32(src, srcPitch32, x + 1, y - 1, width, height);

      color4 = GetPixel32(src, srcPitch32, x - 1, y, width, height);
      color5 = GetPixel32(src, srcPitch32, x,     y, width, height);
      color6 = GetPixel32(src, srcPitch32, x + 1, y, width, height);
      colorS2 = GetPixel32(src, srcPitch32, x + 2, y, width, height);

      color1 = GetPixel32(src, srcPitch32, x - 1, y + 1, width, height);
      color2 = GetPixel32(src, srcPitch32, x,     y + 1, width, height);
      color3 = GetPixel32(src, srcPitch32, x + 1, y + 1, width, height);
      colorS1 = GetPixel32(src, srcPitch32, x + 2, y + 1, width, height);

      colorA1 = GetPixel32(src, srcPitch32, x,     y + 2, width, height);
      colorA2 = GetPixel32(src, srcPitch32, x + 1, y + 2, width, height);

      // --------------------------------------
      // 32-bit mode uses INTERPOLATE for LCD filter compatibility
      if (color2 == color6 && color5 != color3) {
        product1b = product2a = color2;
        if ((color1 == color2) || (color6 == colorB2)) {
          product1a = INTERPOLATE (color2, color5);
          product1a = INTERPOLATE (color2, product1a);
          //                       product1a = color2;
        } else {
          product1a = INTERPOLATE (color5, color6);
        }

        if ((color6 == colorS2) || (color2 == colorA1)) {
          product2b = INTERPOLATE (color2, color3);
          product2b = INTERPOLATE (color2, product2b);
          //                       product2b = color2;
        } else {
          product2b = INTERPOLATE (color2, color3);
        }
      } else if (color5 == color3 && color2 != color6) {
        product2b = product1a = color5;

        if ((colorB1 == color5) || (color3 == colorS1)) {
          product1b = INTERPOLATE (color5, color6);
          product1b = INTERPOLATE (color5, product1b);
          //                       product1b = color5;
        } else {
          product1b = INTERPOLATE (color5, color6);
        }

        if ((color3 == colorA2) || (color4 == color5)) {
          product2a = INTERPOLATE (color5, color2);
          product2a = INTERPOLATE (color5, product2a);
          //                       product2a = color5;
        } else {
          product2a = INTERPOLATE (color2, color3);
        }

      } else if (color5 == color3 && color2 == color6) {
        int r = 0;

        r += GetResult (color6, color5, color1, colorA1);
        r += GetResult (color6, color5, color4, colorB1);
        r += GetResult (color6, color5, colorA2, colorS1);
        r += GetResult (color6, color5, colorB2, colorS2);

        if (r > 0) {
          product1b = product2a = color2;
          product1a = product2b = INTERPOLATE (color5, color6);
        } else if (r < 0) {
          product2b = product1a = color5;
          product1b = product2a = INTERPOLATE (color5, color6);
        } else {
          product2b = product1a = color5;
          product1b = product2a = color2;
        }
      } else {
        product2b = product1a = INTERPOLATE (color2, color6);
        product2b =
          Q_INTERPOLATE (color3, color3, color3, product2b);
        product1a =
          Q_INTERPOLATE (color5, color5, color5, product1a);

        product2a = product1b = INTERPOLATE (color5, color3);
        product2a =
          Q_INTERPOLATE (color2, color2, color2, product2a);
        product1b =
          Q_INTERPOLATE (color6, color6, color6, product1b);

        //                    product1a = color5;
        //                    product1b = color6;
        //                    product2a = color2;
        //                    product2b = color3;
      }
      
      *(dP) = product1a;
      *(dP+1) = product1b;
      *(dP + (dstPitch >> 2)) = product2a;
      *(dP + (dstPitch >> 2) +1) = product2b;
      if (xP) { *xP = color5; xP++; }

      dP += 2;
    }                 // end of for ( x < width )
  }                   // endof: for (y < height)
}

void _2xSaI (uint8_t *srcPtr, uint32_t srcPitch, [[maybe_unused]] uint8_t *deltaPtr,
             uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint8_t  *dP;

#ifdef MMX
  if (cpu_mmx) {
    for (int row = 0; row < height; row++) {
      _2xSaILine (srcPtr + row * srcPitch, deltaPtr + row * srcPitch,
                  srcPitch, width, dstPtr + row * dstPitch * 2, dstPitch);
    }
  } else
#endif
  {
    uint32_t Nextline = srcPitch >> 1;
    const uint16_t* src = reinterpret_cast<const uint16_t*>(srcPtr);
    int srcPitch16 = static_cast<int>(Nextline);

    for (int y = 0; y < height; y++) {
      dP = dstPtr + y * (dstPitch << 1);

      for (int x = 0; x < width; x++) {

        uint32_t colorA, colorB;
        uint32_t colorC, colorD,
          colorE, colorF, colorG, colorH,
          colorI, colorJ, colorK, colorL,

          colorM, colorN, colorO, colorP;
        uint32_t product, product1, product2;

        //---------------------------------------
        // Map of the pixels:                    I|E F|J
        //                                       G|A B|K
        //                                       H|C D|L
        //                                       M|N O|P
        colorI = GetPixel16(src, srcPitch16, x - 1, y - 1, width, height);
        colorE = GetPixel16(src, srcPitch16, x,     y - 1, width, height);
        colorF = GetPixel16(src, srcPitch16, x + 1, y - 1, width, height);
        colorJ = GetPixel16(src, srcPitch16, x + 2, y - 1, width, height);

        colorG = GetPixel16(src, srcPitch16, x - 1, y, width, height);
        colorA = GetPixel16(src, srcPitch16, x,     y, width, height);
        colorB = GetPixel16(src, srcPitch16, x + 1, y, width, height);
        colorK = GetPixel16(src, srcPitch16, x + 2, y, width, height);

        colorH = GetPixel16(src, srcPitch16, x - 1, y + 1, width, height);
        colorC = GetPixel16(src, srcPitch16, x,     y + 1, width, height);
        colorD = GetPixel16(src, srcPitch16, x + 1, y + 1, width, height);
        colorL = GetPixel16(src, srcPitch16, x + 2, y + 1, width, height);

        colorM = GetPixel16(src, srcPitch16, x - 1, y + 2, width, height);
        colorN = GetPixel16(src, srcPitch16, x,     y + 2, width, height);
        colorO = GetPixel16(src, srcPitch16, x + 1, y + 2, width, height);
        colorP = GetPixel16(src, srcPitch16, x + 2, y + 2, width, height);

        if ((colorA == colorD) && (colorB != colorC)) {
          if (((colorA == colorE) && (colorB == colorL)) ||
              ((colorA == colorC) && (colorA == colorF)
               && (colorB != colorE) && (colorB == colorJ))) {
            product = colorA;
          } else {
            product = INTERPOLATE (colorA, colorB);
          }

          if (((colorA == colorG) && (colorC == colorO)) ||
              ((colorA == colorB) && (colorA == colorH)
               && (colorG != colorC) && (colorC == colorM))) {
            product1 = colorA;
          } else {
            product1 = INTERPOLATE (colorA, colorC);
          }
          product2 = colorA;
        } else if ((colorB == colorC) && (colorA != colorD)) {
          if (((colorB == colorF) && (colorA == colorH)) ||
              ((colorB == colorE) && (colorB == colorD)
               && (colorA != colorF) && (colorA == colorI))) {
            product = colorB;
          } else {
            product = INTERPOLATE (colorA, colorB);
          }

          if (((colorC == colorH) && (colorA == colorF)) ||
              ((colorC == colorG) && (colorC == colorD)
               && (colorA != colorH) && (colorA == colorI))) {
            product1 = colorC;
          } else {
            product1 = INTERPOLATE (colorA, colorC);
          }
          product2 = colorB;
        } else if ((colorA == colorD) && (colorB == colorC)) {
          if (colorA == colorB) {
            product = colorA;
            product1 = colorA;
            product2 = colorA;
          } else {
            int r = 0;

            product1 = INTERPOLATE (colorA, colorC);
            product = INTERPOLATE (colorA, colorB);

            r +=
              GetResult1 (colorA, colorB, colorG, colorE,
                          colorI);
            r +=
              GetResult2 (colorB, colorA, colorK, colorF,
                          colorJ);
            r +=
              GetResult2 (colorB, colorA, colorH, colorN,
                          colorM);
            r +=
              GetResult1 (colorA, colorB, colorL, colorO,
                          colorP);

            if (r > 0)
              product2 = colorA;
            else if (r < 0)
              product2 = colorB;
            else {
              product2 =
                Q_INTERPOLATE (colorA, colorB, colorC,
                               colorD);
            }
          }
        } else {
          product2 = Q_INTERPOLATE (colorA, colorB, colorC, colorD);

          if ((colorA == colorC) && (colorA == colorF)
              && (colorB != colorE) && (colorB == colorJ)) {
            product = colorA;
          } else if ((colorB == colorE) && (colorB == colorD)
                     && (colorA != colorF) && (colorA == colorI)) {
            product = colorB;
          } else {
            product = INTERPOLATE (colorA, colorB);
          }

          if ((colorA == colorB) && (colorA == colorH)
              && (colorG != colorC) && (colorC == colorM)) {
            product1 = colorA;
          } else if ((colorC == colorG) && (colorC == colorD)
                     && (colorA != colorH) && (colorA == colorI)) {
            product1 = colorC;
          } else {
            product1 = INTERPOLATE (colorA, colorC);
          }
        }

#ifdef WORDS_BIGENDIAN
        product = (colorA << 16) | product ;
        product1 = (product1 << 16) | product2 ;
#else
        product = colorA | (product << 16);
        product1 = product1 | (product2 << 16);
#endif
        *((int32_t *) dP) = product;
        *((uint32_t *) (dP + dstPitch)) = product1;

        dP += sizeof (uint32_t);
      }                 // end of for ( x < width )
    }                   // endof: for (y < height)
  }
}

void _2xSaI32 (uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
               uint8_t *dstPtr, uint32_t dstPitch, int width, int height)
{
  uint32_t  *dP;

  uint32_t Nextline = srcPitch >> 2;
  const uint32_t* src = reinterpret_cast<const uint32_t*>(srcPtr);
  int srcPitch32 = static_cast<int>(Nextline);

  for (int y = 0; y < height; y++) {
    dP = reinterpret_cast<uint32_t*>(dstPtr + y * (dstPitch << 1));

    for (int x = 0; x < width; x++) {
      uint32_t colorA, colorB;
      uint32_t colorC, colorD,
        colorE, colorF, colorG, colorH,
        colorI, colorJ, colorK, colorL,

        colorM, colorN, colorO, colorP;
      uint32_t product, product1, product2;

      //---------------------------------------
      // Map of the pixels:                    I|E F|J
      //                                       G|A B|K
      //                                       H|C D|L
      //                                       M|N O|P
      colorI = GetPixel32(src, srcPitch32, x - 1, y - 1, width, height);
      colorE = GetPixel32(src, srcPitch32, x,     y - 1, width, height);
      colorF = GetPixel32(src, srcPitch32, x + 1, y - 1, width, height);
      colorJ = GetPixel32(src, srcPitch32, x + 2, y - 1, width, height);

      colorG = GetPixel32(src, srcPitch32, x - 1, y, width, height);
      colorA = GetPixel32(src, srcPitch32, x,     y, width, height);
      colorB = GetPixel32(src, srcPitch32, x + 1, y, width, height);
      colorK = GetPixel32(src, srcPitch32, x + 2, y, width, height);

      colorH = GetPixel32(src, srcPitch32, x - 1, y + 1, width, height);
      colorC = GetPixel32(src, srcPitch32, x,     y + 1, width, height);
      colorD = GetPixel32(src, srcPitch32, x + 1, y + 1, width, height);
      colorL = GetPixel32(src, srcPitch32, x + 2, y + 1, width, height);

      colorM = GetPixel32(src, srcPitch32, x - 1, y + 2, width, height);
      colorN = GetPixel32(src, srcPitch32, x,     y + 2, width, height);
      colorO = GetPixel32(src, srcPitch32, x + 1, y + 2, width, height);
      colorP = GetPixel32(src, srcPitch32, x + 2, y + 2, width, height);

      if ((colorA == colorD) && (colorB != colorC)) {
        if (((colorA == colorE) && (colorB == colorL)) ||
            ((colorA == colorC) && (colorA == colorF)
             && (colorB != colorE) && (colorB == colorJ))) {
          product = colorA;
        } else {
          product = INTERPOLATE (colorA, colorB);
        }

        if (((colorA == colorG) && (colorC == colorO)) ||
            ((colorA == colorB) && (colorA == colorH)
             && (colorG != colorC) && (colorC == colorM))) {
          product1 = colorA;
        } else {
          product1 = INTERPOLATE (colorA, colorC);
        }
        product2 = colorA;
      } else if ((colorB == colorC) && (colorA != colorD)) {
        if (((colorB == colorF) && (colorA == colorH)) ||
            ((colorB == colorE) && (colorB == colorD)
             && (colorA != colorF) && (colorA == colorI))) {
          product = colorB;
        } else {
          product = INTERPOLATE (colorA, colorB);
        }

        if (((colorC == colorH) && (colorA == colorF)) ||
            ((colorC == colorG) && (colorC == colorD)
             && (colorA != colorH) && (colorA == colorI))) {
          product1 = colorC;
        } else {
          product1 = INTERPOLATE (colorA, colorC);
        }
        product2 = colorB;
      } else if ((colorA == colorD) && (colorB == colorC)) {
        if (colorA == colorB) {
          product = colorA;
          product1 = colorA;
          product2 = colorA;
        } else {
          int r = 0;

          product1 = INTERPOLATE (colorA, colorC);
          product = INTERPOLATE (colorA, colorB);

          r +=
            GetResult1 (colorA, colorB, colorG, colorE,
                        colorI);
          r +=
            GetResult2 (colorB, colorA, colorK, colorF,
                        colorJ);
          r +=
            GetResult2 (colorB, colorA, colorH, colorN,
                           colorM);
          r +=
            GetResult1 (colorA, colorB, colorL, colorO,
                           colorP);

          if (r > 0)
            product2 = colorA;
          else if (r < 0)
            product2 = colorB;
          else {
            product2 =
              Q_INTERPOLATE (colorA, colorB, colorC,
                               colorD);
            }
        }
      } else {
        product2 = Q_INTERPOLATE (colorA, colorB, colorC, colorD);

        if ((colorA == colorC) && (colorA == colorF)
            && (colorB != colorE) && (colorB == colorJ)) {
          product = colorA;
        } else if ((colorB == colorE) && (colorB == colorD)
                   && (colorA != colorF) && (colorA == colorI)) {
          product = colorB;
        } else {
          product = INTERPOLATE (colorA, colorB);
        }

        if ((colorA == colorB) && (colorA == colorH)
            && (colorG != colorC) && (colorC == colorM)) {
          product1 = colorA;
        } else if ((colorC == colorG) && (colorC == colorD)
                   && (colorA != colorH) && (colorA == colorI)) {
          product1 = colorC;
        } else {
          product1 = INTERPOLATE (colorA, colorC);
        }
      }
      *(dP) = colorA;
      *(dP + 1) = product;
      *(dP + (dstPitch >> 2)) = product1;
      *(dP + (dstPitch >> 2) + 1) = product2;

      dP += 2;
    }                 // end of for ( x < width )
  }                   // endof: for (y < height)
}

static uint32_t Bilinear (uint32_t A, uint32_t B, uint32_t x)
{
  unsigned long areaA, areaB;
  unsigned long result;

  if (A == B)
    return A;

  areaB = (x >> 11) & 0x1f;     // reduce 16 bit fraction to 5 bits
  areaA = 0x20 - areaB;

  A = (A & redblueMask) | ((A & greenMask) << 16);
  B = (B & redblueMask) | ((B & greenMask) << 16);

  result = ((areaA * A) + (areaB * B)) >> 5;

  return (result & redblueMask) | ((result >> 16) & greenMask);
}

static uint32_t Bilinear4 (uint32_t A, uint32_t B, uint32_t C, uint32_t D, uint32_t x,
                         uint32_t y)
{
  unsigned long areaA, areaB, areaC, areaD;
  unsigned long result, xy;

  x = (x >> 11) & 0x1f;
  y = (y >> 11) & 0x1f;
  xy = (x * y) >> 5;

  A = (A & redblueMask) | ((A & greenMask) << 16);
  B = (B & redblueMask) | ((B & greenMask) << 16);
  C = (C & redblueMask) | ((C & greenMask) << 16);
  D = (D & redblueMask) | ((D & greenMask) << 16);

  areaA = 0x20 + xy - x - y;
  areaB = x - xy;
  areaC = y - xy;
  areaD = xy;

  result = ((areaA * A) + (areaB * B) + (areaC * C) + (areaD * D)) >> 5;

  return (result & redblueMask) | ((result >> 16) & greenMask);
}

void Scale_2xSaI (uint8_t *srcPtr, uint32_t srcPitch, uint8_t * /* deltaPtr */,
                  uint8_t *dstPtr, uint32_t dstPitch,
                  uint32_t dstWidth, uint32_t dstHeight, int width, int height)
{
  uint8_t  *dP;

  uint32_t w;
  uint32_t h;
  uint32_t dw;
  uint32_t dh;
  uint32_t hfinish;
  uint32_t wfinish;

  uint32_t Nextline = srcPitch >> 1;
  const uint16_t* src = reinterpret_cast<const uint16_t*>(srcPtr);
  int srcPitch16 = static_cast<int>(Nextline);

  wfinish = (width - 1) << 16;  // convert to fixed point
  dw = wfinish / (dstWidth - 1);
  hfinish = (height - 1) << 16; // convert to fixed point
  dh = hfinish / (dstHeight - 1);

  for (h = 0; h < hfinish; h += dh) {
    uint32_t y1, y2;

    y1 = h & 0xffff;    // fraction part of fixed point
    int srcY = static_cast<int>(h >> 16);
    dP = dstPtr;
    y2 = 0x10000 - y1;

    w = 0;

    for (; w < wfinish;) {
      uint32_t A, B, C, D;
      uint32_t E, F, G, H;
      uint32_t I, J, K, L;
      uint32_t x1, x2, a1, f1, f2;
      uint32_t product1 = 0;

      int srcX = static_cast<int>(w >> 16);

      // Safe boundary-clamped pixel access
      A = GetPixel16(src, srcPitch16, srcX,     srcY,     width, height);
      B = GetPixel16(src, srcPitch16, srcX + 1, srcY,     width, height);
      C = GetPixel16(src, srcPitch16, srcX,     srcY + 1, width, height);
      D = GetPixel16(src, srcPitch16, srcX + 1, srcY + 1, width, height);
      E = GetPixel16(src, srcPitch16, srcX,     srcY - 1, width, height);
      F = GetPixel16(src, srcPitch16, srcX + 1, srcY - 1, width, height);
      G = GetPixel16(src, srcPitch16, srcX - 1, srcY,     width, height);
      H = GetPixel16(src, srcPitch16, srcX - 1, srcY + 1, width, height);
      I = GetPixel16(src, srcPitch16, srcX + 2, srcY,     width, height);
      J = GetPixel16(src, srcPitch16, srcX + 2, srcY + 1, width, height);
      K = GetPixel16(src, srcPitch16, srcX,     srcY + 2, width, height);
      L = GetPixel16(src, srcPitch16, srcX + 1, srcY + 2, width, height);

      x1 = w & 0xffff;  // fraction part of fixed point
      x2 = 0x10000 - x1;

      /*0*/
      if (A == B && C == D && A == C)
        product1 = A;
      else /*1*/ if (A == D && B != C) {
        f1 = (x1 >> 1) + (0x10000 >> 2);
        f2 = (y1 >> 1) + (0x10000 >> 2);
        if (y1 <= f1 && A == J && A != E)       // close to B
          {
            a1 = f1 - y1;
            product1 = Bilinear (A, B, a1);
          } else if (y1 >= f1 && A == G && A != L)      // close to C
            {
              a1 = y1 - f1;
              product1 = Bilinear (A, C, a1);
            }
        else if (x1 >= f2 && A == E && A != J)  // close to B
          {
            a1 = x1 - f2;
            product1 = Bilinear (A, B, a1);
          }
        else if (x1 <= f2 && A == L && A != G)  // close to C
          {
            a1 = f2 - x1;
            product1 = Bilinear (A, C, a1);
          }
        else if (y1 >= x1)      // close to C
          {
            a1 = y1 - x1;
            product1 = Bilinear (A, C, a1);
          }
        else if (y1 <= x1)      // close to B
          {
            a1 = x1 - y1;
            product1 = Bilinear (A, B, a1);
          }
      }
      else
        /*2*/
        if (B == C && A != D)
          {
            f1 = (x1 >> 1) + (0x10000 >> 2);
            f2 = (y1 >> 1) + (0x10000 >> 2);
            if (y2 >= f1 && B == H && B != F)   // close to A
              {
                a1 = y2 - f1;
                product1 = Bilinear (B, A, a1);
              }
            else if (y2 <= f1 && B == I && B != K)      // close to D
              {
                a1 = f1 - y2;
                product1 = Bilinear (B, D, a1);
              }
            else if (x2 >= f2 && B == F && B != H)      // close to A
              {
                a1 = x2 - f2;
                product1 = Bilinear (B, A, a1);
              }
            else if (x2 <= f2 && B == K && B != I)      // close to D
              {
                a1 = f2 - x2;
                product1 = Bilinear (B, D, a1);
              }
            else if (y2 >= x1)  // close to A
              {
                a1 = y2 - x1;
                product1 = Bilinear (B, A, a1);
              }
            else if (y2 <= x1)  // close to D
              {
                a1 = x1 - y2;
                product1 = Bilinear (B, D, a1);
              }
          }
      /*3*/
        else
          {
            product1 = Bilinear4 (A, B, C, D, x1, y1);
          }

      //end First Pixel
      *(uint32_t *) dP = product1;
      dP += 2;
      w += dw;
    }
    dstPtr += dstPitch;
  }
}

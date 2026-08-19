/*
 * GBA Color Correction Shader Implementation
 *
 * Shader modified by Pokefan531.
 * Color Mangler
 * Original Author: hunterk
 * Original License: Public domain
 *
 * This code is adapted from the original shader logic.
 */

#include "components/filters_agb/filters_agb.h"
#include <cmath>
#include <algorithm>

extern int systemColorDepth;
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

extern uint8_t  systemColorMap8[0x10000];
extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];

// --- Global Constants and Variables for GBA Color Correction ---
// One profile per (panel variant, color mode), taken from the matching shader.
// Matrices use the column-major order of GLSL mat4.
namespace {

struct LcdProfile {
    float m[4][4];       // Column 0-2 = R/G/B, column 3 = black lift + luminance
    float in_gamma[3];   // Per-channel input gamma
    float out_gamma;     // Output gamma; the pipeline raises to 1/out_gamma
    float darken_scale;  // Darken slider multiplier, 0 = shader has no darken
};

// Every AGB-family shader uses a flat 2.2 in and out.
#define AGB_GAMMA 2.2f, 2.2f, 2.2f
#define AGB_OUT   2.2f

const LcdProfile kProfiles[kGbaFilterVariantCount][3] = {
    // gba-color. darken_screen is applied 1:1.
    {
        {{{0.905f, 0.10f,   0.1575f, 0.0f},
          {0.195f, 0.65f,   0.1425f, 0.0f},
          {-0.10f, 0.25f,   0.70f,   0.0f},
          {0.0f,   0.0f,    0.0f,    0.91f}}, {AGB_GAMMA}, AGB_OUT, 1.0f},
        {{{0.76f,  0.125f,  0.16f,   0.0f},
          {0.27f,  0.6375f, 0.18f,   0.0f},
          {-0.03f, 0.2375f, 0.66f,   0.0f},
          {0.0f,   0.0f,    0.0f,    0.97f}}, {AGB_GAMMA}, AGB_OUT, 1.0f},
        {{{0.61f,  0.155f,  0.16f,   0.0f},
          {0.345f, 0.615f,  0.1875f, 0.0f},
          {0.045f, 0.23f,   0.6525f, 0.0f},
          {0.0f,   0.0f,    0.0f,    1.0f}},  {AGB_GAMMA}, AGB_OUT, 1.0f},
    },
    // sp101-color
    {
        {{{0.96f,   0.0325f, 0.001f,  0.0f},
          {0.11f,   0.89f,   -0.03f,  0.0f},
          {-0.07f,  0.0775f, 1.029f,  0.0f},
          {0.0f,    0.0f,    0.0f,    0.935f}}, {AGB_GAMMA}, AGB_OUT, 0.0f},
        {{{0.805f,  0.0675f, 0.017f,  0.0f},
          {0.24f,   0.86f,   0.02f,   0.0f},
          {-0.045f, 0.0725f, 0.963f,  0.0f},
          {0.0f,    0.0f,    0.0f,    0.955f}}, {AGB_GAMMA}, AGB_OUT, 0.0f},
        {{{0.625f,  0.10f,   0.015f,  0.0f},
          {0.35f,   0.82f,   0.0325f, 0.0f},
          {0.025f,  0.08f,   0.9525f, 0.0f},
          {0.0f,    0.0f,    0.0f,    1.0f}},  {AGB_GAMMA}, AGB_OUT, 0.0f},
    },
    // gbMicro-color
    {
        {{{0.8025f,  0.10f,   0.1225f, 0.0f},
          {0.31f,    0.6875f, 0.1125f, 0.0f},
          {-0.1125f, 0.2125f, 0.765f,  0.0f},
          {0.0f,     0.0f,    0.0f,    0.9f}},  {AGB_GAMMA}, AGB_OUT, 0.0f},
        {{{0.6675f,  0.125f,  0.13f,   0.0f},
          {0.3825f,  0.675f,  0.1475f, 0.0f},
          {-0.05f,   0.20f,   0.7225f, 0.0f},
          {0.0f,     0.0f,    0.0f,    0.96f}}, {AGB_GAMMA}, AGB_OUT, 0.0f},
        {{{0.525f,   0.15f,   0.13f,   0.0f},
          {0.43f,    0.65f,   0.155f,  0.0f},
          {0.045f,   0.20f,   0.715f,  0.0f},
          {0.0f,     0.0f,    0.0f,    1.0f}},  {AGB_GAMMA}, AGB_OUT, 0.0f},
    },
    // nds-color
    {
        {{{0.835f,  0.10f,    0.105f,  0.0f},
          {0.27f,   0.6375f,  0.175f,  0.0f},
          {-0.105f, 0.2625f,  0.72f,   0.0f},
          {0.0f,    0.0f,     0.0f,    0.905f}}, {AGB_GAMMA}, AGB_OUT, 0.0f},
        {{{0.70f,   0.125f,   0.12f,   0.0f},
          {0.34f,   0.625f,   0.20f,   0.0f},
          {-0.04f,  0.25f,    0.68f,   0.0f},
          {0.0f,    0.0f,     0.0f,    0.96f}},  {AGB_GAMMA}, AGB_OUT, 0.0f},
        {{{0.555f,  0.1475f,  0.1175f, 0.0f},
          {0.39f,   0.6075f,  0.2075f, 0.0f},
          {0.055f,  0.245f,   0.675f,  0.0f},
          {0.0f,    0.0f,     0.0f,    1.0f}},   {AGB_GAMMA}, AGB_OUT, 0.0f},
    },
    // dslite-color
    {
        {{{0.93f,   0.025f,  0.008f,  0.0f},
          {0.14f,   0.90f,   -0.03f,  0.0f},
          {-0.07f,  0.075f,  1.022f,  0.0f},
          {0.0f,    0.0f,    0.0f,    0.935f}}, {AGB_GAMMA}, AGB_OUT, 0.0f},
        {{{0.76f,   0.055f,  0.0225f, 0.0f},
          {0.27f,   0.875f,  0.0225f, 0.0f},
          {-0.03f,  0.07f,   0.955f,  0.0f},
          {0.0f,    0.0f,    0.0f,    0.97f}},  {AGB_GAMMA}, AGB_OUT, 0.0f},
        {{{0.585f,  0.09f,   0.0225f, 0.0f},
          {0.3725f, 0.825f,  0.035f,  0.0f},
          {0.0425f, 0.085f,  0.9425f, 0.0f},
          {0.0f,    0.0f,    0.0f,    1.0f}},   {AGB_GAMMA}, AGB_OUT, 0.0f},
    },
    // NSO-gba-color (darken applied 1:1; shader default is 0.8)
    {
        {{{0.865f,  0.0575f, 0.0575f, 0.0f},
          {0.1225f, 0.925f,  0.1225f, 0.0f},
          {0.0125f, 0.0125f, 0.82f,   0.0f},
          {0.0f,    0.0f,    0.0f,    1.0f}}, {AGB_GAMMA}, AGB_OUT, 1.0f},
        {{{0.72f,   0.0875f, 0.0725f, 0.0f},
          {0.2675f, 0.9f,    0.185f,  0.0f},
          {0.0125f, 0.0125f, 0.7425f, 0.0f},
          {0.0f,    0.0f,    0.0f,    1.0f}}, {AGB_GAMMA}, AGB_OUT, 1.0f},
        {{{0.57f,   0.115f,  0.0725f, 0.0f},
          {0.3825f, 0.8625f, 0.195f,  0.0f},
          {0.0475f, 0.0225f, 0.7325f, 0.0f},
          {0.0f,    0.0f,    0.0f,    1.0f}}, {AGB_GAMMA}, AGB_OUT, 1.0f},
    },
};

#undef AGB_GAMMA
#undef AGB_OUT

// Screen darkening factor. Default to 0.0f
float darken_screen = 0.0f;

// Color mode (0 for sRGB, 1 for DCI, 2 for Rec2020). Default to sRGB (0).
int color_mode = 0;

// Panel variant. Default to GBA (0).
int variant = kGbaFilterGba;

// The active profile, resolved for the per-pixel loop.
struct LcdKernel {
    float gamma[3];   // Input gamma with darken_screen folded in
    float mat[3][3];  // mat[out][in], transposed from LcdProfile
    float off[3];     // Black lift, scaled by the alpha channel
    float lum;
    float out_recip;  // 1 / out_gamma
};

LcdKernel kernel;

}  // namespace

// --- Function Implementations ---

// Forward declaration of a helper function to set the profile based on color_mode
static void set_profile_from_mode();

// This constructor-like function runs once when the program starts.
struct GbafilterInitializer {
    GbafilterInitializer() {
        set_profile_from_mode();
    }
};
static GbafilterInitializer __gbafilter_initializer;


// Helper function to resolve 'variant' and 'color_mode' into the kernel.
static void set_profile_from_mode() {
    const int v = (variant >= 0 && variant < kGbaFilterVariantCount)
                      ? variant : kGbaFilterGba;
    const int m = (color_mode >= 0 && color_mode < 3) ? color_mode : 0;
    const LcdProfile& p = kProfiles[v][m];

    kernel.lum = p.m[3][3];
    kernel.out_recip = 1.0f / p.out_gamma;

    for (int row = 0; row < 3; row++) {
        kernel.gamma[row] = p.in_gamma[row] + darken_screen * p.darken_scale;

        // The shader lift column is scaled by the clamped alpha channel, which
        // is lum for an opaque framebuffer.
        kernel.off[row] = p.m[3][row] * kernel.lum;

        for (int col = 0; col < 3; col++)
            kernel.mat[row][col] = p.m[col][row];
    }
}


// Public function to set color mode, darken screen and panel variant from
// external calls.
void gbafilter_set_params(int new_color_mode, float new_darken_screen,
                          int new_variant) {
    color_mode = new_color_mode;
    darken_screen = fmaxf(0.0f, fminf(1.0f, new_darken_screen)); // Clamp to 0.0-1.0
    variant = new_variant;

    // Call the helper to update the kernel based on the new parameters.
    set_profile_from_mode();
}

bool gbafilter_variant_has_darken(int v) {
    if (v < 0 || v >= kGbaFilterVariantCount)
        return true;
    return kProfiles[v][0].darken_scale != 0.0f;
}

// Shared by every palette function below.
static inline void apply_filter(float& r, float& g, float& b) {
    // 1. Apply initial gamma (including darken_screen) to convert to linear space.
    r = powf(r, kernel.gamma[0]);
    g = powf(g, kernel.gamma[1]);
    b = powf(b, kernel.gamma[2]);

    // 2. Apply luminance factor and clamp.
    r = fmaxf(0.0f, fminf(1.0f, r * kernel.lum));
    g = fmaxf(0.0f, fminf(1.0f, g * kernel.lum));
    b = fmaxf(0.0f, fminf(1.0f, b * kernel.lum));

    // 3. Apply color profile matrix.
    const float tr = kernel.mat[0][0] * r + kernel.mat[0][1] * g +
                     kernel.mat[0][2] * b + kernel.off[0];
    const float tg = kernel.mat[1][0] * r + kernel.mat[1][1] * g +
                     kernel.mat[1][2] * b + kernel.off[1];
    const float tb = kernel.mat[2][0] * r + kernel.mat[2][1] * g +
                     kernel.mat[2][2] * b + kernel.off[2];

    // 4. Apply display gamma. Mirror through the origin; the matrices carry
    // negative cross terms and powf() rejects a negative base.
    r = copysignf(powf(fabsf(tr), kernel.out_recip), tr);
    g = copysignf(powf(fabsf(tg), kernel.out_recip), tg);
    b = copysignf(powf(fabsf(tb), kernel.out_recip), tb);

    // Final clamp: ensure values are within 0.0-1.0 range
    r = fmaxf(0.0f, fminf(1.0f, r));
    g = fmaxf(0.0f, fminf(1.0f, g));
    b = fmaxf(0.0f, fminf(1.0f, b));
}

void gbafilter_update_colors(bool lcd) {
    switch (systemColorDepth) {
    case 8: {
        for (int i = 0; i < 0x10000; i++) {
            systemColorMap8[i] = (uint8_t)((((i & 0x1f) << 3) & 0xE0) |
                ((((i & 0x3e0) >> 5) << 0) & 0x1C) |
                ((((i & 0x7c00) >> 10) >> 3) & 0x3));
        }
        if (lcd)
            gbafilter_pal8(systemColorMap8, 0x10000);
    } break;
    case 16: {
        for (int i = 0x0; i < 0x10000; i++) {
            // GBA uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            // Always output RGB555 (the LCD filter assumes this layout)
            systemColorMap16[i] = ((i & 0x1f) << 10) |  // R
                (((i & 0x3e0) >> 5) << 5) |  // G
                (((i & 0x7c00) >> 10) << 0);   // B
        }
        if (lcd)
            gbafilter_pal(systemColorMap16, 0x10000);
    } break;
    case 24:
    case 32: {
        // Hardcode shifts for 24/32-bit to avoid a race with filter threads
        // temporarily overriding the globals to 32-bit values (19/11/3).
        // 32-bit is unaffected in practice (never goes through the non-32bpp
        // filter conversion path), but 24-bit suffers the same concurrency
        // issue as 16-bit did with systemRedShift etc.
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
        const int rShift = 3, gShift = 11, bShift = 19;
#else
        const int rShift = 27, gShift = 19, bShift = 11;
#endif
        for (int i = 0; i < 0x10000; i++) {
            uint8_t r5 = static_cast<uint8_t>(i & 0x1f);
            uint8_t g5 = static_cast<uint8_t>((i & 0x3e0) >> 5);
            uint8_t b5 = static_cast<uint8_t>((i & 0x7c00) >> 10);

            // Scale 5-bit to 8-bit: 0x1F -> 0xFF (bit replication preserves white)
            uint8_t r8 = (r5 << 3) | (r5 >> 2);
            uint8_t g8 = (g5 << 3) | (g5 >> 2);
            uint8_t b8 = (b5 << 3) | (b5 >> 2);

            uint32_t final_pix = 0;
            final_pix |= ((r8 >> 3) & 0x1f) << rShift;
            final_pix |= (r8 & 0x07)        << (rShift - 3);
            final_pix |= ((g8 >> 3) & 0x1f) << gShift;
            final_pix |= (g8 & 0x07)        << (gShift - 3);
            final_pix |= ((b8 >> 3) & 0x1f) << bShift;
            final_pix |= (b8 & 0x07)        << (bShift - 3);
            systemColorMap32[i] = final_pix;
        }
        if (lcd)
            gbafilter_pal32(systemColorMap32, 0x10000);
    } break;
    }
}

void gbafilter_pal8(uint8_t* buf, int count)
{
    while (count--) {
        uint8_t pix = *buf;

        uint8_t original_r_val_3bit = (uint8_t)((pix & 0xE0) >> 5);
        uint8_t original_g_val_3bit = (uint8_t)((pix & 0x1C) >> 2);
        uint8_t original_b_val_2bit = (uint8_t)(pix & 0x3);

        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_3bit / 7.0f;
        float g = (float)original_g_val_3bit / 7.0f;
        float b = (float)original_b_val_2bit / 3.0f;

        apply_filter(r, g, b);
        const float transformed_r = r;
        const float transformed_g = g;
        const float transformed_b = b;

        // Convert back to 3-bit or 2-bit (0-7 or 0-3) integer and combine into uint8_t
        // Apply 5-bit to 5-bit conversion, as this palette is for 16-bit output.
        uint8_t final_red = (uint8_t)(transformed_r * 7.0f + 0.5f);
        uint8_t final_green = (uint8_t)(transformed_g * 7.0f + 0.5f);
        uint8_t final_blue = (uint8_t)(transformed_b * 3.0f + 0.5f);

        // Ensure values are strictly within 0-7 or 0-3 range after rounding
        if (final_red > 7) final_red = 7;
        if (final_green > 7) final_green = 7;
        if (final_blue > 3) final_blue = 3;

        *buf++ = ((final_red & 0x7) << 5) |
            ((final_green & 0x7) << 2) |
            (final_blue & 0x3);
    }
}

void gbafilter_pal(uint16_t* buf, int count)
{
    while (count--) {
        uint16_t pix = *buf;

        // Hardcode RGB555 shifts (10/5/0) - systemRedShift etc. may be
        // temporarily overridden to 32-bit values (19/11/3) by filter
        // threads running concurrently, which would corrupt the palette.
        uint8_t original_r_val_5bit = (pix >> 10) & 0x1f;
        uint8_t original_g_val_5bit = (pix >> 5) & 0x1f;
        uint8_t original_b_val_5bit = (pix >> 0) & 0x1f;

        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_5bit / 31.0f;
        float g = (float)original_g_val_5bit / 31.0f;
        float b = (float)original_b_val_5bit / 31.0f;

        apply_filter(r, g, b);
        const float transformed_r = r;
        const float transformed_g = g;
        const float transformed_b = b;

        // Convert back to 5-bit (0-31) integer and combine into uint16_t
        // Apply 5-bit to 5-bit conversion, as this palette is for 16-bit output.
        uint8_t final_red = (uint8_t)(transformed_r * 31.0f + 0.5f);
        uint8_t final_green = (uint8_t)(transformed_g * 31.0f + 0.5f);
        uint8_t final_blue = (uint8_t)(transformed_b * 31.0f + 0.5f);

        // Ensure values are strictly within 0-31 range after rounding
        if (final_red > 31) final_red = 31;
        if (final_green > 31) final_green = 31;
        if (final_blue > 31) final_blue = 31;

        *buf++ = (final_red   << 10) |
                 (final_green <<  5) |
                  final_blue;
    }
}

void gbafilter_pal32(uint32_t* buf, int count)
{
    // Hardcode shifts to avoid race with filter threads temporarily overriding globals.
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
    const int rShift = 3, gShift = 11, bShift = 19;
#else
    const int rShift = 27, gShift = 19, bShift = 11;
#endif

    while (count--) {
        uint32_t pix = *buf;

        // Extract the 5 MSBs of each 8-bit channel from its packed position.
        uint8_t original_r_val_5bit = (uint8_t)((pix >> rShift) & 0x1f);
        uint8_t original_g_val_5bit = (uint8_t)((pix >> gShift) & 0x1f);
        uint8_t original_b_val_5bit = (uint8_t)((pix >> bShift) & 0x1f);


        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_5bit / 31.0f;
        float g = (float)original_g_val_5bit / 31.0f;
        float b = (float)original_b_val_5bit / 31.0f;

        apply_filter(r, g, b);
        const float transformed_r = r;
        const float transformed_g = g;
        const float transformed_b = b;


        // Convert the floating-point values to 8-bit integer components (0-255).
        // Values are already guaranteed to be in 0-255 range since they are uint8_t
        // and the floating point values are clamped to 0.0-1.0 before conversion.
        uint8_t final_red_8bit = (uint8_t)(transformed_r * 255.0f + 0.5f);
        uint8_t final_green_8bit = (uint8_t)(transformed_g * 255.0f + 0.5f);
        uint8_t final_blue_8bit = (uint8_t)(transformed_b * 255.0f + 0.5f);

        // --- 8bit shift scaling logic ---
        // This maps 8-bit color to the 5-bit shifted format,
        // while allowing FFFFFF, enhancing whites and color.
        // It uses the top 5 bits of the 8-bit value for the GBA's 5-bit component position,
        // and the bottom 3 bits to fill the lower, normally zeroed, positions.

        uint32_t final_pix = 0;

        // Red component
        final_pix |= ((final_red_8bit >> 3) & 0x1f) << rShift;
        final_pix |= (final_red_8bit & 0x07)        << (rShift - 3);

        // Green component
        final_pix |= ((final_green_8bit >> 3) & 0x1f) << gShift;
        final_pix |= (final_green_8bit & 0x07)        << (gShift - 3);

        // Blue component
        final_pix |= ((final_blue_8bit >> 3) & 0x1f) << bShift;
        final_pix |= (final_blue_8bit & 0x07)        << (bShift - 3);

        // Preserve existing alpha if present (assuming it's at bits 24-31 for 32-bit depth)
        if (systemColorDepth == 32) {
            final_pix |= (pix & (0xFF << 24));
        }

        *buf++ = final_pix;
    }
}

void gbafilter_update_colors_native(bool lcd) {
    switch (systemColorDepth) {
    case 8: {
        for (int i = 0; i < 0x10000; i++) {
            systemColorMap8[i] = (uint8_t)((((i & 0x1f) << 3) & 0xE0) |
                ((((i & 0x3e0) >> 5) << 0) & 0x1C) |
                ((((i & 0x7c00) >> 10) >> 3) & 0x3));
        }
        if (lcd)
            gbafilter_pal8(systemColorMap8, 0x10000);
    } break;
    case 16: {
        #ifdef FRONTEND_SUPPORTS_BGR1555
        for (int i = 0x0; i < 0x10000; i++) {
            // GBA uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            systemColorMap16[i] = ((i & 0x1f) << systemRedShift) |
                (((i & 0x3e0) >> 5) << systemGreenShift) |
                (((i & 0x7c00) >> 10) << systemBlueShift);
        }
        if (lcd)
            gbafilter_pal(systemColorMap16, 0x10000);
        #else
        for (int i = 0x0; i < 0x10000; i++) {
            // GBA uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            // GB/GBC uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            int r5 = i & 0x1F;
            int g5 = (i >> 5) & 0x1F;
            int b5 = (i >> 10) & 0x1F;

            // Map to 16-bit RGB565 (5-6-5)
            int g6 = (g5 << 1) | (g5 >> 4);

            systemColorMap16[i] = static_cast<uint16_t>(
                (r5 << systemRedShift) |
                (g6 << (systemGreenShift - 1)) |
                (b5 << systemBlueShift));
        }
        if (lcd)
            gbafilter_pal_565(systemColorMap16, 0x10000);
        #endif
    } break;
    case 24:
    case 32: {
        for (int i = 0; i < 0x10000; i++) {
            // GBA uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            int r5 = i & 0x1F;
            int g5 = (i >> 5) & 0x1F;
            int b5 = (i >> 10) & 0x1F;

            // Expand 5-bit to 8-bit components
            uint8_t final_red_8bit = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
            uint8_t final_green_8bit = static_cast<uint8_t>((g5 << 3) | (g5 >> 2));
            uint8_t final_blue_8bit = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));

            uint32_t final_pix = 0;

            // Red component
            // 5 most significant bits (MSBs) for the 'systemRedShift' position
            final_pix |= ((final_red_8bit >> 3) & 0x1f) << systemRedShift;
            // 3 least significant bits (LSBs) for the 'base' position (systemRedShift - 3)
            final_pix |= (final_red_8bit & 0x07) << (systemRedShift - 3);

            // Green component
            // 5 MSBs for the 'systemGreenShift' position
            final_pix |= ((final_green_8bit >> 3) & 0x1f) << systemGreenShift;
            // 3 LSBs for the 'base' position (systemGreenShift - 3)
            final_pix |= (final_green_8bit & 0x07) << (systemGreenShift - 3);

            // Blue component
            // 5 MSBs for the 'systemBlueShift' position
            final_pix |= ((final_blue_8bit >> 3) & 0x1f) << systemBlueShift;
            // 3 LSBs for the 'base' position (systemBlueShift - 3)
            final_pix |= (final_blue_8bit & 0x07) << (systemBlueShift - 3);

            systemColorMap32[i] = final_pix;
        }
        if (lcd)
            gbafilter_pal_888(systemColorMap32, 0x10000);
    } break;
    }
}

void gbafilter_pal_565(uint16_t* buf, int count)
{
    while (count--) {
        uint16_t pix = *buf;

        uint8_t original_r_val_5bit = (uint8_t)((pix >> systemRedShift) & 0x1f);
        uint8_t original_g_val_6bit = (uint8_t)((pix >> (systemGreenShift - 1)) & 0x3f);
        uint8_t original_b_val_5bit = (uint8_t)((pix >> systemBlueShift) & 0x1f);

        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_5bit / 31.0f;
        float g = (float)original_g_val_6bit / 63.0f;
        float b = (float)original_b_val_5bit / 31.0f;

        apply_filter(r, g, b);
        const float transformed_r = r;
        const float transformed_g = g;
        const float transformed_b = b;

        // Convert back to 5-bit (0-31) integer and combine into uint16_t
        // Apply 5-bit to 5-bit conversion, as this palette is for 16-bit output.
        uint8_t final_red   = (uint8_t)(transformed_r * 31.0f + 0.5f);
        uint8_t final_green = (uint8_t)(transformed_g * 63.0f + 0.5f);
        uint8_t final_blue  = (uint8_t)(transformed_b * 31.0f + 0.5f);

        // Ensure values are strictly within 0-31 range after rounding
        if (final_red > 31)   final_red   = 31;
        if (final_green > 63) final_green = 63;
        if (final_blue > 31)  final_blue  = 31;

        *buf++ = (final_red << systemRedShift) |
            (final_green << (systemGreenShift - 1)) |
            (final_blue << systemBlueShift);
    }
}

void gbafilter_pal_888(uint32_t* buf, int count)
{
    while (count--) {
        uint32_t pix = *buf;

        // Extract original 5-bit R, G, B values from the shifted positions in the 32-bit pixel.
        // These shifts pull out the 5-bit value from its shifted position (e.g., bits 3-7 for Red).
        uint8_t original_r_val_8bit = (uint8_t)((pix >> (systemRedShift - 3)) & 0xff);
        uint8_t original_g_val_8bit = (uint8_t)((pix >> (systemGreenShift - 3)) & 0xff);
        uint8_t original_b_val_8bit = (uint8_t)((pix >> (systemBlueShift - 3)) & 0xff);

        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_8bit / 255.0f;
        float g = (float)original_g_val_8bit / 255.0f;
        float b = (float)original_b_val_8bit / 255.0f;

        apply_filter(r, g, b);
        const float transformed_r = r;
        const float transformed_g = g;
        const float transformed_b = b;

        // Convert the floating-point values to 8-bit integer components (0-255).
        // Values are already guaranteed to be in 0-255 range since they are uint8_t
        // and the floating point values are clamped to 0.0-1.0 before conversion.
        uint8_t final_red_8bit   = (uint8_t)(transformed_r * 255.0f + 0.5f);
        uint8_t final_green_8bit = (uint8_t)(transformed_g * 255.0f + 0.5f);
        uint8_t final_blue_8bit  = (uint8_t)(transformed_b * 255.0f + 0.5f);

        uint32_t final_pix = 0;

        // Red component
        // 5 most significant bits (MSBs) for the 'systemRedShift' position
        final_pix |= ((final_red_8bit >> 3) & 0x1f) << systemRedShift;
        // 3 least significant bits (LSBs) for the 'base' position (systemRedShift - 3)
        final_pix |= (final_red_8bit & 0x07) << (systemRedShift - 3);

        // Green component
        // 5 MSBs for the 'systemGreenShift' position
        final_pix |= ((final_green_8bit >> 3) & 0x1f) << systemGreenShift;
        // 3 LSBs for the 'base' position (systemGreenShift - 3)
        final_pix |= (final_green_8bit & 0x07) << (systemGreenShift - 3);

        // Blue component
        // 5 MSBs for the 'systemBlueShift' position
        final_pix |= ((final_blue_8bit >> 3) & 0x1f) << systemBlueShift;
        // 3 LSBs for the 'base' position (systemBlueShift - 3)
        final_pix |= (final_blue_8bit & 0x07) << (systemBlueShift - 3);

        // Preserve existing alpha if present (assuming it's at bits 24-31 for 32-bit depth)
        if (systemColorDepth == 32) {
            final_pix |= (pix & (0xFF << 24));
        }

        *buf++ = final_pix;
    }
}

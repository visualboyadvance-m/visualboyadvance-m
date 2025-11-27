/*
 * GBC Color Correction Shader Implementation
 *
 * Shader modified by Pokefan531.
 * Color Mangler
 * Original Author: hunterk
 * Original License: Public domain
 *
 * This code is adapted from the original shader logic.
 */

#include "components/filters_cgb/filters_cgb.h"
#include <cmath>
#include <algorithm>

extern int systemColorDepth;
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

extern uint8_t  systemColorMap8[0x10000];
extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];

// --- Global Constants and Variables for GBC Color Correction ---
// Define the color profile matrix as a static const float 2D array
// This replicates the column-major order of GLSL mat4 for easier translation.
// Format: { {col0_row0, col0_row1, col0_row2, col0_row3}, ... }
static const float GBC_sRGB[4][4] = {
    {0.905f, 0.10f, 0.1575f, 0.0f}, // Column 0 (R output contributions from R, G, B, A)
    {0.195f, 0.65f, 0.1425f, 0.0f}, // Column 1 (G output contributions from R, G, B, A)
    {-0.10f, 0.25f, 0.70f,   0.0f}, // Column 2 (B output contributions from R, G, B, A)
    {0.0f,   0.0f,  0.0f,    0.91f} // Column 3 (A/Luminance contribution)
};

static const float GBC_DCI[4][4] = {
    {0.76f,  0.125f,  0.16f, 0.0f},
    {0.27f,  0.6375f, 0.18f, 0.0f},
    {-0.03f, 0.2375f, 0.66f, 0.0f},
    {0.0f,   0.0f,    0.0f,  0.97f}
};

static const float GBC_Rec2020[4][4] = {
    {0.61f,  0.155f, 0.16f,   0.0f},
    {0.345f, 0.615f, 0.1875f, 0.0f},
    {0.045f, 0.23f,  0.6525f, 0.0f},
    {0.0f,   0.0f,   0.0f,    1.0f}
};

// Screen lightening factor. Default to 0.0f.
static float lighten_screen = 0.0f;

// Color mode (0 for sRGB, 1 for DCI, 2 for Rec2020). Default to sRGB (0).
static int color_mode = 0;

// Pointer to the currently selected color profile matrix.
static const float (*profile)[4];

// Global constants from the shader for gamma correction values
static const float target_gamma = 2.2f;
static const float display_gamma = 2.2f;


// --- Function Implementations ---

// Forward declaration of a helper function to set the profile based on color_mode
static void set_profile_from_mode();

// This constructor-like function runs once when the program starts.
struct GbcfilterInitializer {
    GbcfilterInitializer() {
        set_profile_from_mode();
    }
};
static GbcfilterInitializer __gbcfilter_initializer;


// Helper function to set the 'profile' pointer based on the 'color_mode' variable.
static void set_profile_from_mode() {
    if (color_mode == 0) {
        profile = GBC_sRGB;
    }
    else if (color_mode == 1) {
        profile = GBC_DCI;
    }
    else if (color_mode == 2) {
        profile = GBC_Rec2020;
    }
    else {
        profile = GBC_sRGB; // Default to sRGB if an invalid mode is set
    }
}


// Public function to set color mode and darken screen from external calls
void gbcfilter_set_params(int new_color_mode, float new_lighten_screen) {
    color_mode = new_color_mode;
    lighten_screen = fmaxf(0.0f, fminf(1.0f, new_lighten_screen)); // Clamp to 0.0-1.0

    // Call the helper to update 'profile' based on the new 'color_mode'
    set_profile_from_mode();
}

void gbcfilter_update_colors(bool lcd) {
    switch (systemColorDepth) {
    case 8: {
        for (int i = 0; i < 0x10000; i++) {
            systemColorMap8[i] = (uint8_t)((((i & 0x1f) << 3) & 0xE0) |
                ((((i & 0x3e0) >> 5) << 0) & 0x1C) |
                ((((i & 0x7c00) >> 10) >> 3) & 0x3));
        }
        if (lcd)
            gbcfilter_pal8(systemColorMap8, 0x10000);
    } break;
    case 16: {
        for (int i = 0x0; i < 0x10000; i++) {
            // GB/GBC uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            systemColorMap16[i] = ((i & 0x1f) << systemRedShift) |
                (((i & 0x3e0) >> 5) << systemGreenShift) |
                (((i & 0x7c00) >> 10) << systemBlueShift);
        }
        if (lcd)
            gbcfilter_pal(systemColorMap16, 0x10000);
    } break;
    case 24:
    case 32: {
        for (int i = 0; i < 0x10000; i++) {
            // GB/GBC uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            systemColorMap32[i] = ((i & 0x1f) << systemRedShift) |
                (((i & 0x3e0) >> 5) << systemGreenShift) |
                (((i & 0x7c00) >> 10) << systemBlueShift);
        }
        if (lcd)
            gbcfilter_pal32(systemColorMap32, 0x10000);
    } break;
    }
}

void gbcfilter_pal8(uint8_t* buf, int count)
{
    // Pre-calculate constants for efficiency within function scope
    const float target_gamma_exponent = target_gamma + (lighten_screen * -1.0f);
    const float display_gamma_reciprocal = 1.0f / display_gamma;
    const float luminance_factor = profile[3][3]; // profile[3].w from GLSL

    while (count--) {
        uint8_t pix = *buf;

        uint8_t original_r_val_3bit = (uint8_t)((pix & 0xE0) >> 5);
        uint8_t original_g_val_3bit = (uint8_t)((pix & 0x1C) >> 2);
        uint8_t original_b_val_2bit = (uint8_t)(pix & 0x3);

        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_3bit / 7.0f;
        float g = (float)original_g_val_3bit / 7.0f;
        float b = (float)original_b_val_2bit / 3.0f;

        // 1. Apply initial gamma (including lighten_screen as exponent) to convert to linear space.
        // This step will affect non-"white" values.
        r = powf(r, target_gamma_exponent);
        g = powf(g, target_gamma_exponent);
        b = powf(b, target_gamma_exponent);

        // 2. Apply luminance factor and clamp.
        r = fmaxf(0.0f, fminf(1.0f, r * luminance_factor));
        g = fmaxf(0.0f, fminf(1.0f, g * luminance_factor));
        b = fmaxf(0.0f, fminf(1.0f, b * luminance_factor));

        // 3. Apply color profile matrix (using profile[column][row] access)
        float transformed_r = profile[0][0] * r + profile[1][0] * g + profile[2][0] * b;
        float transformed_g = profile[0][1] * r + profile[1][1] * g + profile[2][1] * b;
        float transformed_b = profile[0][2] * r + profile[1][2] * g + profile[2][2] * b;

        // 4. Apply display gamma to convert back for display.
        transformed_r = copysignf(powf(fabsf(transformed_r), display_gamma_reciprocal), transformed_r);
        transformed_g = copysignf(powf(fabsf(transformed_g), display_gamma_reciprocal), transformed_g);
        transformed_b = copysignf(powf(fabsf(transformed_b), display_gamma_reciprocal), transformed_b);

        // Final clamp: ensure values are within 0.0-1.0 range
        transformed_r = fmaxf(0.0f, fminf(1.0f, transformed_r));
        transformed_g = fmaxf(0.0f, fminf(1.0f, transformed_g));
        transformed_b = fmaxf(0.0f, fminf(1.0f, transformed_b));

        // Convert back to 3-bit or 2-bit (0-7 or 0-3) integer and combine into uint8_t
        // Apply 3-bit or 2-bit to 8-bit conversion, as this palette is for 8-bit output.
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
void gbcfilter_pal(uint16_t* buf, int count)
{
    // Pre-calculate constants for efficiency within function scope
    const float target_gamma_exponent = target_gamma + (lighten_screen * -1.0f);
    const float display_gamma_reciprocal = 1.0f / display_gamma;
    const float luminance_factor = profile[3][3]; // profile[3].w from GLSL

    while (count--) {
        uint16_t pix = *buf;

        uint8_t original_r_val_5bit = (uint8_t)((pix >> systemRedShift) & 0x1f);
        uint8_t original_g_val_5bit = (uint8_t)((pix >> systemGreenShift) & 0x1f);
        uint8_t original_b_val_5bit = (uint8_t)((pix >> systemBlueShift) & 0x1f);

        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_5bit / 31.0f;
        float g = (float)original_g_val_5bit / 31.0f;
        float b = (float)original_b_val_5bit / 31.0f;

        // 1. Apply initial gamma (including lighten_screen as exponent) to convert to linear space.
        // This step will affect non-"white" values.
        r = powf(r, target_gamma_exponent);
        g = powf(g, target_gamma_exponent);
        b = powf(b, target_gamma_exponent);

        // 2. Apply luminance factor and clamp.
        r = fmaxf(0.0f, fminf(1.0f, r * luminance_factor));
        g = fmaxf(0.0f, fminf(1.0f, g * luminance_factor));
        b = fmaxf(0.0f, fminf(1.0f, b * luminance_factor));

        // 3. Apply color profile matrix (using profile[column][row] access)
        float transformed_r = profile[0][0] * r + profile[1][0] * g + profile[2][0] * b;
        float transformed_g = profile[0][1] * r + profile[1][1] * g + profile[2][1] * b;
        float transformed_b = profile[0][2] * r + profile[1][2] * g + profile[2][2] * b;

        // 4. Apply display gamma to convert back for display.
        transformed_r = copysignf(powf(fabsf(transformed_r), display_gamma_reciprocal), transformed_r);
        transformed_g = copysignf(powf(fabsf(transformed_g), display_gamma_reciprocal), transformed_g);
        transformed_b = copysignf(powf(fabsf(transformed_b), display_gamma_reciprocal), transformed_b);

        // Final clamp: ensure values are within 0.0-1.0 range
        transformed_r = fmaxf(0.0f, fminf(1.0f, transformed_r));
        transformed_g = fmaxf(0.0f, fminf(1.0f, transformed_g));
        transformed_b = fmaxf(0.0f, fminf(1.0f, transformed_b));

        // Convert back to 5-bit (0-31) integer and combine into uint16_t
        // Apply 5-bit to 5-bit conversion, as this palette is for 16-bit output.
        uint8_t final_red = (uint8_t)(transformed_r * 31.0f + 0.5f);
        uint8_t final_green = (uint8_t)(transformed_g * 31.0f + 0.5f);
        uint8_t final_blue = (uint8_t)(transformed_b * 31.0f + 0.5f);

        // Ensure values are strictly within 0-31 range after rounding
        if (final_red > 31) final_red = 31;
        if (final_green > 31) final_green = 31;
        if (final_blue > 31) final_blue = 31;

        *buf++ = (final_red << systemRedShift) |
            (final_green << systemGreenShift) |
            (final_blue << systemBlueShift);
    }
}

void gbcfilter_pal32(uint32_t* buf, int count)
{
    // Pre-calculate constants for efficiency within function scope
    const float target_gamma_exponent = target_gamma + (lighten_screen * -1.0f);
    const float display_gamma_reciprocal = 1.0f / display_gamma;
    const float luminance_factor = profile[3][3]; // profile[3].w from GLSL

    while (count--) {
        uint32_t pix = *buf;

        // Extract original 5-bit R, G, B values from the shifted positions in the 32-bit pixel.
        // These shifts pull out the 5-bit value from its shifted position (e.g., bits 3-7 for Red).
        uint8_t original_r_val_5bit = (uint8_t)((pix >> systemRedShift) & 0x1f);
        uint8_t original_g_val_5bit = (uint8_t)((pix >> systemGreenShift) & 0x1f);
        uint8_t original_b_val_5bit = (uint8_t)((pix >> systemBlueShift) & 0x1f);


        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_5bit / 31.0f;
        float g = (float)original_g_val_5bit / 31.0f;
        float b = (float)original_b_val_5bit / 31.0f;

        // 1. Apply initial gamma (including lighten_screen as exponent) to convert to linear space.
        r = powf(r, target_gamma_exponent);
        g = powf(g, target_gamma_exponent);
        b = powf(b, target_gamma_exponent);

        // 2. Apply luminance factor and clamp.
        r = fmaxf(0.0f, fminf(1.0f, r * luminance_factor));
        g = fmaxf(0.0f, fminf(1.0f, g * luminance_factor));
        b = fmaxf(0.0f, fminf(1.0f, b * luminance_factor));

        // 3. Apply color profile matrix
        float transformed_r = profile[0][0] * r + profile[1][0] * g + profile[2][0] * b;
        float transformed_g = profile[0][1] * r + profile[1][1] * g + profile[2][1] * b;
        float transformed_b = profile[0][2] * r + profile[1][2] * g + profile[2][2] * b;

        // 4. Apply display gamma.
        transformed_r = copysignf(powf(fabsf(transformed_r), display_gamma_reciprocal), transformed_r);
        transformed_g = copysignf(powf(fabsf(transformed_g), display_gamma_reciprocal), transformed_g);
        transformed_b = copysignf(powf(fabsf(transformed_b), display_gamma_reciprocal), transformed_b);

        // Final clamp: ensure values are within 0.0-1.0 range
        transformed_r = fmaxf(0.0f, fminf(1.0f, transformed_r));
        transformed_g = fmaxf(0.0f, fminf(1.0f, transformed_g));
        transformed_b = fmaxf(0.0f, fminf(1.0f, transformed_b));


        // Convert the floating-point values to 8-bit integer components (0-255).
        // Values are already guaranteed to be in 0-255 range since they are uint8_t
        // and the floating point values are clamped to 0.0-1.0 before conversion.
        uint8_t final_red_8bit = (uint8_t)(transformed_r * 255.0f + 0.5f);
        uint8_t final_green_8bit = (uint8_t)(transformed_g * 255.0f + 0.5f);
        uint8_t final_blue_8bit = (uint8_t)(transformed_b * 255.0f + 0.5f);

        // --- NEW PACKING LOGIC ---
        // This is the critical change to correctly map 8-bit color to the 5-bit shifted format,
        // while allowing FFFFFF.
        // It uses the top 5 bits of the 8-bit value for the GBC's 5-bit component position,
        // and the bottom 3 bits to fill the lower, normally zeroed, positions.

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

void gbcfilter_update_colors_native(bool lcd) {
    switch (systemColorDepth) {
    case 8: {
        for (int i = 0; i < 0x10000; i++) {
            systemColorMap8[i] = (uint8_t)((((i & 0x1f) << 3) & 0xE0) |
                ((((i & 0x3e0) >> 5) << 0) & 0x1C) |
                ((((i & 0x7c00) >> 10) >> 3) & 0x3));
        }
        if (lcd)
            gbcfilter_pal8(systemColorMap8, 0x10000);
    } break;
    case 16: {
        for (int i = 0x0; i < 0x10000; i++) {
            // GB/GBC uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            int r5 = i & 0x1F;
            int g5 = (i >> 5) & 0x1F;
            int b5 = (i >> 10) & 0x1F;

            // Map to 16-bit RGB565 (5-6-5)
            int g6 = (g5 << 1) | (g5 >> 4);

            systemColorMap16[i] =
                (r5 << systemRedShift) |
                (g6 << (systemGreenShift - 1)) |
                (b5 << systemBlueShift);
        }
        if (lcd)
            gbcfilter_pal_565(systemColorMap16, 0x10000);
    } break;
    case 24:
    case 32: {
        for (int i = 0; i < 0x10000; i++) {
            // GB/GBC uses BGR555 format: 0BBBBBGGGGGRRRRR
            // Red: bits 0-4, Green: bits 5-9, Blue: bits 10-14
            int r5 = i & 0x1F;
            int g5 = (i >> 5) & 0x1F;
            int b5 = (i >> 10) & 0x1F;

            // Expand 5-bit to 8-bit components
            uint8_t final_red_8bit = (r5 << 3) | (r5 >> 2);
            uint8_t final_green_8bit = (g5 << 3) | (g5 >> 2);
            uint8_t final_blue_8bit = (b5 << 3) | (b5 >> 2);

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
            gbcfilter_pal_888(systemColorMap32, 0x10000);
    } break;
    }
}

#define APPLY_FILTER(r, g, b) \
    do { \
        /* 1. Apply initial gamma (including darken_screen as exponent) to convert to linear space. */ \
        /* This step will affect non-"white" values. */ \
        r = powf(r, target_gamma_exponent); \
        g = powf(g, target_gamma_exponent); \
        b = powf(b, target_gamma_exponent); \
        /* 2. Apply luminance factor and clamp. */ \
        r = fmaxf(0.0f, fminf(1.0f, r * luminance_factor)); \
        g = fmaxf(0.0f, fminf(1.0f, g * luminance_factor)); \
        b = fmaxf(0.0f, fminf(1.0f, b * luminance_factor)); \
        /* 3. Apply color profile matrix (using profile[column][row] access) */ \
        transformed_r = profile[0][0] * r + profile[1][0] * g + profile[2][0] * b; \
        transformed_g = profile[0][1] * r + profile[1][1] * g + profile[2][1] * b; \
        transformed_b = profile[0][2] * r + profile[1][2] * g + profile[2][2] * b; \
        /* 4. Apply display gamma to convert back for display. */ \
        transformed_r = copysignf(powf(fabsf(transformed_r), display_gamma_reciprocal), transformed_r); \
        transformed_g = copysignf(powf(fabsf(transformed_g), display_gamma_reciprocal), transformed_g); \
        transformed_b = copysignf(powf(fabsf(transformed_b), display_gamma_reciprocal), transformed_b); \
        /* Final clamp: ensure values are within 0.0-1.0 range */ \
        transformed_r = fmaxf(0.0f, fminf(1.0f, transformed_r)); \
        transformed_g = fmaxf(0.0f, fminf(1.0f, transformed_g)); \
        transformed_b = fmaxf(0.0f, fminf(1.0f, transformed_b)); \
    } while(0)

void gbcfilter_pal_565(uint16_t* buf, int count)
{
    // Pre-calculate constants for efficiency within function scope
    const float target_gamma_exponent = target_gamma + (lighten_screen * -1.0f);
    const float display_gamma_reciprocal = 1.0f / display_gamma;
    const float luminance_factor = profile[3][3]; // profile[3].w from GLSL

    while (count--) {
        float transformed_r, transformed_g, transformed_b;
        uint16_t pix = *buf;

        uint8_t original_r_val_5bit = (uint8_t)((pix >> systemRedShift) & 0x1f);
        uint8_t original_g_val_6bit = (uint8_t)((pix >> (systemGreenShift - 1)) & 0x3f);
        uint8_t original_b_val_5bit = (uint8_t)((pix >> systemBlueShift) & 0x1f);

        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_5bit / 31.0f;
        float g = (float)original_g_val_6bit / 63.0f;
        float b = (float)original_b_val_5bit / 31.0f;

        APPLY_FILTER(r, g, b);

        // Convert back to 5-bit (0-31) integer and combine into uint16_t
        // Apply 5-bit to 5-bit conversion, as this palette is for 16-bit output.
        uint8_t final_red = (uint8_t)(transformed_r * 31.0f + 0.5f);
        uint8_t final_green = (uint8_t)(transformed_g * 63.0f + 0.5f);
        uint8_t final_blue = (uint8_t)(transformed_b * 31.0f + 0.5f);

        // Ensure values are strictly within 0-31 range after rounding
        if (final_red > 31) final_red = 31;
        if (final_green > 63) final_green = 63;
        if (final_blue > 31) final_blue = 31;

        *buf++ = (final_red << systemRedShift) |
            (final_green << (systemGreenShift - 1)) |
            (final_blue << systemBlueShift);
    }
}

void gbcfilter_pal_888(uint32_t* buf, int count)
{
    // Pre-calculate constants for efficiency within function scope
    const float target_gamma_exponent = target_gamma + (lighten_screen * -1.0f);
    const float display_gamma_reciprocal = 1.0f / display_gamma;
    const float luminance_factor = profile[3][3]; // profile[3].w from GLSL

    while (count--) {
        float transformed_r, transformed_g, transformed_b;
        uint32_t pix = *buf;

        // Extract original 5-bit R, G, B values from the shifted positions in the 32-bit pixel.
        // These shifts pull out the 5-bit value from its shifted position (e.g., bits 3-7 for Red).
        uint8_t original_r_val_8bit = (uint8_t)((pix >> systemRedShift) & 0xff);
        uint8_t original_g_val_8bit = (uint8_t)((pix >> systemGreenShift) & 0xff);
        uint8_t original_b_val_8bit = (uint8_t)((pix >> systemBlueShift) & 0xff);


        // Normalize to 0.0-1.0 for calculations
        float r = (float)original_r_val_8bit / 255.0f;
        float g = (float)original_g_val_8bit / 255.0f;
        float b = (float)original_b_val_8bit / 255.0f;

        APPLY_FILTER(r, g, b);

        // Convert the floating-point values to 8-bit integer components (0-255).
        // Values are already guaranteed to be in 0-255 range since they are uint8_t
        // and the floating point values are clamped to 0.0-1.0 before conversion.
        uint8_t final_red_8bit = (uint8_t)(transformed_r * 255.0f + 0.5f);
        uint8_t final_green_8bit = (uint8_t)(transformed_g * 255.0f + 0.5f);
        uint8_t final_blue_8bit = (uint8_t)(transformed_b * 255.0f + 0.5f);

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

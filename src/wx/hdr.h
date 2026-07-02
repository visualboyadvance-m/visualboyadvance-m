#ifndef VBAM_WX_HDR_H_
#define VBAM_WX_HDR_H_

#include <cstdint>

// ----------------------------------------------------------------------------
// HDR output encoding (renderer-independent).
//
// The emulator core produces a low-dynamic-range, color-corrected RGBA8 image
// (see src/components/filters_agb). This module takes that final, post-filter
// image and re-encodes it for an HDR display surface. None of this depends on
// OpenGL / Vulkan / D3D12 / Metal -- each renderer only has to pick a target
// Encoding, allocate a surface of the matching format, and call the encoder
// for the pixels it is about to upload.
//
// Two encodings are supported, matching the two ways the major platforms model
// HDR:
//
//   kPQ10        BT.2100 PQ (SMPTE ST.2084) packed into A2B10G10R10. This is
//                the 10-bit unsigned packing shared verbatim by
//                  - OpenGL  GL_RGB10_A2 + GL_UNSIGNED_INT_2_10_10_10_REV
//                  - Vulkan  VK_FORMAT_A2B10G10R10_UNORM_PACK32
//                  - D3D12   DXGI_FORMAT_R10G10B10A2_UNORM
//                paired with a BT.2020 PQ colorspace on the swapchain/surface.
//                Used on Wayland, Windows and Vulkan.
//
//   kScRGBFp16   Linear scRGB (Rec.709 primaries, 1.0 == 80 nits, values may
//                exceed 1.0) as four IEEE-754 half floats. This is what macOS
//                EDR (kCGColorSpaceExtendedLinearSRGB / rgba16Float) and the
//                Windows scRGB swapchain colorspace expect.
//
// The transfer applied before encoding bakes the three user-facing knobs:
//   1. sdr_reference_nits  -- diffuse/"paper" white target (overall brightness)
//   2. peak_nits           -- ceiling for pure white and boosted highlights
//   3. highlight_knee      -- input level above which highlights ramp toward
//                             peak, leaving mid-tones at the reference
//   4. shadow_contrast     -- gamma below the knee that deepens blacks/shadows
//                             for more contrast, leaving highlights untouched
// ----------------------------------------------------------------------------

namespace hdr {

enum class Encoding {
    kNone = 0,
    kPQ10,       // A2B10G10R10, BT.2020 PQ. 4 bytes/pixel.
    kScRGBFp16,  // RGBA half-float, linear scRGB. 8 bytes/pixel.
};

// Bytes per pixel of the encoded output for a given encoding.
constexpr int BytesPerPixel(Encoding e) {
    return e == Encoding::kScRGBFp16 ? 8 : e == Encoding::kPQ10 ? 4 : 0;
}

struct Settings {
    bool  enabled = false;
    // Diffuse white in nits. Set above the ~203 nit SDR reference to make the
    // whole picture read brighter than a normal SDR window.
    float sdr_reference_nits = 203.0f;
    // Peak white / highlight ceiling in nits.
    float peak_nits = 600.0f;
    // Normalized input (0..1, post-linearization) at which highlight boost
    // begins. 1.0 disables the boost (everything tracks the reference).
    float highlight_knee = 0.75f;
    // Shadow contrast: a gamma applied to luminance below the knee, pivoted at
    // the knee so highlights are unaffected. 1.0 is neutral (linear); > 1.0
    // deepens blacks and dark shades and steepens the low end for more
    // contrast. Hue preserving (one scale per pixel, as with the boost).
    float shadow_contrast = 1.0f;
    // True if the corrected pixels are already in BT.2020 primaries (i.e. the
    // user picked the Rec2020 color-correction profile). When false the input
    // is assumed to be Rec.709/sRGB primaries and is converted as needed.
    bool  input_is_rec2020 = false;
    // scRGB encode: the nits value that maps to 1.0 in the float output. macOS
    // EDR (a relative surface, SDR white == system brightness) maps the reference
    // white to 1.0. SDL on Windows drives an absolute scRGB swapchain and auto-
    // multiplies its SDR white point in, so there this is 80 x that white point
    // (the system SDR-white nits) to cancel the multiply and emit true nits.
    float scrgb_white_nits = 80.0f;
    // scRGB encode target primaries: false = Rec.709 (SDL3 / generic scRGB),
    // true = Display P3 (macOS extended-linear-Display-P3 EDR surfaces).
    bool  scrgb_target_p3 = false;
};

// ----------------------------------------------------------------------------
// Platform HDR availability (determined once at startup).
//
// Availability is a property of the current display/session, independent of
// whether the HDR option is on. DetectAvailability() probes the platform; the
// getters are then safe to call from anywhere (UI, startup option fixup).
// ----------------------------------------------------------------------------

// Probe the current platform/session. Call once at startup, after the GUI
// toolkit/display connection is up. Cheap and cached; repeat calls are no-ops.
void DetectAvailability();

// True if the current display + an available renderer can actually present HDR.
bool HdrAvailable();

// True if a 10-bit "deep color" SDR visual is available for OpenGL. Only ever
// true on X11 (Linux/BSD); used for the banding-reduction option there.
bool DeepColor10Available();

// The current display's usable HDR peak in nits, for our luminance model (EDR
// headroom x SDR-reference white on macOS). 0 when unknown -- then callers keep
// their fixed peak default and slider range. Queried live; depends on the
// reference-white setting. Currently non-zero only on macOS.
uint32_t DisplayPeakNits();

// True if the display supports HDR but the user has it switched off in Windows
// display settings (so HdrAvailable() is false even though the hardware is
// capable). Lets the dialog suppress the "HDR not available" warning when HDR
// is merely turned off and could just be enabled, rather than truly absent.
// Queried live. Always false on non-Windows platforms.
bool WindowsHdrDisabled();

// macOS equivalent of WindowsHdrDisabled(): true if a connected display is
// HDR-capable (per its EDID) but the System Settings "High Dynamic Range" toggle
// is off, so HdrAvailable() is false only because HDR is switched off. Queried
// live. Always false on non-macOS platforms.
bool MacosHdrDisabled();

// Set by the platform probe (DetectAvailability); not for general use.
void SetAvailability(bool hdr_available, bool deep10_available);

// (Re)build the internal lookup tables from `s`. Cheap (does the pow()/PQ math
// over 256 entries); call whenever the HDR options change, not per frame.
void Configure(const Settings& s);

const Settings& CurrentSettings();

// Encode `rows` scanlines of `row_pixels` pixels each.
//
// `src` points at tightly-readable RGBA8 pixels (R in the lowest address byte,
// matching the wx 32-bit todraw buffer and GL_RGBA/GL_UNSIGNED_BYTE), with
// `src_stride` bytes between scanlines. `dst` receives the encoded pixels with
// `dst_stride` bytes between scanlines.
//
// EncodePQ10  writes one uint32_t per pixel (A2B10G10R10).
// EncodeScRGBFp16 writes four uint16_t (halfs) per pixel.
void EncodePQ10(const uint8_t* src, int src_stride, int row_pixels, int rows,
                uint32_t* dst, int dst_stride);
void EncodeScRGBFp16(const uint8_t* src, int src_stride, int row_pixels, int rows,
                     uint16_t* dst, int dst_stride);

}  // namespace hdr

#endif  // VBAM_WX_HDR_H_

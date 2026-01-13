// ScaleFX Filter - Main Entry Points
// CPU implementation of the ScaleFX shader algorithm
// Supports 3x and 9x scaling with optional reverse-AA

#include "scalefx.h"
#include "scalefx_manager.h"
#include "scalefx_simd.h"
#include <cstring>

namespace scalefx {

// Forward declarations for pass functions
void Pass0(const uint32_t* src, int srcPitch, float* dst, int width, int height);
void Pass1(const float* pass0, float* dst, int width, int height, float sfx_clr, int sfx_saa);
void Pass2(const float* pass0, const float* pass1, float* dst, int width, int height);
void Pass3(const float* pass2, float* dst, int width, int height, int sfx_scn);
void Pass4(const uint32_t* src, int srcPitch, const float* pass3,
           uint32_t* dst, int dstPitch, int width, int height,
           float sfx_raa, bool hybrid_mode);
void Pass4_9x(const uint32_t* src, int srcPitch, const float* pass3,
              uint32_t* dst, int dstPitch, int width, int height,
              float sfx_raa, bool hybrid_mode);

// Run the complete ScaleFX pipeline (3x output)
static void RunPipeline3x(const uint32_t* src, int srcPitch,
                          uint32_t* dst, int dstPitch,
                          int width, int height) {
    auto& manager = ScaleFXManager::Instance();

    // Ensure buffers are allocated
    if (!manager.IsInitialized(width, height)) {
        manager.Init(width, height);
    }

    auto buffers = manager.GetBuffers();
    const auto& config = manager.GetConfig();

    // Pass 0: Edge distance metrics
    Pass0(src, srcPitch, buffers.pass0, width, height);

    // Pass 1: Corner strength calculation
    Pass1(buffers.pass0, buffers.pass1, width, height, config.sfx_clr, config.sfx_saa);

    // Pass 2: Junction conflict resolution
    Pass2(buffers.pass0, buffers.pass1, buffers.pass2, width, height);

    // Pass 3: Edge level determination
    Pass3(buffers.pass2, buffers.pass3, width, height, config.sfx_scn);

    // Pass 4: Final 3x output with optional rAA
    Pass4(src, srcPitch, buffers.pass3, dst, dstPitch, width, height,
          config.sfx_raa, config.hybrid_mode);
}

// Run the ScaleFX pipeline with native 9x output
static void RunPipeline9x(const uint32_t* src, int srcPitch,
                          uint32_t* dst, int dstPitch,
                          int width, int height) {
    auto& manager = ScaleFXManager::Instance();

    // Ensure buffers are allocated
    if (!manager.IsInitialized(width, height)) {
        manager.Init(width, height);
    }

    auto buffers = manager.GetBuffers();
    const auto& config = manager.GetConfig();

    // Pass 0: Edge distance metrics
    Pass0(src, srcPitch, buffers.pass0, width, height);

    // Pass 1: Corner strength calculation
    Pass1(buffers.pass0, buffers.pass1, width, height, config.sfx_clr, config.sfx_saa);

    // Pass 2: Junction conflict resolution
    Pass2(buffers.pass0, buffers.pass1, buffers.pass2, width, height);

    // Pass 3: Edge level determination
    Pass3(buffers.pass2, buffers.pass3, width, height, config.sfx_scn);

    // Pass 4: Final 9x output with interpolation between edge-detected samples
    Pass4_9x(src, srcPitch, buffers.pass3, dst, dstPitch, width, height,
             config.sfx_raa, config.hybrid_mode);
}

} // namespace scalefx

// Public API implementations

void scalefx_init(int srcWidth, int srcHeight) {
    scalefx::ScaleFXManager::Instance().Init(srcWidth, srcHeight);
}

void scalefx_cleanup() {
    scalefx::ScaleFXManager::Instance().Cleanup();
}

// ScaleFX 3x filter
void scalefx3x32(uint8_t* src, uint32_t srcPitch, uint8_t* /*delta*/,
                 uint8_t* dst, uint32_t dstPitch, int width, int height) {
    scalefx::RunPipeline3x(
        reinterpret_cast<const uint32_t*>(src), static_cast<int>(srcPitch),
        reinterpret_cast<uint32_t*>(dst), static_cast<int>(dstPitch),
        width, height);
}

// ScaleFX 9x filter (native single-pass 9x with interpolation)
void scalefx9x32(uint8_t* src, uint32_t srcPitch, uint8_t* /*delta*/,
                 uint8_t* dst, uint32_t dstPitch, int width, int height) {
    scalefx::RunPipeline9x(
        reinterpret_cast<const uint32_t*>(src), static_cast<int>(srcPitch),
        reinterpret_cast<uint32_t*>(dst), static_cast<int>(dstPitch),
        width, height);
}

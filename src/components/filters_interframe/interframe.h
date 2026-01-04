/// Interframe blending filters
/// Thread-safe per-region buffering for multithreaded FilterThread

#ifndef VBAM_COMPONENTS_FILTERS_INTERFRAME_INTERFRAME_H_
#define VBAM_COMPONENTS_FILTERS_INTERFRAME_INTERFRAME_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

extern int RGB_LOW_BITS_MASK;

// ============================================================================
// InterframeManager - Thread-safe IFB with per-region buffering
// ============================================================================

/// Thread-safe interframe blending manager.
/// Each region maintains its own frame history buffers, eliminating race
/// conditions when multiple threads process different regions simultaneously.
class InterframeManager {
public:
    /// Get singleton instance
    static InterframeManager& Instance();

    /// Initialize buffers for the given dimensions and region count.
    /// Call before using ApplySmartIBRegion/ApplyMotionBlurRegion.
    /// @param width Frame width in pixels
    /// @param height Frame height in pixels
    /// @param pitch Row stride in bytes
    /// @param numRegions Number of parallel regions (typically number of threads)
    void Init(int width, int height, int pitch, int numRegions);

    /// Free all buffers and reset state
    void Cleanup();

    /// Clear existing buffers for reuse
    void Clear();

    /// Apply Smart IFB to a specific region.
    /// Each region has independent frame history - no synchronization needed.
    /// @param srcPtr Source/destination buffer (modified in-place)
    /// @param srcPitch Row stride in bytes
    /// @param width Frame width in pixels
    /// @param starty Starting Y coordinate for this region
    /// @param height Height of this region in pixels
    /// @param colorDepth Bits per pixel (8, 16, 24, or 32)
    /// @param regionId Region index (0 to numRegions-1)
    void ApplySmartIBRegion(uint8_t* srcPtr, uint32_t srcPitch,
                            int width, int starty, int height,
                            int colorDepth, int regionId);

    /// Apply Motion Blur IFB to a specific region.
    /// Each region has independent frame history - no synchronization needed.
    /// @param srcPtr Source/destination buffer (modified in-place)
    /// @param srcPitch Row stride in bytes
    /// @param width Frame width in pixels
    /// @param starty Starting Y coordinate for this region
    /// @param height Height of this region in pixels
    /// @param colorDepth Bits per pixel (8, 16, 24, or 32)
    /// @param regionId Region index (0 to numRegions-1)
    void ApplyMotionBlurRegion(uint8_t* srcPtr, uint32_t srcPitch,
                               int width, int starty, int height,
                               int colorDepth, int regionId);

private:
    InterframeManager() = default;
    ~InterframeManager() = default;
    InterframeManager(const InterframeManager&) = delete;
    InterframeManager& operator=(const InterframeManager&) = delete;

    /// Internal cleanup without locking (caller must hold init_mutex_)
    void CleanupInternal();

    struct RegionState {
        std::unique_ptr<uint8_t[]> frame1_;  // 1 frame ago
        std::unique_ptr<uint8_t[]> frame2_;  // 2 frames ago
        std::unique_ptr<uint8_t[]> frame3_;  // 3 frames ago
    };

    std::vector<RegionState> regions_;
    std::mutex init_mutex_;
    int width_ = 0;
    int height_ = 0;
    int pitch_ = 0;
    size_t buffer_size_ = 0;
    bool initialized_ = false;
};

// ============================================================================
// Legacy API (for SDL/libretro ports - single-threaded)
// ============================================================================

void InterframeFilterInit();

/// Call to reset frame history (e.g., when starting a new game)
void InterframeCleanup();

/// Clear frame history buffers without deallocating
void InterframeClear();

// Smart Interframe Blending - all are MMX/SSE2-accelerated if available
void SmartIB(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height);
void SmartIB8(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height);
void SmartIB24(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height);
void SmartIB32(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height);

void MotionBlurIB(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height);
void MotionBlurIB8(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height);
void MotionBlurIB24(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height);
void MotionBlurIB32(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height);

// Convenience overloads with starty=0
void SmartIB(uint8_t* srcPtr, uint32_t srcPitch, int width, int height);
void SmartIB8(uint8_t* srcPtr, uint32_t srcPitch, int width, int height);
void SmartIB24(uint8_t* srcPtr, uint32_t srcPitch, int width, int height);
void SmartIB32(uint8_t* srcPtr, uint32_t srcPitch, int width, int height);

void MotionBlurIB(uint8_t* srcPtr, uint32_t srcPitch, int width, int height);
void MotionBlurIB8(uint8_t* srcPtr, uint32_t srcPitch, int width, int height);
void MotionBlurIB24(uint8_t* srcPtr, uint32_t srcPitch, int width, int height);
void MotionBlurIB32(uint8_t* srcPtr, uint32_t srcPitch, int width, int height);

#endif  // VBAM_COMPONENTS_FILTERS_INTERFRAME_INTERFRAME_H_

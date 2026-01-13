// ScaleFX Buffer Manager
// Thread-local pattern for persistent intermediate buffer allocation
// Each thread gets its own instance to enable safe parallel processing

#ifndef SCALEFX_MANAGER_H
#define SCALEFX_MANAGER_H

#include <cstdint>
#include <memory>

namespace scalefx {

class ScaleFXManager {
public:
    // Get thread-local instance (each thread has its own buffers)
    static ScaleFXManager& Instance();

    // Initialize buffers for given source dimensions
    // Must be called before using ScaleFX filters
    void Init(int srcWidth, int srcHeight);

    // Free all allocated buffers
    void Cleanup();

    // Check if initialized with sufficient size
    bool IsInitialized(int width, int height) const;

    // Intermediate pass buffers (4 floats per pixel: R, G, B, A or metrics)
    struct PassBuffers {
        float* pass0;  // Edge distance metrics
        float* pass1;  // Corner strengths
        float* pass2;  // Junction resolution data
        float* pass3;  // Edge level / subpixel tags
    };

    // Get buffer pointers for processing
    PassBuffers GetBuffers();

    // Configuration parameters (matching GLSL shader uniforms)
    struct Config {
        float sfx_clr = 0.50f;   // Clarity threshold (0.01-1.0, default 0.50)
        int sfx_saa = 1;         // Smooth anti-aliasing toggle (0 or 1)
        int sfx_scn = 1;         // Edge scan mode toggle (0 or 1)
        float sfx_raa = 2.0f;    // Reverse-AA sharpness (0.0-10.0, default 2.0)
        bool hybrid_mode = true; // Enable rAA hybrid mode
    };

    Config& GetConfig() { return config_; }
    const Config& GetConfig() const { return config_; }

    // Get current dimensions
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    ScaleFXManager() = default;
    ~ScaleFXManager();

    // Non-copyable
    ScaleFXManager(const ScaleFXManager&) = delete;
    ScaleFXManager& operator=(const ScaleFXManager&) = delete;

    // Allocate a single pass buffer
    void AllocateBuffer(std::unique_ptr<float[]>& buffer, size_t size);

    // Buffer storage
    std::unique_ptr<float[]> pass0_buf_;
    std::unique_ptr<float[]> pass1_buf_;
    std::unique_ptr<float[]> pass2_buf_;
    std::unique_ptr<float[]> pass3_buf_;

    // Current dimensions
    int width_ = 0;
    int height_ = 0;
    size_t buffer_size_ = 0;  // Total floats per buffer (width * height * 4)
    bool initialized_ = false;

    // Configuration
    Config config_;
};

} // namespace scalefx

#endif // SCALEFX_MANAGER_H

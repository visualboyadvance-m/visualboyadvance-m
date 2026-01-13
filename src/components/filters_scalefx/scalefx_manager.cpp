// ScaleFX Buffer Manager Implementation

#include "scalefx_manager.h"
#include <cstring>

namespace scalefx {

ScaleFXManager& ScaleFXManager::Instance() {
    // Thread-local instance ensures each thread has its own buffers
    // This enables safe parallel processing without data races
    thread_local ScaleFXManager instance;
    return instance;
}

ScaleFXManager::~ScaleFXManager() {
    Cleanup();
}

void ScaleFXManager::Init(int srcWidth, int srcHeight) {
    // Check if already initialized with same or larger size
    if (initialized_ && width_ >= srcWidth && height_ >= srcHeight) {
        return;
    }

    // Clean up existing buffers if resizing
    if (initialized_) {
        pass0_buf_.reset();
        pass1_buf_.reset();
        pass2_buf_.reset();
        pass3_buf_.reset();
    }

    width_ = srcWidth;
    height_ = srcHeight;

    // Each buffer stores 4 floats per pixel (like RGBA or 4 metric values)
    buffer_size_ = static_cast<size_t>(width_) * height_ * 4;

    // Allocate all intermediate buffers
    AllocateBuffer(pass0_buf_, buffer_size_);
    AllocateBuffer(pass1_buf_, buffer_size_);
    AllocateBuffer(pass2_buf_, buffer_size_);
    AllocateBuffer(pass3_buf_, buffer_size_);

    // Zero-initialize buffers
    std::memset(pass0_buf_.get(), 0, buffer_size_ * sizeof(float));
    std::memset(pass1_buf_.get(), 0, buffer_size_ * sizeof(float));
    std::memset(pass2_buf_.get(), 0, buffer_size_ * sizeof(float));
    std::memset(pass3_buf_.get(), 0, buffer_size_ * sizeof(float));

    initialized_ = true;
}

void ScaleFXManager::Cleanup() {
    pass0_buf_.reset();
    pass1_buf_.reset();
    pass2_buf_.reset();
    pass3_buf_.reset();

    width_ = 0;
    height_ = 0;
    buffer_size_ = 0;
    initialized_ = false;
}

bool ScaleFXManager::IsInitialized(int width, int height) const {
    return initialized_ && width_ >= width && height_ >= height;
}

ScaleFXManager::PassBuffers ScaleFXManager::GetBuffers() {
    return PassBuffers{
        pass0_buf_.get(),
        pass1_buf_.get(),
        pass2_buf_.get(),
        pass3_buf_.get()
    };
}

void ScaleFXManager::AllocateBuffer(std::unique_ptr<float[]>& buffer, size_t size) {
    buffer = std::make_unique<float[]>(size);
}

} // namespace scalefx

#ifndef VBAM_COMPONENTS_FILTERS_DLSSNR_DLSSNR_H_
#define VBAM_COMPONENTS_FILTERS_DLSSNR_DLSSNR_H_

// DLSS NR: the recovered DLSS 5 neural-rendering graph as a VBA-M display
// filter, through libdlssnr (third_party/dlss-nr-on-vulkan): the nr_frame C API
// with libxmx, the weights and the shaders linked in statically.
//
// The filter keeps the source resolution (scale 1x): one RGB frame in, one RGB
// frame out. A forward pass takes hundreds of milliseconds even on a fast GPU
// (the network extent is at least 320x320), so the work is asynchronous. The
// panel creates a Filter when the option is selected (the "initializer"); the
// filter thread then calls Apply32() every frame, which hands the newest source
// frame to a worker thread and writes back the most recent finished result,
// or passes the source through until the first result exists.
//
// Each frame gets what the `nr_frame` command gives a picture with no flags:
// the standard profile, frame index 0, no control mask, and the still path
// with no history, so a frame's output depends on that frame alone and matches
// `nr_frame IN.png OUT.png` on the same pixels.
//
// The weights are shared process-wide: the model opens on the first pass a
// Filter asks for (about two seconds, on the worker, never on the UI thread)
// and closes when the last Filter goes. All libnr_frame calls are serialized,
// since libxmx is one device.
//
// A Vulkan renderer can lend its instance and device (ShareVulkan): the model
// then runs on the renderer's device and a queue the renderer hands over,
// instead of libxmx opening a second instance on the same GPU. The lender must
// withdraw the share (WithdrawVulkanShare) before destroying its device; that
// closes the model, and the next pass reopens it standalone or on whatever is
// shared by then.
//
// The header is always includable; the implementation is compiled only when the
// dlss-nr-on-vulkan tree is part of the build (VBAM_ENABLE_DLSS_NR is defined
// for every target that links vbam-components-filters-dlssnr).

#include <cstdint>
#include <memory>
#include <string>

namespace dlssnr {

// True when the filter is compiled into this build.
inline constexpr bool Available() {
#ifdef VBAM_ENABLE_DLSS_NR
    return true;
#else
    return false;
#endif
}

// Vulkan objects a renderer lends to the model. Opaque pointers, so this header
// needs no Vulkan include: VkInstance, VkPhysicalDevice, VkDevice, VkQueue.
struct VulkanShare {
    void* instance = nullptr;
    void* physical_device = nullptr;
    void* device = nullptr;
    // A compute-capable queue of `queue_family`. If the renderer also submits on
    // it, `lock`/`unlock` bracket every submit libxmx makes and the renderer
    // takes the same lock around its own submits, presents and idle waits.
    void* queue = nullptr;
    uint32_t queue_family = 0;
    // Whether VK_KHR_cooperative_matrix was enabled on `device`.
    bool cooperative_matrix = false;
    // The renderer's vkGetInstanceProcAddr: the library that made `instance`.
    void* get_instance_proc_addr = nullptr;
    void (*lock)(void*) = nullptr;
    void (*unlock)(void*) = nullptr;
    void* lock_context = nullptr;
    // Identifies the lender, for WithdrawVulkanShare().
    const void* owner = nullptr;
};

// Registers the device the model should run on from its next open. A model
// already open is closed (waiting for a pass in flight), so the next pass
// reopens it on the shared device. The device must satisfy the requirements
// documented for nr_frame_adopt_vulkan(); if adopting fails, the model falls
// back to its own instance and Filter::Device() says so.
void ShareVulkan(const VulkanShare& share);

// Drops the share registered by `owner` (a no-op for anyone else), closing the
// model first if it runs on that device. Call before destroying the device.
void WithdrawVulkanShare(const void* owner);

// True while the open model runs on a lent device.
bool UsingSharedVulkan();

class Filter {
public:
    // Starts the worker thread, which opens (or shares) the model.
    Filter();
    // Stops the worker and releases the model. Blocks while a pass in flight
    // finishes.
    ~Filter();

    Filter(const Filter&) = delete;
    Filter& operator=(const Filter&) = delete;

    // Filters one 32-bit frame at scale 1. `src` and `dst` point at the first
    // data pixel of `height` rows, `instride`/`outstride` bytes apart; the
    // 8-bit channels sit at bit positions `red_shift`, `green_shift` and
    // `blue_shift` of each uint32 (VBA-M's systemRedShift - 3 and friends).
    // Writes the newest finished result when it matches the frame size,
    // otherwise copies the source through. Never blocks on the network.
    void Apply32(const uint8_t* src, int instride, uint8_t* dst, int outstride,
                 int width, int height, int red_shift, int green_shift, int blue_shift);

    // The model opened and at least one frame can be processed.
    bool Ready() const;
    // Opening the model or a pass failed; Error() says why. Apply32() then
    // passes the source through, and the panel drops back to no filter.
    bool Failed() const;
    std::string Error() const;

    // The Vulkan device name and GEMM path reported by libnr_frame, once Ready().
    std::string Device() const;
    // Wall-clock time of the most recent pass, in milliseconds (0 before one).
    double LastFrameMs() const;
    // Number of finished passes.
    uint64_t FramesDone() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace dlssnr

#endif  // VBAM_COMPONENTS_FILTERS_DLSSNR_DLSSNR_H_

#ifndef VBAM_COMPONENTS_FILTERS_DLSSNR_DLSSNR_H_
#define VBAM_COMPONENTS_FILTERS_DLSSNR_DLSSNR_H_

// DLSS NR: the recovered DLSS 5 neural-rendering graph as a VBA-M display
// filter, through libdlssnr (third_party/dlss-nr-on-vulkan): the nr_frame C API
// with libxmx, the weights and the shaders linked in statically.
//
// The filter keeps the source resolution (scale 1x): one RGB frame in, one RGB
// frame out, driven the way PCSX2's GSDLSSNR drives libframe, which is the way
// src/ref/nr_frame_main.c does: the same parameters (the profile, then the
// explicit overrides -- Settings is nr_frame's command line), the optional
// control mask, the features, the network at the vendor's extent, the head
// cropped, then the composition. Without history a pass's output is
// nr_frame's for the same picture, byte for byte. With history the previous
// output (and, for the composition's floor, the previous input) goes into the
// features and the composition as well.
//
// Frames taller than Settings::max_height are scaled down (bilinear) before
// the model runs and the result is scaled back up (bilinear), as PCSX2 does on
// the GPU, so the cost of a pass is bounded whatever size the frame is.
//
// The three steps run on three threads per Filter (features, network,
// composition), so the caller never waits. With history a frame's features
// wait for the previous frame's composition, so only one frame is in the
// network or composition at a time; without it up to three frames are in
// flight. Frames arriving while the pipeline is full are skipped, and
// Apply32() writes the newest finished result, a few frames behind the
// emulator. Until the first pass finishes the source passes straight through.
//
// The weights are shared process-wide: the model opens on the first pass a
// Filter asks for (about two seconds, on the features thread, never on the UI
// thread) and closes when the last Filter goes. libxmx is one device, so one
// Filter's frames are in the model at a time; another Filter's wait for them.
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
#include <vector>

namespace dlssnr {

// Values for the kDispDlssNrStage option: whether the pass runs before the
// display filter, at the source resolution; after it, over the filter's scaled
// output; or at display size, over the filter's output scaled up (nearest
// neighbour) by the largest whole factor that fits the panel in physical
// pixels, so the network sees the picture as it is shown -- what `nr_frame`
// gets from a screenshot of the game area -- and the renderer draws it 1:1.
enum Stage : uint32_t {
    kBeforeFilter = 0,
    kAfterFilter = 1,
    kAtDisplay = 2,
};

// nr_frame's --profile values, in the order of its PROFILES table.
enum Profile : uint32_t {
    kProfileStandard = 0,
    kProfileNatural = 1,
    kProfileCinematic = 2,
    kProfileNeutral = 3,
    kProfileVendor = 4,
    kProfileCount
};

// nr_frame's command line, as settings (PCSX2's GSDLSSNR::Settings).
struct Settings {
    float intensity = 1.0f;          // --intensity
    uint32_t profile = kProfileStandard;  // --profile
    int style_index = -1;            // --style-index, negative keeps the profile's
    float local_tone = -1.0f;        // --local-tone, negative keeps the profile's
    float local_structure = -1.0f;   // --local-structure, negative keeps the profile's
    float skin_structure = -1.0f;    // --skin-structure, negative leaves it unset
    float auto_mask = -1.0f;         // --auto-mask, negative leaves it unset
    float detail_strength = 1.0f;    // --detail-strength
    float colour_strength = 1.0f;    // --colour-strength
    float detail_radius = 4.0f;      // --detail-radius
    int frame_index = 0;             // --frame-index
    std::string control_mask;        // --control-mask, a PNG, resized to the frame
    bool history = true;             // not nr_frame's: feed the previous output back in
    int max_height = 480;            // frames taller than this are filtered scaled down; 0 never
};

// Reads the control mask for Settings::control_mask: `rgb` receives the
// picture as 8-bit RGB, (height, width, 3). The component has no image
// library of its own, so the frontend registers one (wxImage, QImage). Called
// from the features thread. Without a loader the mask is ignored.
using ImageLoader = bool (*)(const std::string& path, int* width, int* height,
                             std::vector<uint8_t>* rgb);
void SetImageLoader(ImageLoader loader);

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
    // Whether VK_KHR_workgroup_memory_explicit_layout was enabled with its
    // scalar-block-layout and 16-bit-access features. The model's staged GEMM
    // needs it; without it those shapes run on the smaller kernels.
    bool workgroup_memory_explicit_layout = false;
    // The renderer's vkGetInstanceProcAddr: the library that made `instance`.
    void* get_instance_proc_addr = nullptr;
    void (*lock)(void*) = nullptr;
    void (*unlock)(void*) = nullptr;
    void* lock_context = nullptr;
    // Identifies the lender, for WithdrawVulkanShare().
    const void* owner = nullptr;
};

// Registers the device the model should run on from its next open, and does
// not block: a renderer lends its device while it is being constructed, on the
// UI thread. A model already open predates the share, so the pipeline closes
// it and reopens on the lent device at its next frame. The device
// must satisfy the requirements documented for nr_frame_adopt_vulkan(); if
// adopting fails, the model falls back to its own instance and
// Filter::Device() says so.
void ShareVulkan(const VulkanShare& share);

// Drops the share registered by `owner` (a no-op, and immediate, for anyone
// else), closing the model first if it runs on that device. Call before
// destroying the device.
//
// This is the one call here that waits, and it has to: the model may be on
// that device at this instant, and neither a pass nor a model open can be
// interrupted. Everything else is arranged so the wait is rare -- only the
// renderer that actually lent the device reaches it.
void WithdrawVulkanShare(const void* owner);

// True while the open model runs on a lent device.
bool UsingSharedVulkan();

class Filter {
public:
    // Starts the stage threads, the first of which opens (or shares) the model.
    Filter();
    // Retires the stages and lets go of the model. Does not block. A frame, or
    // a model open, already running is left to finish on its own stage thread;
    // the last stage to go releases the model and disposes of the state behind
    // this object. Opening the model compiles every compute pipeline, which
    // some drivers take many seconds over and nothing can interrupt, so
    // waiting for it here would freeze whichever thread destroys a panel --
    // the UI thread, every time.
    //
    // A renderer that lent its device must still call WithdrawVulkanShare()
    // before destroying it: that one does wait, because a retiring stage may
    // be using the device at this instant. It waits only for work already
    // begun -- a stage asked to quit starts no more.
    ~Filter();

    Filter(const Filter&) = delete;
    Filter& operator=(const Filter&) = delete;

    // Filters one 32-bit frame at scale 1. `src` and `dst` point at the first
    // data pixel of `height` rows, `instride`/`outstride` bytes apart; the
    // 8-bit channels sit at bit positions `red_shift`, `green_shift` and
    // `blue_shift` of each uint32 (VBA-M's systemRedShift - 3 and friends).
    // Hands the frame to the pipeline with `settings` if it has room, then
    // writes the newest finished result when it matches the frame size,
    // otherwise copies the source through. Never blocks on the network.
    void Apply32(const uint8_t* src, int instride, uint8_t* dst, int outstride,
                 int width, int height, int red_shift, int green_shift, int blue_shift,
                 const Settings& settings);

    // Forgets the temporal history (the next pass starts afresh).
    void ResetHistory();

    // The model opened and at least one frame can be processed.
    bool Ready() const;
    // Opening the model or a pass failed; Error() says why. Apply32() then
    // passes the source through, and the panel drops back to no filter.
    bool Failed() const;
    std::string Error() const;

    // The Vulkan device name and GEMM path reported by libnr_frame, once Ready().
    std::string Device() const;
    // Wall-clock time of the most recent frame through the pipeline, from its
    // features to its composition, in milliseconds (0 before one).
    double LastFrameMs() const;
    // Number of finished passes.
    uint64_t FramesDone() const;

private:
    struct Impl;
    // Shared with the stage threads, which outlive this object when they are
    // retired mid-frame; the last one out drops the final reference.
    std::shared_ptr<Impl> impl_;
};

}  // namespace dlssnr

#endif  // VBAM_COMPONENTS_FILTERS_DLSSNR_DLSSNR_H_

#ifndef VBAM_QT_RENDERERS_VULKAN_PANEL_H_
#define VBAM_QT_RENDERERS_VULKAN_PANEL_H_

#include "qt/drawing-panel.h"

#ifndef NO_VULKAN

#include <cstdint>
#include <mutex>
#include <vector>

#include <QSize>

// The Vulkan library is never linked: every entry point is resolved at run
// time through vkGetInstanceProcAddr from the system loader (vulkan-1.dll,
// libvulkan.so.1, libvulkan.dylib / libMoltenVK.dylib), so the renderer is
// simply reported unavailable on machines without Vulkan.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// True if the Vulkan loader could be loaded at run time (cached). GameArea's
// renderer fallback skips Vulkan when this is false.
bool VbamQtVulkanRuntimeAvailable();

// Vulkan renderer presenting into this panel's native window: Win32 surface
// on Windows, VK_EXT_metal_surface (MoltenVK) on macOS, xcb / Xlib surface on
// X11, and on Android VK_KHR_android_surface on an overlay SurfaceView hosted
// in the Qt activity (see android-compat.h) -- Qt exposes no ANativeWindow for a
// QWindow. Since that SurfaceView covers the Qt content, the on-screen
// controller is composited into the frame here on Android, as the wx port
// does. Port of the wx VKDrawingPanel without its HDR / deep-color and Wayland
// subsurface paths.
class VKDrawingPanel final : public NativeDrawingPanel {
    Q_OBJECT

public:
    VKDrawingPanel(QWidget* parent, int _width, int _height);
    ~VKDrawingPanel() override;

protected:
    void DrawingPanelInit() override;
    void Present() override;
    void OnNativeResize(const QSize& device_pixels) override;

private:
    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();

    // ── DLSS NR: one Vulkan instance for the renderer and the model ─────────
    // libxmx (the model's compute runtime) runs on this panel's instance and
    // device instead of opening a second one (see the wx port for the full
    // note): CreateInstance() asks for Vulkan 1.3, CreateLogicalDevice()
    // enables the features the graph needs and takes a compute queue for it,
    // ShareVulkanWithDlssNr() hands them over, and the destructor withdraws
    // them (closing the model) before the device goes. queue_mutex_ brackets
    // every submit, present and idle wait on our queues; libxmx takes it too.
    void ShareVulkanWithDlssNr();
    void WithdrawVulkanFromDlssNr();
    static void LockQueueThunk(void* self);
    static void UnlockQueueThunk(void* self);

    bool CreateSwapchain();
    void DestroySwapchain();
    bool RecreateSwapchain();  // on resize / VK_ERROR_OUT_OF_DATE_KHR

    bool CreateImageViews();
    bool CreateRenderPass();
    bool CreateFramebuffers();

    bool CreateDescriptorSetLayout();
    bool CreateGraphicsPipeline();

    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();

    bool CreateTexture(uint32_t tex_w, uint32_t tex_h, VkFormat fmt);
    void DestroyTexture();
    bool CreateDescriptorPoolAndSet();

    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags props) const;

    // The swapchain is created with the surface's currentTransform (when the
    // driver supports taking it back) and the quad is rotated to match; see
    // PreTransformMatrix(). On Android this is a real 90/180/270 rotation
    // whenever the window orientation differs from the panel's natural one;
    // desktop surfaces report identity.
    VkSurfaceTransformFlagBitsKHR swapchain_pre_transform_ =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    // Row-major 2x2 matrix mapping display-oriented NDC into the swapchain's
    // pre-transformed space: pos' = (m[0]*x + m[1]*y, m[2]*x + m[3]*y).
    void PreTransformMatrix(float out_mat[4]) const;

#if defined(Q_OS_ANDROID)
    // Glues the overlay SurfaceView to the panel's current on-screen rect.
    void SyncAndroidOverlayGeometry();
    // Renders the on-screen controller into osc_image_ and records its upload
    // into `cmd`. Returns true if the overlay is to be drawn this frame.
    bool UpdateOverlayTexture(VkCommandBuffer cmd);
    void DestroyOverlayTexture();

    void* android_window_ = nullptr;  // ANativeWindow*, one reference owned here
    QSize android_overlay_size_;

    // On-screen controller overlay: blended pipeline, its descriptor set and
    // the texture the widget is rendered into.
    VkPipeline osc_pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSet osc_desc_set_ = VK_NULL_HANDLE;
    VkImage osc_image_ = VK_NULL_HANDLE;
    VkDeviceMemory osc_memory_ = VK_NULL_HANDLE;
    VkImageView osc_view_ = VK_NULL_HANDLE;
    VkSampler osc_sampler_ = VK_NULL_HANDLE;
    VkBuffer osc_staging_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory osc_staging_memory_ = VK_NULL_HANDLE;
    VkDeviceSize osc_staging_size_ = 0;
    uint32_t osc_width_ = 0;
    uint32_t osc_height_ = 0;
    uint32_t osc_revision_ = 0;
#endif

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;

    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_family_ = UINT32_MAX;
    uint32_t present_family_ = UINT32_MAX;

    uint32_t instance_api_version_ = 0;         // what CreateInstance asked for
    std::mutex queue_mutex_;                     // see the DLSS NR block above
    VkQueue compute_queue_ = VK_NULL_HANDLE;     // lent to libxmx
    uint32_t compute_family_ = UINT32_MAX;
    uint32_t compute_queue_index_ = 0;
    bool compute_is_graphics_queue_ = false;
    bool dlssnr_share_ok_ = false;               // device has what libxmx needs
    bool dlssnr_coopmat_ = false;                // VK_KHR_cooperative_matrix enabled
    bool dlssnr_explicit_layout_ = false;        // VK_KHR_workgroup_memory_explicit_layout enabled
    bool dlssnr_shared_ = false;                 // we registered a share

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent_ = {};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_views_;
    std::vector<VkFramebuffer> framebuffers_;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;

    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet desc_set_ = VK_NULL_HANDLE;

    VkCommandPool cmd_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmd_buffers_;

    static constexpr int kMaxFramesInFlight = 2;
    VkSemaphore image_available_sem_[kMaxFramesInFlight] = {};
    VkSemaphore render_finished_sem_[kMaxFramesInFlight] = {};
    VkFence in_flight_fence_[kMaxFramesInFlight] = {};
    int current_frame_ = 0;

    VkImage tex_image_ = VK_NULL_HANDLE;
    VkDeviceMemory tex_memory_ = VK_NULL_HANDLE;
    VkImageView tex_view_ = VK_NULL_HANDLE;
    VkSampler tex_sampler_ = VK_NULL_HANDLE;
    VkFormat tex_format_ = VK_FORMAT_UNDEFINED;

    VkBuffer staging_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory_ = VK_NULL_HANDLE;
    VkDeviceSize staging_size_ = 0;

    uint32_t texture_width_ = 0;
    uint32_t texture_height_ = 0;
    bool vsync_ = false;
    bool swapchain_dirty_ = false;

    void* metal_layer_ = nullptr;  // CAMetalLayer* (macOS)

    static const uint32_t kVertSpv[];
    static const size_t kVertSpvSize;
    static const uint32_t kFragSpv[];
    static const size_t kFragSpvSize;
};

#endif  // !NO_VULKAN

#endif  // VBAM_QT_RENDERERS_VULKAN_PANEL_H_

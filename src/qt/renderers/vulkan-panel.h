#ifndef VBAM_QT_RENDERERS_VULKAN_PANEL_H_
#define VBAM_QT_RENDERERS_VULKAN_PANEL_H_

#include "qt/drawing-panel.h"

#ifndef NO_VULKAN

#include <cstdint>
#include <vector>

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
// X11. Port of the wx VKDrawingPanel without its HDR / deep-color, Wayland
// subsurface and Android overlay paths.
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

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;

    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_family_ = UINT32_MAX;
    uint32_t present_family_ = UINT32_MAX;

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

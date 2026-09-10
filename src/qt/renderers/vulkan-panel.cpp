#include "qt/renderers/vulkan-panel.h"

#ifndef NO_VULKAN

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLibrary>
#include <QStringList>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(Q_OS_MACOS)
#include <vulkan/vulkan_metal.h>
#include "qt/renderers/mac-support.h"
#elif defined(Q_OS_ANDROID)
#include <android/log.h>
#include <android/native_window.h>
#include <vulkan/vulkan_android.h>
#include "qt/android-compat.h"
#include "qt/app.h"
#include "qt/game-area.h"
#include "qt/main-window.h"
#include "qt/widgets/on-screen-controller.h"
#elif defined(Q_OS_UNIX)
#include <QtGui/qguiapplication_platform.h>
#if __has_include(<xcb/xcb.h>)
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#define VBAM_VK_HAVE_XCB 1
#endif
#if __has_include(<X11/Xlib.h>)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#define VBAM_VK_HAVE_XLIB 1
#endif
#endif

#include "core/base/system.h"
#include "qt/config/option-proxy.h"
#include "qt/log.h"

namespace {

QString Tr(const char* s) {
    return QCoreApplication::translate("vbam", s);
}

// ── Run-time Vulkan loader (volk-style) ──────────────────────────────────────
// vulkan.h is included with VK_NO_PROTOTYPES and every function we use is
// resolved from the loader library via vkGetInstanceProcAddr, so the loader is
// never a load-time dependency of the executable.

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

#define VBAM_VK_GLOBAL_FUNCS(F) \
    F(vkCreateInstance) \
    F(vkEnumerateInstanceExtensionProperties)

#define VBAM_VK_INSTANCE_FUNCS(F) \
    F(vkAcquireNextImageKHR) \
    F(vkAllocateCommandBuffers) \
    F(vkAllocateDescriptorSets) \
    F(vkAllocateMemory) \
    F(vkBeginCommandBuffer) \
    F(vkBindBufferMemory) \
    F(vkBindImageMemory) \
    F(vkCmdBeginRenderPass) \
    F(vkCmdBindDescriptorSets) \
    F(vkCmdBindPipeline) \
    F(vkCmdClearColorImage) \
    F(vkCmdCopyBufferToImage) \
    F(vkCmdDraw) \
    F(vkCmdEndRenderPass) \
    F(vkCmdPipelineBarrier) \
    F(vkCmdPushConstants) \
    F(vkCmdSetScissor) \
    F(vkCmdSetViewport) \
    F(vkCreateBuffer) \
    F(vkCreateCommandPool) \
    F(vkCreateDescriptorPool) \
    F(vkCreateDescriptorSetLayout) \
    F(vkCreateDevice) \
    F(vkCreateFence) \
    F(vkCreateFramebuffer) \
    F(vkCreateGraphicsPipelines) \
    F(vkCreateImage) \
    F(vkCreateImageView) \
    F(vkCreatePipelineLayout) \
    F(vkCreateRenderPass) \
    F(vkCreateSampler) \
    F(vkCreateSemaphore) \
    F(vkCreateShaderModule) \
    F(vkCreateSwapchainKHR) \
    F(vkDestroyBuffer) \
    F(vkDestroyCommandPool) \
    F(vkDestroyDescriptorPool) \
    F(vkDestroyDescriptorSetLayout) \
    F(vkDestroyDevice) \
    F(vkDestroyFence) \
    F(vkDestroyFramebuffer) \
    F(vkDestroyImage) \
    F(vkDestroyImageView) \
    F(vkDestroyInstance) \
    F(vkDestroyPipeline) \
    F(vkDestroyPipelineLayout) \
    F(vkDestroyRenderPass) \
    F(vkDestroySampler) \
    F(vkDestroySemaphore) \
    F(vkDestroyShaderModule) \
    F(vkDestroySurfaceKHR) \
    F(vkDestroySwapchainKHR) \
    F(vkDeviceWaitIdle) \
    F(vkEndCommandBuffer) \
    F(vkEnumerateDeviceExtensionProperties) \
    F(vkEnumeratePhysicalDevices) \
    F(vkFreeMemory) \
    F(vkGetBufferMemoryRequirements) \
    F(vkGetDeviceQueue) \
    F(vkGetImageMemoryRequirements) \
    F(vkGetPhysicalDeviceMemoryProperties) \
    F(vkGetPhysicalDeviceProperties) \
    F(vkGetPhysicalDeviceQueueFamilyProperties) \
    F(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    F(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    F(vkGetPhysicalDeviceSurfacePresentModesKHR) \
    F(vkGetPhysicalDeviceSurfaceSupportKHR) \
    F(vkGetSwapchainImagesKHR) \
    F(vkMapMemory) \
    F(vkQueuePresentKHR) \
    F(vkQueueSubmit) \
    F(vkResetCommandBuffer) \
    F(vkResetFences) \
    F(vkUnmapMemory) \
    F(vkUpdateDescriptorSets) \
    F(vkWaitForFences)

#if defined(Q_OS_WIN)
#define VBAM_VK_SURFACE_FUNCS(F) F(vkCreateWin32SurfaceKHR)
#elif defined(Q_OS_MACOS)
#define VBAM_VK_SURFACE_FUNCS(F) F(vkCreateMetalSurfaceEXT)
#elif defined(Q_OS_ANDROID)
#define VBAM_VK_SURFACE_FUNCS(F) F(vkCreateAndroidSurfaceKHR)
#else
#if defined(VBAM_VK_HAVE_XCB) && defined(VBAM_VK_HAVE_XLIB)
#define VBAM_VK_SURFACE_FUNCS(F) F(vkCreateXcbSurfaceKHR) F(vkCreateXlibSurfaceKHR)
#elif defined(VBAM_VK_HAVE_XCB)
#define VBAM_VK_SURFACE_FUNCS(F) F(vkCreateXcbSurfaceKHR)
#elif defined(VBAM_VK_HAVE_XLIB)
#define VBAM_VK_SURFACE_FUNCS(F) F(vkCreateXlibSurfaceKHR)
#else
#define VBAM_VK_SURFACE_FUNCS(F)
#endif
#endif

#define VBAM_VK_DECLARE(name) PFN_##name name = nullptr;
VBAM_VK_GLOBAL_FUNCS(VBAM_VK_DECLARE)
VBAM_VK_INSTANCE_FUNCS(VBAM_VK_DECLARE)
VBAM_VK_SURFACE_FUNCS(VBAM_VK_DECLARE)
#undef VBAM_VK_DECLARE

// Loads the loader library and the global-level functions. Cached.
bool VulkanBootstrap() {
    static int state = 0;  // 0 = untried, 1 = ok, -1 = failed
    if (state != 0)
        return state == 1;

    static QLibrary vklib;
    QStringList names;
#if defined(Q_OS_WIN)
    names << QStringLiteral("vulkan-1");
#elif defined(Q_OS_MACOS)
    // MoltenVK itself first: it is a complete Vulkan implementation with its
    // own vkGetInstanceProcAddr, and needs no ICD manifest -- the Khronos
    // loader only works when a MoltenVK_icd.json is installed where it looks,
    // which package-manager / vcpkg installs rarely provide.
#ifdef VBAM_VULKAN_LIBRARY_DIR
    // The library directory Vulkan was found in at configure time (a vcpkg
    // tree, the LunarG SDK, Homebrew), which is normally not on the dyld path.
    names << QStringLiteral(VBAM_VULKAN_LIBRARY_DIR "/libMoltenVK.dylib");
#endif
    names << QStringLiteral("MoltenVK") << QStringLiteral("libMoltenVK.dylib")
          << QStringLiteral("/usr/local/lib/libMoltenVK.dylib")
          << QStringLiteral("/opt/homebrew/lib/libMoltenVK.dylib");
#ifdef VBAM_VULKAN_LIBRARY_DIR
    names << QStringLiteral(VBAM_VULKAN_LIBRARY_DIR "/libvulkan.1.dylib")
          << QStringLiteral(VBAM_VULKAN_LIBRARY_DIR "/libvulkan.dylib");
#endif
    names << QStringLiteral("vulkan") << QStringLiteral("libvulkan.1.dylib")
          << QStringLiteral("/usr/local/lib/libvulkan.1.dylib")
          << QStringLiteral("/opt/homebrew/lib/libvulkan.1.dylib");
#elif defined(Q_OS_ANDROID)
    // The platform loader; every Vulkan-capable device ships it.
    names << QStringLiteral("libvulkan.so") << QStringLiteral("vulkan");
#else
    names << QStringLiteral("libvulkan.so.1") << QStringLiteral("libvulkan.so")
          << QStringLiteral("vulkan");
#endif
    for (const QString& n : names) {
        vklib.setFileName(n);
        if (vklib.load())
            break;
    }
    if (!vklib.isLoaded()) {
        vbam::LogDebug(QStringLiteral("Vulkan: no loader library found"));
        state = -1;
        return false;
    }
    vbam::LogDebug(QStringLiteral("Vulkan: using %1").arg(vklib.fileName()));

    vkGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(vklib.resolve("vkGetInstanceProcAddr"));
    if (!vkGetInstanceProcAddr) {
        state = -1;
        return false;
    }

#define VBAM_VK_LOAD_GLOBAL(name) \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(nullptr, #name)); \
    if (!name) { state = -1; return false; }
    VBAM_VK_GLOBAL_FUNCS(VBAM_VK_LOAD_GLOBAL)
#undef VBAM_VK_LOAD_GLOBAL

    state = 1;
    return true;
}

// Resolves the instance-/device-level functions once an instance exists.
void VulkanLoadInstanceFns(VkInstance instance) {
#define VBAM_VK_LOAD_INSTANCE(name) \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name));
    VBAM_VK_INSTANCE_FUNCS(VBAM_VK_LOAD_INSTANCE)
    VBAM_VK_SURFACE_FUNCS(VBAM_VK_LOAD_INSTANCE)
#undef VBAM_VK_LOAD_INSTANCE
}

bool HasInstanceExtension(const std::vector<VkExtensionProperties>& exts, const char* name) {
    for (const auto& e : exts)
        if (strcmp(e.extensionName, name) == 0)
            return true;
    return false;
}

}  // namespace

bool VbamQtVulkanRuntimeAvailable() {
    return VulkanBootstrap();
}

// ─── Constructor / destructor ────────────────────────────────────────────────

VKDrawingPanel::VKDrawingPanel(QWidget* parent, int _width, int _height)
    : NativeDrawingPanel(parent, _width, _height) {
    vsync_ = OPTION(kPrefVsync);

    const bool ok = CreateInstance() && CreateSurface() && PickPhysicalDevice() &&
                    CreateLogicalDevice() && CreateSwapchain() && CreateImageViews() &&
                    CreateRenderPass() && CreateDescriptorSetLayout() &&
                    CreateGraphicsPipeline() && CreateFramebuffers() && CreateCommandPool() &&
                    CreateCommandBuffers() && CreateSyncObjects() && CreateDescriptorPoolAndSet();
    if (ok)
        vbam::LogDebug(QStringLiteral("Vulkan device created successfully"));

    DrawingPanelInit();
}

VKDrawingPanel::~VKDrawingPanel() {
    StopFilterThreads();

    if (device_ != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device_);

    DestroyTexture();
#if defined(Q_OS_ANDROID)
    DestroyOverlayTexture();
#endif

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        if (image_available_sem_[i]) vkDestroySemaphore(device_, image_available_sem_[i], nullptr);
        if (render_finished_sem_[i]) vkDestroySemaphore(device_, render_finished_sem_[i], nullptr);
        if (in_flight_fence_[i]) vkDestroyFence(device_, in_flight_fence_[i], nullptr);
    }

    if (cmd_pool_) vkDestroyCommandPool(device_, cmd_pool_, nullptr);

    DestroySwapchain();

    if (desc_pool_) vkDestroyDescriptorPool(device_, desc_pool_, nullptr);
    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
#if defined(Q_OS_ANDROID)
    if (osc_pipeline_) vkDestroyPipeline(device_, osc_pipeline_, nullptr);
#endif
    if (pipeline_layout_) vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    if (desc_set_layout_) vkDestroyDescriptorSetLayout(device_, desc_set_layout_, nullptr);
    if (render_pass_) vkDestroyRenderPass(device_, render_pass_, nullptr);

    if (device_) vkDestroyDevice(device_, nullptr);
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);

#if defined(Q_OS_ANDROID)
    // Release our ANativeWindow reference and drop the overlay SurfaceView it
    // came from, after the Vulkan surface built on it is gone.
    if (android_window_) {
        ANativeWindow_release(static_cast<ANativeWindow*>(android_window_));
        android_window_ = nullptr;
        VbamDestroyAndroidVideoSurface();
    }
#endif
}

// ─── CreateInstance ───────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateInstance() {
    if (!VulkanBootstrap())
        return false;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VBA-M";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "VBA-M";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    uint32_t ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> inst_exts(ext_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, inst_exts.data());

    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME};
    VkInstanceCreateFlags flags = 0;

#if defined(Q_OS_WIN)
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(Q_OS_MACOS)
    // Modern MoltenVK exposes VK_EXT_metal_surface (via the portability
    // enumeration layer); the old VK_MVK_macos_surface is deprecated.
    extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    if (HasInstanceExtension(inst_exts, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    if (HasInstanceExtension(inst_exts, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#elif defined(Q_OS_ANDROID)
    // Required by CreateSurface()'s vkCreateAndroidSurfaceKHR.
    if (!HasInstanceExtension(inst_exts, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
        vbam::LogDebug(QStringLiteral("Vulkan: VK_KHR_android_surface not available"));
        return false;
    }
    extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#else
    // Only the surface extensions the loader exposes: requesting a missing one
    // fails vkCreateInstance with VK_ERROR_EXTENSION_NOT_PRESENT.
    bool have_surface = false;
#if defined(VBAM_VK_HAVE_XCB)
    if (HasInstanceExtension(inst_exts, VK_KHR_XCB_SURFACE_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
        have_surface = true;
    }
#endif
#if defined(VBAM_VK_HAVE_XLIB)
    if (HasInstanceExtension(inst_exts, VK_KHR_XLIB_SURFACE_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
        have_surface = true;
    }
#endif
    if (!have_surface) {
        vbam::LogDebug(QStringLiteral("Vulkan: no X11 surface extension available"));
        return false;
    }
#endif

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.flags = flags;
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    const VkResult res = vkCreateInstance(&ci, nullptr, &instance_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create Vulkan instance: %1").arg(static_cast<int>(res)));
        return false;
    }
    VulkanLoadInstanceFns(instance_);
    vbam::LogDebug(QStringLiteral("Vulkan instance created"));
    return true;
}

// ─── CreateSurface ────────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateSurface() {
    VkResult res = VK_ERROR_INITIALIZATION_FAILED;
#if defined(Q_OS_WIN)
    if (!vkCreateWin32SurfaceKHR)
        return false;
    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hwnd = static_cast<HWND>(NativeHandle());
    ci.hinstance = GetModuleHandle(nullptr);
    res = vkCreateWin32SurfaceKHR(instance_, &ci, nullptr, &surface_);
#elif defined(Q_OS_MACOS)
    if (!vkCreateMetalSurfaceEXT)
        return false;
    // MoltenVK wants a CAMetalLayer; Qt's NSView carries a plain CALayer, so
    // install one (the helper is a no-op when it is already a CAMetalLayer).
    metal_layer_ = VbamQtEnsureMetalLayer(NativeHandle());
    if (!metal_layer_) {
        vbam::LogError(Tr("Failed to obtain a Metal layer for the Vulkan surface"));
        return false;
    }
    const QSize px = DevicePixelSize();
    VbamQtMetalLayerResize(metal_layer_, px.width(), px.height(), devicePixelRatioF());
    VkMetalSurfaceCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    ci.pLayer = static_cast<const CAMetalLayer*>(metal_layer_);
    res = vkCreateMetalSurfaceEXT(instance_, &ci, nullptr, &surface_);
#elif defined(Q_OS_ANDROID)
    // VK_KHR_android_surface takes an ANativeWindow*, which Qt's Android QPA
    // exposes for no QWindow: every widget is drawn into the activity's single
    // QtSurface. Take the route Qt's own QAndroidPlatformVulkanWindow does and
    // attach a SurfaceView to the activity over this panel's on-screen rect,
    // rendering into its Surface's ANativeWindow. The rect comes from this
    // widget, not from its top-level window: the overlay has to cover exactly
    // the panel, or the picture is centred in the wrong box (a window-sized
    // overlay also runs under the action bar and past the bottom of the screen).
    if (!vkCreateAndroidSurfaceKHR)
        return false;
    if (!android_window_)
        android_window_ = VbamCreateAndroidVideoSurface(this);
    if (!android_window_) {
        vbam::LogError(Tr("Failed to obtain an Android native window for the Vulkan surface"));
        return false;
    }
    VkAndroidSurfaceCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    ci.window = static_cast<ANativeWindow*>(android_window_);
    res = vkCreateAndroidSurfaceKHR(instance_, &ci, nullptr, &surface_);
#else
    if (QGuiApplication::platformName() != QLatin1String("xcb")) {
        // Wayland: Qt exposes no public wl_surface for a widget; GameArea falls
        // back to the next renderer.
        vbam::LogDebug(QStringLiteral("Vulkan: unsupported platform %1")
                           .arg(QGuiApplication::platformName()));
        return false;
    }
    auto* x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11)
        return false;
#if defined(VBAM_VK_HAVE_XCB)
    if (vkCreateXcbSurfaceKHR && x11->connection()) {
        VkXcbSurfaceCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        ci.connection = x11->connection();
        ci.window = static_cast<xcb_window_t>(winId());
        res = vkCreateXcbSurfaceKHR(instance_, &ci, nullptr, &surface_);
    }
#endif
#if defined(VBAM_VK_HAVE_XLIB)
    if (res != VK_SUCCESS && vkCreateXlibSurfaceKHR && x11->display()) {
        VkXlibSurfaceCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        ci.dpy = static_cast<Display*>(x11->display());
        ci.window = static_cast<Window>(winId());
        res = vkCreateXlibSurfaceKHR(instance_, &ci, nullptr, &surface_);
    }
#endif
#endif
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create Vulkan surface: %1").arg(static_cast<int>(res)));
        return false;
    }
    return true;
}

// ─── PickPhysicalDevice ───────────────────────────────────────────────────────
bool VKDrawingPanel::PickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        vbam::LogError(Tr("No Vulkan physical devices found"));
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (auto& pd : devices) {
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qcount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qcount, qprops.data());

        uint32_t gfx = UINT32_MAX, prs = UINT32_MAX;
        for (uint32_t i = 0; i < qcount; ++i) {
            if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                gfx = i;
            VkBool32 present_sup = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface_, &present_sup);
            if (present_sup)
                prs = i;
            if (gfx != UINT32_MAX && prs != UINT32_MAX)
                break;
        }
        if (gfx == UINT32_MAX || prs == UINT32_MAX)
            continue;

        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> exts(ext_count);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, exts.data());
        if (!HasInstanceExtension(exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            continue;

        uint32_t fmt_count = 0, mode_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface_, &fmt_count, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface_, &mode_count, nullptr);
        if (fmt_count == 0 || mode_count == 0)
            continue;

        physical_device_ = pd;
        graphics_family_ = gfx;
        present_family_ = prs;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(pd, &props);
        vbam::LogDebug(QStringLiteral("Selected Vulkan device: %1")
                           .arg(QString::fromUtf8(props.deviceName)));
        return true;
    }

    vbam::LogError(Tr("No suitable Vulkan physical device found"));
    return false;
}

// ─── CreateLogicalDevice ──────────────────────────────────────────────────────
bool VKDrawingPanel::CreateLogicalDevice() {
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_cis;

    auto add_queue = [&](uint32_t family) {
        VkDeviceQueueCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        ci.queueFamilyIndex = family;
        ci.queueCount = 1;
        ci.pQueuePriorities = &priority;
        queue_cis.push_back(ci);
    };

    add_queue(graphics_family_);
    if (present_family_ != graphics_family_)
        add_queue(present_family_);

    std::vector<const char*> dev_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#if defined(Q_OS_MACOS)
    // Required by MoltenVK when the portability enumeration layer is active.
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> exts(ext_count);
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &ext_count, exts.data());
    if (HasInstanceExtension(exts, "VK_KHR_portability_subset"))
        dev_exts.push_back("VK_KHR_portability_subset");
#endif

    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = static_cast<uint32_t>(queue_cis.size());
    ci.pQueueCreateInfos = queue_cis.data();
    ci.enabledExtensionCount = static_cast<uint32_t>(dev_exts.size());
    ci.ppEnabledExtensionNames = dev_exts.data();

    const VkResult res = vkCreateDevice(physical_device_, &ci, nullptr, &device_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create Vulkan logical device: %1").arg(static_cast<int>(res)));
        return false;
    }

    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_, 0, &present_queue_);
    return true;
}

// ─── CreateSwapchain ──────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps);

    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, formats.data());
    if (formats.empty())
        return false;

    VkSurfaceFormatKHR chosen_fmt = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_fmt = f;
            break;
        }
    }

    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &mode_count,
                                              modes.data());

    VkPresentModeKHR chosen_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!vsync_) {
        for (auto& m : modes)
            if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) { chosen_mode = m; break; }
        if (chosen_mode == VK_PRESENT_MODE_FIFO_KHR)
            for (auto& m : modes)
                if (m == VK_PRESENT_MODE_MAILBOX_KHR) { chosen_mode = m; break; }
    }

    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        const QSize px = DevicePixelSize();
        extent.width = std::clamp(static_cast<uint32_t>(px.width()), caps.minImageExtent.width,
                                  caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(px.height()), caps.minImageExtent.height,
                                   caps.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0)
        return false;

    uint32_t img_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && img_count > caps.maxImageCount)
        img_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surface_;
    ci.minImageCount = img_count;
    ci.imageFormat = chosen_fmt.format;
    ci.imageColorSpace = chosen_fmt.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // for vkCmdClearColorImage

    uint32_t qfamilies[] = {graphics_family_, present_family_};
    if (graphics_family_ != present_family_) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = qfamilies;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    // Present without any transform of the presentation engine's own: ask for
    // the surface's currentTransform, then rotate our quad to match (the shader
    // gets the matrix through push constants). On Android currentTransform is
    // ROTATE_90/180/270 whenever the window orientation differs from the
    // panel's natural one, and currentExtent is reported in that pre-transformed
    // space -- drawing an unrotated quad into it is what tips the picture over.
    // A driver that cannot take currentTransform back gets identity, and then
    // does the rotation itself, so the picture stays upright either way.
    if (caps.supportedTransforms & caps.currentTransform)
        swapchain_pre_transform_ = caps.currentTransform;
    else
        swapchain_pre_transform_ = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    ci.preTransform = swapchain_pre_transform_;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = chosen_mode;
    ci.clipped = VK_TRUE;

    const VkResult res = vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create Vulkan swapchain: %1").arg(static_cast<int>(res)));
        return false;
    }

    swapchain_format_ = chosen_fmt.format;
    swapchain_extent_ = extent;

    uint32_t sc_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &sc_count, nullptr);
    swapchain_images_.resize(sc_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &sc_count, swapchain_images_.data());

    vbam::LogDebug(QStringLiteral("Vulkan swapchain created: %1x%2, %3 images, vsync=%4, "
                                  "pre-transform=0x%5")
                       .arg(extent.width).arg(extent.height).arg(sc_count)
                       .arg(vsync_ ? QStringLiteral("on") : QStringLiteral("off"))
                       .arg(static_cast<unsigned>(swapchain_pre_transform_), 0, 16));
#if defined(Q_OS_ANDROID)
    // The one line worth having in logcat when the panel misbehaves on a device.
    __android_log_print(ANDROID_LOG_INFO, "VBAM",
                        "VK swapchain %ux%u images=%u pre-transform=0x%x panel=%dx%d",
                        extent.width, extent.height, sc_count,
                        static_cast<unsigned>(swapchain_pre_transform_), QWidget::width(),
                        QWidget::height());
#endif
    return true;
}

// ─── PreTransformMatrix ───────────────────────────────────────────────────────
//
// The swapchain images live in the surface's pre-transformed space, which for a
// rotated surface is the display's space turned by -preTransform. Geometry is
// therefore built in display-oriented NDC and mapped over with this matrix; a
// VK_SURFACE_TRANSFORM_ROTATE_90 surface wants its content rotated 90 degrees
// clockwise, which in Vulkan's y-down clip space is (x, y) -> (-y, x).
void VKDrawingPanel::PreTransformMatrix(float out_mat[4]) const {
    // Rotation, then the mirror (x negation) that the MIRROR variants prepend.
    float c = 1.f, s = 0.f;
    switch (swapchain_pre_transform_) {
        case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
        case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
            c = 0.f; s = 1.f; break;
        case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
        case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
            c = -1.f; s = 0.f; break;
        case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
        case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
            c = 0.f; s = -1.f; break;
        default:  // IDENTITY, HORIZONTAL_MIRROR, INHERIT
            break;
    }
    float mirror = 1.f;
    switch (swapchain_pre_transform_) {
        case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:
        case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
        case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
        case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
            mirror = -1.f; break;
        default:
            break;
    }
    // rotate(c, s) * mirror(x)
    out_mat[0] = c * mirror;
    out_mat[1] = -s;
    out_mat[2] = s * mirror;
    out_mat[3] = c;
}

void VKDrawingPanel::DestroySwapchain() {
    for (auto fb : framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    for (auto iv : swapchain_views_) vkDestroyImageView(device_, iv, nullptr);
    swapchain_views_.clear();
    if (swapchain_) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

bool VKDrawingPanel::RecreateSwapchain() {
    vkDeviceWaitIdle(device_);
    DestroySwapchain();
    vsync_ = OPTION(kPrefVsync);
#if defined(Q_OS_MACOS)
    // MoltenVK reports the layer's drawableSize as the surface extent.
    const QSize px = DevicePixelSize();
    VbamQtMetalLayerResize(metal_layer_, px.width(), px.height(), devicePixelRatioF());
#endif
    swapchain_dirty_ = false;
    return CreateSwapchain() && CreateImageViews() && CreateFramebuffers();
}

// ─── Image views / render pass / framebuffers ────────────────────────────────
bool VKDrawingPanel::CreateImageViews() {
    swapchain_views_.resize(swapchain_images_.size());
    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        VkImageViewCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = swapchain_images_[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = swapchain_format_;
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.layerCount = 1;

        const VkResult res = vkCreateImageView(device_, &ci, nullptr, &swapchain_views_[i]);
        if (res != VK_SUCCESS) {
            vbam::LogError(Tr("Failed to create swapchain image view %1: %2")
                               .arg(static_cast<int>(i)).arg(static_cast<int>(res)));
            return false;
        }
    }
    return true;
}

bool VKDrawingPanel::CreateRenderPass() {
    VkAttachmentDescription color_att{};
    color_att.format = swapchain_format_;
    color_att.samples = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments = &color_att;
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    const VkResult res = vkCreateRenderPass(device_, &ci, nullptr, &render_pass_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create render pass: %1").arg(static_cast<int>(res)));
        return false;
    }
    return true;
}

bool VKDrawingPanel::CreateFramebuffers() {
    framebuffers_.resize(swapchain_views_.size());
    for (size_t i = 0; i < swapchain_views_.size(); ++i) {
        VkFramebufferCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass = render_pass_;
        ci.attachmentCount = 1;
        ci.pAttachments = &swapchain_views_[i];
        ci.width = swapchain_extent_.width;
        ci.height = swapchain_extent_.height;
        ci.layers = 1;

        const VkResult res = vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i]);
        if (res != VK_SUCCESS) {
            vbam::LogError(Tr("Failed to create framebuffer %1: %2")
                               .arg(static_cast<int>(i)).arg(static_cast<int>(res)));
            return false;
        }
    }
    return true;
}

// ─── Descriptors / pipeline ──────────────────────────────────────────────────
bool VKDrawingPanel::CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1;
    ci.pBindings = &binding;

    const VkResult res = vkCreateDescriptorSetLayout(device_, &ci, nullptr, &desc_set_layout_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create descriptor set layout: %1").arg(static_cast<int>(res)));
        return false;
    }
    return true;
}

bool VKDrawingPanel::CreateGraphicsPipeline() {
    auto make_module = [&](const uint32_t* spv, size_t bytes) -> VkShaderModule {
        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = bytes;
        ci.pCode = spv;
        VkShaderModule m = VK_NULL_HANDLE;
        vkCreateShaderModule(device_, &ci, nullptr, &m);
        return m;
    };

    VkShaderModule vert_mod = make_module(kVertSpv, kVertSpvSize);
    VkShaderModule frag_mod = make_module(kFragSpv, kFragSpvSize);

    if (!vert_mod || !frag_mod) {
        vbam::LogError(Tr("Failed to create shader modules"));
        if (vert_mod) vkDestroyShaderModule(device_, vert_mod, nullptr);
        if (frag_mod) vkDestroyShaderModule(device_, frag_mod, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_mod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_mod;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vert_input{};
    vert_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo vp_state{};
    vp_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_state.viewportCount = 1;
    vp_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rast{};
    rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode = VK_CULL_MODE_NONE;
    rast.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rast.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // No blending: the emulator's 32-bit pixel format leaves the alpha byte at
    // 0, which the upload copies verbatim.
    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_att;

    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dyn_states;

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(float) * 12;  // src_rect + dst_rect + rotation matrix

    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &desc_set_layout_;
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges = &pc_range;

    VkResult res = vkCreatePipelineLayout(device_, &layout_ci, nullptr, &pipeline_layout_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create pipeline layout: %1").arg(static_cast<int>(res)));
        vkDestroyShaderModule(device_, vert_mod, nullptr);
        vkDestroyShaderModule(device_, frag_mod, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipe_ci{};
    pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_ci.stageCount = 2;
    pipe_ci.pStages = stages;
    pipe_ci.pVertexInputState = &vert_input;
    pipe_ci.pInputAssemblyState = &ia;
    pipe_ci.pViewportState = &vp_state;
    pipe_ci.pRasterizationState = &rast;
    pipe_ci.pMultisampleState = &ms;
    pipe_ci.pColorBlendState = &blend;
    pipe_ci.pDynamicState = &dyn;
    pipe_ci.layout = pipeline_layout_;
    pipe_ci.renderPass = render_pass_;
    pipe_ci.subpass = 0;

    res = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &pipeline_);

#if defined(Q_OS_ANDROID)
    // Same pipeline with straight-alpha blending, for the on-screen controller
    // overlay composited over the frame (see UpdateOverlayTexture).
    if (res == VK_SUCCESS) {
        blend_att.blendEnable = VK_TRUE;
        blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_att.colorBlendOp = VK_BLEND_OP_ADD;
        blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipe_ci, nullptr,
                                      &osc_pipeline_) != VK_SUCCESS) {
            // Not fatal: the frame still presents, only the overlay is missing.
            vbam::LogDebug(QStringLiteral("Vulkan: failed to create the overlay pipeline"));
            osc_pipeline_ = VK_NULL_HANDLE;
        }
    }
#endif

    vkDestroyShaderModule(device_, vert_mod, nullptr);
    vkDestroyShaderModule(device_, frag_mod, nullptr);

    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create graphics pipeline: %1").arg(static_cast<int>(res)));
        return false;
    }
    return true;
}

// ─── Command pool / buffers / sync ───────────────────────────────────────────
bool VKDrawingPanel::CreateCommandPool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = graphics_family_;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    const VkResult res = vkCreateCommandPool(device_, &ci, nullptr, &cmd_pool_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create command pool: %1").arg(static_cast<int>(res)));
        return false;
    }
    return true;
}

bool VKDrawingPanel::CreateCommandBuffers() {
    cmd_buffers_.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = cmd_pool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = kMaxFramesInFlight;

    const VkResult res = vkAllocateCommandBuffers(device_, &ai, cmd_buffers_.data());
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to allocate command buffers: %1").arg(static_cast<int>(res)));
        return false;
    }
    return true;
}

bool VKDrawingPanel::CreateSyncObjects() {
    VkSemaphoreCreateInfo sem_ci{};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fen_ci{};
    fen_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fen_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device_, &sem_ci, nullptr, &image_available_sem_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &sem_ci, nullptr, &render_finished_sem_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fen_ci, nullptr, &in_flight_fence_[i]) != VK_SUCCESS) {
            vbam::LogError(Tr("Failed to create sync objects for frame %1").arg(i));
            return false;
        }
    }
    return true;
}

bool VKDrawingPanel::CreateDescriptorPoolAndSet() {
    // Two sets: the emulator frame, plus the on-screen controller overlay the
    // Android path composites on top of it (see UpdateOverlayTexture).
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 2;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets = 2;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = &pool_size;

    VkResult res = vkCreateDescriptorPool(device_, &ci, nullptr, &desc_pool_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create descriptor pool: %1").arg(static_cast<int>(res)));
        return false;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = desc_pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &desc_set_layout_;

    res = vkAllocateDescriptorSets(device_, &ai, &desc_set_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to allocate descriptor set: %1").arg(static_cast<int>(res)));
        return false;
    }
#if defined(Q_OS_ANDROID)
    res = vkAllocateDescriptorSets(device_, &ai, &osc_desc_set_);
    if (res != VK_SUCCESS) {
        // Not fatal: the frame still presents, only the overlay is missing.
        vbam::LogDebug(QStringLiteral("Vulkan: failed to allocate the overlay descriptor set"));
        osc_desc_set_ = VK_NULL_HANDLE;
    }
#endif
    return true;
}

uint32_t VKDrawingPanel::FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
        if ((type_filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return UINT32_MAX;
}

// ─── Texture ─────────────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateTexture(uint32_t tex_w, uint32_t tex_h, VkFormat fmt) {
    DestroyTexture();

    VkImageCreateInfo img_ci{};
    img_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_ci.imageType = VK_IMAGE_TYPE_2D;
    img_ci.format = fmt;
    img_ci.extent = {tex_w, tex_h, 1};
    img_ci.mipLevels = 1;
    img_ci.arrayLayers = 1;
    img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult res = vkCreateImage(device_, &img_ci, nullptr, &tex_image_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create texture image: %1").arg(static_cast<int>(res)));
        return false;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device_, tex_image_, &mem_req);

    VkMemoryAllocateInfo alloc_ci{};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_req.size;
    alloc_ci.memoryTypeIndex =
        FindMemoryType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (alloc_ci.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(device_, &alloc_ci, nullptr, &tex_memory_) != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to allocate texture memory"));
        return false;
    }
    vkBindImageMemory(device_, tex_image_, tex_memory_, 0);

    VkImageViewCreateInfo view_ci{};
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image = tex_image_;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = fmt;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.layerCount = 1;

    res = vkCreateImageView(device_, &view_ci, nullptr, &tex_view_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create texture image view: %1").arg(static_cast<int>(res)));
        return false;
    }

    const VkFilter vk_filter = OPTION(kDispBilinear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

    VkSamplerCreateInfo samp_ci{};
    samp_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samp_ci.magFilter = vk_filter;
    samp_ci.minFilter = vk_filter;
    samp_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samp_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.maxLod = 0.0f;

    res = vkCreateSampler(device_, &samp_ci, nullptr, &tex_sampler_);
    if (res != VK_SUCCESS) {
        vbam::LogError(Tr("Failed to create texture sampler: %1").arg(static_cast<int>(res)));
        return false;
    }

    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView = tex_view_;
    img_info.sampler = tex_sampler_;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = desc_set_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &img_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    const VkDeviceSize needed = static_cast<VkDeviceSize>(tex_w) * tex_h * 4;
    if (needed > staging_size_) {
        if (staging_buffer_) {
            vkDestroyBuffer(device_, staging_buffer_, nullptr);
            vkFreeMemory(device_, staging_memory_, nullptr);
            staging_buffer_ = VK_NULL_HANDLE;
            staging_memory_ = VK_NULL_HANDLE;
        }

        VkBufferCreateInfo buf_ci{};
        buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_ci.size = needed;
        buf_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        if (vkCreateBuffer(device_, &buf_ci, nullptr, &staging_buffer_) != VK_SUCCESS) {
            vbam::LogError(Tr("Failed to create staging buffer"));
            return false;
        }

        VkMemoryRequirements stg_req;
        vkGetBufferMemoryRequirements(device_, staging_buffer_, &stg_req);

        VkMemoryAllocateInfo stg_alloc{};
        stg_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        stg_alloc.allocationSize = stg_req.size;
        stg_alloc.memoryTypeIndex = FindMemoryType(
            stg_req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (stg_alloc.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(device_, &stg_alloc, nullptr, &staging_memory_) != VK_SUCCESS) {
            vbam::LogError(Tr("Failed to allocate staging memory"));
            return false;
        }
        vkBindBufferMemory(device_, staging_buffer_, staging_memory_, 0);
        staging_size_ = needed;
    }

    tex_format_ = fmt;
    texture_width_ = tex_w;
    texture_height_ = tex_h;
    return true;
}

void VKDrawingPanel::DestroyTexture() {
    if (tex_sampler_) { vkDestroySampler(device_, tex_sampler_, nullptr); tex_sampler_ = VK_NULL_HANDLE; }
    if (tex_view_) { vkDestroyImageView(device_, tex_view_, nullptr); tex_view_ = VK_NULL_HANDLE; }
    if (tex_image_) { vkDestroyImage(device_, tex_image_, nullptr); tex_image_ = VK_NULL_HANDLE; }
    if (tex_memory_) { vkFreeMemory(device_, tex_memory_, nullptr); tex_memory_ = VK_NULL_HANDLE; }

    if (staging_buffer_) { vkDestroyBuffer(device_, staging_buffer_, nullptr); staging_buffer_ = VK_NULL_HANDLE; }
    if (staging_memory_) { vkFreeMemory(device_, staging_memory_, nullptr); staging_memory_ = VK_NULL_HANDLE; }
    staging_size_ = 0;
    texture_width_ = 0;
    texture_height_ = 0;
}

// ─── Init / resize ───────────────────────────────────────────────────────────
#if defined(Q_OS_ANDROID)
// ─── Android overlay ─────────────────────────────────────────────────────────

void VKDrawingPanel::SyncAndroidOverlayGeometry() {
    if (!android_window_)
        return;
    VbamSetAndroidVideoSurfaceGeometry(this);
}

// The Android swapchain presents into a SurfaceView stacked over the Qt
// content, which hides the on-screen controller widget living under it. Render
// that widget to RGBA and keep it in a texture here so Present() can composite
// it over the emulator frame; the widget stays where it is and keeps receiving
// touches.
bool VKDrawingPanel::UpdateOverlayTexture(VkCommandBuffer cmd) {
    if (!osc_desc_set_ || !osc_pipeline_)
        return false;

    MainWindow* frame = vbamApp().frame;
    GameArea* game_area = frame ? frame->GetPanel() : nullptr;
    widgets::OnScreenController* osc = game_area ? game_area->on_screen_controller() : nullptr;
    if (!osc || !osc->isVisible())
        return false;

    // Keep the controller inside the visible area. GameArea does this too when
    // it (re)creates the overlay, but the content size only becomes known once
    // the activity has laid the video surface out, which can be later than that.
    {
        int visible_w = 0, visible_h = 0;
        if (VbamAndroidVisibleClientSize(this, &visible_w, &visible_h)) {
            const QSize want(std::min(QWidget::width(), visible_w),
                             std::min(QWidget::height(), visible_h));
            if (osc->size() != want)
                osc->resize(want);
        }
    }

    const QSize osc_size = osc->size();
    if (osc_size.width() < 1 || osc_size.height() < 1)
        return false;

    const bool resized = static_cast<uint32_t>(osc_size.width()) != osc_width_ ||
                         static_cast<uint32_t>(osc_size.height()) != osc_height_;
    if (!resized && osc_revision_ == osc->revision())
        return osc_image_ != VK_NULL_HANDLE;  // unchanged, reuse the texture

    // Render at device resolution: the texture is presented 1:1 into a native
    // surface, so the widget's logical size would come out soft.
    const double osc_scale = osc->devicePixelRatioF() > 0 ? osc->devicePixelRatioF() : 1.0;

    std::vector<uint8_t> pixels;
    int w = 0, h = 0;
    if (!osc->RenderRgba(&pixels, &w, &h, osc_scale) || w < 1 || h < 1) {
        static bool warned = false;  // once per session, not at 60 Hz
        if (!warned) {
            warned = true;
            __android_log_print(ANDROID_LOG_WARN, "VBAM",
                                "on-screen controller render failed (%dx%d)", osc_size.width(),
                                osc_size.height());
        }
        return false;
    }

    if (osc_image_ == VK_NULL_HANDLE || static_cast<uint32_t>(w) != osc_width_ ||
        static_cast<uint32_t>(h) != osc_height_) {
        // A frame still in flight may be sampling the old image through the
        // overlay descriptor set, and both are about to be replaced. Resizes are
        // rare (panel geometry changes), so idling here costs nothing.
        if (osc_image_ != VK_NULL_HANDLE)
            vkDeviceWaitIdle(device_);
        DestroyOverlayTexture();

        VkImageCreateInfo img_ci{};
        img_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_ci.imageType = VK_IMAGE_TYPE_2D;
        img_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
        img_ci.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        img_ci.mipLevels = 1;
        img_ci.arrayLayers = 1;
        img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
        img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device_, &img_ci, nullptr, &osc_image_) != VK_SUCCESS) {
            DestroyOverlayTexture();
            return false;
        }

        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(device_, osc_image_, &mem_req);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = mem_req.size;
        alloc.memoryTypeIndex =
            FindMemoryType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (alloc.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(device_, &alloc, nullptr, &osc_memory_) != VK_SUCCESS) {
            DestroyOverlayTexture();
            return false;
        }
        vkBindImageMemory(device_, osc_image_, osc_memory_, 0);

        VkImageViewCreateInfo view_ci{};
        view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image = osc_image_;
        view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
        view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.levelCount = 1;
        view_ci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &view_ci, nullptr, &osc_view_) != VK_SUCCESS) {
            DestroyOverlayTexture();
            return false;
        }

        // Sampled 1:1 (drawn at panel resolution); linear only softens its
        // edges while the surface is being resized.
        VkSamplerCreateInfo samp_ci{};
        samp_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samp_ci.magFilter = VK_FILTER_LINEAR;
        samp_ci.minFilter = VK_FILTER_LINEAR;
        samp_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samp_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samp_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samp_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samp_ci.maxLod = 0.0f;
        if (vkCreateSampler(device_, &samp_ci, nullptr, &osc_sampler_) != VK_SUCCESS) {
            DestroyOverlayTexture();
            return false;
        }

        VkDescriptorImageInfo img_info{};
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_info.imageView = osc_view_;
        img_info.sampler = osc_sampler_;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = osc_desc_set_;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &img_info;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

        osc_width_ = static_cast<uint32_t>(w);
        osc_height_ = static_cast<uint32_t>(h);
    }

    const VkDeviceSize needed = static_cast<VkDeviceSize>(w) * h * 4;
    if (needed > osc_staging_size_) {
        if (osc_staging_buffer_) {
            vkDestroyBuffer(device_, osc_staging_buffer_, nullptr);
            vkFreeMemory(device_, osc_staging_memory_, nullptr);
            osc_staging_buffer_ = VK_NULL_HANDLE;
            osc_staging_memory_ = VK_NULL_HANDLE;
        }
        VkBufferCreateInfo buf_ci{};
        buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_ci.size = needed;
        buf_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (vkCreateBuffer(device_, &buf_ci, nullptr, &osc_staging_buffer_) != VK_SUCCESS)
            return false;

        VkMemoryRequirements stg_req;
        vkGetBufferMemoryRequirements(device_, osc_staging_buffer_, &stg_req);
        VkMemoryAllocateInfo stg_alloc{};
        stg_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        stg_alloc.allocationSize = stg_req.size;
        stg_alloc.memoryTypeIndex =
            FindMemoryType(stg_req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (stg_alloc.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(device_, &stg_alloc, nullptr, &osc_staging_memory_) != VK_SUCCESS)
            return false;
        vkBindBufferMemory(device_, osc_staging_buffer_, osc_staging_memory_, 0);
        osc_staging_size_ = needed;
    }

    void* mapped = nullptr;
    if (vkMapMemory(device_, osc_staging_memory_, 0, needed, 0, &mapped) != VK_SUCCESS)
        return false;
    memcpy(mapped, pixels.data(), static_cast<size_t>(needed));
    vkUnmapMemory(device_, osc_staging_memory_);

    VkImageMemoryBarrier to_dst{};
    to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_dst.image = osc_image_;
    to_dst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_dst);

    VkBufferImageCopy copy{};
    copy.bufferRowLength = static_cast<uint32_t>(w);
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    vkCmdCopyBufferToImage(cmd, osc_staging_buffer_, osc_image_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier to_read = to_dst;
    to_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &to_read);

    if (osc_revision_ == 0)
        __android_log_print(ANDROID_LOG_INFO, "VBAM",
                            "on-screen controller overlay uploaded %dx%d", w, h);
    osc_revision_ = osc->revision();
    return true;
}

void VKDrawingPanel::DestroyOverlayTexture() {
    if (osc_sampler_) { vkDestroySampler(device_, osc_sampler_, nullptr); osc_sampler_ = VK_NULL_HANDLE; }
    if (osc_view_) { vkDestroyImageView(device_, osc_view_, nullptr); osc_view_ = VK_NULL_HANDLE; }
    if (osc_image_) { vkDestroyImage(device_, osc_image_, nullptr); osc_image_ = VK_NULL_HANDLE; }
    if (osc_memory_) { vkFreeMemory(device_, osc_memory_, nullptr); osc_memory_ = VK_NULL_HANDLE; }
    if (osc_staging_buffer_) {
        vkDestroyBuffer(device_, osc_staging_buffer_, nullptr);
        osc_staging_buffer_ = VK_NULL_HANDLE;
    }
    if (osc_staging_memory_) {
        vkFreeMemory(device_, osc_staging_memory_, nullptr);
        osc_staging_memory_ = VK_NULL_HANDLE;
    }
    osc_staging_size_ = 0;
    osc_width_ = 0;
    osc_height_ = 0;
    osc_revision_ = 0;
}
#endif  // Q_OS_ANDROID

void VKDrawingPanel::DrawingPanelInit() {
    DrawingPanelBase::DrawingPanelInit();

    // No logical device -> Vulkan setup (done in the constructor) failed. Flag
    // it so GameArea falls back to the next renderer in the priority list.
    if (!device_ || !swapchain_ || !pipeline_) {
        init_failed_ = true;
        return;
    }

    vbam::LogDebug(QStringLiteral("VKDrawingPanel initialized: %1x%2 (scale: %3)")
                       .arg(ScaledWidth()).arg(ScaledHeight()).arg(scale));
}

void VKDrawingPanel::OnNativeResize(const QSize& device_pixels) {
    (void)device_pixels;
#if defined(Q_OS_ANDROID)
    // The overlay SurfaceView follows the panel; the swapchain follows the
    // surface once Android has laid it out (see Present()).
    android_overlay_size_ = QWidget::size();
    SyncAndroidOverlayGeometry();
#endif
    if (device_)
        swapchain_dirty_ = true;
}

// ─── Present ─────────────────────────────────────────────────────────────────
void VKDrawingPanel::Present() {
    if (!device_ || !swapchain_ || init_failed_)
        return;

#if defined(Q_OS_ANDROID)
    // The overlay SurfaceView is created before GameArea lays the panel out, so
    // glue it to the panel rect as soon as that rect is real (and whenever it
    // changes without a resize event reaching us).
    const QSize panel_size = QWidget::size();
    if (panel_size != android_overlay_size_ && panel_size.width() > 1 &&
        panel_size.height() > 1) {
        android_overlay_size_ = panel_size;
        SyncAndroidOverlayGeometry();
    }

    // That resize lands on the Android UI thread, so the surface follows a few
    // frames later; rebuild the swapchain whenever the surface no longer matches
    // it instead of waiting for a VK_ERROR_OUT_OF_DATE_KHR that a driver may not
    // report. Also covers the tiny surface the constructor starts out with.
    {
        VkSurfaceCapabilitiesKHR caps{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps) ==
                VK_SUCCESS &&
            caps.currentExtent.width != UINT32_MAX &&
            (caps.currentExtent.width != swapchain_extent_.width ||
             caps.currentExtent.height != swapchain_extent_.height)) {
            if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0)
                return;  // surface not presentable yet
            swapchain_dirty_ = true;
        }
    }
#endif

    if (swapchain_dirty_ && !RecreateSwapchain())
        return;

    vkWaitForFences(device_, 1, &in_flight_fence_[current_frame_], VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult res = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                         image_available_sem_[current_frame_], VK_NULL_HANDLE,
                                         &image_index);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        vbam::LogDebug(QStringLiteral("Failed to acquire swapchain image: %1").arg(static_cast<int>(res)));
        return;
    }

    vkResetFences(device_, 1, &in_flight_fence_[current_frame_]);

    VkCommandBuffer cmd = cmd_buffers_[current_frame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_ci{};
    begin_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_ci.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_ci);

    if (!todraw) {
        // No frame yet: clear to black.
        VkClearColorValue clear_val = {{0.f, 0.f, 0.f, 1.f}};

        VkImageMemoryBarrier to_clear{};
        to_clear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_clear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        to_clear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_clear.image = swapchain_images_[image_index];
        to_clear.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        to_clear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_clear);

        VkImageSubresourceRange sub_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, swapchain_images_[image_index],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_val, 1, &sub_range);

        VkImageMemoryBarrier to_present = to_clear;
        to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_present.dstAccessMask = 0;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_present);
    } else {
        const bool out_8 = panel_color_depth_ == 8;
        const bool out_16 = panel_color_depth_ == 16;
        const bool out_24 = panel_color_depth_ == 24;
        const VkFormat vk_fmt = out_16 ? VK_FORMAT_R5G6B5_UNORM_PACK16 : VK_FORMAT_B8G8R8A8_UNORM;

        const int scaled_width = ScaledWidth();
        const int scaled_height = ScaledHeight();

        if (!tex_image_ || static_cast<int>(texture_width_) != scaled_width ||
            static_cast<int>(texture_height_) != scaled_height || tex_format_ != vk_fmt) {
            if (!CreateTexture(static_cast<uint32_t>(scaled_width),
                               static_cast<uint32_t>(scaled_height), vk_fmt)) {
                vkEndCommandBuffer(cmd);
                return;
            }
        }

        const int src_pitch = SourcePitch();
        const uint8_t* src = SourcePixels();

        void* mapped = nullptr;
        vkMapMemory(device_, staging_memory_, 0, staging_size_, 0, &mapped);
        uint8_t* stg = static_cast<uint8_t*>(mapped);

        if (out_8) {
            for (int y = 0; y < scaled_height; ++y) {
                const uint8_t* sr = src;
                uint8_t* dr = stg + static_cast<size_t>(y) * scaled_width * 4;
                for (int x = 0; x < scaled_width; ++x, ++sr, dr += 4) {
                    const uint8_t p = *sr;
                    if (p == 0xff) {
                        dr[0] = dr[1] = dr[2] = 0xff;
                    } else {
                        dr[0] = (p & 0x3) << 6;
                        dr[1] = ((p >> 2) & 0x7) << 5;
                        dr[2] = ((p >> 5) & 0x7) << 5;
                    }
                    dr[3] = 0xFF;
                }
                src += src_pitch;
            }
        } else if (out_16) {
            const uint16_t* src16 = reinterpret_cast<const uint16_t*>(src);
            for (int y = 0; y < scaled_height; ++y) {
                const uint16_t* sr = src16;
                uint16_t* dr = reinterpret_cast<uint16_t*>(stg + static_cast<size_t>(y) * scaled_width * 2);
                for (int x = 0; x < scaled_width; ++x, ++sr, ++dr) {
                    const uint16_t px = *sr;
                    const uint8_t r5 = (px >> 10) & 0x1f;
                    const uint8_t g5 = (px >> 5) & 0x1f;
                    const uint8_t b5 = px & 0x1f;
                    const uint8_t g6 = static_cast<uint8_t>((g5 << 1) | (g5 >> 4));
                    *dr = static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
                }
                src16 += src_pitch / 2;
            }
        } else if (out_24) {
            for (int y = 0; y < scaled_height; ++y) {
                const uint8_t* sr = src;
                uint8_t* dr = stg + static_cast<size_t>(y) * scaled_width * 4;
                for (int x = 0; x < scaled_width; ++x, sr += 3, dr += 4) {
                    dr[0] = sr[2];
                    dr[1] = sr[1];
                    dr[2] = sr[0];
                    dr[3] = 0xFF;
                }
                src += src_pitch;
            }
        } else {
            for (int y = 0; y < scaled_height; ++y) {
                const uint8_t* sr = src;
                uint8_t* dr = stg + static_cast<size_t>(y) * scaled_width * 4;
                for (int x = 0; x < scaled_width; ++x, sr += 4, dr += 4) {
                    dr[0] = sr[2];
                    dr[1] = sr[1];
                    dr[2] = sr[0];
                    dr[3] = 0xFF;
                }
                src += src_pitch;
            }
        }

        vkUnmapMemory(device_, staging_memory_);

        VkImageMemoryBarrier to_dst{};
        to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_dst.image = tex_image_;
        to_dst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_dst);

        VkBufferImageCopy copy{};
        copy.bufferRowLength = texture_width_;
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {texture_width_, texture_height_, 1};
        vkCmdCopyBufferToImage(cmd, staging_buffer_, tex_image_,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        VkImageMemoryBarrier to_read{};
        to_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        to_read.image = tex_image_;
        to_read.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        to_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_read);

        // The overlay upload has to happen outside the render pass too.
        bool draw_overlay = false;
#if defined(Q_OS_ANDROID)
        draw_overlay = UpdateOverlayTexture(cmd);
#endif

        VkClearValue clear_val{};
        clear_val.color = {{0.f, 0.f, 0.f, 1.f}};

        VkRenderPassBeginInfo rp_begin{};
        rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass = render_pass_;
        rp_begin.framebuffer = framebuffers_[image_index];
        rp_begin.renderArea = {{0, 0}, swapchain_extent_};
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues = &clear_val;

        vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchain_extent_.width);
        viewport.height = static_cast<float>(swapchain_extent_.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = swapchain_extent_;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                &desc_set_, 0, nullptr);

        // On the desktop the widget is already aspect-fitted by GameArea, so
        // the frame fills the surface. On Android the panel is handed the whole
        // game area and the picture is letterboxed here, in display-oriented
        // space, like the GLES renderer does.
        float dst_x0 = -1.f, dst_y0 = -1.f, dst_x1 = 1.f, dst_y1 = 1.f;
#if defined(Q_OS_ANDROID)
        // Overlay crop: how much of the panel the surface actually covers, in
        // 0..1 of the on-screen controller texture. 1,1 unless the surface was
        // clipped (see below).
        float osc_u1 = 1.f, osc_v1 = 1.f;
        {
            // The panel widget can be larger than the visible drawing area --
            // Qt's window overhangs the Android content view below the action
            // bar -- and the SurfaceView is clamped to that area, so the
            // surface, not the panel, is what the picture has to fit. Take the
            // extent as display-oriented, flipping it when the driver reports
            // pre-rotated extents (they differ).
            float surf_w = static_cast<float>(swapchain_extent_.width);
            float surf_h = static_cast<float>(swapchain_extent_.height);
            const float win_w = static_cast<float>(QWidget::width());
            const float win_h = static_cast<float>(QWidget::height());
            if (win_w >= 1.f && win_h >= 1.f && (win_w >= win_h) != (surf_w >= surf_h))
                std::swap(surf_w, surf_h);

            // The overlay texture is rendered at device resolution from the
            // controller widget, itself clamped to the visible area, so this
            // normally resolves to the whole texture.
            if (osc_width_ >= 1 && osc_height_ >= 1) {
                osc_u1 = std::min(1.f, surf_w / static_cast<float>(osc_width_));
                osc_v1 = std::min(1.f, surf_h / static_cast<float>(osc_height_));
            }

            const float tex_aspect = static_cast<float>(scaled_width) / static_cast<float>(scaled_height);
            const float win_aspect = surf_w / surf_h;
            if (win_aspect > tex_aspect) {
                const float ndc_w = tex_aspect / win_aspect;
                dst_x0 = -ndc_w;
                dst_x1 = ndc_w;
            } else {
                const float ndc_h = win_aspect / tex_aspect;
                dst_y0 = -ndc_h;
                dst_y1 = ndc_h;
            }
        }
#endif

        float rot[4];
        PreTransformMatrix(rot);

        const float pc[12] = {0.f, 0.f, 1.f, 1.f, dst_x0, dst_y0, dst_x1, dst_y1,
                              rot[0], rot[1], rot[2], rot[3]};
        vkCmdPushConstants(cmd, pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(pc), pc);

        vkCmdDraw(cmd, 4, 1, 0, 0);

        // The on-screen controller covers the whole panel and is blended over
        // the frame, letterbox bars included -- the way the widget would look
        // if it were visible above this surface.
        if (draw_overlay) {
#if defined(Q_OS_ANDROID)
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, osc_pipeline_);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0,
                                    1, &osc_desc_set_, 0, nullptr);
            // Sampling only the covered part of the overlay keeps the controls
            // where the widget itself puts them, so touches still line up.
            const float osc_pc[12] = {0.f, 0.f, osc_u1, osc_v1, -1.f, -1.f, 1.f, 1.f,
                                      rot[0], rot[1], rot[2], rot[3]};
            vkCmdPushConstants(cmd, pipeline_layout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(osc_pc), osc_pc);
            vkCmdDraw(cmd, 4, 1, 0, 0);
#endif
        }
        vkCmdEndRenderPass(cmd);
    }

    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &image_available_sem_[current_frame_];
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &render_finished_sem_[current_frame_];

    vkQueueSubmit(graphics_queue_, 1, &submit, in_flight_fence_[current_frame_]);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_sem_[current_frame_];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain_;
    present_info.pImageIndices = &image_index;

    res = vkQueuePresentKHR(present_queue_, &present_info);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        swapchain_dirty_ = true;
    else if (res != VK_SUCCESS)
        vbam::LogDebug(QStringLiteral("Failed to present swapchain image: %1").arg(static_cast<int>(res)));

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}

// ─── SPIR-V shaders ─────────────────────────────────────────────────────────
// Same textured-quad shaders as the wx port (glslangValidator -V output):
//
//   #version 450
//   layout(push_constant) uniform PC {
//       vec4 src_rect;   // u0, v0, u1, v1
//       vec4 dst_rect;   // x0, y0, x1, y1, in NDC
//       vec4 rot;        // row-major 2x2 pre-rotation matrix (a, b, c, d)
//   } pc;
//   layout(location = 0) out vec2 o_uv;
//   void main() {
//       vec2 pos = vec2((gl_VertexIndex & 2) == 0 ? pc.dst_rect.x : pc.dst_rect.z,
//                       (gl_VertexIndex & 1) == 0 ? pc.dst_rect.y : pc.dst_rect.w);
//       vec2 uv  = vec2((gl_VertexIndex & 2) == 0 ? pc.src_rect.x : pc.src_rect.z,
//                       (gl_VertexIndex & 1) == 0 ? pc.src_rect.y : pc.src_rect.w);
//       vec2 xf = vec2(pc.rot.x * pos.x + pc.rot.y * pos.y,
//                      pc.rot.z * pos.x + pc.rot.w * pos.y);
//       gl_Position = vec4(xf, 0.0, 1.0);
//       o_uv = uv;
//   }
//
// The fragment stage is a plain textured-quad sample of set 0 binding 0.

const uint32_t VKDrawingPanel::kVertSpv[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000076,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0008000f, 0x00000000, 0x00000004, 0x6e69616d,
    0x00000000, 0x0000000c, 0x0000006a, 0x00000074,
    0x00030003, 0x00000002, 0x000001c2, 0x00040005,
    0x00000004, 0x6e69616d, 0x00000000, 0x00030005,
    0x00000009, 0x00736f70, 0x00060005, 0x0000000c,
    0x565f6c67, 0x65747265, 0x646e4978, 0x00007865,
    0x00030005, 0x00000018, 0x00004350, 0x00060006,
    0x00000018, 0x00000000, 0x5f637273, 0x74636572,
    0x00000000, 0x00060006, 0x00000018, 0x00000001,
    0x5f747364, 0x74636572, 0x00000000, 0x00040006,
    0x00000018, 0x00000002, 0x00746f72, 0x00030005,
    0x0000001a, 0x00006370, 0x00030005, 0x00000035,
    0x00007675, 0x00030005, 0x0000004f, 0x00006678,
    0x00060005, 0x00000068, 0x505f6c67, 0x65567265,
    0x78657472, 0x00000000, 0x00060006, 0x00000068,
    0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69,
    0x00070006, 0x00000068, 0x00000001, 0x505f6c67,
    0x746e696f, 0x657a6953, 0x00000000, 0x00070006,
    0x00000068, 0x00000002, 0x435f6c67, 0x4470696c,
    0x61747369, 0x0065636e, 0x00070006, 0x00000068,
    0x00000003, 0x435f6c67, 0x446c6c75, 0x61747369,
    0x0065636e, 0x00030005, 0x0000006a, 0x00000000,
    0x00040005, 0x00000074, 0x76755f6f, 0x00000000,
    0x00040047, 0x0000000c, 0x0000000b, 0x0000002a,
    0x00030047, 0x00000018, 0x00000002, 0x00050048,
    0x00000018, 0x00000000, 0x00000023, 0x00000000,
    0x00050048, 0x00000018, 0x00000001, 0x00000023,
    0x00000010, 0x00050048, 0x00000018, 0x00000002,
    0x00000023, 0x00000020, 0x00030047, 0x00000068,
    0x00000002, 0x00050048, 0x00000068, 0x00000000,
    0x0000000b, 0x00000000, 0x00050048, 0x00000068,
    0x00000001, 0x0000000b, 0x00000001, 0x00050048,
    0x00000068, 0x00000002, 0x0000000b, 0x00000003,
    0x00050048, 0x00000068, 0x00000003, 0x0000000b,
    0x00000004, 0x00040047, 0x00000074, 0x0000001e,
    0x00000000, 0x00020013, 0x00000002, 0x00030021,
    0x00000003, 0x00000002, 0x00030016, 0x00000006,
    0x00000020, 0x00040017, 0x00000007, 0x00000006,
    0x00000002, 0x00040020, 0x00000008, 0x00000007,
    0x00000007, 0x00040015, 0x0000000a, 0x00000020,
    0x00000001, 0x00040020, 0x0000000b, 0x00000001,
    0x0000000a, 0x0004003b, 0x0000000b, 0x0000000c,
    0x00000001, 0x0004002b, 0x0000000a, 0x0000000e,
    0x00000002, 0x0004002b, 0x0000000a, 0x00000010,
    0x00000000, 0x00020014, 0x00000011, 0x00040020,
    0x00000013, 0x00000007, 0x00000006, 0x00040017,
    0x00000017, 0x00000006, 0x00000004, 0x0005001e,
    0x00000018, 0x00000017, 0x00000017, 0x00000017,
    0x00040020, 0x00000019, 0x00000009, 0x00000018,
    0x0004003b, 0x00000019, 0x0000001a, 0x00000009,
    0x0004002b, 0x0000000a, 0x0000001b, 0x00000001,
    0x00040015, 0x0000001c, 0x00000020, 0x00000000,
    0x0004002b, 0x0000001c, 0x0000001d, 0x00000000,
    0x00040020, 0x0000001e, 0x00000009, 0x00000006,
    0x0004002b, 0x0000001c, 0x00000022, 0x00000002,
    0x0004002b, 0x0000001c, 0x0000002c, 0x00000001,
    0x0004002b, 0x0000001c, 0x00000030, 0x00000003,
    0x0004001c, 0x00000067, 0x00000006, 0x0000002c,
    0x0006001e, 0x00000068, 0x00000017, 0x00000006,
    0x00000067, 0x00000067, 0x00040020, 0x00000069,
    0x00000003, 0x00000068, 0x0004003b, 0x00000069,
    0x0000006a, 0x00000003, 0x0004002b, 0x00000006,
    0x0000006c, 0x00000000, 0x0004002b, 0x00000006,
    0x0000006d, 0x3f800000, 0x00040020, 0x00000071,
    0x00000003, 0x00000017, 0x00040020, 0x00000073,
    0x00000003, 0x00000007, 0x0004003b, 0x00000073,
    0x00000074, 0x00000003, 0x00050036, 0x00000002,
    0x00000004, 0x00000000, 0x00000003, 0x000200f8,
    0x00000005, 0x0004003b, 0x00000008, 0x00000009,
    0x00000007, 0x0004003b, 0x00000013, 0x00000014,
    0x00000007, 0x0004003b, 0x00000013, 0x00000029,
    0x00000007, 0x0004003b, 0x00000008, 0x00000035,
    0x00000007, 0x0004003b, 0x00000013, 0x00000039,
    0x00000007, 0x0004003b, 0x00000013, 0x00000045,
    0x00000007, 0x0004003b, 0x00000008, 0x0000004f,
    0x00000007, 0x0004003d, 0x0000000a, 0x0000000d,
    0x0000000c, 0x000500c7, 0x0000000a, 0x0000000f,
    0x0000000d, 0x0000000e, 0x000500aa, 0x00000011,
    0x00000012, 0x0000000f, 0x00000010, 0x000300f7,
    0x00000016, 0x00000000, 0x000400fa, 0x00000012,
    0x00000015, 0x00000021, 0x000200f8, 0x00000015,
    0x00060041, 0x0000001e, 0x0000001f, 0x0000001a,
    0x0000001b, 0x0000001d, 0x0004003d, 0x00000006,
    0x00000020, 0x0000001f, 0x0003003e, 0x00000014,
    0x00000020, 0x000200f9, 0x00000016, 0x000200f8,
    0x00000021, 0x00060041, 0x0000001e, 0x00000023,
    0x0000001a, 0x0000001b, 0x00000022, 0x0004003d,
    0x00000006, 0x00000024, 0x00000023, 0x0003003e,
    0x00000014, 0x00000024, 0x000200f9, 0x00000016,
    0x000200f8, 0x00000016, 0x0004003d, 0x00000006,
    0x00000025, 0x00000014, 0x0004003d, 0x0000000a,
    0x00000026, 0x0000000c, 0x000500c7, 0x0000000a,
    0x00000027, 0x00000026, 0x0000001b, 0x000500aa,
    0x00000011, 0x00000028, 0x00000027, 0x00000010,
    0x000300f7, 0x0000002b, 0x00000000, 0x000400fa,
    0x00000028, 0x0000002a, 0x0000002f, 0x000200f8,
    0x0000002a, 0x00060041, 0x0000001e, 0x0000002d,
    0x0000001a, 0x0000001b, 0x0000002c, 0x0004003d,
    0x00000006, 0x0000002e, 0x0000002d, 0x0003003e,
    0x00000029, 0x0000002e, 0x000200f9, 0x0000002b,
    0x000200f8, 0x0000002f, 0x00060041, 0x0000001e,
    0x00000031, 0x0000001a, 0x0000001b, 0x00000030,
    0x0004003d, 0x00000006, 0x00000032, 0x00000031,
    0x0003003e, 0x00000029, 0x00000032, 0x000200f9,
    0x0000002b, 0x000200f8, 0x0000002b, 0x0004003d,
    0x00000006, 0x00000033, 0x00000029, 0x00050050,
    0x00000007, 0x00000034, 0x00000025, 0x00000033,
    0x0003003e, 0x00000009, 0x00000034, 0x0004003d,
    0x0000000a, 0x00000036, 0x0000000c, 0x000500c7,
    0x0000000a, 0x00000037, 0x00000036, 0x0000000e,
    0x000500aa, 0x00000011, 0x00000038, 0x00000037,
    0x00000010, 0x000300f7, 0x0000003b, 0x00000000,
    0x000400fa, 0x00000038, 0x0000003a, 0x0000003e,
    0x000200f8, 0x0000003a, 0x00060041, 0x0000001e,
    0x0000003c, 0x0000001a, 0x00000010, 0x0000001d,
    0x0004003d, 0x00000006, 0x0000003d, 0x0000003c,
    0x0003003e, 0x00000039, 0x0000003d, 0x000200f9,
    0x0000003b, 0x000200f8, 0x0000003e, 0x00060041,
    0x0000001e, 0x0000003f, 0x0000001a, 0x00000010,
    0x00000022, 0x0004003d, 0x00000006, 0x00000040,
    0x0000003f, 0x0003003e, 0x00000039, 0x00000040,
    0x000200f9, 0x0000003b, 0x000200f8, 0x0000003b,
    0x0004003d, 0x00000006, 0x00000041, 0x00000039,
    0x0004003d, 0x0000000a, 0x00000042, 0x0000000c,
    0x000500c7, 0x0000000a, 0x00000043, 0x00000042,
    0x0000001b, 0x000500aa, 0x00000011, 0x00000044,
    0x00000043, 0x00000010, 0x000300f7, 0x00000047,
    0x00000000, 0x000400fa, 0x00000044, 0x00000046,
    0x0000004a, 0x000200f8, 0x00000046, 0x00060041,
    0x0000001e, 0x00000048, 0x0000001a, 0x00000010,
    0x0000002c, 0x0004003d, 0x00000006, 0x00000049,
    0x00000048, 0x0003003e, 0x00000045, 0x00000049,
    0x000200f9, 0x00000047, 0x000200f8, 0x0000004a,
    0x00060041, 0x0000001e, 0x0000004b, 0x0000001a,
    0x00000010, 0x00000030, 0x0004003d, 0x00000006,
    0x0000004c, 0x0000004b, 0x0003003e, 0x00000045,
    0x0000004c, 0x000200f9, 0x00000047, 0x000200f8,
    0x00000047, 0x0004003d, 0x00000006, 0x0000004d,
    0x00000045, 0x00050050, 0x00000007, 0x0000004e,
    0x00000041, 0x0000004d, 0x0003003e, 0x00000035,
    0x0000004e, 0x00060041, 0x0000001e, 0x00000050,
    0x0000001a, 0x0000000e, 0x0000001d, 0x0004003d,
    0x00000006, 0x00000051, 0x00000050, 0x00050041,
    0x00000013, 0x00000052, 0x00000009, 0x0000001d,
    0x0004003d, 0x00000006, 0x00000053, 0x00000052,
    0x00050085, 0x00000006, 0x00000054, 0x00000051,
    0x00000053, 0x00060041, 0x0000001e, 0x00000055,
    0x0000001a, 0x0000000e, 0x0000002c, 0x0004003d,
    0x00000006, 0x00000056, 0x00000055, 0x00050041,
    0x00000013, 0x00000057, 0x00000009, 0x0000002c,
    0x0004003d, 0x00000006, 0x00000058, 0x00000057,
    0x00050085, 0x00000006, 0x00000059, 0x00000056,
    0x00000058, 0x00050081, 0x00000006, 0x0000005a,
    0x00000054, 0x00000059, 0x00060041, 0x0000001e,
    0x0000005b, 0x0000001a, 0x0000000e, 0x00000022,
    0x0004003d, 0x00000006, 0x0000005c, 0x0000005b,
    0x00050041, 0x00000013, 0x0000005d, 0x00000009,
    0x0000001d, 0x0004003d, 0x00000006, 0x0000005e,
    0x0000005d, 0x00050085, 0x00000006, 0x0000005f,
    0x0000005c, 0x0000005e, 0x00060041, 0x0000001e,
    0x00000060, 0x0000001a, 0x0000000e, 0x00000030,
    0x0004003d, 0x00000006, 0x00000061, 0x00000060,
    0x00050041, 0x00000013, 0x00000062, 0x00000009,
    0x0000002c, 0x0004003d, 0x00000006, 0x00000063,
    0x00000062, 0x00050085, 0x00000006, 0x00000064,
    0x00000061, 0x00000063, 0x00050081, 0x00000006,
    0x00000065, 0x0000005f, 0x00000064, 0x00050050,
    0x00000007, 0x00000066, 0x0000005a, 0x00000065,
    0x0003003e, 0x0000004f, 0x00000066, 0x0004003d,
    0x00000007, 0x0000006b, 0x0000004f, 0x00050051,
    0x00000006, 0x0000006e, 0x0000006b, 0x00000000,
    0x00050051, 0x00000006, 0x0000006f, 0x0000006b,
    0x00000001, 0x00070050, 0x00000017, 0x00000070,
    0x0000006e, 0x0000006f, 0x0000006c, 0x0000006d,
    0x00050041, 0x00000071, 0x00000072, 0x0000006a,
    0x00000010, 0x0003003e, 0x00000072, 0x00000070,
    0x0004003d, 0x00000007, 0x00000075, 0x00000035,
    0x0003003e, 0x00000074, 0x00000075, 0x000100fd,
    0x00010038
};
const size_t VKDrawingPanel::kVertSpvSize = sizeof(kVertSpv);
         
// Frag: 464 bytes
const uint32_t VKDrawingPanel::kFragSpv[] = {
    0x07230203u, 0x00010300u, 0x000D000Au, 0x0000001Eu, 0x00000000u, 0x00020011u,
    0x00000001u, 0x0003000Eu, 0x00000000u, 0x00000001u, 0x0007000Fu, 0x00000004u,
    0x0000000Eu, 0x6E69616Du, 0x00000000u, 0x0000000Cu, 0x0000000Du, 0x00030010u,
    0x0000000Eu, 0x00000007u, 0x00030003u, 0x00000002u, 0x000001C2u, 0x00040047u,
    0x0000000Bu, 0x00000022u, 0x00000000u, 0x00040047u, 0x0000000Bu, 0x00000021u,
    0x00000000u, 0x00040047u, 0x0000000Cu, 0x0000001Eu, 0x00000000u, 0x00040047u,
    0x0000000Du, 0x0000001Eu, 0x00000000u, 0x00020013u, 0x00000001u, 0x00030021u,
    0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u, 0x00000020u, 0x00040017u,
    0x00000004u, 0x00000003u, 0x00000002u, 0x00040017u, 0x00000005u, 0x00000003u,
    0x00000004u, 0x00090019u, 0x00000006u, 0x00000003u, 0x00000001u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x0003001Bu, 0x00000007u,
    0x00000006u, 0x00040020u, 0x00000008u, 0x00000000u, 0x00000007u, 0x00040020u,
    0x00000009u, 0x00000001u, 0x00000004u, 0x00040020u, 0x0000000Au, 0x00000003u,
    0x00000005u, 0x0004003Bu, 0x00000008u, 0x0000000Bu, 0x00000000u, 0x0004003Bu,
    0x00000009u, 0x0000000Cu, 0x00000001u, 0x0004003Bu, 0x0000000Au, 0x0000000Du,
    0x00000003u, 0x00050036u, 0x00000001u, 0x0000000Eu, 0x00000000u, 0x00000002u,
    0x000200F8u, 0x00000014u, 0x0004003Du, 0x00000007u, 0x00000015u, 0x0000000Bu,
    0x0004003Du, 0x00000004u, 0x00000016u, 0x0000000Cu, 0x00050057u, 0x00000005u,
    0x00000017u, 0x00000015u, 0x00000016u, 0x0003003Eu, 0x0000000Du, 0x00000017u,
    0x000100FDu, 0x00010038u,
};
const size_t VKDrawingPanel::kFragSpvSize = sizeof(kFragSpv);

#endif  // !NO_VULKAN

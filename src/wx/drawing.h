#ifndef VBAM_WX_DRAWING_H_
#define VBAM_WX_DRAWING_H_

#include "wx/wxvbam.h"

class BasicDrawingPanel : public DrawingPanel {
public:
    BasicDrawingPanel(wxWindow* parent, int _width, int _height);

protected:
    void DrawArea(wxWindowDC& dc);
    virtual void DrawImage(wxWindowDC& dc, wxImage* im);
};

// wx <= 2.8 may not be compiled with opengl support
#if !wxCHECK_VERSION(2, 9, 0) && !wxUSE_GLCANVAS
    #define NO_OGL
#endif

#ifndef NO_OGL
#include <wx/glcanvas.h>

// shuffled parms for 2.9 indicates non-auto glcontext
// before 2.9, wxMAC does not have this (but wxGTK & wxMSW do)
#if !wxCHECK_VERSION(2, 9, 0) && defined(__WXMAC__)
    #define wxglc(a, b, c, d, e, f) wxGLCanvas(a, b, d, e, f, wxEmptyString, c)
    #define wxGL_IMPLICIT_CONTEXT
#else
    #define wxglc wxGLCanvas
#endif

class GLDrawingPanel : public DrawingPanelBase, public wxGLCanvas {
public:
    GLDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~GLDrawingPanel();

protected:
    void DrawArea(wxWindowDC& dc) override;
    void PaintEv(wxPaintEvent& ev) override;
    void OnSize(wxSizeEvent& ev) override;
    void AdjustViewport();
    void RefreshGL();
#ifndef wxGL_IMPLICIT_CONTEXT
    wxGLContext* ctx = NULL;
#endif
    bool SetContext();
    void DrawingPanelInit() override;
    GLuint texid, vlist;
    int texsize;
};
#endif

#include <wx/string.h>

class SDLDrawingPanel : public DrawingPanel {
public:
    SDLDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~SDLDrawingPanel();

protected:
    void DrawArea();
    void DrawArea(uint8_t** data);
    void DrawArea(wxWindowDC&) override;
    void PaintEv(wxPaintEvent& ev) override;
    void EraseBackground(wxEraseEvent& ev) override;
    void OnSize(wxSizeEvent& ev) override;
    void DrawingPanelInit() override;

private:
    SDL_Window *sdlwindow = NULL;
    SDL_Texture *texture = NULL;
    SDL_Renderer *renderer = NULL;
    wxString renderername = wxEmptyString;

    DECLARE_EVENT_TABLE()
};

#if defined(__WXMSW__) && !defined(NO_D3D)
// Forward declarations for Direct3D 9
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DTexture9;

class DXDrawingPanel : public DrawingPanel {
public:
    DXDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~DXDrawingPanel();

protected:
    void DrawArea(wxWindowDC&);
    void DrawingPanelInit();
    void OnSize(wxSizeEvent& ev);

private:
    bool ResetDevice();

    IDirect3D9* d3d;
    IDirect3DDevice9* device;
    IDirect3DTexture9* texture;
    int texture_width;
    int texture_height;
    bool using_d3d9ex;
};
#endif

#if defined(__WXMSW__) && !defined(NO_D3D12)
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

static const UINT FRAME_COUNT = 2; // Double-buffering

using Microsoft::WRL::ComPtr;

class DX12DrawingPanel : public DrawingPanel {
public:
    DX12DrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~DX12DrawingPanel();
    
protected:
    void DrawArea(wxWindowDC&);
    void DrawingPanelInit();
    void OnSize(wxSizeEvent& ev);
    
private:
    void WaitForGPU();
    bool ResizeSwapChain();

    ComPtr<ID3D12Device>                device;
    ComPtr<ID3D12CommandQueue>          command_queue;
    ComPtr<IDXGISwapChain3>             swap_chain;
    ComPtr<ID3D12DescriptorHeap>        rtv_heap;
    ComPtr<ID3D12DescriptorHeap>        srv_heap;
    ComPtr<ID3D12Resource>              render_targets[FRAME_COUNT];
    ComPtr<ID3D12CommandAllocator>      command_allocator;
    ComPtr<ID3D12GraphicsCommandList>   command_list;
    ComPtr<ID3D12RootSignature>         root_signature;
    ComPtr<ID3D12PipelineState>         pipeline_state;
    ComPtr<ID3D12Resource>              vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW            vertex_buffer_view;
    ComPtr<ID3D12Resource>              texture;
    ComPtr<ID3D12Resource>              upload_heap;   // staging, kept alive per-frame
    ComPtr<ID3D12Fence>                 fence;
    UINT64                              fence_value;
    HANDLE                              fence_event;
    UINT                                frame_index;
    UINT                                rtv_descriptor_size;
    int                                 texture_width;
    int                                 texture_height;
    HMODULE                             hD3DCompiler;
    HMODULE                             hD3D12;
	HMODULE                             hDXGI;
};
#endif

#if defined(__WXMAC__)
#ifndef NO_METAL
#ifdef __OBJC__
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <CoreGraphics/CoreGraphics.h>
#endif

#include <simd/simd.h>

typedef enum AAPLVertexInputIndex
{
    AAPLVertexInputIndexVertices     = 0,
    AAPLVertexInputIndexViewportSize = 1,
} AAPLVertexInputIndex;

// Texture index values shared between shader and C code to ensure Metal shader buffer inputs match
//   Metal API texture set calls
typedef enum AAPLTextureIndex
{
    AAPLTextureIndexBaseColor = 0,
} AAPLTextureIndex;

//  This structure defines the layout of each vertex in the array of vertices set as an input to the
//    Metal vertex shader.  Since this header is shared between the .metal shader and C code,
//    you can be sure that the layout of the vertex array in the code matches the layout that
//    the vertex shader expects

typedef struct
{
    // Positions in pixel space. A value of 100 indicates 100 pixels from the origin/center.
    vector_float2 position;

    // 2D texture coordinate
    vector_float2 textureCoordinate;
} AAPLVertex;

bool is_macosx_1012_or_newer();

class MetalDrawingPanel : public DrawingPanel {
public:
    MetalDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~MetalDrawingPanel();

protected:
    void DrawArea();
    void DrawArea(uint8_t** data);
    void DrawArea(wxWindowDC&) override;
    void PaintEv(wxPaintEvent& ev) override;
    void EraseBackground(wxEraseEvent& ev) override;
    void OnSize(wxSizeEvent& ev) override;
    void DrawingPanelInit() override;

#ifdef __OBJC__
private:
    void CreateMetalView();
    id<MTLTexture> CreateTextureWithData(void *data, NSUInteger bytesPerRow);

    NSView *view;
    MTKView *metalView;
    NSRect metalFrame;
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    id<MTLRenderPipelineState> _pipelineState;
    id<MTLTexture> _texture;
    id<MTLBuffer> _vertices;
    id<MTLSamplerState> _sampler;
    NSUInteger _numVertices;
    vector_uint2 _viewportSize;
    vector_uint2 _contentSize;
    uint32_t *_conversion_buffer;
    size_t _conversion_buffer_size;
#endif
};
#endif

class Quartz2DDrawingPanel : public BasicDrawingPanel {
public:
    Quartz2DDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual void DrawImage(wxWindowDC& dc, wxImage* im);
};
#endif

#ifndef NO_VULKAN
#include <vulkan/vulkan.h>

#if defined(__WXMSW__)
#include <vulkan/vulkan_win32.h>
#elif defined(__WXMAC__)
#include <vulkan/vulkan_metal.h>
#elif defined(__WXGTK__)
#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xlib.h>
#endif

#include <vector>
#include <cstdint>
 
// Forward-declare the wxWindow types we reference
class wxWindow;
class wxWindowDC;
class wxSizeEvent;
 
class VKDrawingPanel : public DrawingPanel {
public:
    VKDrawingPanel(wxWindow* parent, int _width, int _height);
    virtual ~VKDrawingPanel();
 
protected:
    void DrawArea(wxWindowDC& dc) override;
    void DrawingPanelInit() override;
    void OnSize(wxSizeEvent& ev) override;
 
private:
    // ── Instance / surface ───────────────────────────────────────────────────
    bool CreateInstance();

#ifdef __WXMSW__
    bool CreateSurfaceWIN32();
#elif defined(__WXMAC__)
    bool CreateSurfaceMACOS();
#elif defined(__WXGTK__)
    bool CreateSurfaceXLIB(Window win);
    bool CreateSurfaceWAYLAND(struct wl_surface *wayland_surface, struct wl_display *wayland_display);
    bool CreateSurfaceUNIX();
#endif

    // ── Physical / logical device ────────────────────────────────────────────
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
 
    // ── Swapchain ────────────────────────────────────────────────────────────
    bool CreateSwapchain();
    void DestroySwapchain();
    bool RecreateSwapchain();         // Called on resize / VK_ERROR_OUT_OF_DATE_KHR
 
    // ── Image views & render pass ────────────────────────────────────────────
    bool CreateImageViews();
    bool CreateRenderPass();
    bool CreateFramebuffers();
 
    // ── Descriptor set layout & pipeline ────────────────────────────────────
    bool CreateDescriptorSetLayout();
    bool CreateGraphicsPipeline();
 
    // ── Command infrastructure ────────────────────────────────────────────────
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();
 
    // ── Staging texture ──────────────────────────────────────────────────────
    bool CreateTexture(uint32_t tex_w, uint32_t tex_h, VkFormat fmt);
    void DestroyTexture();
    bool CreateDescriptorPoolAndSet();
 
    // ── Upload helpers ───────────────────────────────────────────────────────
    bool UploadPixels(const uint8_t* src_pixels,
                      uint32_t      src_pitch_bytes,
                      VkFormat      fmt);
 
    // ── Memory helper ────────────────────────────────────────────────────────
    uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags props) const;
 
    // ── Vulkan handles ───────────────────────────────────────────────────────
    VkInstance               instance_         = VK_NULL_HANDLE;
    VkSurfaceKHR             surface_          = VK_NULL_HANDLE;
    VkPhysicalDevice         physical_device_  = VK_NULL_HANDLE;
    VkDevice                 device_           = VK_NULL_HANDLE;
 
    VkQueue                  graphics_queue_   = VK_NULL_HANDLE;
    VkQueue                  present_queue_    = VK_NULL_HANDLE;
    uint32_t                 graphics_family_  = UINT32_MAX;
    uint32_t                 present_family_   = UINT32_MAX;
 
    VkSwapchainKHR           swapchain_        = VK_NULL_HANDLE;
    VkFormat                 swapchain_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D               swapchain_extent_ = {};
    std::vector<VkImage>     swapchain_images_;
    std::vector<VkImageView> swapchain_views_;
    std::vector<VkFramebuffer> framebuffers_;
 
    VkRenderPass             render_pass_      = VK_NULL_HANDLE;
    VkDescriptorSetLayout    desc_set_layout_  = VK_NULL_HANDLE;
    VkPipelineLayout         pipeline_layout_  = VK_NULL_HANDLE;
    VkPipeline               pipeline_         = VK_NULL_HANDLE;
 
    VkDescriptorPool         desc_pool_        = VK_NULL_HANDLE;
    VkDescriptorSet          desc_set_         = VK_NULL_HANDLE;
 
    VkCommandPool            cmd_pool_         = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmd_buffers_;
 
    // Per-frame sync (double-buffered)
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    VkSemaphore image_available_sem_[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore render_finished_sem_[MAX_FRAMES_IN_FLIGHT] = {};
    VkFence     in_flight_fence_   [MAX_FRAMES_IN_FLIGHT] = {};
    int         current_frame_ = 0;
 
    // ── Texture resources ────────────────────────────────────────────────────
    VkImage        tex_image_        = VK_NULL_HANDLE;
    VkDeviceMemory tex_memory_       = VK_NULL_HANDLE;
    VkImageView    tex_view_         = VK_NULL_HANDLE;
    VkSampler      tex_sampler_      = VK_NULL_HANDLE;
    VkFormat       tex_format_       = VK_FORMAT_UNDEFINED;
 
    // Staging buffer (CPU-writable, uploaded each frame)
    VkBuffer       staging_buffer_   = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory_   = VK_NULL_HANDLE;
    VkDeviceSize   staging_size_     = 0;
 
    // ── State ────────────────────────────────────────────────────────────────
    uint32_t texture_width_  = 0;
    uint32_t texture_height_ = 0;
    bool     vsync_          = false;
    bool     have_wayland_surface_ = false;
    bool     have_xlib_surface_ = false;

    // Shader SPIR-V (compiled inline via glslang or supplied pre-compiled).
    // Stored as static constexpr uint32_t arrays at the bottom of the .cpp.
    static const uint32_t kVertSpv[];
    static const size_t   kVertSpvSize;
    static const uint32_t kFragSpv[];
    static const size_t   kFragSpvSize;
};
#endif // !defined(NO_VULKAN)

#endif // VBAM_WX_DRAWING_H_

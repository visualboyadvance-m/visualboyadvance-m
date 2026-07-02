#ifndef VBAM_WX_DRAWING_H_
#define VBAM_WX_DRAWING_H_

#include "wx/wxvbam.h"
#include "wx/wayland.h"

class BasicDrawingPanel : public DrawingPanel {
public:
    BasicDrawingPanel(wxWindow* parent, int _width, int _height);

protected:
    void DrawArea(wxWindowDC& dc);
    virtual void DrawImage(wxWindowDC& dc, wxImage* im);
};

#if defined(HAVE_WAYLAND_SUPPORT)
// Native Wayland software driver backing the Simple renderer on Wayland. When
// HDR is on and the compositor supports BT.2020 PQ color management, it presents
// a true 10-bit HDR image through a dedicated child wl_subsurface: a CPU-filled
// shm buffer in ABGR2101010 (PQ-encoded from the corrected RGBA8 frame), nearest-
// scaled to the panel and tagged BT.2020 PQ via wp_color_manager_v1 -- the same
// compositor color-management path the OpenGL renderer uses, but with a software
// (shm) buffer instead of an EGL surface, so it needs no GPU 3D context. Without
// HDR (or color-management support) it falls back to the ordinary 8-bit wx
// software path. Structurally mirrors the X11 XOrg deep-color driver.
class WaylandDrawingPanel : public BasicDrawingPanel {
public:
    WaylandDrawingPanel(wxWindow* parent, int _width, int _height);
    ~WaylandDrawingPanel() override;
    void DrawImage(wxWindowDC& dc, wxImage* im) override;

    // wl_buffer.release trampoline -> mark the buffer free for reuse. Public so
    // the file-static wl_buffer_listener can reference it; not for general use.
    static void BufferReleaseCb(void* data, struct wl_buffer* buf);

protected:
    // True once the child subsurface + an ABGR2101010 shm buffer were obtained
    // and the compositor accepted a BT.2020 PQ tag (set in EnsureSetup).
    bool SupportsHdr() const override { return wl_hdr_ready_; }
    hdr::Encoding PreferredHdrEncoding() const override { return hdr::Encoding::kPQ10; }

private:
    void EnsureSetup();
    void Teardown();
    bool EnsureBuffers(int w, int h);   // (re)allocate the shm pool/buffers
    int  AcquireBuffer();               // index of a free buffer, or -1
    bool PresentHdr();                  // returns false to fall back to wx path

    bool  wl_setup_done_ = false;
    bool  wl_hdr_ready_  = false;  // subsurface + 2101010 shm + CM PQ obtained
    bool  wl_mapped_     = false;  // subsurface currently presenting (vs hidden)

    // Opaque Wayland handles (kept void* so drawing.h needs no Wayland headers).
    void* wl_display_         = nullptr;  // wl_display*       (not owned)
    void* wl_queue_           = nullptr;  // wl_event_queue*   (owned) -- our
                                          // objects live here so dispatching
                                          // buffer-release events never reenters
                                          // GTK's default-queue handlers
    void* wl_parent_surface_  = nullptr;  // wl_surface*       (GTK's, not owned)
    void* wl_compositor_      = nullptr;  // wl_compositor*    (owned)
    void* wl_subcompositor_   = nullptr;  // wl_subcompositor* (owned)
    void* wl_shm_             = nullptr;  // wl_shm*           (owned)
    void* wl_child_surface_   = nullptr;  // wl_surface*       (owned)
    void* wl_subsurface_      = nullptr;  // wl_subsurface*    (owned)
    // Optional wp_viewporter: when present, the buffer is source-resolution and
    // the compositor scales it to the window, so we PQ-encode only source pixels
    // (not the whole window) each frame. Null -> CPU nearest-scale fallback.
    void* wl_viewporter_      = nullptr;  // wp_viewporter*    (owned)
    void* wl_viewport_        = nullptr;  // wp_viewport*      (owned)
    int   wl_vp_dst_w_        = 0;        // last viewport destination set
    int   wl_vp_dst_h_        = 0;

    // Double-buffered shm: one pool mapping holding two buffers, ping-ponged so
    // we never overwrite a buffer the compositor is still reading.
    void* wl_pool_            = nullptr;  // wl_shm_pool*
    void* wl_buffer_[2]       = {nullptr, nullptr};  // wl_buffer*
    uint8_t* wl_buffer_data_[2] = {nullptr, nullptr}; // mmap'd CPU pointers
    bool  wl_buffer_busy_[2]  = {false, false};       // held by compositor
    int   wl_pool_fd_         = -1;
    size_t wl_pool_size_      = 0;
    int   wl_buf_w_           = 0;
    int   wl_buf_h_           = 0;

    std::vector<uint8_t> wl_scaled_;  // RGBA8 nearest-scale scratch
};
#endif  // HAVE_WAYLAND_SUPPORT

#if defined(__WXGTK__)
// Native "XOrg" software driver that backs the Simple renderer on X11. On a
// depth-30 screen with deep color on it presents a true 10-bit image through a
// dedicated child X window (the wx panel's own window is normally a depth-24
// visual that cannot accept a 10-bit image) via XPutImage; without a 10-bit
// visual it falls back to the ordinary 8-bit wx DrawImage path. Uses no Wayland
// headers -- it is an X11-only path (Wayland keeps the plain wx software path).
class XOrgDrawingPanel : public BasicDrawingPanel {
public:
    XOrgDrawingPanel(wxWindow* parent, int _width, int _height);
    ~XOrgDrawingPanel() override;
    void DrawImage(wxWindowDC& dc, wxImage* im) override;

protected:
    // True once a depth-30 visual and its child window were obtained.
    bool DeepColorActive() const override { return xorg_deep_color_; }

private:
    void EnsureSetup();
    void Teardown();

    bool          xorg_setup_done_ = false;
    bool          xorg_deep_color_ = false;  // got a depth-30 visual + child window
    bool          xorg_mapped_     = false;  // child window currently mapped
    void*         xorg_display_    = nullptr; // Display*
    unsigned long xorg_window_     = 0;       // child Window (depth 30)
    void*         xorg_gc_         = nullptr; // GC
    void*         xorg_visual_     = nullptr; // Visual*
    void*         xorg_image_      = nullptr; // XImage*
    int           xorg_img_w_      = 0;
    int           xorg_img_h_      = 0;
    int           xorg_r_shift_    = 20;
    int           xorg_g_shift_    = 10;
    int           xorg_b_shift_    = 0;
    std::vector<uint32_t> xorg_buf_;          // 2101010 scratch (also XImage data)
};
#endif  // __WXGTK__

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
    bool SupportsHdr() const override;
    hdr::Encoding PreferredHdrEncoding() const override { return hdr::Encoding::kPQ10; }
    bool DeepColorActive() const override { return gl_deep_color_; }
    // True if the EGL display exposes EGL_EXT_gl_colorspace_bt2020_pq, i.e. we
    // can actually create the BT.2020 PQ surface HDR presentation needs.
    bool HdrEglColorspaceAvailable() const;
    // Set up / tear down the BT.2020 PQ EGL surface on Wayland. No-ops elsewhere.
    void SetupHdrSurface();
    void DestroyHdrSurface();
#ifndef wxGL_IMPLICIT_CONTEXT
    wxGLContext* ctx = NULL;
#endif
    bool SetContext();
    void DrawingPanelInit() override;
    GLuint texid, vlist;
    int texsize;
    void* hdr_egl_surface_ = nullptr;  // EGLSurface (Wayland PQ); nullptr if none
    bool  gl_deep_color_ = false;       // got a >=10-bit visual for deep color
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
#ifdef ENABLE_SDL3
    // SDL3 presents HDR via a linear-scRGB renderer + float texture, except on
    // Wayland, where SDL can't expose HDR on our borrowed subsurface so we drive
    // BT.2020 PQ through the compositor and feed a 10-bit PQ texture instead.
    bool SupportsHdr() const override { return sdl_hdr_; }
    hdr::Encoding PreferredHdrEncoding() const override {
        return sdl_wl_hdr_tagged_ ? hdr::Encoding::kPQ10 : hdr::Encoding::kScRGBFp16;
    }
    // SDL auto-multiplies the renderer's SDR white point into the color scale, so
    // to emit absolute nits (as the PQ renderers do) map 80 * white_point nits to
    // 1.0 -- the encoder's divide then cancels SDL's multiply. macOS EDR is a
    // relative surface (SDR white == system brightness), so there keep the
    // reference-white model (return 0 -> the reference maps to 1.0).
    float HdrScRgbWhiteNits() const override {
#if defined(__WXMAC__)
        return 0.0f;
#else
        return sdl_sdr_white_point_ > 1.0f ? 80.0f * sdl_sdr_white_point_ : 0.0f;
#endif
    }
#endif
    // 10-bit "deep color" SDR (X11): active when the chosen SDL backend could
    // create a 2101010 texture for this panel.
    bool DeepColorActive() const override { return sdl_deep_color_; }

private:
    SDL_Window *sdlwindow = NULL;
    SDL_Texture *texture = NULL;
    SDL_Renderer *renderer = NULL;
    wxString renderername = wxEmptyString;
    bool sdl_hdr_ = false;            // HDR active on this SDL3 renderer
    // The renderer's SDR white point (SDL_PROP_RENDERER_SDR_WHITE_POINT_FLOAT),
    // which SDL auto-multiplies into the color scale. 1.0 on relative surfaces
    // (macOS EDR); > 1 on Windows scRGB swapchains (system SDR white / 80).
    float sdl_sdr_white_point_ = 1.0f;
    // Wayland only: HDR driven through the compositor (child surface tagged
    // BT.2020 PQ, 10-bit PQ texture) rather than SDL's own scRGB HDR path.
    // Always declared so the upload/encoding branches compile everywhere.
    bool sdl_wl_hdr_tagged_ = false;
    // The Wayland child wl_surface SDL presents into (as void* to keep Wayland
    // headers out of drawing.h). Always declared so the SDL HDR renderer branch
    // compiles everywhere; only ever non-null under HAVE_WAYLAND_SUPPORT, where
    // it is created and torn down.
    void* sdl_wl_child_surface_ = nullptr;  // wl_surface* (owned; given to SDL)
    std::vector<uint8_t> sdl_hdr_buf_; // scRGB fp16 scratch for HDR upload
    bool sdl_deep_color_ = false;      // 10-bit SDR texture active (X11 deep color)
#ifdef ENABLE_SDL3
    SDL_PixelFormat sdl_deep_color_fmt_ = SDL_PIXELFORMAT_UNKNOWN; // chosen 10-bit format
#endif
    std::vector<uint8_t> sdl_deep_color_buf_; // 2101010 scratch for deep-color upload

#if defined(HAVE_WAYLAND_SUPPORT)
    // On native Wayland, SDL renders into a dedicated child wl_subsurface: it
    // cannot present into GTK's own wl_surface (GTK already drives it), the same
    // reason VKDrawingPanel uses a subsurface. Opaque void* to keep Wayland
    // headers out of drawing.h; created in DrawingPanelInit, torn down in the
    // destructor, repositioned in OnSize.
    void* sdl_wl_compositor_    = nullptr;  // wl_compositor*    (owned)
    void* sdl_wl_subcompositor_ = nullptr;  // wl_subcompositor* (owned)
    // sdl_wl_child_surface_ (the wl_surface*) is declared unconditionally above.
    void* sdl_wl_subsurface_    = nullptr;  // wl_subsurface*    (owned)
    void* sdl_wl_queue_         = nullptr;  // wl_event_queue*   (owned; private bind queue)
#endif

#if defined(__WXMAC__)
    // State captured before SDL takes over the window's contentView, used to undo
    // it after SDL is set up so the wx status bar stays visible (DrawingPanelInit).
    void* sdl_cocoa_view_         = nullptr;  // NSView* handed to SDL (weak)
    void* sdl_saved_window_       = nullptr;  // NSWindow* (weak)
    void* sdl_saved_content_view_ = nullptr;  // original contentView (retained)
    void* sdl_saved_superview_    = nullptr;  // sdl_cocoa_view_'s original superview (weak)
#endif

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
#include <dxgi1_6.h>
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
    bool SupportsHdr() const override { return hdr_swapchain_; }
    hdr::Encoding PreferredHdrEncoding() const override { return hdr::Encoding::kPQ10; }

private:
    void WaitForGPU();
    bool ResizeSwapChain();

    bool        hdr_swapchain_ = false;                       // HDR10 PQ swapchain active
    DXGI_FORMAT rt_format_      = DXGI_FORMAT_B8G8R8A8_UNORM;  // back-buffer / RTV format

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

#if defined(__WXMSW__) && !defined(NO_D3D11)
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

// D3D11 software-style driver backing the Simple renderer on Windows. Windows
// has no CPU-mappable HDR path in plain GDI, so the lightweight "Simple" route
// to HDR is a minimal flip-model DXGI swapchain. Each frame the corrected image
// is uploaded at its *source* resolution into a small DYNAMIC texture and drawn
// as a full-screen quad, letting the GPU scale it to the window via the sampler
// -- so the CPU only ever touches source-sized pixels (no full-window upscale)
// and there is no extra copy to the back buffer. HDR encodes the source to
// 10-bit BT.2020 PQ on the CPU into an R10G10B10A2 texture and presents it to a
// 10-bit back buffer tagged PQ via SetColorSpace1/SetHDRMetaData (mirroring the
// D3D12 path); SDR uploads B8G8R8A8 to an ordinary 8-bit back buffer. The device
// is created on hardware, falling back to WARP (the CPU rasterizer -- the true
// "software" tier); if even that fails it extends BasicDrawingPanel, so it falls
// back to the plain wx software path and Simple is never unusable.
class DX11DrawingPanel : public BasicDrawingPanel {
public:
    DX11DrawingPanel(wxWindow* parent, int _width, int _height);
    ~DX11DrawingPanel() override;
    // Like the other Simple sub-drivers (Quartz2D/XOrg/Wayland) we override
    // DrawImage so BasicDrawingPanel::DrawArea still does all the bit-depth ->
    // image conversion: HDR presents PQ10 from the 32-bit todraw, SDR presents
    // the 24-bit `im`. Falls back to the base wx path if the device is gone.
    void DrawImage(wxWindowDC& dc, wxImage* im) override;

protected:
    bool SupportsHdr() const override { return hdr_swapchain_; }
    hdr::Encoding PreferredHdrEncoding() const override { return hdr::Encoding::kPQ10; }

private:
    bool CreateDeviceAndSwapchain();   // hardware then WARP; false -> wx fallback
    bool InitPipeline();               // compile shaders, build quad VB + sampler
    void ApplyHdrColorSpace();         // set BT.2020 PQ colorspace; retried until it takes
    bool EnsureSourceTexture(int w, int h, DXGI_FORMAT fmt);  // DYNAMIC upload tex
    bool ResizeSwapchain(int w, int h);                       // also rebuilds the RTV
    bool PresentFrame(wxImage* im);    // returns false to fall back to wx path
    void Teardown();

    bool        hdr_swapchain_ = false;                       // HDR10 PQ active
    bool        colorspace_applied_ = false;                   // SetColorSpace1 took
    DXGI_FORMAT rt_format_     = DXGI_FORMAT_B8G8R8A8_UNORM;   // back-buffer format

    Microsoft::WRL::ComPtr<ID3D11Device>             device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>      context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3>          swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   rtv_;            // back-buffer RTV
    // Minimal blit pipeline: a full-screen quad samples a source-sized texture,
    // so the GPU does the upscale to the window.
    Microsoft::WRL::ComPtr<ID3D11VertexShader>       vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>        ps_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>        input_layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       sampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>    rasterizer_;     // solid, no cull
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          src_tex_;        // DYNAMIC, source-sized
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> src_srv_;
    bool   pipeline_ready_ = false;  // shaders/quad/sampler built
    bool   allow_tearing_ = false;
    int    sc_w_  = 0, sc_h_  = 0;   // current swapchain (back-buffer) size
    int    src_w_ = 0, src_h_ = 0;   // current source-texture size
    DXGI_FORMAT src_fmt_ = DXGI_FORMAT_UNKNOWN;  // current source-texture format
};
#endif

#if defined(__WXMAC__)
bool is_macosx_1012_or_newer();
bool is_macosx_11_or_newer();

// Undo SDL's macOS window takeover so the wx tree (status bar) stays live while
// SDL renders. Capture the handed view's state just before SDL_CreateWindow;
// reattach (re-nest the view, restore wx's contentView) after SDL is set up.
void VbamSdlCaptureViewState(void* sdl_view, void** out_window, void** out_content_view, void** out_superview);
void VbamSdlReattachViewState(void* sdl_view, void* window, void* content_view, void* superview);

// Remove any leftover SDL Metal subviews (CAMetalLayer-backed) from the borrowed
// host (GameArea) NSView. SDL does not detach the SDL3_cocoametalview it adds to
// an externally provided view when its window/renderer is destroyed, so it (and
// the layer holding the last presented frame) lingers and shows through as stale
// bars after a sub-renderer switch. Called on SDL panel teardown.
void VbamRemoveSdlMetalViews();

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
    // macOS EDR: available when the screen has HDR headroom and a float layer
    // was created (set in CreateMetalView). Uses linear scRGB fp16 in P3.
    bool SupportsHdr() const override { return metal_is_hdr_; }
    hdr::Encoding PreferredHdrEncoding() const override { return hdr::Encoding::kScRGBFp16; }
    bool HdrScRgbUsesP3() const override { return true; }
    bool metal_is_hdr_ = false;

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
    void DrawImage(wxWindowDC& dc, wxImage* im) override;

protected:
    // CoreGraphics can present EDR by drawing an extended-linear Display P3
    // float image into an EDR-enabled layer. SupportsHdr() checks the EDR
    // headroom of the screen this window is actually on (see macsupport.mm),
    // which is a stronger test than the global startup probe -- the window may
    // be on an SDR external display even when the main screen supports EDR.
    bool SupportsHdr() const override;
    hdr::Encoding PreferredHdrEncoding() const override { return hdr::Encoding::kScRGBFp16; }
    bool HdrScRgbUsesP3() const override { return true; }
};
#endif

#ifndef NO_VULKAN
// On Windows and Linux the Vulkan loader is resolved at run time (LoadLibrary /
// dlopen + vkGetInstanceProcAddr; see the function-pointer table and VbamVulkan*
// loaders in panel.cpp), so the import library is not linked. VK_NO_PROTOTYPES
// suppresses the linked prototypes so that table can define the symbols instead.
// macOS still links Vulkan (MoltenVK) for now, so it keeps the prototypes.
#if defined(__WXMSW__) || defined(__WXGTK__)
#define VK_NO_PROTOTYPES
#endif
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
    // HDR is available when we obtained an HDR10 PQ swapchain (set in
    // CreateSwapchain). Works on any platform whose surface advertises it.
    bool SupportsHdr() const override { return swapchain_is_hdr_; }
    bool DeepColorActive() const override { return vk_deep_color_; }
    hdr::Encoding PreferredHdrEncoding() const override { return hdr::Encoding::kPQ10; }

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
    bool                     swapchain_is_hdr_ = false;  // HDR10 PQ swapchain
    bool                     vk_deep_color_    = false;  // 10-bit SDR swapchain (X11 deep color)
    bool                     hdr_metadata_ext_ = false;  // VK_EXT_hdr_metadata enabled
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

#if defined(__WXGTK__) && !defined(NO_WAYLAND)
    // On Wayland the Vulkan swapchain cannot present to GTK's own wl_surface
    // (GTK already drives it). We create a dedicated child wl_subsurface and
    // present to that instead. These are owned by this panel.
    struct wl_compositor*    wl_compositor_    = nullptr;
    struct wl_subcompositor* wl_subcompositor_ = nullptr;
    struct wl_surface*       wl_child_surface_ = nullptr;
    struct wl_subsurface*    wl_subsurface_    = nullptr;
    // Create the child surface/subsurface under `parent`; returns the child
    // wl_surface to build the Vulkan surface on, or nullptr on failure.
    struct wl_surface* CreateWaylandSubsurface(struct wl_display* dpy,
                                               struct wl_surface* parent);
    void DestroyWaylandSubsurface();
    void PositionWaylandSubsurface();
#endif

    // Shader SPIR-V (compiled inline via glslang or supplied pre-compiled).
    // Stored as static constexpr uint32_t arrays at the bottom of the .cpp.
    static const uint32_t kVertSpv[];
    static const size_t   kVertSpvSize;
    static const uint32_t kFragSpv[];
    static const size_t   kFragSpvSize;
};
#endif // !defined(NO_VULKAN)

#endif // VBAM_WX_DRAWING_H_

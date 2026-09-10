#ifndef VBAM_QT_RENDERERS_D3D_PANEL_H_
#define VBAM_QT_RENDERERS_D3D_PANEL_H_

#include "qt/drawing-panel.h"

#if defined(_WIN32)

#include <windows.h>

#if !defined(NO_D3D)
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DTexture9;

// Direct3D 9 (9Ex when available) renderer into this panel's HWND. Port of
// the wx DXDrawingPanel.
class DXDrawingPanel final : public NativeDrawingPanel {
    Q_OBJECT

public:
    DXDrawingPanel(QWidget* parent, int _width, int _height);
    ~DXDrawingPanel() override;

protected:
    void DrawingPanelInit() override;
    void Present() override;
    void OnNativeResize(const QSize& device_pixels) override;

private:
    bool ResetDevice();

    HMODULE hD3D9_ = nullptr;
    IDirect3D9* d3d_ = nullptr;
    IDirect3DDevice9* device_ = nullptr;
    IDirect3DTexture9* texture_ = nullptr;
    int texture_width_ = 0;
    int texture_height_ = 0;
    bool using_d3d9ex_ = false;
};
#endif  // !NO_D3D

#if !defined(NO_D3D12)
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

// Direct3D 12 renderer into this panel's HWND (flip-model swapchain, textured
// quad). Port of the wx DX12DrawingPanel without its HDR10 swapchain path.
class DX12DrawingPanel final : public NativeDrawingPanel {
    Q_OBJECT

public:
    DX12DrawingPanel(QWidget* parent, int _width, int _height);
    ~DX12DrawingPanel() override;

protected:
    void DrawingPanelInit() override;
    void Present() override;
    void OnNativeResize(const QSize& device_pixels) override;

private:
    static constexpr UINT kFrameCount = 2;  // Double-buffering

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    bool Setup();
    void WaitForGPU();
    bool ResizeSwapChain();

    DXGI_FORMAT rt_format_ = DXGI_FORMAT_B8G8R8A8_UNORM;

    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> command_queue_;
    ComPtr<IDXGISwapChain3> swap_chain_;
    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    ComPtr<ID3D12DescriptorHeap> srv_heap_;
    ComPtr<ID3D12Resource> render_targets_[kFrameCount];
    ComPtr<ID3D12CommandAllocator> command_allocator_;
    ComPtr<ID3D12GraphicsCommandList> command_list_;
    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_state_;
    ComPtr<ID3D12Resource> vertex_buffer_;
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view_ = {};
    ComPtr<ID3D12Resource> texture_;
    ComPtr<ID3D12Resource> upload_heap_;  // staging, kept alive per-frame
    ComPtr<ID3D12Fence> fence_;
    UINT64 fence_value_ = 0;
    HANDLE fence_event_ = nullptr;
    UINT frame_index_ = 0;
    UINT rtv_descriptor_size_ = 0;
    int texture_width_ = 0;
    int texture_height_ = 0;
    HMODULE hD3DCompiler_ = nullptr;
    HMODULE hD3D12_ = nullptr;
    HMODULE hDXGI_ = nullptr;
    std::vector<uint8_t> rgba_;
};
#endif  // !NO_D3D12

#endif  // _WIN32

#endif  // VBAM_QT_RENDERERS_D3D_PANEL_H_

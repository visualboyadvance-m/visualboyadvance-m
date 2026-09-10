#include "qt/renderers/d3d-panel.h"

#if defined(_WIN32)

#include <cmath>
#include <cstring>

#include <QCoreApplication>

#include "core/base/system.h"
#include "qt/config/option-proxy.h"
#include "qt/log.h"

namespace {

QString Tr(const char* s) {
    return QCoreApplication::translate("vbam", s);
}

QString Hr(HRESULT hr) {
    return QStringLiteral("0x%1").arg(static_cast<unsigned long>(hr), 8, 16, QLatin1Char('0'));
}

}  // namespace

// ============================================================================
// Direct3D 9
// ============================================================================

#if !defined(NO_D3D)

#include <d3d9.h>

namespace {
typedef HRESULT(WINAPI* LPDIRECT3DCREATE9EX)(UINT, IDirect3D9Ex**);
typedef IDirect3D9*(WINAPI* LPDIRECT3DCREATE9)(UINT);
}  // namespace

DXDrawingPanel::DXDrawingPanel(QWidget* parent, int _width, int _height)
    : NativeDrawingPanel(parent, _width, _height) {
    HRESULT hr = E_FAIL;

    // Direct3D 9Ex (Vista+) when available, else plain Direct3D 9; both via
    // dynamic loading so the binary has no import-time dependency on d3d9.dll.
    hD3D9_ = LoadLibraryW(L"d3d9.dll");
    if (!hD3D9_) {
        vbam::LogError(Tr("Failed to load d3d9.dll"));
        DrawingPanelInit();
        return;
    }

    auto create9ex = reinterpret_cast<LPDIRECT3DCREATE9EX>(
        reinterpret_cast<void*>(GetProcAddress(hD3D9_, "Direct3DCreate9Ex")));
    if (create9ex) {
        hr = create9ex(D3D_SDK_VERSION, reinterpret_cast<IDirect3D9Ex**>(&d3d_));
        if (SUCCEEDED(hr)) {
            using_d3d9ex_ = true;
            vbam::LogDebug(QStringLiteral("Using Direct3D 9Ex"));
        }
    }
    if (!d3d_) {
        auto create9 = reinterpret_cast<LPDIRECT3DCREATE9>(
            reinterpret_cast<void*>(GetProcAddress(hD3D9_, "Direct3DCreate9")));
        if (create9)
            d3d_ = create9(D3D_SDK_VERSION);
        if (!d3d_) {
            vbam::LogError(Tr("Failed to create Direct3D 9 interface"));
            DrawingPanelInit();
            return;
        }
        vbam::LogDebug(QStringLiteral("Using Direct3D 9"));
    }

    D3DDISPLAYMODE d3ddm;
    hr = d3d_->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to get adapter display mode: %1").arg(Hr(hr)));
        DrawingPanelInit();
        return;
    }

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferWidth = 0;   // window size
    d3dpp.BackBufferHeight = 0;  // window size
    d3dpp.PresentationInterval =
        OPTION(kPrefVsync) ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

    const HWND hwnd = static_cast<HWND>(NativeHandle());

    // FlipEx first (D3D9Ex), then Discard.
    for (int swap_attempt = 0; swap_attempt < 2; swap_attempt++) {
        if (swap_attempt == 0 && using_d3d9ex_) {
            d3dpp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
            d3dpp.BackBufferCount = 2;
        } else {
            d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            d3dpp.BackBufferCount = 0;
        }

        hr = d3d_->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &device_);
        if (FAILED(hr)) {
            hr = d3d_->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                    D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &device_);
        }
        if (SUCCEEDED(hr) || !using_d3d9ex_)
            break;
    }

    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create Direct3D device: %1").arg(Hr(hr)));
        device_ = nullptr;
    }

    DrawingPanelInit();
}

DXDrawingPanel::~DXDrawingPanel() {
    StopFilterThreads();
    if (texture_) { texture_->Release(); texture_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
    if (d3d_) { d3d_->Release(); d3d_ = nullptr; }
    if (hD3D9_) { FreeLibrary(hD3D9_); hD3D9_ = nullptr; }
}

void DXDrawingPanel::DrawingPanelInit() {
    DrawingPanelBase::DrawingPanelInit();
    if (!device_) {
        init_failed_ = true;
        return;
    }
    texture_width_ = ScaledWidth();
    texture_height_ = ScaledHeight();
    did_init = true;
}

bool DXDrawingPanel::ResetDevice() {
    if (!device_ || !d3d_)
        return false;

    // D3DPOOL_DEFAULT resources must be released before Reset().
    if (texture_) {
        texture_->Release();
        texture_ = nullptr;
        texture_width_ = 0;
        texture_height_ = 0;
    }

    D3DDISPLAYMODE d3ddm;
    HRESULT hr = d3d_->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr))
        return false;

    const QSize px = DevicePixelSize();
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferWidth = px.width();
    d3dpp.BackBufferHeight = px.height();
    d3dpp.PresentationInterval =
        OPTION(kPrefVsync) ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    if (using_d3d9ex_) {
        d3dpp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
        d3dpp.BackBufferCount = 2;
    } else {
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferCount = 0;
    }

    hr = device_->Reset(&d3dpp);
    if (FAILED(hr) && using_d3d9ex_) {
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferCount = 0;
        hr = device_->Reset(&d3dpp);
    }
    if (FAILED(hr)) {
        vbam::LogDebug(QStringLiteral("Failed to reset Direct3D device: %1").arg(Hr(hr)));
        return false;
    }
    return true;
}

void DXDrawingPanel::OnNativeResize(const QSize& device_pixels) {
    (void)device_pixels;
    if (device_)
        ResetDevice();
}

void DXDrawingPanel::Present() {
    if (!device_)
        return;

    if (!todraw) {
        device_->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        if (using_d3d9ex_)
            static_cast<IDirect3DDevice9Ex*>(device_)->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
        else
            device_->Present(nullptr, nullptr, nullptr, nullptr);
        return;
    }

    const bool out_8 = panel_color_depth_ == 8;
    const bool out_16 = panel_color_depth_ == 16;
    const bool out_24 = panel_color_depth_ == 24;
    const D3DFORMAT tex_format = out_16 ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;

    const int scaled_width = ScaledWidth();
    const int scaled_height = ScaledHeight();

    if (!texture_ || texture_width_ != scaled_width || texture_height_ != scaled_height) {
        if (texture_) {
            texture_->Release();
            texture_ = nullptr;
        }
        const HRESULT hr = device_->CreateTexture(scaled_width, scaled_height, 1, D3DUSAGE_DYNAMIC,
                                                  tex_format, D3DPOOL_DEFAULT, &texture_, nullptr);
        if (FAILED(hr)) {
            vbam::LogDebug(QStringLiteral("Failed to create texture: %1").arg(Hr(hr)));
            return;
        }
        texture_width_ = scaled_width;
        texture_height_ = scaled_height;
    }

    D3DLOCKED_RECT locked_rect;
    if (FAILED(texture_->LockRect(0, &locked_rect, nullptr, D3DLOCK_DISCARD)))
        return;

    const int src_pitch = SourcePitch();
    const uint8_t* src = SourcePixels();
    uint8_t* dst = static_cast<uint8_t*>(locked_rect.pBits);

    if (out_8) {
        for (int y = 0; y < scaled_height; y++) {
            const uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                const uint8_t p = *src_row++;
                if (p == 0xff) {
                    dst_row[0] = dst_row[1] = dst_row[2] = 0xff;
                } else {
                    dst_row[0] = (p & 0x3) << 6;         // B
                    dst_row[1] = ((p >> 2) & 0x7) << 5;  // G
                    dst_row[2] = ((p >> 5) & 0x7) << 5;  // R
                }
                dst_row[3] = 0;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    } else if (out_16) {
        // RGB555 (R=14:10, G=9:5, B=4:0) -> R5G6B5.
        const uint16_t* src16 = reinterpret_cast<const uint16_t*>(src);
        for (int y = 0; y < scaled_height; y++) {
            const uint16_t* src_row = src16;
            uint16_t* dst_row = reinterpret_cast<uint16_t*>(dst);
            for (int x = 0; x < scaled_width; x++, src_row++) {
                const uint16_t pixel = *src_row;
                const uint8_t r5 = (pixel >> 10) & 0x1f;
                const uint8_t g5 = (pixel >> 5) & 0x1f;
                const uint8_t b5 = pixel & 0x1f;
                const uint8_t g6 = static_cast<uint8_t>((g5 << 1) | (g5 >> 4));
                *dst_row++ = static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
            }
            src16 += src_pitch / 2;
            dst += locked_rect.Pitch;
        }
    } else if (out_24) {
        for (int y = 0; y < scaled_height; y++) {
            const uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                dst_row[0] = src_row[2];  // B
                dst_row[1] = src_row[1];  // G
                dst_row[2] = src_row[0];  // R
                dst_row[3] = 0;
                src_row += 3;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    } else {
        // 32-bit R,G,B,X -> B,G,R,X
        for (int y = 0; y < scaled_height; y++) {
            const uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                dst_row[0] = src_row[2];
                dst_row[1] = src_row[1];
                dst_row[2] = src_row[0];
                dst_row[3] = src_row[3];
                src_row += 4;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    }

    texture_->UnlockRect(0);

    device_->BeginScene();
    device_->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    device_->SetTexture(0, texture_);

    const D3DTEXTUREFILTERTYPE filter = OPTION(kDispBilinear) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    device_->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
    device_->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
    device_->SetRenderState(D3DRS_LIGHTING, FALSE);
    device_->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

    const QSize px = DevicePixelSize();
    const float win_width = static_cast<float>(px.width());
    const float win_height = static_cast<float>(px.height());

    struct Vertex {
        float x, y, z, rhw;
        float u, v;
    };
    // The panel widget is already aspect-fitted by GameArea: fill it.
    const Vertex vertices[4] = {
        {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {win_width, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f},
        {win_width, win_height, 0.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, win_height, 0.0f, 1.0f, 0.0f, 1.0f},
    };
    device_->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertex));
    device_->EndScene();

    if (using_d3d9ex_)
        static_cast<IDirect3DDevice9Ex*>(device_)->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
    else
        device_->Present(nullptr, nullptr, nullptr, nullptr);
}

#endif  // !NO_D3D

// ============================================================================
// Direct3D 12
// ============================================================================

#if !defined(NO_D3D12)

namespace {

const char* g_VertexShaderSrc = R"(
    struct VSIn  { float4 pos : POSITION; float2 uv : TEXCOORD0; };
    struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
    VSOut main(VSIn input) { VSOut o; o.pos = input.pos; o.uv = input.uv; return o; }
)";

const char* g_PixelShaderSrc = R"(
    Texture2D    tex : register(t0);
    SamplerState smp : register(s0);
    struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
    float4 main(PSIn input) : SV_TARGET { return tex.Sample(smp, input.uv); }
)";

typedef HRESULT(WINAPI* LPFND3DCompile)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
                                        ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**,
                                        ID3DBlob**);
typedef HRESULT(WINAPI* LPFND3D12SerializeRootSignature)(const D3D12_ROOT_SIGNATURE_DESC*,
                                                         D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**,
                                                         ID3DBlob**);
typedef HRESULT(WINAPI* LPFNCreateDXGIFactory1)(REFIID, void**);
typedef HRESULT(WINAPI* LPFND3D12CreateDevice)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);

// Uploads pixel data into a new DEFAULT-heap texture through `upload_heap`
// (which must stay alive until the GPU has executed the copy).
Microsoft::WRL::ComPtr<ID3D12Resource> UploadTexture(
    ID3D12Device* device, ID3D12GraphicsCommandList* cmd_list,
    Microsoft::WRL::ComPtr<ID3D12Resource>& upload_heap, DXGI_FORMAT format, UINT tex_w,
    UINT tex_h, const void* src_pixels, UINT src_row_bytes) {
    using Microsoft::WRL::ComPtr;

    D3D12_HEAP_PROPERTIES default_heap = {};
    default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC tex_desc = {};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Width = tex_w;
    tex_desc.Height = tex_h;
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = format;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    ComPtr<ID3D12Resource> tex;
    HRESULT hr = device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &tex_desc,
                                                 D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                 IID_PPV_ARGS(&tex));
    if (FAILED(hr))
        return nullptr;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT num_rows;
    UINT64 row_size_bytes, total_bytes;
    device->GetCopyableFootprints(&tex_desc, 0, 1, 0, &footprint, &num_rows, &row_size_bytes,
                                  &total_bytes);

    D3D12_HEAP_PROPERTIES upload_heap_props = {};
    upload_heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC upload_desc = {};
    upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload_desc.Width = total_bytes;
    upload_desc.Height = 1;
    upload_desc.DepthOrArraySize = 1;
    upload_desc.MipLevels = 1;
    upload_desc.SampleDesc.Count = 1;
    upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_desc,
                                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                         IID_PPV_ARGS(&upload_heap));
    if (FAILED(hr))
        return nullptr;

    uint8_t* mapped = nullptr;
    upload_heap->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
    for (UINT row = 0; row < tex_h; ++row) {
        memcpy(mapped + row * footprint.Footprint.RowPitch,
               static_cast<const uint8_t*>(src_pixels) + row * src_row_bytes, src_row_bytes);
    }
    upload_heap->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION src_loc = {};
    src_loc.pResource = upload_heap.Get();
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_loc.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
    dst_loc.pResource = tex.Get();
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_loc.SubresourceIndex = 0;

    cmd_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = tex.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd_list->ResourceBarrier(1, &barrier);

    return tex;
}

}  // namespace

DX12DrawingPanel::DX12DrawingPanel(QWidget* parent, int _width, int _height)
    : NativeDrawingPanel(parent, _width, _height) {
    if (!Setup()) {
        // Leave device_ null: DrawingPanelInit() flags the failure.
        device_.Reset();
    }
    DrawingPanelInit();
}

bool DX12DrawingPanel::Setup() {
    HRESULT hr;
    bool using_warp = false;

    hD3DCompiler_ = LoadLibraryW(L"d3dcompiler_47.dll");
    hD3D12_ = LoadLibraryW(L"d3d12.dll");
    hDXGI_ = LoadLibraryW(L"dxgi.dll");
    if (!hD3DCompiler_ || !hD3D12_ || !hDXGI_) {
        vbam::LogError(Tr("Failed to load D3D12 or D3DCompiler DLLs"));
        return false;
    }

    auto create_device = reinterpret_cast<LPFND3D12CreateDevice>(
        reinterpret_cast<void*>(GetProcAddress(hD3D12_, "D3D12CreateDevice")));
    auto serialize_rs = reinterpret_cast<LPFND3D12SerializeRootSignature>(
        reinterpret_cast<void*>(GetProcAddress(hD3D12_, "D3D12SerializeRootSignature")));
    auto d3d_compile = reinterpret_cast<LPFND3DCompile>(
        reinterpret_cast<void*>(GetProcAddress(hD3DCompiler_, "D3DCompile")));
    auto create_factory = reinterpret_cast<LPFNCreateDXGIFactory1>(
        reinterpret_cast<void*>(GetProcAddress(hDXGI_, "CreateDXGIFactory1")));
    if (!create_factory || !create_device || !d3d_compile || !serialize_rs) {
        vbam::LogError(Tr("Failed to get D3D12 function pointers"));
        return false;
    }

    // --- 1. DXGI Factory & Adapter ---
    ComPtr<IDXGIFactory4> factory;
    hr = create_factory(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create DXGI factory: %1").arg(Hr(hr)));
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
        hr = create_device(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_ID3D12Device, nullptr);
        if (SUCCEEDED(hr))
            break;
        adapter = nullptr;
    }
    if (!adapter) {
        factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        using_warp = true;
    }

    // --- 2. Device ---
    hr = create_device(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_));
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create D3D12 device: %1").arg(Hr(hr)));
        return false;
    }
    vbam::LogDebug(QStringLiteral("D3D12 device created (%1)")
                       .arg(using_warp ? QStringLiteral("WARP") : QStringLiteral("hardware")));

    // --- 3. Command queue ---
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue_));
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create command queue: %1").arg(Hr(hr)));
        return false;
    }

    // --- 4. Swap chain ---
    const QSize px = DevicePixelSize();
    const HWND hwnd = static_cast<HWND>(NativeHandle());
    DXGI_SWAP_CHAIN_DESC1 sc_desc = {};
    sc_desc.BufferCount = kFrameCount;
    sc_desc.Width = static_cast<UINT>(px.width());
    sc_desc.Height = static_cast<UINT>(px.height());
    sc_desc.Format = rt_format_;
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc_desc.SampleDesc.Count = 1;
    sc_desc.Flags = OPTION(kPrefVsync) ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(command_queue_.Get(), hwnd, &sc_desc, nullptr, nullptr,
                                         &sc1);
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create swap chain: %1").arg(Hr(hr)));
        return false;
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    hr = sc1.As(&swap_chain_);
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to get IDXGISwapChain3: %1").arg(Hr(hr)));
        return false;
    }
    frame_index_ = swap_chain_->GetCurrentBackBufferIndex();

    // --- 5. RTV heap ---
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.NumDescriptors = kFrameCount;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hr = device_->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap_));
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create RTV heap: %1").arg(Hr(hr)));
        return false;
    }
    rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // --- 6. SRV heap ---
    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {};
    srv_heap_desc.NumDescriptors = 1;
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device_->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&srv_heap_));
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create SRV heap: %1").arg(Hr(hr)));
        return false;
    }

    // --- 7. RTVs ---
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < kFrameCount; i++) {
            hr = swap_chain_->GetBuffer(i, IID_PPV_ARGS(&render_targets_[i]));
            if (FAILED(hr)) {
                vbam::LogError(Tr("Failed to get back buffer %1: %2").arg(i).arg(Hr(hr)));
                return false;
            }
            device_->CreateRenderTargetView(render_targets_[i].Get(), nullptr, rtv_handle);
            rtv_handle.ptr += rtv_descriptor_size_;
        }
    }

    // --- 8. Command allocator / list ---
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&command_allocator_));
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create command allocator: %1").arg(Hr(hr)));
        return false;
    }
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator_.Get(),
                                    nullptr, IID_PPV_ARGS(&command_list_));
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create command list: %1").arg(Hr(hr)));
        return false;
    }
    command_list_->Close();

    // --- 9. Root signature ---
    {
        D3D12_DESCRIPTOR_RANGE srv_range = {};
        srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srv_range.NumDescriptors = 1;

        D3D12_ROOT_PARAMETER root_param = {};
        root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_param.DescriptorTable.NumDescriptorRanges = 1;
        root_param.DescriptorTable.pDescriptorRanges = &srv_range;
        root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = OPTION(kDispBilinear) ? D3D12_FILTER_MIN_MAG_MIP_LINEAR
                                               : D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
        rs_desc.NumParameters = 1;
        rs_desc.pParameters = &root_param;
        rs_desc.NumStaticSamplers = 1;
        rs_desc.pStaticSamplers = &sampler;
        rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature_blob, error_blob;
        hr = serialize_rs(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob);
        if (FAILED(hr)) {
            vbam::LogError(Tr("Failed to serialize root signature: %1")
                               .arg(error_blob ? QString::fromLatin1(static_cast<char*>(
                                                     error_blob->GetBufferPointer()))
                                               : QStringLiteral("unknown")));
            return false;
        }
        hr = device_->CreateRootSignature(0, signature_blob->GetBufferPointer(),
                                          signature_blob->GetBufferSize(),
                                          IID_PPV_ARGS(&root_signature_));
        if (FAILED(hr)) {
            vbam::LogError(Tr("Failed to create root signature: %1").arg(Hr(hr)));
            return false;
        }
    }

    // --- 10. Shaders & PSO ---
    {
        ComPtr<ID3DBlob> vs_blob, ps_blob, error_blob;
        UINT compile_flags = 0;
        hr = d3d_compile(g_VertexShaderSrc, strlen(g_VertexShaderSrc), nullptr, nullptr, nullptr,
                         "main", "vs_5_0", compile_flags, 0, &vs_blob, &error_blob);
        if (FAILED(hr)) {
            vbam::LogError(Tr("VS compile failed: %1")
                               .arg(error_blob ? QString::fromLatin1(static_cast<char*>(
                                                     error_blob->GetBufferPointer()))
                                               : QStringLiteral("unknown")));
            return false;
        }
        hr = d3d_compile(g_PixelShaderSrc, strlen(g_PixelShaderSrc), nullptr, nullptr, nullptr,
                         "main", "ps_5_0", compile_flags, 0, &ps_blob, &error_blob);
        if (FAILED(hr)) {
            vbam::LogError(Tr("PS compile failed: %1")
                               .arg(error_blob ? QString::fromLatin1(static_cast<char*>(
                                                     error_blob->GetBufferPointer()))
                                               : QStringLiteral("unknown")));
            return false;
        }

        D3D12_INPUT_ELEMENT_DESC input_layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.pRootSignature = root_signature_.Get();
        pso_desc.VS = {vs_blob->GetBufferPointer(), vs_blob->GetBufferSize()};
        pso_desc.PS = {ps_blob->GetBufferPointer(), ps_blob->GetBufferSize()};
        pso_desc.InputLayout = {input_layout, _countof(input_layout)};
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = rt_format_;
        pso_desc.SampleDesc.Count = 1;
        pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_desc.RasterizerState.DepthClipEnable = TRUE;
        pso_desc.DepthStencilState.DepthEnable = FALSE;
        pso_desc.DepthStencilState.StencilEnable = FALSE;
        pso_desc.SampleMask = UINT_MAX;

        hr = device_->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state_));
        if (FAILED(hr)) {
            vbam::LogError(Tr("Failed to create PSO: %1").arg(Hr(hr)));
            return false;
        }
    }

    // --- 11. Vertex buffer ---
    {
        const UINT vb_size = 4 * sizeof(float) * 6;
        D3D12_HEAP_PROPERTIES heap_props = {};
        heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC res_desc = {};
        res_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        res_desc.Width = vb_size;
        res_desc.Height = 1;
        res_desc.DepthOrArraySize = 1;
        res_desc.MipLevels = 1;
        res_desc.SampleDesc.Count = 1;
        res_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hr = device_->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(&vertex_buffer_));
        if (FAILED(hr)) {
            vbam::LogError(Tr("Failed to create vertex buffer: %1").arg(Hr(hr)));
            return false;
        }
        vertex_buffer_view_.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
        vertex_buffer_view_.StrideInBytes = sizeof(float) * 6;
        vertex_buffer_view_.SizeInBytes = vb_size;
    }

    // --- 12. Fence ---
    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    if (FAILED(hr)) {
        vbam::LogError(Tr("Failed to create fence: %1").arg(Hr(hr)));
        return false;
    }
    fence_value_ = 1;
    fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fence_event_) {
        vbam::LogError(Tr("Failed to create fence event"));
        return false;
    }

    vbam::LogDebug(QStringLiteral("D3D12 pipeline fully initialized"));
    return true;
}

DX12DrawingPanel::~DX12DrawingPanel() {
    StopFilterThreads();
    WaitForGPU();
    if (fence_event_) {
        CloseHandle(fence_event_);
        fence_event_ = nullptr;
    }
    // ComPtr members release themselves; the DLLs stay loaded for the
    // process (their objects may still be alive in other panels).
}

void DX12DrawingPanel::WaitForGPU() {
    if (!command_queue_ || !fence_ || !fence_event_)
        return;
    command_queue_->Signal(fence_.Get(), fence_value_);
    fence_->SetEventOnCompletion(fence_value_, fence_event_);
    WaitForSingleObject(fence_event_, INFINITE);
    ++fence_value_;
}

void DX12DrawingPanel::DrawingPanelInit() {
    DrawingPanelBase::DrawingPanelInit();
    if (!device_ || !swap_chain_ || !pipeline_state_) {
        init_failed_ = true;
        return;
    }
    texture_width_ = ScaledWidth();
    texture_height_ = ScaledHeight();
    did_init = true;
}

bool DX12DrawingPanel::ResizeSwapChain() {
    if (!swap_chain_ || !device_)
        return false;

    WaitForGPU();
    for (UINT i = 0; i < kFrameCount; i++)
        render_targets_[i].Reset();

    const QSize px = DevicePixelSize();
    const UINT flags = OPTION(kPrefVsync) ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    const HRESULT hr = swap_chain_->ResizeBuffers(kFrameCount, static_cast<UINT>(px.width()),
                                                  static_cast<UINT>(px.height()), rt_format_, flags);
    if (FAILED(hr)) {
        vbam::LogDebug(QStringLiteral("ResizeBuffers failed: %1").arg(Hr(hr)));
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; i++) {
        swap_chain_->GetBuffer(i, IID_PPV_ARGS(&render_targets_[i]));
        device_->CreateRenderTargetView(render_targets_[i].Get(), nullptr, rtv_handle);
        rtv_handle.ptr += rtv_descriptor_size_;
    }
    frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
    return true;
}

void DX12DrawingPanel::OnNativeResize(const QSize& device_pixels) {
    (void)device_pixels;
    if (device_)
        ResizeSwapChain();
}

void DX12DrawingPanel::Present() {
    if (!device_ || !command_queue_ || !swap_chain_ || init_failed_)
        return;

    command_allocator_->Reset();
    command_list_->Reset(command_allocator_.Get(), pipeline_state_.Get());

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = render_targets_[frame_index_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    command_list_->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += static_cast<SIZE_T>(frame_index_) * rtv_descriptor_size_;

    const float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    command_list_->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
    command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    bool drew = false;
    if (todraw) {
        const int scaled_width = ScaledWidth();
        const int scaled_height = ScaledHeight();
        const bool out_8 = panel_color_depth_ == 8;
        const bool out_16 = panel_color_depth_ == 16;
        const bool out_24 = panel_color_depth_ == 24;

        // Convert to a B8G8R8A8 staging buffer.
        rgba_.resize(static_cast<size_t>(scaled_width) * scaled_height * 4);
        uint8_t* dst_pixels = rgba_.data();
        const int src_pitch = SourcePitch();
        const uint8_t* src = SourcePixels();

        if (out_8) {
            for (int y = 0; y < scaled_height; y++) {
                const uint8_t* sr = src;
                uint8_t* dr = dst_pixels + static_cast<size_t>(y) * scaled_width * 4;
                for (int x = 0; x < scaled_width; x++, sr++, dr += 4) {
                    const uint8_t p = *sr;
                    if (p == 0xff) {
                        dr[0] = dr[1] = dr[2] = 0xff;
                    } else {
                        dr[0] = (p & 0x3) << 6;
                        dr[1] = ((p >> 2) & 0x7) << 5;
                        dr[2] = ((p >> 5) & 0x7) << 5;
                    }
                    dr[3] = 0xff;
                }
                src += src_pitch;
            }
        } else if (out_16) {
            const uint16_t* src16 = reinterpret_cast<const uint16_t*>(src);
            for (int y = 0; y < scaled_height; y++) {
                const uint16_t* sr = src16;
                uint8_t* dr = dst_pixels + static_cast<size_t>(y) * scaled_width * 4;
                for (int x = 0; x < scaled_width; x++, sr++, dr += 4) {
                    const uint16_t p = *sr;
                    if ((p & 0x7fff) == 0x7fff) {
                        dr[0] = dr[1] = dr[2] = 0xff;
                    } else {
                        const uint8_t r5 = (p >> 10) & 0x1f;
                        const uint8_t g5 = (p >> 5) & 0x1f;
                        const uint8_t b5 = p & 0x1f;
                        dr[0] = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
                        dr[1] = static_cast<uint8_t>((g5 << 3) | (g5 >> 2));
                        dr[2] = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
                    }
                    dr[3] = 0xff;
                }
                src16 += src_pitch / 2;
            }
        } else if (out_24) {
            for (int y = 0; y < scaled_height; y++) {
                const uint8_t* sr = src;
                uint8_t* dr = dst_pixels + static_cast<size_t>(y) * scaled_width * 4;
                for (int x = 0; x < scaled_width; x++, sr += 3, dr += 4) {
                    dr[0] = sr[2];
                    dr[1] = sr[1];
                    dr[2] = sr[0];
                    dr[3] = 0xff;
                }
                src += src_pitch;
            }
        } else {
            for (int y = 0; y < scaled_height; y++) {
                const uint8_t* sr = src;
                uint8_t* dr = dst_pixels + static_cast<size_t>(y) * scaled_width * 4;
                for (int x = 0; x < scaled_width; x++, sr += 4, dr += 4) {
                    dr[0] = sr[2];
                    dr[1] = sr[1];
                    dr[2] = sr[0];
                    dr[3] = 0xff;
                }
                src += src_pitch;
            }
        }

        texture_ = UploadTexture(device_.Get(), command_list_.Get(), upload_heap_,
                                 DXGI_FORMAT_B8G8R8A8_UNORM, static_cast<UINT>(scaled_width),
                                 static_cast<UINT>(scaled_height), dst_pixels,
                                 static_cast<UINT>(scaled_width) * 4);
        if (texture_) {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Texture2D.MipLevels = 1;
            device_->CreateShaderResourceView(texture_.Get(), &srv_desc,
                                              srv_heap_->GetCPUDescriptorHandleForHeapStart());

            command_list_->SetGraphicsRootSignature(root_signature_.Get());
            command_list_->SetPipelineState(pipeline_state_.Get());
            ID3D12DescriptorHeap* heaps[] = {srv_heap_.Get()};
            command_list_->SetDescriptorHeaps(1, heaps);
            command_list_->SetGraphicsRootDescriptorTable(
                0, srv_heap_->GetGPUDescriptorHandleForHeapStart());

            const QSize px = DevicePixelSize();
            const float win_w = static_cast<float>(px.width());
            const float win_h = static_cast<float>(px.height());
            D3D12_VIEWPORT vp = {0.0f, 0.0f, win_w, win_h, 0.0f, 1.0f};
            D3D12_RECT scissor = {0, 0, static_cast<LONG>(win_w), static_cast<LONG>(win_h)};
            command_list_->RSSetViewports(1, &vp);
            command_list_->RSSetScissorRects(1, &scissor);

            // Full-surface quad (the widget is already aspect-fitted by GameArea).
            struct Vertex {
                float x, y, z, w, u, v;
            };
            const Vertex verts[4] = {
                {-1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f},
                {1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f},
                {-1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f},
                {1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f},
            };
            void* vb_mapped = nullptr;
            vertex_buffer_->Map(0, nullptr, &vb_mapped);
            memcpy(vb_mapped, verts, sizeof(verts));
            vertex_buffer_->Unmap(0, nullptr);

            command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            command_list_->IASetVertexBuffers(0, 1, &vertex_buffer_view_);
            command_list_->DrawInstanced(4, 1, 0, 0);
            drew = true;
        } else {
            vbam::LogDebug(QStringLiteral("D3D12 texture upload failed"));
        }
    }
    (void)drew;

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    command_list_->ResourceBarrier(1, &barrier);
    command_list_->Close();

    ID3D12CommandList* lists[] = {command_list_.Get()};
    command_queue_->ExecuteCommandLists(1, lists);

    const UINT present_flags = OPTION(kPrefVsync) ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    swap_chain_->Present(OPTION(kPrefVsync) ? 1 : 0, present_flags);

    WaitForGPU();  // simple per-frame sync
    frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
}

#endif  // !NO_D3D12

#endif  // _WIN32

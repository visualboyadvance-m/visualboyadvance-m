// Minimal RPI render plugin for the proxy test: a 2x nearest-neighbour
// scaler for 16-bit (565) pixels. Built as a 32-bit DLL alongside the host
// so the 64-bit test can load it through the proxy and verify the pixels
// that come back. Same ABI as the Kega Fusion plugins VBA-M loads.

#include <windows.h>

#include "wx/rpi.h"

static void __cdecl TestPluginOutput(RENDER_PLUGIN_OUTP* p) {
    if (!p || !p->SrcPtr || !p->DstPtr) {
        return;
    }
    for (unsigned long y = 0; y < p->SrcH; y++) {
        const uint16_t* src = reinterpret_cast<const uint16_t*>(
            static_cast<const uint8_t*>(p->SrcPtr) + y * p->SrcPitch);
        uint16_t* dst0 = reinterpret_cast<uint16_t*>(
            static_cast<uint8_t*>(p->DstPtr) + (2 * y) * p->DstPitch);
        uint16_t* dst1 = reinterpret_cast<uint16_t*>(
            static_cast<uint8_t*>(p->DstPtr) + (2 * y + 1) * p->DstPitch);
        for (unsigned long x = 0; x < p->SrcW; x++) {
            const uint16_t px = src[x];
            dst0[2 * x] = px;
            dst0[2 * x + 1] = px;
            dst1[2 * x] = px;
            dst1[2 * x + 1] = px;
        }
    }
    p->OutW = p->SrcW * 2;
    p->OutH = p->SrcH * 2;
}

static RENDER_PLUGIN_INFO g_info = {
    "VBA-M Test 2x",
    RPI_VERSION | RPI_565_SUPP | RPI_OUT_SCL2,
    nullptr,
    TestPluginOutput,
};

extern "C" __declspec(dllexport) RENDER_PLUGIN_INFO* __cdecl RenderPluginGetInfo(void) {
    return &g_info;
}

extern "C" __declspec(dllexport) void __cdecl RenderPluginOutput(RENDER_PLUGIN_OUTP* p) {
    TestPluginOutput(p);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    return TRUE;
}

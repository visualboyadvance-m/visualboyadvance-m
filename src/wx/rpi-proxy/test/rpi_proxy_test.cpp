// Smoke test for the 32-bit RPI host: launched from this 64-bit process the
// way visualboyadvance-m launches it, then driven through one command round
// trip over the shared-memory channel. No plugin is involved; the point is
// that the host process builds for this machine, starts, completes the ready
// handshake and answers. On arm64 that also covers the x86 emulation layer
// the real thing depends on.
//
// With a second argument naming the 32-bit test plugin built next to the
// host, it also loads that plugin and runs a frame through it, checking the
// scaled pixels that come back through the shared buffer.
//
// Usage: rpi_proxy_test <path-to-vbam-rpi-host.exe> [path-to-vbam-rpi-test-plugin.rpi]
// Exit codes identify the failing stage so ctest output says what broke.

#include <cstdio>
#include <cstring>
#include <vector>

#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <wx/log.h>
#include <wx/string.h>

#include "wx/rpi-proxy/RpiProxyClient.h"
#include "wx/rpi.h"

namespace {

int fail(int code, const char* what) {
    std::fprintf(stderr, "rpi_proxy_test: FAIL (%d): %s\n", code, what);
    return code;
}

// Load the 32-bit test plugin (a 2x nearest scaler for 565 pixels) through
// the host and push one small frame through it, checking every output pixel.
int run_plugin_stage(rpi_proxy::RpiProxyClient& client, const wxString& plugin) {
    if (!wxFileExists(plugin)) {
        return fail(11, "test plugin not found at the path given");
    }

    RENDER_PLUGIN_INFO info;
    std::memset(&info, 0, sizeof(info));
    if (!client.LoadPlugin(plugin, &info)) {
        return fail(12, "LoadPlugin failed for the test plugin");
    }
    if (std::strcmp(info.Name, "VBA-M Test 2x") != 0) {
        return fail(13, "plugin name did not round-trip (GetInfo)");
    }
    if ((info.Flags & RPI_OUT_SCLMSK) != RPI_OUT_SCL2 || !(info.Flags & RPI_565_SUPP)) {
        return fail(14, "plugin flags did not round-trip (GetInfo)");
    }

    if (!client.StartThread(0)) {
        return fail(15, "StartThread(0) failed");
    }

    const unsigned long src_w = 8, src_h = 4;
    const unsigned long dst_w = src_w * 2, dst_h = src_h * 2;
    const unsigned long src_pitch = src_w * 2, dst_pitch = dst_w * 2;

    // Zero-copy path: write straight into the shared src buffer, read the
    // shared dst buffer, exactly as the drawing panel does.
    uint16_t* src = static_cast<uint16_t*>(client.GetSrcBuffer(0));
    uint16_t* dst = static_cast<uint16_t*>(client.GetDstBuffer(0));
    if (!src || !dst) {
        return fail(16, "shared pixel buffers unavailable after StartThread");
    }
    for (unsigned long y = 0; y < src_h; y++) {
        for (unsigned long x = 0; x < src_w; x++) {
            src[y * src_w + x] = static_cast<uint16_t>(x * 7 + y * 131 + 1);
        }
    }
    std::memset(dst, 0, dst_pitch * dst_h);

    RENDER_PLUGIN_OUTP outp;
    std::memset(&outp, 0, sizeof(outp));
    outp.Size = sizeof(outp);
    outp.Flags = info.Flags;
    outp.SrcPtr = src;
    outp.SrcPitch = src_pitch;
    outp.SrcW = src_w;
    outp.SrcH = src_h;
    outp.DstPtr = dst;
    outp.DstPitch = dst_pitch;
    outp.DstW = dst_w;
    outp.DstH = dst_h;
    outp.OutW = dst_w;
    outp.OutH = dst_h;

    if (!client.ApplyFilter(&outp, 0)) {
        return fail(17, "ApplyFilter through the host failed");
    }

    for (unsigned long y = 0; y < dst_h; y++) {
        for (unsigned long x = 0; x < dst_w; x++) {
            const uint16_t want = src[(y / 2) * src_w + (x / 2)];
            const uint16_t got = dst[y * dst_w + x];
            if (got != want) {
                std::fprintf(stderr, "rpi_proxy_test: dst(%lu,%lu) = %04x, expected %04x\n",
                             x, y, got, want);
                return fail(18, "plugin output pixels did not match the 2x scale of the input");
            }
        }
    }

    client.StopThread(0);
    client.UnloadPlugin();
    std::printf("rpi_proxy_test: plugin loaded, %lux%lu frame scaled 2x through the host ok\n",
                src_w, src_h);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: rpi_proxy_test <vbam-rpi-host.exe>\n");
        return 2;
    }

    wxInitializer wx_init;
    if (!wx_init.IsOk()) {
        return fail(3, "wxInitializer failed");
    }
    wxLog::SetActiveTarget(new wxLogStderr());

    using rpi_proxy::RpiProxyClient;

    if (!RpiProxyClient::IsProcess64Bit()) {
        return fail(4, "test process is not 64-bit; the proxy is only used from 64-bit builds");
    }

    const wxString host(argv[1]);
    if (!wxFileExists(host)) {
        return fail(5, "host executable not found at the path given");
    }
    if (!RpiProxyClient::Is32BitDll(host)) {
        return fail(6, "host executable is not a 32-bit PE image");
    }

    // The client deletes its host path on destruction, so work on a copy.
    wxString host_copy = wxFileName::CreateTempFileName("vbam-rpi-host-test-");
    if (host_copy.empty()) {
        return fail(7, "could not create a temp file");
    }
    wxRemoveFile(host_copy);
    host_copy += ".exe";
    if (!wxCopyFile(host, host_copy, true)) {
        return fail(7, "could not copy the host executable to temp");
    }

    int result = 0;
    {
        RpiProxyClient client;
        client.SetHostExePath(host_copy);

        // A path that exists as a directory but is no plugin: the host must
        // launch, answer, and report it invalid. An empty result means the
        // host never came up or never answered.
        std::vector<wxString> paths;
        paths.push_back(wxFileName(wxFileName::GetTempDir(), "not-a-plugin.rpi").GetFullPath());

        std::vector<RpiProxyClient::PluginEnumResult> r = client.EnumeratePlugins(paths);
        if (r.empty()) {
            result = fail(8, "host did not launch or did not answer the first EnumeratePlugins");
        } else if (r.size() != 1 || r[0].valid) {
            result = fail(9, "unexpected EnumeratePlugins result for a non-plugin path");
        } else {
            // Second round trip must reuse the running host rather than start another.
            r = client.EnumeratePlugins(paths);
            if (r.size() != 1 || r[0].valid) {
                result = fail(10, "second EnumeratePlugins round trip failed");
            }
        }

        if (result == 0 && argc >= 3) {
            result = run_plugin_stage(client, wxString(argv[2]));
        }
        // ~RpiProxyClient terminates the host and removes host_copy.
    }

    if (wxFileExists(host_copy)) {
        wxRemoveFile(host_copy);
    }

    if (result == 0) {
        std::printf("rpi_proxy_test: host launched, handshake and 2 round trips ok\n");
    }
    return result;
}

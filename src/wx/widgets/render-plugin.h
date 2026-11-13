#ifndef VBAM_WX_WIDGETS_RENDER_PLUGIN_H_
#define VBAM_WX_WIDGETS_RENDER_PLUGIN_H_

#include <wx/defs.h>
#include <wx/string.h>

#if wxUSE_DYNLIB_CLASS
#include <wx/dynlib.h>
#else
// wx built without dynamic library support (wxRetro68: classic Mac OS, where
// render plugins, which are Windows DLLs, could not be loaded anyway). A
// library that never loads, so the plugin filter finds no plugin and the
// code around it needs no conditionals.
enum {
    wxDL_LAZY = 0x00000001,
    wxDL_NOW = 0x00000002,
    wxDL_GLOBAL = 0x00000004,
    wxDL_VERBATIM = 0x00000008,
    wxDL_NOSHARE = 0x00000010,
    wxDL_QUIET = 0x00000020,
    wxDL_DEFAULT = wxDL_NOW,
};

class wxDynamicLibrary {
public:
    wxDynamicLibrary() = default;
    wxDynamicLibrary(const wxString&, int = wxDL_DEFAULT) {}

    bool Load(const wxString&, int = wxDL_DEFAULT) { return false; }
    bool IsLoaded() const { return false; }
    void Unload() {}
    bool HasSymbol(const wxString&) const { return false; }
    void* GetSymbol(const wxString&, bool* success = nullptr) const {
        if (success)
            *success = false;
        return nullptr;
    }
};
#endif

#include "wx/rpi.h"

// VBAM_RPI_PROXY_SUPPORT is defined by CMake when the proxy library is available
// and linked. Don't auto-detect here as it causes link errors for targets
// that don't link vbam-rpi-proxy.
#ifdef VBAM_RPI_PROXY_SUPPORT
#include "wx/rpi-proxy/RpiProxyClient.h"
#endif

namespace widgets {

// Initializes a RENDER_PLUGIN_INFO, if the plugin at `path` is valid.
// Otherwise, returns NULL.
RENDER_PLUGIN_INFO* MaybeLoadFilterPlugin(const wxString& path, wxDynamicLibrary* filter_plugin);

#ifdef VBAM_RPI_PROXY_SUPPORT
// Check if a plugin requires the proxy (32-bit plugin on 64-bit build)
bool PluginNeedsProxy(const wxString& path);

// Load a plugin via the proxy. Returns true on success.
bool MaybeLoadFilterPluginViaProxy(const wxString& path,
    rpi_proxy::RpiProxyClient* proxy_client,
    RENDER_PLUGIN_INFO* info_out);
#endif

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_RENDER_PLUGIN_H_

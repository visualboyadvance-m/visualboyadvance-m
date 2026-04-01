#ifndef VBAM_WX_WIDGETS_RENDER_PLUGIN_H_
#define VBAM_WX_WIDGETS_RENDER_PLUGIN_H_

#include <wx/dynlib.h>
#include <wx/string.h>

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

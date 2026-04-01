#include "wx/widgets/render-plugin.h"

#include "core/base/check.h"

namespace widgets {

RENDER_PLUGIN_INFO* MaybeLoadFilterPlugin(const wxString& path, wxDynamicLibrary* filter_plugin) {
    VBAM_CHECK(filter_plugin);

    if (!filter_plugin->Load(path, wxDL_VERBATIM | wxDL_NOW | wxDL_QUIET)) {
        return NULL;
    }

    RENDPLUG_GetInfo get_info =
        (RENDPLUG_GetInfo)filter_plugin->GetSymbol("RenderPluginGetInfo");

    if (!get_info) {
        filter_plugin->Unload();
        return NULL;
    }

    // need to be able to write to plugin_info to set Output() and Flags
    RENDER_PLUGIN_INFO* plugin_info = get_info();
    if (!plugin_info) {
        filter_plugin->Unload();
        return NULL;
    }

    // Accept both RPI version 1 and version 2 plugins
    unsigned int pluginVersion = plugin_info->Flags & 0xff;
    if (pluginVersion != 1 && pluginVersion != 2) {
        filter_plugin->Unload();
        return NULL;
    }

    // Accept 555, 565, or 888 color formats
    // Version 1 plugins may not set color format flags - assume 565 support for compatibility
    if ((plugin_info->Flags & (RPI_555_SUPP | RPI_565_SUPP | RPI_888_SUPP)) == 0) {
        if (pluginVersion == 1) {
            // Version 1 plugins without color flags - assume 565 support (common for GBA)
            plugin_info->Flags |= RPI_565_SUPP;
        } else {
            filter_plugin->Unload();
            return NULL;
        }
    }

    return plugin_info;
}

#ifdef VBAM_RPI_PROXY_SUPPORT

bool PluginNeedsProxy(const wxString& path) {
    return rpi_proxy::RpiProxyClient::NeedsProxy(path);
}

bool MaybeLoadFilterPluginViaProxy(const wxString& path,
    rpi_proxy::RpiProxyClient* proxy_client,
    RENDER_PLUGIN_INFO* info_out) {
    VBAM_CHECK(proxy_client);
    VBAM_CHECK(info_out);

    if (!proxy_client->LoadPlugin(path, info_out)) {
        return false;
    }

    return true;
}

#endif  // VBAM_RPI_PROXY_SUPPORT

}  // namespace widgets

#include "qt/widgets/render-plugin.h"

#include "core/base/check.h"

namespace widgets {

RENDER_PLUGIN_INFO* MaybeLoadFilterPlugin(const QString& path, QLibrary* filter_plugin) {
    VBAM_CHECK(filter_plugin);

    filter_plugin->setFileName(path);
    // Resolve all symbols now (wxDL_NOW) and never touch the file name
    // (wxDL_VERBATIM): the plugin path is complete as configured.
    filter_plugin->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!filter_plugin->load()) {
        return nullptr;
    }

    RENDPLUG_GetInfo get_info =
        reinterpret_cast<RENDPLUG_GetInfo>(filter_plugin->resolve("RenderPluginGetInfo"));

    if (!get_info) {
        filter_plugin->unload();
        return nullptr;
    }

    // need to be able to write to plugin_info to set Output() and Flags
    RENDER_PLUGIN_INFO* plugin_info = get_info();
    if (!plugin_info) {
        filter_plugin->unload();
        return nullptr;
    }

    // Accept both RPI version 1 and version 2 plugins
    unsigned int pluginVersion = plugin_info->Flags & 0xff;
    if (pluginVersion != 1 && pluginVersion != 2) {
        filter_plugin->unload();
        return nullptr;
    }

    // Accept 555, 565, or 888 color formats
    // Version 1 plugins may not set color format flags - assume 565 support for compatibility
    if ((plugin_info->Flags & (RPI_555_SUPP | RPI_565_SUPP | RPI_888_SUPP)) == 0) {
        if (pluginVersion == 1) {
            // Version 1 plugins without color flags - assume 565 support (common for GBA)
            plugin_info->Flags |= RPI_565_SUPP;
        } else {
            filter_plugin->unload();
            return nullptr;
        }
    }

    return plugin_info;
}

}  // namespace widgets

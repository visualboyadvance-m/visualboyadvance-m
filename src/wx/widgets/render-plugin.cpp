#include "wx/widgets/render-plugin.h"

namespace widgets {

RENDER_PLUGIN_INFO* MaybeLoadFilterPlugin(const wxString& path, wxDynamicLibrary* filter_plugin) {
    assert(filter_plugin);

    if (!filter_plugin->Load(path, wxDL_VERBATIM | wxDL_NOW | wxDL_QUIET)) {
        return nullptr;
    }

    RENDPLUG_GetInfo get_info =
        (RENDPLUG_GetInfo)filter_plugin->GetSymbol("RenderPluginGetInfo");

    if (!get_info) {
        filter_plugin->Unload();
        return nullptr;
    }

    // need to be able to write to plugin_info to set Output() and Flags
    RENDER_PLUGIN_INFO* plugin_info = get_info();
    if (!plugin_info) {
        filter_plugin->Unload();
        return nullptr;
    }

    // TODO: Should this be < RPI_VERISON?
    if ((plugin_info->Flags & 0xff) != RPI_VERSION) {
        filter_plugin->Unload();
        return nullptr;
    }

    // RPI_565_SUPP is not supported, although it would be possible and it
    // would make Cairo more efficient
    if ((plugin_info->Flags & (RPI_555_SUPP | RPI_888_SUPP)) == 0) {
        filter_plugin->Unload();
        return nullptr;
    }

    return plugin_info;
}

}  // namespace widgets

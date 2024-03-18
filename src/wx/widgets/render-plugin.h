#ifndef VBAM_WX_WIDGETS_RENDER_PLUGIN_H_
#define VBAM_WX_WIDGETS_RENDER_PLUGIN_H_

#include <wx/dynlib.h>
#include <wx/string.h>

#include "wx/rpi.h"

namespace widgets {

// Initializes a RENDER_PLUGIN_INFO, if the plugin at `path` is valid.
// Otherwise, returns nullptr.
RENDER_PLUGIN_INFO* MaybeLoadFilterPlugin(const wxString& path, wxDynamicLibrary* filter_plugin);

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_RENDER_PLUGIN_H_
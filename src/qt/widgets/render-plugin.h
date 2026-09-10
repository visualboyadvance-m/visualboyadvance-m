#ifndef VBAM_QT_WIDGETS_RENDER_PLUGIN_H_
#define VBAM_QT_WIDGETS_RENDER_PLUGIN_H_

#include <QLibrary>
#include <QString>

#include "qt/rpi.h"

namespace widgets {

// Loads the Kega Fusion style render plugin at `path` into `filter_plugin` and
// returns its RENDER_PLUGIN_INFO if it is a usable (version 1 or 2, 555/565/888
// capable) plugin. Returns nullptr otherwise, with `filter_plugin` unloaded.
// The returned struct belongs to the plugin; callers copy it before modifying
// Flags/Output.
RENDER_PLUGIN_INFO* MaybeLoadFilterPlugin(const QString& path, QLibrary* filter_plugin);

}  // namespace widgets

#endif  // VBAM_QT_WIDGETS_RENDER_PLUGIN_H_

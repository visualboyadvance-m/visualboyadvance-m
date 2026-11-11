// emu.screenshot([path]) → path|nil, err
//
// The wx frontend hands the composed framebuffer to the engine every
// frame via LuaEngine::CaptureFrame() (called from systemDrawScreen
// right after the gui.* overlay is rendered, so a screenshot shows
// exactly what the user sees). The cache itself and the PNG writer
// live in lua_engine.cpp next to the Impl fields they use; this file
// is just the Lua binding.
//
//   emu.screenshot()             → "vbam_shot_<frame>.png" in the cwd
//   emu.screenshot("/tmp/a.png") → the given path
//
// On failure returns nil plus an error string ("no frame captured yet"
// when called before the first frame, e.g. with no ROM loaded).

#include "wx/lua/lua_engine.h"
#include "wx/lua/lua_internal.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdio>
#include <string>

namespace vbam {
namespace wx {

// Frame counter shared with lua_api_emu.cpp (bumped per frame from
// systemDrawScreen).
namespace lua_internal {
extern int g_frame_count;
}  // namespace lua_internal

// Exported for the emu table in lua_api_emu.cpp.
int l_screenshot(lua_State* L) {
    auto* e = lua_internal::EngineFromState(L);
    if (!e) return 0;

    std::string path;
    if (lua_gettop(L) >= 1 && !lua_isnoneornil(L, 1)) {
        size_t len;
        const char* s = luaL_checklstring(L, 1, &len);
        path.assign(s, len);
    } else {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "vbam_shot_%d.png",
                      lua_internal::g_frame_count);
        path = buf;
    }

    std::string err;
    if (e->SaveScreenshot(path, &err)) {
        lua_pushlstring(L, path.data(), path.size());
        return 1;
    }
    lua_pushnil(L);
    lua_pushlstring(L, err.data(), err.size());
    return 2;
}

}  // namespace wx
}  // namespace vbam

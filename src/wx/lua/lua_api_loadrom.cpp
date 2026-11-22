// emu.loadrom(path) → true | nil, err
//
// Asks the host frontend to load a ROM. The actual implementation is
// frontend-specific and installed through lua_internal::g_loadrom_fn:
//
//   * wx GUI build (sys.cpp): verifies the path is readable and has a
//     ROM extension (so LoadGame's modal error dialogs stay out of the
//     automation path), then defers GameArea::LoadGame() to the next
//     event-loop iteration with wxCallAfter. Deferring matters: the
//     Lua pump runs inside systemDrawScreen(), and LoadGame tears down
//     the very panel whose draw callback is on the stack.
//   * Headless test builds: the pointer stays null and this returns
//     "not available in this build".
//
// The load itself is asynchronous, so a `true` return only means the
// request was queued. Scripts detect completion via emu.romgeneration()
// (bumped once per successful load in GameArea::LoadGame) and
// emu.romname(); the bridge's `loadrom` command wraps that wait.

#include "wx/lua/lua_engine.h"
#include "wx/lua/lua_internal.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <string>

namespace vbam {
namespace wx {

namespace lua_internal {
LoadRomFn g_loadrom_fn = nullptr;
}  // namespace lua_internal

// Exported for the emu table in lua_api_emu.cpp.
int l_loadrom(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    if (!lua_internal::g_loadrom_fn) {
        lua_pushnil(L);
        lua_pushstring(L, "emu.loadrom is not available in this build");
        return 2;
    }
    std::string err;
    if (lua_internal::g_loadrom_fn(std::string(path), &err)) {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushnil(L);
    lua_pushlstring(L, err.data(), err.size());
    return 2;
}

}  // namespace wx
}  // namespace vbam

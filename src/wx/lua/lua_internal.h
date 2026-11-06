#ifndef VBAM_WX_LUA_LUA_INTERNAL_H_
#define VBAM_WX_LUA_LUA_INTERNAL_H_

#include <string>

struct lua_State;

namespace vbam {
namespace wx {

class LuaEngine;

namespace lua_internal {

inline constexpr const char* kEnginePtr = "vbam.engineptr";

LuaEngine* EngineFromState(lua_State* L);

// emu.loadrom backend. The wx GUI frontend installs the real
// implementation at startup (it defers to GameArea::LoadGame via
// wxCallAfter, so it must live in a wx-linked TU); headless test
// builds leave it null and emu.loadrom reports "not available".
// Returns true when the load request was accepted (the actual load
// happens on the next event-loop iteration), false + *err otherwise.
using LoadRomFn = bool (*)(const std::string& path, std::string* err);
extern LoadRomFn g_loadrom_fn;

}  // namespace lua_internal
}  // namespace wx
}  // namespace vbam

#endif  // VBAM_WX_LUA_LUA_INTERNAL_H_

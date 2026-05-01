#ifndef VBAM_WX_LUA_LUA_INTERNAL_H_
#define VBAM_WX_LUA_LUA_INTERNAL_H_

extern "C" {
#include <lua.h>
}

namespace vbam {
namespace wx {

class LuaEngine;

namespace lua_internal {

inline constexpr const char* kEnginePtr = "vbam.engineptr";

LuaEngine* EngineFromState(lua_State* L);

}  // namespace lua_internal
}  // namespace wx
}  // namespace vbam

#endif  // VBAM_WX_LUA_LUA_INTERNAL_H_

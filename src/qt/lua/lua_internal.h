#ifndef VBAM_QT_LUA_LUA_INTERNAL_H_
#define VBAM_QT_LUA_LUA_INTERNAL_H_

struct lua_State;

namespace vbam {
namespace lua {

class LuaEngine;

namespace lua_internal {

inline constexpr const char* kEnginePtr = "vbam.engineptr";

LuaEngine* EngineFromState(lua_State* L);

}  // namespace lua_internal
}  // namespace lua
}  // namespace vbam

#endif  // VBAM_QT_LUA_LUA_INTERNAL_H_

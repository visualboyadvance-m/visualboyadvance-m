// FCEUX `joypad` namespace bindings.
//
//   joypad.get([port=0])     → table of pressed buttons by name
//   joypad.set([port=0,] tbl) → install a per-frame button override
//   joypad.read              alias for get
//   joypad.write             alias for set
//
// Button names match FCEUX's GBA scripts (which inherited the GB
// names): "A","B","select","start","right","left","up","down","R","L".
// The mask is built using the bit positions FCEUX expects so existing
// scripts work without changes:
//   A=0  B=1  select=2  start=3  right=4  left=5  up=6  down=7
//   R=8  L=9  (GBA only)

#include "wx/lua/lua_engine.h"
#include "wx/lua/lua_internal.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdint>

#include "core/base/system.h"

namespace vbam {
namespace wx {

namespace {

using lua_internal::EngineFromState;

struct Btn { const char* name; int bit; };
const Btn kBtns[] = {
    {"A", 0}, {"B", 1}, {"select", 2}, {"start", 3},
    {"right", 4}, {"left", 5}, {"up", 6}, {"down", 7},
    {"R", 8}, {"L", 9},
    {nullptr, 0},
};

int ParsePort(lua_State* L, int idx) {
    int port = 0;
    if (lua_isnumber(L, idx)) port = (int)lua_tointeger(L, idx) - 1;
    if (port < 0) port = 0;
    if (port > 3) port = 3;
    return port;
}

// joypad.get reads from systemReadJoypad(port) — which is declared
// in core/base/system.h. The wx frontend's mask matches FCEUX bit
// positions (A=0..L=9).
int l_get(lua_State* L) {
    int port = ParsePort(L, 1);
    uint32_t mask = systemReadJoypad(port);
    lua_newtable(L);
    for (const Btn* b = kBtns; b->name; ++b) {
        if (mask & (1u << b->bit)) {
            lua_pushboolean(L, 1);
            lua_setfield(L, -2, b->name);
        }
    }
    return 1;
}

int l_set(lua_State* L) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    int port = 0;
    int tbl_idx;
    if (lua_isnumber(L, 1)) { port = ParsePort(L, 1); tbl_idx = 2; }
    else                    { tbl_idx = 1; }

    if (lua_isnil(L, tbl_idx)) {
        e->ClearJoypadOverride(port);
        return 0;
    }
    luaL_checktype(L, tbl_idx, LUA_TTABLE);
    uint32_t mask = 0;
    for (const Btn* b = kBtns; b->name; ++b) {
        lua_getfield(L, tbl_idx, b->name);
        if (lua_toboolean(L, -1)) mask |= (1u << b->bit);
        lua_pop(L, 1);
    }
    e->SetJoypadOverride(port, mask);
    return 0;
}

const luaL_Reg kJoypadFns[] = {
    {"get",   l_get},
    {"read",  l_get},
    {"set",   l_set},
    {"write", l_set},
    {nullptr, nullptr},
};

}  // namespace

void LuaApi_InstallJoypad(lua_State* L, LuaEngine* /*engine*/) {
    luaL_newlib(L, kJoypadFns);
    lua_setglobal(L, "joypad");
}

}  // namespace wx
}  // namespace vbam

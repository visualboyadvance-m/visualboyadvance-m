// FCEUX `bit` namespace bindings — same surface as Lua-BitOp / FCEUX:
//
//   bit.band(...)  bit.bor(...)  bit.bxor(...)  bit.bnot(x)
//   bit.lshift(x,n)  bit.rshift(x,n)  bit.arshift(x,n)
//   bit.rol(x,n)     bit.ror(x,n)
//   bit.tobit(x)     bit.tohex(x [,n])
//
// All values are treated as 32-bit; the "tobit" sign-extension mirrors
// LuaBitOp so existing scripts that mix booleans / negative numbers
// behave the same.
//
// Lua 5.4 has these operations as built-in operators (`a & b`, `~a`,
// etc.), but FCEUX scripts written for 5.1 still call the bit
// library so we provide it.

#include "wx/lua/lua_engine.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdint>
#include <cstdio>

namespace vbam {
namespace wx {
namespace {

uint32_t ArgU32(lua_State* L, int idx) {
    return (uint32_t)(lua_Integer)luaL_checkinteger(L, idx);
}

int l_band(lua_State* L) {
    int n = lua_gettop(L); uint32_t v = 0xffffffffu;
    for (int i = 1; i <= n; ++i) v &= ArgU32(L, i);
    lua_pushinteger(L, v); return 1;
}
int l_bor(lua_State* L) {
    int n = lua_gettop(L); uint32_t v = 0;
    for (int i = 1; i <= n; ++i) v |= ArgU32(L, i);
    lua_pushinteger(L, v); return 1;
}
int l_bxor(lua_State* L) {
    int n = lua_gettop(L); uint32_t v = 0;
    for (int i = 1; i <= n; ++i) v ^= ArgU32(L, i);
    lua_pushinteger(L, v); return 1;
}
int l_bnot(lua_State* L) {
    lua_pushinteger(L, (uint32_t)~ArgU32(L, 1)); return 1;
}
int l_lshift(lua_State* L) {
    uint32_t v = ArgU32(L, 1); uint32_t n = ArgU32(L, 2) & 31;
    lua_pushinteger(L, v << n); return 1;
}
int l_rshift(lua_State* L) {
    uint32_t v = ArgU32(L, 1); uint32_t n = ArgU32(L, 2) & 31;
    lua_pushinteger(L, v >> n); return 1;
}
int l_arshift(lua_State* L) {
    int32_t v = (int32_t)ArgU32(L, 1); int n = (int)(ArgU32(L, 2) & 31);
    lua_pushinteger(L, (uint32_t)(v >> n)); return 1;
}
int l_rol(lua_State* L) {
    uint32_t v = ArgU32(L, 1); uint32_t n = ArgU32(L, 2) & 31;
    lua_pushinteger(L, (v << n) | (v >> ((32 - n) & 31))); return 1;
}
int l_ror(lua_State* L) {
    uint32_t v = ArgU32(L, 1); uint32_t n = ArgU32(L, 2) & 31;
    lua_pushinteger(L, (v >> n) | (v << ((32 - n) & 31))); return 1;
}
int l_tobit(lua_State* L) {
    lua_pushinteger(L, (int32_t)ArgU32(L, 1)); return 1;
}
int l_tohex(lua_State* L) {
    uint32_t v = ArgU32(L, 1);
    int n = lua_isinteger(L, 2) ? (int)lua_tointeger(L, 2) : 8;
    char fmt[16]; std::snprintf(fmt, sizeof fmt, "%%0%dx", n < 0 ? -n : n);
    char buf[64]; std::snprintf(buf, sizeof buf, fmt, v);
    if (n < 0) for (char* p = buf; *p; ++p)
        if (*p >= 'a' && *p <= 'f') *p = (char)(*p - 'a' + 'A');
    lua_pushstring(L, buf);
    return 1;
}

const luaL_Reg kBitFns[] = {
    {"band", l_band}, {"bor",  l_bor}, {"bxor", l_bxor}, {"bnot", l_bnot},
    {"lshift", l_lshift}, {"rshift", l_rshift}, {"arshift", l_arshift},
    {"rol", l_rol}, {"ror", l_ror},
    {"tobit", l_tobit}, {"tohex", l_tohex},
    {nullptr, nullptr},
};

}  // namespace

void LuaApi_InstallBit(lua_State* L, LuaEngine* /*engine*/) {
    luaL_newlib(L, kBitFns);
    lua_setglobal(L, "bit");
}

}  // namespace wx
}  // namespace vbam

// FCEUX `rom` namespace bindings.
//
//   rom.readbyte(a)   — unsigned 8-bit read from the loaded ROM image
//   rom.readword(a)   — 16-bit little-endian
//   rom.readdword(a)  — 32-bit little-endian
//
// `a` is 0-based into the raw ROM blob — distinct from `memory.read*`
// which uses CPU-bus addresses (so reads from $08000000+ on GBA, or
// the bank-switched window on GB). Out-of-range reads return 0.

#include "wx/lua/lua_engine.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdint>

#include "core/gb/gb.h"
#include "core/gb/gbCartData.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaGlobals.h"

namespace vbam {
namespace wx {

namespace {

inline uint8_t* ActiveRom(uint32_t* size) {
    if (gbRom) {
        if (size) *size = (uint32_t)g_gbCartData.rom_size();
        return gbRom;
    }
    if (size) *size = (uint32_t)0x02000000u;  // Up to 32 MiB GBA ROM.
    return g_rom;
}

int l_readbyte(lua_State* L) {
    uint32_t off = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t sz = 0;
    uint8_t* p = ActiveRom(&sz);
    if (!p || off >= sz) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, p[off]);
    return 1;
}

int l_readword(lua_State* L) {
    uint32_t off = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t sz = 0;
    uint8_t* p = ActiveRom(&sz);
    if (!p || off + 1 >= sz) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8));
    return 1;
}

int l_readdword(lua_State* L) {
    uint32_t off = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t sz = 0;
    uint8_t* p = ActiveRom(&sz);
    if (!p || off + 3 >= sz) { lua_pushinteger(L, 0); return 1; }
    uint32_t v = (uint32_t)p[off]            |
                 ((uint32_t)p[off + 1] << 8) |
                 ((uint32_t)p[off + 2] << 16)|
                 ((uint32_t)p[off + 3] << 24);
    lua_pushinteger(L, v);
    return 1;
}

const luaL_Reg kRomFns[] = {
    {"readbyte",  l_readbyte},
    {"readword",  l_readword},
    {"readdword", l_readdword},
    {nullptr, nullptr},
};

}  // namespace

void LuaApi_InstallRom(lua_State* L, LuaEngine* /*engine*/) {
    luaL_newlib(L, kRomFns);
    lua_setglobal(L, "rom");
}

}  // namespace wx
}  // namespace vbam

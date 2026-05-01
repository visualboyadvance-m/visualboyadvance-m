// FCEUX `memory` namespace bindings.
//
//   memory.readbyte(a)            unsigned 8-bit read
//   memory.readbytesigned(a)      signed 8-bit read
//   memory.readword(a)            unsigned 16-bit (little-endian)
//   memory.readwordsigned(a)
//   memory.readdword(a)
//   memory.readdwordsigned(a)
//   memory.writebyte (a, v)
//   memory.writeword (a, v)       little-endian
//   memory.writedword(a, v)
//
//   memory.registerread (addr, [size,] f)   — f(addr,size,value)
//   memory.registerwrite(addr, [size,] f)   — f(addr,size,value)
//   memory.registerexec (addr, f)           — f(addr)
//
// Address dispatching: the GBA memory map is 32-bit; the GB map is
// 16-bit. We pick the right reader at call time based on which core
// is active (`gbRom != nullptr` ⇒ GB).

#include "wx/lua/lua_engine.h"
#include "wx/lua/lua_internal.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdint>

#include "core/gb/gb.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaInline.h"
#include "core/gba/gbaGlobals.h"

extern uint8_t gbReadMemory(uint16_t addr);
extern void    gbWriteMemory(uint16_t addr, uint8_t value);

namespace vbam {
namespace wx {

namespace {

using lua_internal::EngineFromState;

inline bool IsGameBoy() { return gbRom != nullptr; }

uint8_t ReadByte(uint32_t a) {
    if (IsGameBoy()) return gbReadMemory(static_cast<uint16_t>(a));
    return CPUReadByte(a);
}
uint16_t ReadWord(uint32_t a) {
    if (IsGameBoy()) {
        return static_cast<uint16_t>(gbReadMemory(static_cast<uint16_t>(a))) |
               (static_cast<uint16_t>(gbReadMemory(static_cast<uint16_t>(a + 1))) << 8);
    }
    return static_cast<uint16_t>(CPUReadHalfWord(a));
}
uint32_t ReadDword(uint32_t a) {
    if (IsGameBoy()) {
        return static_cast<uint32_t>(gbReadMemory(static_cast<uint16_t>(a)))         |
              (static_cast<uint32_t>(gbReadMemory(static_cast<uint16_t>(a + 1))) << 8) |
              (static_cast<uint32_t>(gbReadMemory(static_cast<uint16_t>(a + 2))) << 16)|
              (static_cast<uint32_t>(gbReadMemory(static_cast<uint16_t>(a + 3))) << 24);
    }
    return CPUReadMemory(a);
}
void WriteByte(uint32_t a, uint8_t v) {
    if (IsGameBoy()) { gbWriteMemory(static_cast<uint16_t>(a), v); return; }
    CPUWriteByte(a, v);
}
void WriteWord(uint32_t a, uint16_t v) {
    if (IsGameBoy()) {
        gbWriteMemory(static_cast<uint16_t>(a), v & 0xff);
        gbWriteMemory(static_cast<uint16_t>(a + 1), (v >> 8) & 0xff);
        return;
    }
    CPUWriteHalfWord(a, v);
}
void WriteDword(uint32_t a, uint32_t v) {
    if (IsGameBoy()) {
        gbWriteMemory(static_cast<uint16_t>(a),     (v      ) & 0xff);
        gbWriteMemory(static_cast<uint16_t>(a + 1), (v >>  8) & 0xff);
        gbWriteMemory(static_cast<uint16_t>(a + 2), (v >> 16) & 0xff);
        gbWriteMemory(static_cast<uint16_t>(a + 3), (v >> 24) & 0xff);
        return;
    }
    CPUWriteMemory(a, v);
}

int l_readbyte (lua_State* L) {
    lua_pushinteger(L, ReadByte((uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}
int l_readbytesigned (lua_State* L) {
    lua_pushinteger(L, (int8_t)ReadByte((uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}
int l_readword (lua_State* L) {
    lua_pushinteger(L, ReadWord((uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}
int l_readwordsigned (lua_State* L) {
    lua_pushinteger(L, (int16_t)ReadWord((uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}
int l_readdword (lua_State* L) {
    lua_pushinteger(L, ReadDword((uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}
int l_readdwordsigned (lua_State* L) {
    lua_pushinteger(L, (int32_t)ReadDword((uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}
int l_writebyte (lua_State* L) {
    WriteByte ((uint32_t)luaL_checkinteger(L, 1),
               (uint8_t) (luaL_checkinteger(L, 2) & 0xff));
    return 0;
}
int l_writeword (lua_State* L) {
    WriteWord ((uint32_t)luaL_checkinteger(L, 1),
               (uint16_t)(luaL_checkinteger(L, 2) & 0xffff));
    return 0;
}
int l_writedword (lua_State* L) {
    WriteDword((uint32_t)luaL_checkinteger(L, 1),
               (uint32_t)(luaL_checkinteger(L, 2) & 0xffffffff));
    return 0;
}

// memory.registerread(addr, [size,] func)
//
// Stores entries in the registry table "vbam.memread" as a list of
// {start, end, fn} sub-tables; the dispatcher in lua_engine.cpp calls
// every entry whose [start..end) covers the access. Setting `f` to
// nil removes the most recent matching range.
int l_register_mem(lua_State* L, const char* key, bool exec) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    uint32_t addr = (uint32_t)luaL_checkinteger(L, 1);
    int size = 1;
    int fn_index = 2;
    if (lua_isnumber(L, 2) && !exec) { size = (int)lua_tointeger(L, 2); fn_index = 3; }

    if (!lua_isfunction(L, fn_index) && !lua_isnil(L, fn_index))
        luaL_typeerror(L, fn_index, "function or nil");

    lua_getfield(L, LUA_REGISTRYINDEX, key);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, key);
    }
    int t = lua_gettop(L);
    int n = (int)lua_rawlen(L, t);
    lua_newtable(L);
    lua_pushinteger(L, addr);             lua_setfield(L, -2, "start");
    lua_pushinteger(L, addr + size - 1);  lua_setfield(L, -2, "end");
    lua_pushvalue(L, fn_index);           lua_setfield(L, -2, "fn");
    lua_rawseti(L, t, n + 1);
    lua_pop(L, 1);  // pop list

    if      (key == std::string("vbam.memread"))  e->SetHasReadHooks(true);
    else if (key == std::string("vbam.memwrite")) e->SetHasWriteHooks(true);
    else if (key == std::string("vbam.memexec"))  e->SetHasExecHooks(true);
    return 0;
}
int l_registerread (lua_State* L) { return l_register_mem(L, "vbam.memread",  false); }
int l_registerwrite(lua_State* L) { return l_register_mem(L, "vbam.memwrite", false); }
int l_registerexec (lua_State* L) { return l_register_mem(L, "vbam.memexec",  true);  }

const luaL_Reg kMemFns[] = {
    {"readbyte",        l_readbyte},
    {"readbytesigned",  l_readbytesigned},
    {"readword",        l_readword},
    {"readwordsigned",  l_readwordsigned},
    {"readdword",       l_readdword},
    {"readdwordsigned", l_readdwordsigned},
    {"writebyte",       l_writebyte},
    {"writeword",       l_writeword},
    {"writedword",      l_writedword},
    {"registerread",    l_registerread},
    {"registerwrite",   l_registerwrite},
    {"registerexec",    l_registerexec},
    {nullptr, nullptr},
};

}  // namespace

void LuaApi_InstallMemory(lua_State* L, LuaEngine* /*engine*/) {
    luaL_newlib(L, kMemFns);
    lua_setglobal(L, "memory");
}

}  // namespace wx
}  // namespace vbam

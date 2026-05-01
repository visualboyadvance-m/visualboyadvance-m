// FCEUX `savestate` namespace bindings.
//
//   savestate.create([slot])    → opaque object naming a state slot
//   savestate.save  (s)         → save into the state at `s`
//   savestate.load  (s)         → load from the state at `s`
//   savestate.registerload(f)   → fires after each successful load
//   savestate.registersave(f)   → fires after each successful save
//
// For simplicity, the "object" returned by savestate.create is just a
// string — a slot number 0–9 (matching wx's quick-state slots) or an
// arbitrary path for user-named saves. Both vbam GB and GBA cores use
// gz-compressed savestates that the wx CmdLoadState / CmdSaveState
// menu items already write/read; we plug into the same paths so a
// Lua-driven save matches what the user sees in the UI.

#include "wx/lua/lua_engine.h"
#include "wx/lua/lua_internal.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <string>

#include "core/base/system.h"
#include "core/gb/gb.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gba.h"

// Core save/load entry points — file-path overloads. Declared here in
// the global namespace because the in-class `gbReadSaveState` /
// `CPUReadState` overloads in gb.h / gba.h take a `(const uint8_t*)`
// buffer and aren't what we want.
extern bool gbReadSaveState  (const char* name);
extern bool gbWriteSaveState (const char* name);
extern bool CPUReadState     (const char* name);
extern bool CPUWriteState    (const char* name);

namespace vbam {
namespace wx {

namespace {

using lua_internal::EngineFromState;

std::string SlotPath(const std::string& slot) {
    // If the caller passed a digit slot (0..9), expand to the wx
    // quick-state directory; otherwise treat as a literal path. The
    // actual quick-state paths are constructed by the wx event
    // handlers — Lua-driven saves bypass that to keep the binding
    // independent of wx headers, so a digit just becomes ".sav<n>".
    if (slot.size() == 1 && slot[0] >= '0' && slot[0] <= '9')
        return std::string(".sav") + slot;
    return slot;
}

int l_create(lua_State* L) {
    if (lua_gettop(L) >= 1) lua_pushstring(L, luaL_checkstring(L, 1));
    else                    lua_pushstring(L, "1");
    return 1;
}

int l_save(lua_State* L) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    std::string slot = luaL_checkstring(L, 1);
    e->DispatchSaveStateBefore(slot);
    bool ok = gbRom ? gbWriteSaveState(SlotPath(slot).c_str())
                    : CPUWriteState(SlotPath(slot).c_str());
    e->DispatchSaveStateAfter(slot);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int l_load(lua_State* L) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    std::string slot = luaL_checkstring(L, 1);
    e->DispatchLoadStateBefore(slot);
    bool ok = gbRom ? gbReadSaveState(SlotPath(slot).c_str())
                    : CPUReadState(SlotPath(slot).c_str());
    e->DispatchLoadStateAfter(slot);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int l_register(lua_State* L, const char* key) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_getfield(L, LUA_REGISTRYINDEX, key);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, key);
    }
    int n = (int)lua_rawlen(L, -1);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, n + 1);
    lua_pop(L, 1);
    return 0;
}

int l_registerload(lua_State* L) { return l_register(L, "vbam.loadafter"); }
int l_registersave(lua_State* L) { return l_register(L, "vbam.saveafter"); }

const luaL_Reg kSavestateFns[] = {
    {"create",       l_create},
    {"save",         l_save},
    {"load",         l_load},
    {"registerload", l_registerload},
    {"registersave", l_registersave},
    {nullptr, nullptr},
};

}  // namespace

void LuaApi_InstallSavestate(lua_State* L, LuaEngine* /*engine*/) {
    luaL_newlib(L, kSavestateFns);
    lua_setglobal(L, "savestate");
}

}  // namespace wx
}  // namespace vbam

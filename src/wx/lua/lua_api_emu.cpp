// FCEUX `emu` namespace bindings.
//
//   emu.frameadvance()    — yield one frame of emulation
//   emu.pause()           — pause emulation
//   emu.unpause()
//   emu.poweron()         — soft reset
//   emu.message(s)        — wx OSD message
//   emu.print(...)        — log to the wx Lua console
//   emu.framecount()      — frames since power-on
//   emu.romname()         — current ROM filename (basename, no ext)
//   emu.softreset()       — alias for poweron, FCEUX naming
//   emu.registerbefore(f) — fires before each emulated frame
//   emu.registerafter(f)  — fires after each emulated frame
//   emu.registerexit(f)   — fires once when the script is unloaded
//   emu.getsystem()       — "GBA" or "GB" or "SGB", best-effort
//
// emu.frameadvance is a yield point; the wx loop pumps the script
// once per frame, so a `while true do emu.frameadvance() end` loop
// behaves like a coroutine even though we don't use lua coroutines.

#include "wx/lua/lua_engine.h"
#include "wx/lua/lua_internal.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstring>
#include <string>

#include "core/base/system.h"
#include "core/base/file_util.h"
#include "core/gba/gbaGlobals.h"
#include "core/gb/gbCartData.h"
#include "core/gb/gbGlobals.h"

// Globals from the wx frontend — declared here rather than included
// from wxvbam.h so we don't drag in wxWidgets headers in the binding.
extern int  emulating;
extern bool pause_next;
extern bool turbo;

// Core entry points used by emu.poweron / emu.softreset. They live in
// the global namespace; the `extern` must too, so it's declared here
// (outside the anonymous namespace below) rather than tucked next to
// where it's called.
extern void CPUReset();
extern void gbReset();

namespace vbam {
namespace wx {

// One-shot frame-yield flag — emu.frameadvance() sets it; the wx main
// loop returns from the pump once it's true.
namespace lua_internal {
bool g_frame_yield_pending = false;
int  g_frame_count = 0;
std::string g_rom_name;
}  // namespace lua_internal

namespace {

using lua_internal::EngineFromState;

int l_frameadvance(lua_State* L) {
    // emu.frameadvance() is a coroutine yield point. The host loop
    // resumes the script's main coroutine once per emulated frame
    // (see LuaEngine::ResumeScriptCoroutine), so yielding here makes
    // a `while true do emu.frameadvance() end` loop run at exactly
    // one Lua iteration per frame instead of spinning the CPU.
    //
    // If the script is somehow called outside its own coroutine
    // (e.g., from a registerbefore hook), lua_yield returns control
    // to whoever pcall'd us — for hooks that's the dispatcher, which
    // is a tolerable surprise rather than an error.
    lua_internal::g_frame_yield_pending = true;
    return lua_yield(L, 0);
}

int l_pause(lua_State*)   { pause_next = true;  return 0; }
int l_unpause(lua_State*) { pause_next = false; return 0; }

int l_poweron(lua_State*) {
    if (gbRom) ::gbReset();
    else       ::CPUReset();
    return 0;
}

int l_message(lua_State* L) {
    const char* s = luaL_checkstring(L, 1);
    systemScreenMessage(s);
    return 0;
}

int l_print(lua_State* L) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    int n = lua_gettop(L);
    std::string out;
    for (int i = 1; i <= n; ++i) {
        if (i > 1) out += "\t";
        const char* s = luaL_tolstring(L, i, nullptr);
        out += s ? s : "(nil)";
        lua_pop(L, 1);
    }
    e->Log(out);
    return 0;
}

int l_framecount(lua_State* L) {
    lua_pushinteger(L, lua_internal::g_frame_count);
    return 1;
}

int l_romname(lua_State* L) {
    lua_pushstring(L, lua_internal::g_rom_name.c_str());
    return 1;
}

int l_getsystem(lua_State* L) {
    const char* sys = "GBA";
    if (gbRom) sys = gbCgbMode ? "GBC" : (gbSgbMode ? "SGB" : "GB");
    lua_pushstring(L, sys);
    return 1;
}

// emu.registerbefore / registerafter / registerexit — append the
// argument to the corresponding registry list and report any old
// function (FCEUX returns the previous one for chaining).
int l_register_list(lua_State* L, const char* key) {
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

int l_registerbefore(lua_State* L) { return l_register_list(L, "vbam.before"); }
int l_registerafter (lua_State* L) { return l_register_list(L, "vbam.after");  }
int l_registerexit  (lua_State* L) { return l_register_list(L, "vbam.exit");   }

const luaL_Reg kEmuFns[] = {
    {"frameadvance", l_frameadvance},
    {"pause",        l_pause},
    {"unpause",      l_unpause},
    {"poweron",      l_poweron},
    {"softreset",    l_poweron},
    {"message",      l_message},
    {"print",        l_print},
    {"framecount",   l_framecount},
    {"romname",      l_romname},
    {"getsystem",    l_getsystem},
    {"registerbefore", l_registerbefore},
    {"registerafter",  l_registerafter},
    {"registerexit",   l_registerexit},
    {nullptr, nullptr},
};

}  // namespace

void LuaApi_InstallEmu(lua_State* L, LuaEngine* /*engine*/) {
    luaL_newlib(L, kEmuFns);
    lua_setglobal(L, "emu");

    // FCEUX scripts also call print() globally, expecting it to go to
    // the same log stream — replace Lua's default print so it does.
    lua_pushcfunction(L, l_print);
    lua_setglobal(L, "print");
}

}  // namespace wx
}  // namespace vbam

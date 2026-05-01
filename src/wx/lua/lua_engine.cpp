// LuaEngine — owns the lua_State, loads scripts, dispatches hooks.
//
// FCEUX-style hook tables live in the Lua registry under keys like
// "vbam.before" / "vbam.after" / etc. Multiple registerbefore() calls
// stack into a list (run in registration order). Memory hooks are
// keyed by (range, kind) so a script can install hooks on disjoint
// ranges; the dispatcher walks the matching range list per access.

#include "wx/lua/lua_engine.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "core/base/script_hooks.h"
#include "wx/lua/lua_internal.h"

namespace vbam {
namespace wx {

using lua_internal::kEnginePtr;

// Forward decls for the FCEUX-style API binding installers. Each lives
// in its own .cpp under src/wx/lua/ to keep this file manageable.
void LuaApi_InstallEmu       (lua_State* L, LuaEngine* engine);
void LuaApi_InstallMemory    (lua_State* L, LuaEngine* engine);
void LuaApi_InstallJoypad    (lua_State* L, LuaEngine* engine);
void LuaApi_InstallGui       (lua_State* L, LuaEngine* engine);
void LuaApi_InstallSavestate (lua_State* L, LuaEngine* engine);
void LuaApi_InstallRom       (lua_State* L, LuaEngine* engine);
void LuaApi_InstallBit       (lua_State* L, LuaEngine* engine);

namespace {

constexpr const char* kRegBefore   = "vbam.before";
constexpr const char* kRegAfter    = "vbam.after";
constexpr const char* kRegExit     = "vbam.exit";
constexpr const char* kRegSaveBefore = "vbam.savebefore";
constexpr const char* kRegSaveAfter  = "vbam.saveafter";
constexpr const char* kRegLoadBefore = "vbam.loadbefore";
constexpr const char* kRegLoadAfter  = "vbam.loadafter";
constexpr const char* kRegMemRead  = "vbam.memread";
constexpr const char* kRegMemWrite = "vbam.memwrite";
constexpr const char* kRegMemExec  = "vbam.memexec";

}  // namespace

struct LuaEngine::Impl {
    lua_State*   L = nullptr;
    // Coroutine carrying the script's "main body" for emu.frameadvance().
    // Created in LoadFile/LoadString after the chunk compiles; resumed
    // once per frame from ResumeScriptCoroutine. When the body
    // returns (or errors), `script_done` flips true and registered
    // hooks (registerbefore/after/etc.) keep running normally.
    lua_State*   script_co   = nullptr;
    bool         script_done = false;
    LogSink      log_sink;
    GuiOps       gui;
    bool         has_read_hooks  = false;
    bool         has_write_hooks = false;
    bool         has_exec_hooks  = false;
    std::map<int, uint32_t> joypad_overrides;  // port → button mask, FCEUX bits
};

LuaEngine& LuaInstance() {
    static LuaEngine s_instance;
    return s_instance;
}

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace {

// Push a Lua function-list table identified by `key` from the registry,
// creating an empty one on first access.
void PushOrCreateRegistryList(lua_State* L, const char* key) {
    lua_getfield(L, LUA_REGISTRYINDEX, key);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, key);
    }
}

// Iterate the registry list at `key`, calling each Lua function with
// `nargs` args copied from the top of the stack. Each call's return
// values are discarded. The args are popped on return.
void CallRegistryList(LuaEngine* engine, lua_State* L, const char* key, int nargs) {
    if (!L) { if (nargs) lua_pop(L, nargs); return; }
    PushOrCreateRegistryList(L, key);
    int table_idx = lua_gettop(L);
    int n = (int)lua_rawlen(L, table_idx);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, table_idx, i);     // function
        for (int j = 0; j < nargs; ++j)   // duplicate args after the function
            lua_pushvalue(L, table_idx - nargs + j);
        if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            engine->Log(std::string("[lua error] ") + (err ? err : "(nil)"));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1 + nargs);  // pop list table + original args
}

}  // namespace

namespace lua_internal {

LuaEngine* EngineFromState(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kEnginePtr);
    auto* e = static_cast<LuaEngine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return e;
}

}  // namespace lua_internal

// ----------------------------------------------------------------------------
// LuaEngine
// ----------------------------------------------------------------------------

LuaEngine::LuaEngine() : impl_(new Impl) {}

LuaEngine::~LuaEngine() { Stop(); }

void LuaEngine::SetLogSink(LogSink sink) {
    impl_->log_sink = std::move(sink);
}

void LuaEngine::Log(const std::string& s) {
    if (impl_->log_sink) impl_->log_sink(s);
    else std::fprintf(stderr, "[lua] %s\n", s.c_str());
}

lua_State* LuaEngine::L() const { return impl_ ? impl_->L : nullptr; }

bool LuaEngine::IsLoaded() const { return impl_ && impl_->L != nullptr; }

LuaEngine::GuiOps&       LuaEngine::MutableGui()       { return impl_->gui; }
const LuaEngine::GuiOps& LuaEngine::Gui()        const { return impl_->gui; }
void LuaEngine::ClearGui() {
    impl_->gui.texts.clear();
    impl_->gui.boxes.clear();
    impl_->gui.lines.clear();
    impl_->gui.pixels.clear();
}

void LuaEngine::Stop() {
    if (!impl_->L) return;

    // Fire registerexit handlers before tearing down.
    CallRegistryList(this, impl_->L, kRegExit, 0);

    lua_close(impl_->L);
    impl_->L = nullptr;
    impl_->script_co = nullptr;
    impl_->script_done = false;
    impl_->has_read_hooks = impl_->has_write_hooks = impl_->has_exec_hooks = false;
    ::g_vbam_script_has_read_hooks  = 0;
    ::g_vbam_script_has_write_hooks = 0;
    ::g_vbam_script_has_exec_hooks  = 0;
    impl_->joypad_overrides.clear();
    ClearGui();
    script_path_.clear();
}

// Spin up a fresh lua_State, install all FCEUX-style namespaces, and
// stash the engine pointer in the registry so api bindings can find
// us. Returns true on success.
bool LuaEngine::LoadFile(const std::string& path) {
    Stop();
    impl_->L = luaL_newstate();
    if (!impl_->L) { Log("[lua] luaL_newstate failed"); return false; }
    luaL_openlibs(impl_->L);

    // Pin self in the registry so api bindings can find us.
    lua_pushlightuserdata(impl_->L, this);
    lua_setfield(impl_->L, LUA_REGISTRYINDEX, kEnginePtr);

    LuaApi_InstallEmu       (impl_->L, this);
    LuaApi_InstallMemory    (impl_->L, this);
    LuaApi_InstallJoypad    (impl_->L, this);
    LuaApi_InstallGui       (impl_->L, this);
    LuaApi_InstallSavestate (impl_->L, this);
    LuaApi_InstallRom       (impl_->L, this);
    LuaApi_InstallBit       (impl_->L, this);

    if (luaL_loadfile(impl_->L, path.c_str()) != LUA_OK) {
        Log(std::string("[lua load] ") + lua_tostring(impl_->L, -1));
        Stop();
        return false;
    }
    // Push the compiled chunk onto a coroutine instead of running it
    // immediately. The host loop calls ResumeScriptCoroutine() once
    // per frame, which gives `emu.frameadvance()` real coroutine-yield
    // semantics — `while true do emu.frameadvance() end` advances one
    // frame per emulated frame instead of spinning the CPU.
    impl_->script_co = lua_newthread(impl_->L);
    // Pin the thread in the registry so the GC can't reap it while
    // we're still holding script_co.
    lua_setfield(impl_->L, LUA_REGISTRYINDEX, "vbam.script_thread");
    lua_xmove(impl_->L, impl_->script_co, 1);  // move chunk to thread
    impl_->script_done = false;
    script_path_ = path;
    Log("[lua] loaded " + path);
    return true;
}

bool LuaEngine::LoadString(const std::string& chunk, const std::string& name) {
    Stop();
    impl_->L = luaL_newstate();
    if (!impl_->L) return false;
    luaL_openlibs(impl_->L);
    lua_pushlightuserdata(impl_->L, this);
    lua_setfield(impl_->L, LUA_REGISTRYINDEX, kEnginePtr);
    LuaApi_InstallEmu       (impl_->L, this);
    LuaApi_InstallMemory    (impl_->L, this);
    LuaApi_InstallJoypad    (impl_->L, this);
    LuaApi_InstallGui       (impl_->L, this);
    LuaApi_InstallSavestate (impl_->L, this);
    LuaApi_InstallRom       (impl_->L, this);
    LuaApi_InstallBit       (impl_->L, this);

    if (luaL_loadbuffer(impl_->L, chunk.data(), chunk.size(), name.c_str())
        != LUA_OK) {
        Log(std::string("[lua load] ") + lua_tostring(impl_->L, -1));
        Stop();
        return false;
    }
    impl_->script_co = lua_newthread(impl_->L);
    lua_setfield(impl_->L, LUA_REGISTRYINDEX, "vbam.script_thread");
    lua_xmove(impl_->L, impl_->script_co, 1);
    impl_->script_done = false;
    script_path_ = name;
    return true;
}

void LuaEngine::ResumeScriptCoroutine() {
    if (!impl_->L || !impl_->script_co || impl_->script_done) return;
    int nresults = 0;
    int rc = lua_resume(impl_->script_co, impl_->L, 0, &nresults);
    if (rc == LUA_YIELD) {
        // Script yielded via emu.frameadvance() — keep the coroutine
        // alive for next frame's resume. Discard any yielded values.
        if (nresults > 0) lua_pop(impl_->script_co, nresults);
        return;
    }
    impl_->script_done = true;
    if (rc != LUA_OK) {
        const char* err = lua_tostring(impl_->script_co, -1);
        Log(std::string("[lua main] ") + (err ? err : "(nil)"));
        lua_pop(impl_->script_co, 1);
    }
    // The script's main body finished. Hooks (registerbefore/after/...)
    // continue to fire normally on impl_->L.
}

std::string LuaEngine::EvalRepl(const std::string& chunk) {
    if (!impl_->L) return "error: no script loaded";

    // Try to compile as an expression first ("return <chunk>") so the
    // result can be displayed; if that fails, fall back to running it
    // as a statement.
    std::string ret_expr = "return " + chunk;
    bool as_expr = (luaL_loadstring(impl_->L, ret_expr.c_str()) == LUA_OK);
    if (!as_expr) {
        lua_pop(impl_->L, 1);
        if (luaL_loadstring(impl_->L, chunk.c_str()) != LUA_OK) {
            std::string err = lua_tostring(impl_->L, -1);
            lua_pop(impl_->L, 1);
            return std::string("error: ") + err;
        }
    }
    int top = lua_gettop(impl_->L) - 1;
    if (lua_pcall(impl_->L, 0, LUA_MULTRET, 0) != LUA_OK) {
        std::string err = lua_tostring(impl_->L, -1);
        lua_pop(impl_->L, 1);
        return std::string("error: ") + err;
    }
    std::string result;
    int nresults = lua_gettop(impl_->L) - top;
    for (int i = 1; i <= nresults; ++i) {
        const char* s = luaL_tolstring(impl_->L, top + i, nullptr);
        if (i > 1) result += "\t";
        result += s ? s : "(nil)";
        lua_pop(impl_->L, 1);
    }
    lua_pop(impl_->L, nresults);
    return result;
}

void LuaEngine::DispatchBeforeFrame() {
    if (impl_->L) CallRegistryList(this, impl_->L, kRegBefore, 0);
}
void LuaEngine::DispatchAfterFrame() {
    if (impl_->L) CallRegistryList(this, impl_->L, kRegAfter, 0);
}

void LuaEngine::DispatchMemRead(uint32_t addr, int size, uint32_t value) {
    if (!impl_->L || !impl_->has_read_hooks) return;
    lua_pushinteger(impl_->L, (lua_Integer)addr);
    lua_pushinteger(impl_->L, (lua_Integer)size);
    lua_pushinteger(impl_->L, (lua_Integer)value);
    CallRegistryList(this, impl_->L, kRegMemRead, 3);
}
void LuaEngine::DispatchMemWrite(uint32_t addr, int size, uint32_t value) {
    if (!impl_->L || !impl_->has_write_hooks) return;
    lua_pushinteger(impl_->L, (lua_Integer)addr);
    lua_pushinteger(impl_->L, (lua_Integer)size);
    lua_pushinteger(impl_->L, (lua_Integer)value);
    CallRegistryList(this, impl_->L, kRegMemWrite, 3);
}
void LuaEngine::DispatchMemExec(uint32_t addr) {
    if (!impl_->L || !impl_->has_exec_hooks) return;
    lua_pushinteger(impl_->L, (lua_Integer)addr);
    CallRegistryList(this, impl_->L, kRegMemExec, 1);
}
bool LuaEngine::HasReadHooks()  const { return impl_->has_read_hooks; }
bool LuaEngine::HasWriteHooks() const { return impl_->has_write_hooks; }
bool LuaEngine::HasExecHooks()  const { return impl_->has_exec_hooks; }

// Trampolines exposed to gb.cpp / gbaInline.h via script_hooks.h.
// They cheaply delegate to the singleton engine; the core's
// `g_vbam_script_has_*_hooks` flag is checked first so the no-script
// case is two cheap loads + a branch.
namespace {
void TrampolineMemRead (uint32_t addr, int sz, uint32_t v) {
    LuaInstance().DispatchMemRead (addr, sz, v);
}
void TrampolineMemWrite(uint32_t addr, int sz, uint32_t v) {
    LuaInstance().DispatchMemWrite(addr, sz, v);
}
void TrampolineMemExec (uint32_t addr) {
    LuaInstance().DispatchMemExec (addr);
}
struct HookTrampolineInstaller {
    HookTrampolineInstaller() {
        ::g_vbam_script_mem_read  = &TrampolineMemRead;
        ::g_vbam_script_mem_write = &TrampolineMemWrite;
        ::g_vbam_script_mem_exec  = &TrampolineMemExec;
    }
};
HookTrampolineInstaller s_install;
}  // namespace

void LuaEngine::SetHasReadHooks(bool v) {
    impl_->has_read_hooks = v;
    ::g_vbam_script_has_read_hooks = v ? 1 : 0;
}
void LuaEngine::SetHasWriteHooks(bool v) {
    impl_->has_write_hooks = v;
    ::g_vbam_script_has_write_hooks = v ? 1 : 0;
}
void LuaEngine::SetHasExecHooks(bool v) {
    impl_->has_exec_hooks = v;
    ::g_vbam_script_has_exec_hooks = v ? 1 : 0;
}
void LuaEngine::SetJoypadOverride(int port, uint32_t mask) {
    impl_->joypad_overrides[port] = mask;
}
void LuaEngine::ClearJoypadOverride(int port) {
    impl_->joypad_overrides.erase(port);
}

bool LuaEngine::GetJoypadOverride(int port, uint32_t* out_mask) const {
    auto it = impl_->joypad_overrides.find(port);
    if (it == impl_->joypad_overrides.end()) return false;
    if (out_mask) *out_mask = it->second;
    return true;
}

void LuaEngine::DispatchSaveStateBefore(const std::string& slot) {
    if (!impl_->L) return;
    lua_pushstring(impl_->L, slot.c_str());
    CallRegistryList(this, impl_->L, kRegSaveBefore, 1);
}
void LuaEngine::DispatchSaveStateAfter(const std::string& slot) {
    if (!impl_->L) return;
    lua_pushstring(impl_->L, slot.c_str());
    CallRegistryList(this, impl_->L, kRegSaveAfter, 1);
}
void LuaEngine::DispatchLoadStateBefore(const std::string& slot) {
    if (!impl_->L) return;
    lua_pushstring(impl_->L, slot.c_str());
    CallRegistryList(this, impl_->L, kRegLoadBefore, 1);
}
void LuaEngine::DispatchLoadStateAfter(const std::string& slot) {
    if (!impl_->L) return;
    lua_pushstring(impl_->L, slot.c_str());
    CallRegistryList(this, impl_->L, kRegLoadAfter, 1);
}

}  // namespace wx
}  // namespace vbam

// FCEUX `gui` namespace bindings.
//
//   gui.text(x, y, str [, fg [, bg]])
//   gui.box(x, y, w, h [, fillcolor [, outlinecolor]])
//   gui.line(x1, y1, x2, y2 [, color])
//   gui.pixel(x, y [, color])
//
// Each call appends an entry to the engine's GuiOps queue; the wx
// panel reads it after each frame is composed and overlays it on top
// of the emulated framebuffer. Colors are FCEUX-compatible — either
// 0xRRGGBBAA hex or strings like "white" / "#ffaa00ff".

#include "wx/lua/lua_engine.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstdint>
#include <cstring>

namespace vbam {
namespace wx {

namespace {

LuaEngine* EngineFromState(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "vbam.engineptr");
    auto* e = static_cast<LuaEngine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return e;
}

uint32_t ParseColor(lua_State* L, int idx, uint32_t def) {
    if (lua_isnoneornil(L, idx)) return def;
    if (lua_isnumber(L, idx))    return (uint32_t)lua_tointeger(L, idx);
    if (lua_isstring(L, idx)) {
        const char* s = lua_tostring(L, idx);
        // FCEUX shorthand color names.
        struct N { const char* n; uint32_t c; } kNames[] = {
            {"white",  0xffffffff}, {"black", 0x000000ff},
            {"red",    0xff0000ff}, {"green", 0x00ff00ff},
            {"blue",   0x0000ffff}, {"yellow",0xffff00ff},
            {"cyan",   0x00ffffff}, {"magenta",0xff00ffff},
            {"clear",  0x00000000}, {nullptr, 0},
        };
        for (auto* p = kNames; p->n; ++p)
            if (std::strcmp(p->n, s) == 0) return p->c;
        if (s[0] == '#') {
            uint32_t v = 0;
            for (int i = 1; s[i]; ++i) {
                char c = s[i];
                int d = (c >= '0' && c <= '9') ? c - '0'
                      : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                      : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
                if (d < 0) break;
                v = (v << 4) | (uint32_t)d;
            }
            return v;
        }
    }
    return def;
}

int l_text(lua_State* L) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    LuaEngine::GuiText t;
    t.x  = (int)luaL_checkinteger(L, 1);
    t.y  = (int)luaL_checkinteger(L, 2);
    t.s  = luaL_checkstring(L, 3);
    t.fg = ParseColor(L, 4, 0xffffffffu);
    t.bg = ParseColor(L, 5, 0x00000000u);
    e->MutableGui().texts.push_back(std::move(t));
    return 0;
}

int l_box(lua_State* L) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    LuaEngine::GuiBox b;
    b.x = (int)luaL_checkinteger(L, 1);
    b.y = (int)luaL_checkinteger(L, 2);
    b.w = (int)luaL_checkinteger(L, 3);
    b.h = (int)luaL_checkinteger(L, 4);
    b.fill    = ParseColor(L, 5, 0x00000000u);
    b.outline = ParseColor(L, 6, 0xffffffffu);
    e->MutableGui().boxes.push_back(b);
    return 0;
}

int l_line(lua_State* L) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    LuaEngine::GuiLine ln;
    ln.x1 = (int)luaL_checkinteger(L, 1);
    ln.y1 = (int)luaL_checkinteger(L, 2);
    ln.x2 = (int)luaL_checkinteger(L, 3);
    ln.y2 = (int)luaL_checkinteger(L, 4);
    ln.color = ParseColor(L, 5, 0xffffffffu);
    e->MutableGui().lines.push_back(ln);
    return 0;
}

int l_pixel(lua_State* L) {
    auto* e = EngineFromState(L);
    if (!e) return 0;
    LuaEngine::GuiPix p;
    p.x = (int)luaL_checkinteger(L, 1);
    p.y = (int)luaL_checkinteger(L, 2);
    p.color = ParseColor(L, 3, 0xffffffffu);
    e->MutableGui().pixels.push_back(p);
    return 0;
}

const luaL_Reg kGuiFns[] = {
    {"text",  l_text},
    {"box",   l_box},
    {"line",  l_line},
    {"pixel", l_pixel},
    {nullptr, nullptr},
};

}  // namespace

void LuaApi_InstallGui(lua_State* L, LuaEngine* /*engine*/) {
    luaL_newlib(L, kGuiFns);
    lua_setglobal(L, "gui");
}

}  // namespace wx
}  // namespace vbam

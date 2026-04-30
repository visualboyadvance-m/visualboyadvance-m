// Lua 5.4 scripting host for the wx frontend.
//
// One LuaEngine instance owns one lua_State; the rest of the wx code
// pokes it through the global accessors at the bottom of the file.
// Scripts are loaded via LoadFile / LoadString and run in the
// emulator's main thread (synchronous with frame stepping). Hooks are
// dispatched at well-defined call sites in the emulation loop.
//
// API design follows FCEUX's Lua-script convention:
//
//   emu.frameadvance / emu.pause / emu.unpause / emu.poweron
//   emu.print(...)        — writes to the wx log window
//   emu.registerbefore(f) — runs each frame before CPU step
//   emu.registerafter(f)  — runs each frame after CPU step
//   emu.registerexit(f)   — runs once when the script is unloaded
//
//   memory.readbyte/readword/readdword(addr)
//   memory.readbytesigned/readwordsigned/readdwordsigned(addr)
//   memory.writebyte/writeword/writedword(addr, v)
//   memory.registerread / registerwrite / registerexec(addr, [size,] f)
//
//   joypad.get(port=0)        — table of pressed buttons
//   joypad.set(port=0, table) — overrides this frame's buttons
//
//   gui.text(x, y, str [, fg [, bg]])
//   gui.box(x, y, w, h [, fillcolor [, outlinecolor]])
//   gui.line(x1, y1, x2, y2 [, color])
//   gui.pixel(x, y [, color])
//
//   savestate.create() / save(s) / load(s)
//   savestate.registerload(f) / registersave(f)
//
//   rom.readbyte/readword/readdword(addr)
//
//   bit.band, bor, bxor, bshl, bshr, bnot — like FCEUX's bit lib.
//
// All FCEUX-style functions live in their own .cpp file
// (lua_api_*.cpp); this header just declares the engine itself.

#ifndef VBAM_WX_LUA_LUA_ENGINE_H_
#define VBAM_WX_LUA_LUA_ENGINE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct lua_State;

namespace vbam {
namespace wx {

class LuaEngine {
public:
    // Callback the wx Console window registers so script `print()` /
    // `emu.print(...)` output lands in its text control.
    using LogSink = std::function<void(const std::string&)>;

    LuaEngine();
    ~LuaEngine();

    LuaEngine(const LuaEngine&)            = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    // Load+run a script. If a previous script is loaded, it is
    // unloaded first (its registerexit() handlers fire). Returns true
    // on success; on failure, error message is written to the log.
    bool LoadFile(const std::string& path);
    bool LoadString(const std::string& chunk, const std::string& name = "<chunk>");

    // Run a single Lua statement and return its `tostring(...)`'d
    // result (or an empty string if no value, or an error string
    // prefixed with "error:"). Used by the REPL.
    std::string EvalRepl(const std::string& chunk);

    void Stop();
    bool IsLoaded() const;

    // Wired into the wx log/console window.
    void SetLogSink(LogSink sink);
    void Log(const std::string& s);

    // Returns the path of the currently-loaded script, or "" if none.
    const std::string& ScriptPath() const { return script_path_; }

    // ---------------------------------------------------------------
    // Hook dispatchers — called from the emulation loop.
    // ---------------------------------------------------------------

    // Frame hooks. `before` runs before the CPU executes the frame;
    // `after` runs after the frame has been rendered to the
    // back-buffer (so gui.* draws are visible to the user).
    void DispatchBeforeFrame();
    void DispatchAfterFrame();

    // Resume the script's main coroutine if it's waiting on
    // emu.frameadvance(). Called once per frame from
    // systemDrawScreen, just before DispatchBeforeFrame. The first
    // call also kicks off the script body for the first time. When
    // the coroutine returns or errors, future calls are no-ops; the
    // registered hooks (registerbefore/after/exit/memread/...) keep
    // running until Stop().
    void ResumeScriptCoroutine();

    // Render the queued gui.text/box/line/pixel ops into a pixel
    // buffer. Frontends call this between systemDrawScreen() and the
    // panel blit so the overlay lands on top of the framebuffer the
    // user actually sees.
    //
    //   pix         start of the pixel buffer
    //   width       visible width in pixels (160 GB, 240 GBA)
    //   height      visible height
    //   pitch_bytes bytes per row (incl. any column padding the
    //               filters reserve — 241 px for GBA)
    //   bpp         16 or 32 (matches systemColorDepth)
    //   r_shift, g_shift, b_shift
    //               bit positions of each channel in the packed
    //               pixel (matches systemRedShift / Green / Blue)
    //
    // Calling this with bpp ∉ {16, 32} is a no-op.
    void RenderOverlay(uint8_t* pix, int width, int height, int pitch_bytes,
                       int bpp, int r_shift, int g_shift, int b_shift);

    // Memory access hooks. `MemReadHook(addr, sz)` fires whenever the
    // emulated CPU reads `sz` bytes starting at `addr` *in an address
    // range registered via memory.registerread*. Same for writes and
    // execution. Each takes the value being read/written so scripts
    // can intercept it (writes pass the pre-write value).
    //
    // These short-circuit cheaply — they're only invoked when at
    // least one hook is registered for that range, which is checked
    // at the call site.
    void DispatchMemRead (uint32_t addr, int size, uint32_t value);
    void DispatchMemWrite(uint32_t addr, int size, uint32_t value);
    void DispatchMemExec (uint32_t addr);

    // Returns true if at least one read/write/exec hook is registered.
    // Used by the CPU loop to skip the dispatch entirely in the hot
    // path when no script cares.
    bool HasReadHooks()  const;
    bool HasWriteHooks() const;
    bool HasExecHooks()  const;

    // joypad.set() override. The wx joypad polling code asks for the
    // currently-loaded script's joypad mask after kb/joy polling; if
    // the script has installed an override for `port`, we substitute
    // it (button bits are FCEUX-compatible: A=0, B=1, …).
    bool GetJoypadOverride(int port, uint32_t* out_mask) const;

    // Savestate hook dispatchers, called from gba.cpp / gb.cpp save
    // and load paths. Pass the slot or filename that's being saved
    // /loaded; scripts get a Lua representation via savestate.create().
    void DispatchSaveStateBefore(const std::string& slot);
    void DispatchSaveStateAfter (const std::string& slot);
    void DispatchLoadStateBefore(const std::string& slot);
    void DispatchLoadStateAfter (const std::string& slot);

    // Direct lua_State* access for the api binding files. Returns
    // nullptr when no script is loaded.
    lua_State* L() const;

    // GUI overlay draw queue — gui.* functions push into this; the
    // wx panel reads it after the frame is composed and overlays it.
    struct GuiText { int x, y; uint32_t fg, bg; std::string s; };
    struct GuiBox  { int x, y, w, h; uint32_t fill, outline; };
    struct GuiLine { int x1, y1, x2, y2; uint32_t color; };
    struct GuiPix  { int x, y; uint32_t color; };
    struct GuiOps {
        std::vector<GuiText> texts;
        std::vector<GuiBox>  boxes;
        std::vector<GuiLine> lines;
        std::vector<GuiPix>  pixels;
    };
    GuiOps&       MutableGui();
    const GuiOps& Gui() const;
    void          ClearGui();

    // ---- Helpers used by lua_api_*.cpp -----------------------------
    // Mark the corresponding hook-set as non-empty so the dispatcher
    // can short-circuit when no hook is registered.
    void SetHasReadHooks (bool v);
    void SetHasWriteHooks(bool v);
    void SetHasExecHooks (bool v);

    // joypad.set / joypad.set(nil) → install/clear an override for a
    // given port. Mask uses FCEUX bits (A=0..LEFT=6..L=8 etc.).
    void SetJoypadOverride  (int port, uint32_t mask);
    void ClearJoypadOverride(int port);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string script_path_;
};

// Process-wide singleton. Created lazily on first access; lives until
// process exit. wx code uses this rather than carrying the engine
// around — the emulator core can only run one ROM at a time, so one
// engine is enough.
LuaEngine& LuaInstance();

}  // namespace wx
}  // namespace vbam

#endif  // VBAM_WX_LUA_LUA_ENGINE_H_

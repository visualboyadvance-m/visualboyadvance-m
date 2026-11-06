// Standalone smoke-test driver for the wx Lua engine.
//
// Builds the LuaEngine + every lua_api_*.cpp binding without the
// wxWidgets frontend, loads a Lua script (default:
// lua_api_test.lua next to this file), and runs it on a fresh
// coroutine. Prints engine log output to stdout and exits non-zero on
// any "[FAIL]" line or Lua error.
//
// Usage: lua_test_runner [script.lua]

#include "wx/lua/lua_engine.h"
#include "core/base/system.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Frontend stubs that vbam-core expects the embedder to provide. The
// vbam-core-fake target stubs everything except `coreOptions` and a
// few wx-named globals; supply those here so the link can complete.
struct CoreOptions coreOptions;
bool pause_next = false;

int main(int argc, char** argv) {
    const char* script = (argc >= 2) ? argv[1] : "lua_api_test.lua";

    auto& eng = vbam::wx::LuaInstance();

    bool   any_failure = false;
    size_t lines       = 0;
    eng.SetLogSink([&](const std::string& s) {
        std::fputs(s.c_str(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        ++lines;
        if (s.find("[FAIL]")        != std::string::npos ||
            s.find("test failures") != std::string::npos) {
            any_failure = true;
        }
    });

    if (!eng.LoadFile(script)) {
        std::fprintf(stderr, "LoadFile(\"%s\") failed\n", script);
        return 2;
    }

    // The script body runs on a coroutine; ResumeScriptCoroutine kicks
    // it off the first time. Our test script never calls
    // emu.frameadvance(), so it runs to completion in a single resume.
    eng.ResumeScriptCoroutine();

    // --- memory-hook dispatch smoke test --------------------------------
    // The scripted part above only registers hooks — nothing fires
    // them without emulation. Fire them directly through the engine to
    // regression-test the dispatcher: registry entries are
    // {start, end, fn} sub-tables, and dispatch must range-filter and
    // call .fn (an earlier bug pcall'd the entry table itself).
    {
        eng.EvalRepl(
            "memfire = {} "
            "memory.registerread(0x03004000, 4, function(a, s, v) "
            "  memfire[#memfire + 1] = string.format('%x/%d/%x', a, s, v) "
            "end) "
            "memory.registerwrite(0x03004010, function(a, s, v) "
            "  memfire[#memfire + 1] = 'W' .. string.format('%x/%d/%x', a, s, v) "
            "end) "
            "memory.registerexec(0x08000000, function(a) "
            "  memfire[#memfire + 1] = 'X' .. string.format('%x', a) "
            "end)");
        eng.DispatchMemRead (0x03004000, 4, 0xABCD);  // in range
        eng.DispatchMemRead (0x03004003, 1, 0);        // in range
        eng.DispatchMemRead (0x03004004, 1, 0);        // out of range
        eng.DispatchMemWrite(0x03004010, 1, 0x5A);     // in range
        eng.DispatchMemExec (0x08000000);              // in range
        const std::string n = eng.EvalRepl("return #memfire");
        const std::string r = eng.EvalRepl("return table.concat(memfire, ',')");
        const bool dispatch_ok =
            n == "4" &&
            r == "3004000/4/abcd,3004003/1/0,W3004010/1/5a,X8000000";
        std::printf("%s memory hook dispatch (%s)\n",
                    dispatch_ok ? "[ok]  " : "[FAIL]", r.c_str());
        if (!dispatch_ok) any_failure = true;

        // Unregister everything again; the engine must then report no
        // hooks armed (the core-side fast gate turns back off).
        eng.EvalRepl(
            "memory.registerread (0x03004000, 4, nil) "
            "memory.registerwrite(0x03004010, nil) "
            "memory.registerexec (0x08000000, nil)");
        const bool unregistered = !eng.HasReadHooks() && !eng.HasWriteHooks() &&
                                  !eng.HasExecHooks();
        std::printf("%s memory hook unregister disarms gates\n",
                    unregistered ? "[ok]  " : "[FAIL]");
        if (!unregistered) any_failure = true;
    }

    // Drive the registerexit hook so it can print summary info.
    eng.Stop();

    std::fprintf(stderr, "lua_test_runner: %zu log line(s), result=%s\n",
                 lines, any_failure ? "FAIL" : "PASS");
    return any_failure ? 1 : 0;
}

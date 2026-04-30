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

    // Drive the registerexit hook so it can print summary info.
    eng.Stop();

    std::fprintf(stderr, "lua_test_runner: %zu log line(s), result=%s\n",
                 lines, any_failure ? "FAIL" : "PASS");
    return any_failure ? 1 : 0;
}

-- VBA-M Lua API smoke test.
--
-- Exercises every FCEUX-style binding the engine exposes. Designed to
-- run via lua_test_runner with no ROM loaded; assertions that need
-- emulator state (live memory, joypad reads, rom reads) only check the
-- shape of the call, not the returned value.
--
-- Output convention: every check prints "[ok] ..." or "[FAIL] ...".
-- The C++ runner counts FAILs in the log stream and exits non-zero.

local ok_count   = 0
local fail_count = 0

local function check(cond, msg)
    if cond then
        ok_count = ok_count + 1
        emu.print("[ok] " .. msg)
    else
        fail_count = fail_count + 1
        emu.print("[FAIL] " .. msg)
    end
end

local function check_pcall(fn, msg)
    local ok, err = pcall(fn)
    if ok then
        ok_count = ok_count + 1
        emu.print("[ok] " .. msg)
    else
        fail_count = fail_count + 1
        emu.print("[FAIL] " .. msg .. " — " .. tostring(err))
    end
end

emu.print("=== bit library ===")

check(bit.band(0xff,   0x0f) == 0x0f,             "bit.band")
check(bit.bor (0xf0,   0x0f) == 0xff,             "bit.bor")
check(bit.bxor(0xff,   0x0f) == 0xf0,             "bit.bxor")
check(bit.bnot(0)            == 0xffffffff,       "bit.bnot wraps to u32")
check(bit.lshift(1, 4)       == 16,               "bit.lshift")
check(bit.rshift(0xff, 4)    == 0x0f,             "bit.rshift")
-- bit.arshift returns the result in unsigned-32 form (FCEUX/LuaJIT
-- convention), so the sign-extended -1 reads back as 0xffffffff.
check(bit.arshift(-2, 1)     == 0xffffffff,       "bit.arshift sign-extends")
check(bit.rol(0x80000000, 1) == 1,                "bit.rol wraps high bit")
check(bit.ror(1, 1)          == 0x80000000,       "bit.ror wraps low bit")
check(bit.tobit(0x100000000) == 0,                "bit.tobit truncates to 32")
check(bit.tohex(0xdeadbeef)  == "deadbeef",       "bit.tohex lowercase")
check(bit.tohex(0xdeadbeef, -8) == "DEADBEEF",    "bit.tohex uppercase via -n")

emu.print("=== emu namespace ===")

check(type(emu.print)         == "function", "emu.print exists")
check(type(emu.message)       == "function", "emu.message exists")
check(type(emu.frameadvance)  == "function", "emu.frameadvance exists")
check(type(emu.framecount)    == "function", "emu.framecount exists")
check(type(emu.getsystem)     == "function", "emu.getsystem exists")
check(type(emu.romname)       == "function", "emu.romname exists")
check(type(emu.pause)         == "function", "emu.pause exists")
check(type(emu.unpause)       == "function", "emu.unpause exists")
check(type(emu.poweron)       == "function", "emu.poweron exists")
check(type(emu.softreset)     == "function", "emu.softreset exists")
check(type(emu.registerbefore) == "function", "emu.registerbefore exists")
check(type(emu.registerafter)  == "function", "emu.registerafter exists")
check(type(emu.registerexit)   == "function", "emu.registerexit exists")

local fc = emu.framecount()
check(type(fc) == "number", "emu.framecount() returns number, got " .. type(fc))

local sys = emu.getsystem()
check(sys == "GBA" or sys == "GB" or sys == "GBC" or sys == "SGB",
      "emu.getsystem() in {GBA,GB,GBC,SGB}, got " .. tostring(sys))

local rn = emu.romname()
check(type(rn) == "string", "emu.romname() returns string")

emu.message("hello from lua test")

emu.print("=== gui namespace ===")

check_pcall(function() gui.text(10, 20, "hello") end,                      "gui.text 1-color")
check_pcall(function() gui.text(10, 20, "hello", "white") end,             "gui.text 2-color")
check_pcall(function() gui.text(10, 20, "hello", "white", "black") end,    "gui.text fg+bg")
check_pcall(function() gui.text(10, 20, "hello", 0xffffffff, 0x000000ff) end, "gui.text numeric color")
check_pcall(function() gui.text(10, 20, "hex", "#ff8800ff") end,           "gui.text hex color")
check_pcall(function() gui.box(0, 0, 16, 16) end,                          "gui.box no fill")
check_pcall(function() gui.box(0, 0, 16, 16, "red", "white") end,          "gui.box fill+outline")
check_pcall(function() gui.line(0, 0, 100, 50) end,                        "gui.line default color")
check_pcall(function() gui.line(0, 0, 100, 50, "green") end,               "gui.line named color")
check_pcall(function() gui.pixel(5, 5) end,                                "gui.pixel default")
check_pcall(function() gui.pixel(5, 5, "blue") end,                        "gui.pixel named color")

emu.print("=== joypad namespace ===")

check(type(joypad.get)   == "function", "joypad.get exists")
check(type(joypad.set)   == "function", "joypad.set exists")
check(type(joypad.read)  == "function", "joypad.read alias exists")
check(type(joypad.write) == "function", "joypad.write alias exists")

local jp = joypad.get(0)
check(type(jp) == "table", "joypad.get(0) returns table")

local boolean_or_nil = function(v) return v == nil or type(v) == "boolean" end
local ok_keys = true
for _, k in ipairs({"A","B","select","start","right","left","up","down","R","L"}) do
    if not boolean_or_nil(jp[k]) then ok_keys = false break end
end
check(ok_keys, "joypad.get(0) keys are bool-or-nil")

check_pcall(function() joypad.set(0, {A = true, B = false}) end, "joypad.set table")
check_pcall(function() joypad.set(0, nil) end,                   "joypad.set nil clears override")

emu.print("=== memory namespace ===")

check(type(memory.readbyte)        == "function", "memory.readbyte")
check(type(memory.readbytesigned)  == "function", "memory.readbytesigned")
check(type(memory.readword)        == "function", "memory.readword")
check(type(memory.readwordsigned)  == "function", "memory.readwordsigned")
check(type(memory.readdword)       == "function", "memory.readdword")
check(type(memory.readdwordsigned) == "function", "memory.readdwordsigned")
check(type(memory.writebyte)       == "function", "memory.writebyte")
check(type(memory.writeword)       == "function", "memory.writeword")
check(type(memory.writedword)      == "function", "memory.writedword")
check(type(memory.registerread)    == "function", "memory.registerread")
check(type(memory.registerwrite)   == "function", "memory.registerwrite")
check(type(memory.registerexec)    == "function", "memory.registerexec")

-- Live memory reads need a loaded ROM (the core's address-space
-- tables are nullptr otherwise and dereferencing them segfaults). The
-- API-surface contract — "the binding exists and is callable" — is
-- already covered above; we skip the actual fetches here.

check_pcall(function() memory.registerread(0x02000000, function() end) end,
            "memory.registerread(addr, fn)")
check_pcall(function() memory.registerread(0x02000000, 4, function() end) end,
            "memory.registerread(addr, size, fn)")
check_pcall(function() memory.registerread(0x02000000, nil) end,
            "memory.registerread clear")

emu.print("=== rom namespace ===")

check(type(rom.readbyte)  == "function", "rom.readbyte")
check(type(rom.readword)  == "function", "rom.readword")
check(type(rom.readdword) == "function", "rom.readdword")

emu.print("=== savestate namespace ===")

check(type(savestate.create)        == "function", "savestate.create")
check(type(savestate.save)          == "function", "savestate.save")
check(type(savestate.load)          == "function", "savestate.load")
check(type(savestate.registerload)  == "function", "savestate.registerload")
check(type(savestate.registersave)  == "function", "savestate.registersave")

local s = savestate.create()
check(s ~= nil, "savestate.create() returns non-nil")

emu.print("=== hooks ===")

local before_called = false
local after_called  = false
emu.registerbefore(function() before_called = true end)
emu.registerafter (function() after_called  = true end)
emu.registerexit  (function()
    emu.print(string.format("[exit] before=%s after=%s", tostring(before_called), tostring(after_called)))
end)
check(true, "register{before,after,exit} accepted closures")

emu.print(string.format("RESULT: %d ok, %d fail", ok_count, fail_count))

if fail_count > 0 then
    error("test failures: " .. fail_count)
end

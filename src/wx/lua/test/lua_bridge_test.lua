-- Bridge integration test server.
--
-- Loaded by lua_bridge_runner (ctest: wx.lua.bridge). The runner acts
-- as the external client: it connects over TCP, sends one-line
-- commands, and pumps the frame loop; this script polls them off the
-- bridge queue once per emu.frameadvance().
--
-- Commands (line-based, replies are one line each):
--   ping                -> pong
--   press <btns>        -> ok | err   (btns: '+'-separated, e.g. A+B+start)
--   release             -> ok         (clears the joypad override)
--   screenshot <path>   -> ok <path> | err <msg>
--   loadrom <path>      -> ok | err   (headless test build: always err)
--   framecount          -> <n>
--   quit                -> bye        (script exits its loop)
--
-- Port comes from VBAM_TEST_PORT so ctest can pick a free one.

local port = tonumber(os.getenv("VBAM_TEST_PORT") or "18923")

local BTN_NAMES = { "A", "B", "select", "start", "right",
                    "left", "up", "down", "R", "L" }

local function press(arg)
    local t = {}
    for name in arg:gmatch("[^+]+") do
        -- accept any case; the engine keys are lowercase for the
        -- non-single-letter names.
        local key = name:lower()
        if key == "a" or key == "b" or key == "r" or key == "l" then
            key = key:upper()
        end
        t[key] = true
    end
    joypad.set(0, t)
end

if not bridge.listen(port) then
    error("bridge.listen(" .. port .. ") failed")
end

while true do
    local msg = bridge.receive()
    if msg then
        local cmd, arg = msg:match("^(%S+)%s*(.-)%s*$")
        if cmd == "ping" then
            bridge.send("pong")
        elseif cmd == "press" then
            press(arg)
            bridge.send("ok")
        elseif cmd == "release" then
            joypad.set(0, nil)
            bridge.send("ok")
        elseif cmd == "screenshot" then
            local path, err = emu.screenshot(arg)
            if path then bridge.send("ok " .. path)
            else          bridge.send("err " .. tostring(err)) end
        elseif cmd == "loadrom" then
            -- In this headless runner the wx loadrom backend is not
            -- installed, so this must come back as a clean error.
            local ok, err = emu.loadrom(arg)
            if ok then bridge.send("err test build: loadrom unexpectedly succeeded")
            else      bridge.send("err " .. tostring(err)) end
        elseif cmd == "framecount" then
            bridge.send(tostring(emu.framecount()))
        elseif cmd == "quit" then
            bridge.send("bye")
            break
        else
            bridge.send("err unknown command: " .. tostring(cmd))
        end
    end
    emu.frameadvance()
end

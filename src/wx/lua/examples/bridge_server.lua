-- VBA-M external control bridge.
--
-- Run this from the emulator (wx frontend: Tools → "Run Lua script…").
-- It opens a TCP server on 127.0.0.1 so an outside program (Python,
-- etc.) can drive the emulator at frame granularity:
--
--   import socket
--   s = socket.create_connection(("127.0.0.1", 8765))
--   s.sendall(b"press A+start\n")   # each command is one line
--   print(s.recv(4096))             # every command gets one reply line
--
-- Commands:
--   ping                          -> pong
--   loadrom <path>                -> ok <name> | err <why>
--                                  load a ROM file (any game the
--                                  emulator supports); the reply comes
--                                  after the load actually landed
--   press <btns>                  -> ok     hold buttons from this frame on
--   release                       -> ok     clear the button override
--   buttons                       -> "A=1 start=1 ..." (current override)
--   speed <n>                     -> ok     advance n frames (1..600)
--   screenshot [path]             -> ok <path>   (default vbam_shot_<n>.png)
--   savestate <path>              -> ok | err
--   loadstate <path>              -> ok | err
--   reset                         -> ok     soft reset
--   pause / unpause               -> ok
--   framecount                    -> <n>
--   romname                       -> <name>
--   romgeneration                 -> <n>    bumps once per ROM load
--   quit                          -> bye     stops this script
--
-- <btns> is '+'-separated: A B select start up down left right L R.
-- The reply to every command is a single '\n'-terminated line.
--
-- The loop below runs once per emulated frame, so a command takes
-- effect on the next frame boundary — deterministic for RL/automation.

local PORT = tonumber(os.getenv("VBAM_BRIDGE_PORT") or "8765")

if not bridge.listen(PORT) then
    error("bridge.listen(" .. PORT .. ") failed (port in use?)")
end
emu.print("[bridge] listening on 127.0.0.1:" .. PORT)

local held = nil  -- current override table, mirrors what we set

local function parse_buttons(arg)
    local t = {}
    for name in arg:gmatch("[^+]+") do
        local key = name:lower()
        if key == "a" or key == "b" or key == "r" or key == "l" then
            key = key:upper()
        end
        t[key] = true
    end
    return t
end

local function apply_held()
    joypad.set(0, held)
end

local function handle(msg)
    local cmd, arg = msg:match("^(%S+)%s*(.-)%s*$")
    -- clients may quote arguments containing spaces: loadrom "/path/My Rom.gba"
    if #arg >= 2 and arg:sub(1, 1) == '"' and arg:sub(-1) == '"' then
        arg = arg:sub(2, -2)
    end

    if cmd == "ping" then
        bridge.send("pong")
    elseif cmd == "loadrom" then
        if arg == "" then
            bridge.send("err loadrom: missing path")
        else
            local gen = emu.romgeneration()
            local ok, err = emu.loadrom(arg)
            if not ok then
                bridge.send("err " .. tostring(err))
            else
                -- emu.loadrom defers the actual load to the next
                -- event-loop pass; wait (up to 10 s of frames) for the
                -- generation counter to bump, which GameArea::LoadGame
                -- does once the new ROM is committed.
                local waited = 0
                while emu.romgeneration() == gen and waited < 600 do
                    emu.frameadvance()
                    waited = waited + 1
                end
                if emu.romgeneration() ~= gen then
                    bridge.send("ok " .. emu.romname())
                else
                    bridge.send("err loadrom: timed out waiting for the load")
                end
            end
        end
    elseif cmd == "press" then
        held = parse_buttons(arg)
        apply_held()
        bridge.send("ok")
    elseif cmd == "release" then
        held = nil
        apply_held()
        bridge.send("ok")
    elseif cmd == "buttons" then
        local parts = {}
        if held then
            for k in pairs(held) do parts[#parts + 1] = k .. "=1" end
            table.sort(parts)
        end
        bridge.send(#parts > 0 and table.concat(parts, " ") or "(none)")
    elseif cmd == "speed" then
        -- Forward: advance arg frames while holding the override.
        local n = tonumber(arg) or 1
        if n < 1 then n = 1 end
        if n > 600 then n = 600 end
        for _ = 2, n do emu.frameadvance() end
        bridge.send("ok")
    elseif cmd == "screenshot" then
        local path, err
        if arg ~= "" then path, err = emu.screenshot(arg)
        else            path, err = emu.screenshot() end
        if path then bridge.send("ok " .. path)
        else      bridge.send("err " .. tostring(err)) end
    elseif cmd == "savestate" then
        local s = savestate.create(arg ~= "" and arg or nil)
        savestate.save(s)
        bridge.send("ok")
    elseif cmd == "loadstate" then
        local s = savestate.create(arg ~= "" and arg or nil)
        savestate.load(s)
        bridge.send("ok")
    elseif cmd == "reset" then
        emu.poweron()
        bridge.send("ok")
    elseif cmd == "pause" then
        emu.pause()
        bridge.send("ok")
    elseif cmd == "unpause" then
        emu.unpause()
        bridge.send("ok")
    elseif cmd == "framecount" then
        bridge.send(tostring(emu.framecount()))
    elseif cmd == "romname" then
        bridge.send(emu.romname())
    elseif cmd == "romgeneration" then
        bridge.send(tostring(emu.romgeneration()))
    elseif cmd == "quit" then
        bridge.send("bye")
        return false
    else
        bridge.send("err unknown command: " .. tostring(cmd))
    end
    return true
end

while true do
    local msg = bridge.receive()
    if msg and not handle(msg) then break end
    emu.frameadvance()
end

bridge.close()
emu.print("[bridge] closed")

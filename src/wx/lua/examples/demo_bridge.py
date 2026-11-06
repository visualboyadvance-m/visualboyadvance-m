#!/usr/bin/env python3
"""External-control demo for the VBA-M Lua bridge.

Drives the emulator purely from the outside, the way an automation
host would: load a ROM, press buttons, take screenshots. Assumes the
emulator is running with examples/bridge_server.lua active (default
port 8765).

    python3 demo_bridge.py <rom-path> [out-dir]

Outputs numbered PNGs into out-dir (default: demo_shots) and prints
every command/reply pair so the transcript itself is evidence.
"""

import os
import socket
import sys
import time


class Bridge:
    def __init__(self, host="127.0.0.1", port=8765, timeout=30.0):
        self.sock = socket.create_connection((host, port), timeout=5.0)
        self.sock.settimeout(timeout)
        self.buf = b""

    def cmd(self, line, timeout=30.0):
        self.sock.settimeout(timeout)
        self.sock.sendall(line.encode("utf-8") + b"\n")
        while b"\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise EOFError("bridge closed")
            self.buf += chunk
        out, self.buf = self.buf.split(b"\n", 1)
        reply = out.decode("utf-8", "replace").rstrip("\r")
        print(f">>> {line}\n<<< {reply}")
        return reply


def main():
    rom = sys.argv[1] if len(sys.argv) > 1 else "game/Pokemon_Crystal[T+Chi](V1.1).gbc"
    outdir = sys.argv[2] if len(sys.argv) > 2 else "demo_shots"
    os.makedirs(outdir, exist_ok=True)
    rom = os.path.abspath(rom)

    b = Bridge()
    print("=== ping ===")
    assert b.cmd("ping") == "pong"

    print("=== load ROM from outside ===")
    r = b.cmd(f'loadrom "{rom}"', timeout=60)
    assert r.startswith("ok "), f"loadrom failed: {r}"
    print(f"loaded: {r[3:]}")

    # Let the boot sequence settle (Nintendo logo -> title).
    b.cmd("speed 240")  # ~4 s

    n = 0
    def shot(tag):
        nonlocal n
        n += 1
        path = os.path.abspath(os.path.join(outdir, f"{n:02d}_{tag}.png")).replace("\\", "/")
        r = b.cmd(f'screenshot "{path}"')
        assert r == f"ok {path}", r

    shot("boot")

    print("=== press A (title -> menu/intro) ===")
    b.cmd("press A")
    b.cmd("speed 90")
    shot("after_A")
    b.cmd("release")
    b.cmd("speed 60")

    print("=== press Start ===")
    b.cmd("press start")
    b.cmd("speed 90")
    shot("after_start")
    b.cmd("release")
    b.cmd("speed 60")

    print("=== press A again ===")
    b.cmd("press A")
    b.cmd("speed 150")
    shot("after_A2")
    b.cmd("release")
    b.cmd("speed 60")

    print("=== state queries ===")
    print("romname:", b.cmd("romname"))
    print("framecount:", b.cmd("framecount"))
    print("romgeneration:", b.cmd("romgeneration"))

    print("=== savestate round-trip ===")
    state = os.path.abspath(os.path.join(outdir, "demo_state.sav")).replace("\\", "/")
    b.cmd(f'savestate "{state}"')
    shot("saved")

    print("\nALL DEMO STEPS PASSED")


if __name__ == "__main__":
    main()

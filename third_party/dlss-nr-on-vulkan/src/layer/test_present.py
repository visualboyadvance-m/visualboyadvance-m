#!/usr/bin/env python3
"""The layer inside a real `vkQueuePresentKHR`, on a surface that needs no screen.

Everything else about the layer had a test and this did not: the copy out, the copy back,
and who waits for whom. It took a game, so it was never run — and the first time it was, it
found that `present_now` called itself, which is a stack overflow on the first present of
any application. That is what this file is for.

`work/test_present` (C) presents 2N frames through a headless swapchain with the layer in
the chain; this drives it against a stand-in daemon and checks both directions:

  - the first pass clears each image to a colour carrying the frame number, and the daemon
    must be handed exactly that. Copying before the clear has finished — a missing wait —
    shows up as the previous contents.
  - the second pass records nothing, so each image still holds what the layer wrote into it
    last time round, and the daemon must be handed its own answer back.

Both are run twice: with `vkQueueWaitIdle` (the default) and with `NR_LAYER_SYNC=semaphore`.

**What this does not prove.** Dropping the wait on the game's own semaphores from the
semaphore path leaves every check here passing: on this machine the clear has finished long
before the copy is submitted, so the race does not show. It catches a present that never
returns, a copy that goes nowhere and an answer that never lands — not a missing wait. That
needs hardware where the copy can outrun the draw, or a game.
"""
import os
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import threading

import nr_paths

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "src"))
import nr_build  # noqa: E402
BINARY = nr_build.executable("test_present")
LAYER = nr_build.BUILD_DIR / "layer-check"      # the manifests, beside the library they name
REPLY = bytes((17, 34, 51, 255))          # B G R A, nothing a clear in this test produces
FAILURES = []


def check(name, ok, detail=""):
    print(f"  [{'ok  ' if ok else 'FAIL'}] {name}{'  ' + detail if detail else ''}", flush=True)
    if not ok:
        FAILURES.append(name)


def stand_in(path, seen, stop):
    """A daemon that records the frame it was handed and answers with a flat colour."""
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(path)
    server.listen(8)
    server.settimeout(60)
    while not stop.is_set():
        try:
            connection, _ = server.accept()
        except OSError:
            break
        with connection:
            header = connection.recv(16)
            if len(header) < 16:
                continue
            _magic, width, height, _format = struct.unpack("<4I", header)
            want = width * height * 4
            body = b""
            while len(body) < want:
                piece = connection.recv(want - len(body))
                if not piece:
                    break
                body += piece
            seen.append(body[:4])
            connection.sendall(REPLY * (width * height))
    server.close()


def present(mode, rounds=2):
    """Run the C binary once, with the layer live, and return what the daemon saw."""
    with tempfile.TemporaryDirectory() as room:
        path = str(pathlib.Path(room) / "d.sock")
        seen, stop = [], threading.Event()
        thread = threading.Thread(target=stand_in, args=(path, seen, stop), daemon=True)
        thread.start()
        environment = dict(nr_paths.loader_environment(), VK_LAYER_PATH=str(LAYER),
                           ENABLE_NR_LAYER="1", NR_LAYER_SOCKET=path, NR_LAYER_LIVE="1")
        environment.pop("NR_LAYER_TRIGGER", None)
        environment.pop("NR_TEST_NO_LAYER", None)
        if mode:
            environment["NR_LAYER_SYNC"] = mode
        got = subprocess.run([str(BINARY), str(rounds)], capture_output=True, text=True,
                             env=environment, timeout=300)
        stop.set()
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as poke:
            try:                                  # unblock the accept so the thread ends
                poke.connect(path)
            except OSError:
                pass
        thread.join(timeout=10)
    return got, seen


def run(mode):
    label = mode or "queue idle"
    got, seen = present(mode)
    if got.returncode != 0:
        check(f"{label}: the frames present", False,
              (got.stderr.strip().splitlines() or ["no output"])[-1][:120])
        return
    images = sum(1 for line in got.stdout.splitlines() if "drawn" in line)
    check(f"{label}: the frames present", images > 0 and len(seen) >= images,
          f"{images} drawn, {len(seen)} reached the daemon")
    drawn = seen[:images]
    # blue carries the frame number: a copy that ran before the clear would hold the frame
    # before it, or nothing at all
    expected = [(frame + 1, 128, 64, 255) for frame in range(len(drawn))]
    seen_tuples = [tuple(pixel) for pixel in drawn]
    check(f"{label}: the daemon is handed the frame the game drew",
          seen_tuples == expected,
          f"{seen_tuples[:3]} against {expected[:3]}")
    held = [tuple(pixel) for pixel in seen[images:]]
    # the second pass draws nothing, so what the layer wrote last time is what is there —
    # except an image this run never reached, which is still black
    answered = [pixel for pixel in held if pixel != (0, 0, 0, 0)]
    check(f"{label}: what the layer wrote is in the image next time round",
          bool(answered) and all(pixel == tuple(REPLY) for pixel in answered),
          f"{len(answered)} of {len(held)} untouched frames carry the daemon's answer")


def main():
    if not BINARY.exists():
        print("present: skipped (make work/test_present first) — a skip is not a pass")
        return 0
    if not (LAYER / "VkLayer_dlss_nr.json").exists():
        subprocess.run([sys.executable, str(ROOT / "src" / "layer" / "prepare_layer.py"),
                        str(LAYER)], check=True, capture_output=True)
    probe = subprocess.run([str(BINARY), "1"], capture_output=True, text=True,
                           env=dict(nr_paths.loader_environment(), NR_TEST_NO_LAYER="1"),
                           timeout=300)
    if probe.returncode != 0:
        print("present: skipped (no headless surface here: "
              f"{(probe.stderr.strip().splitlines() or ['?'])[-1][:60]}) — a skip is not a pass")
        return 0
    for mode in (None, "semaphore"):
        run(mode)
    if FAILURES:
        print("FAILED: " + ", ".join(FAILURES))
        return 1
    print("present: the layer's own copy out and back, both ways of waiting")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

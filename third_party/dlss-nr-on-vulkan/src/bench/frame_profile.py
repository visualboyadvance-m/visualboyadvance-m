#!/usr/bin/env python3
"""Where a frame's time actually goes, per operation, from GPU timestamps.

`split_cost.py` answers the same question by ablation — run the frame with one half of
the passes removed and difference the wall time — and says itself that the result is
approximate: skipping passes changes the values the rest of the graph works on, and the
buffers stay hot in ways the real frame's do not.

This does not ablate anything. `xmx_profile()` puts one timestamp query after each
recorded pass, so a pass costs `ts[i] - ts[i-1]` on the device's own clock. The barrier
already between passes makes that attribution exact. The frame measured is the frame
that would have run.

    python3 src/bench/frame_profile.py [--size H W] [--runs N]
"""
import argparse
import pathlib
import re
import sys
import time

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "src" / "gpu"))
sys.path.insert(0, str(ROOT / "src" / "ref"))
import nr_frame_resident as F
import nr_model
import xmxres

# Both tables are read from the shaders rather than written out here: a hand-kept
# copy silently mislabels every row the moment a kind is added, and this file
# shipped one such mistake before the tables were generated.
def _kinds(path, pattern):
    text = (ROOT / path).read_text()
    return {int(v): n.lower().replace("_", " ")
            for n, v in re.findall(pattern, text)}


UNARY = _kinds("src/gpu/resident.comp", r"([A-Z][A-Z0-9_]*)\s*=\s*(\d+)u")
ROW = _kinds("src/gpu/attention.comp", r"([A-Z][A-Z0-9_]*)\s*=\s*(\d+)u")


def label(family, sub):
    if family == "unary":
        return "unary: %s" % UNARY.get(sub, "kind %d" % sub)
    if family == "row":
        return "row: %s" % ROW.get(sub, "kind %d" % sub)
    if family.startswith("gemm"):
        return "%s (flags %d)" % (family, sub)
    return family


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--size", nargs=2, type=int, default=(768, 1280))
    parser.add_argument("--runs", type=int, default=3)
    args = parser.parse_args()
    height, width = args.size

    model = nr_model.NeuralRenderingModel.from_safetensors(
        ROOT / "work" / "mlxw" / "dlssnr-logical.safetensors")
    runtime = xmxres.Runtime()
    xmxres.profile(True)
    frame = F.ResidentFrame(runtime, model.weights, height, width)
    features = (np.random.default_rng(11).standard_normal((height, width, 16))
                * 0.3).astype(np.float32)

    frame.run(features, execution="single")          # record and warm
    xmxres.profile_reset()
    started = time.perf_counter()
    for _ in range(args.runs):
        frame.run(features, execution="single")
    wall = (time.perf_counter() - started) / args.runs

    totals = xmxres.profile_totals()
    rows = sorted(((ms / args.runs, n / args.runs, label(f, s))
                   for (f, s), (ms, n) in totals.items()), reverse=True)
    device = sum(r[0] for r in rows)
    print("%dx%d, %d runs\n" % (width, height, args.runs))
    print("  %-28s %9s %8s %9s %7s" % ("pass", "ms", "share", "passes", "us each"))
    for ms, passes, name in rows:
        print("  %-28s %9.2f %7.1f%% %9.0f %7.1f"
              % (name, ms, 100 * ms / device, passes, 1000 * ms / max(passes, 1)))
    print("  %-28s %9.2f %7.1f%%" % ("— device total", device, 100.0))
    print("  %-28s %9.2f" % ("— wall per frame", wall * 1e3))
    print("  %-28s %9.2f  (host: recording, numpy, the rest)"
          % ("— host overhead", wall * 1e3 - device))
    gemm = sum(ms for ms, _, name in rows if name.startswith("gemm"))
    print("\n  GEMM %.1f ms of %.1f (%.0f%%); everything else %.1f ms (%.0f%%)"
          % (gemm, device, 100 * gemm / device, device - gemm, 100 * (device - gemm) / device))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""The native CPU passes against the NumPy they transcribe: byte-identical, or nothing.

Every function in `nr_image.c` exists to be faster, not different. The whole discipline
of this port is that an optimisation keeps the output bit-for-bit, so this runs both
implementations over the same awkward inputs — reversed views, padded crops, a mirrored
network extent, values that sit on an FP16 rounding boundary — and requires equality, not
closeness.
"""
import contextlib
import os
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "src" / "ref"))
sys.path.insert(0, str(ROOT / "src" / "layer"))
import nr_frame      # noqa: E402
import nr_image      # noqa: E402
import nr_daemon     # noqa: E402

FAILURES = []


@contextlib.contextmanager
def numpy_only():
    """Force the reference side onto NumPy.

    Once the native path is wired into `nr_frame` and `nr_daemon`, calling them for the
    "reference" runs the very code under test and every comparison passes for the wrong
    reason. `NR_HOST_NATIVE` is read per call, not cached, so this is enough.
    """
    keep = os.environ.get("NR_HOST_NATIVE")
    os.environ["NR_HOST_NATIVE"] = "0"
    try:
        yield
    finally:
        if keep is None:
            os.environ.pop("NR_HOST_NATIVE", None)
        else:
            os.environ["NR_HOST_NATIVE"] = keep


def check(name, ok, detail=""):
    print(f"  [{'ok  ' if ok else 'FAIL'}] {name}{'  ' + detail if detail else ''}", flush=True)
    if not ok:
        FAILURES.append(name)


def same(name, native, reference, detail=""):
    if native is None:
        check(name, False, "no native library built")
        return
    equal = np.array_equal(native, reference)
    if not equal:
        gap = np.abs(np.asarray(native, np.float64) - np.asarray(reference, np.float64))
        detail = f"{(gap > 0).sum()} of {gap.size} differ, worst {gap.max():.3e}"
    check(name, equal, detail)


def codec_checks(rng):
    for kind, vk_format, bgra in (("bgra8", 44, 1), ("rgba8", 37, 0)):
        raw = rng.integers(0, 256, (48, 80, 4), dtype=np.uint8).tobytes()
        with numpy_only():
            reference = nr_daemon.decode(raw, 80, 48, vk_format)
        same(f"decode {kind}", nr_image.decode8(raw, 80, 48, bgra),
             reference)
        image = rng.random((48, 80, 3), dtype=np.float32)
        # values that land exactly on a rounding boundary, and outside the range
        image[0, 0], image[0, 1], image[0, 2] = 0.0, 1.0, 0.5
        image[1, 0], image[1, 1] = -0.1, 1.1
        image[1, 2], image[1, 3] = 1.0 / 255, 254.5 / 255
        with numpy_only():
            reference = np.frombuffer(nr_daemon.encode(image, raw, vk_format), np.uint8)
        same(f"encode {kind}", np.frombuffer(nr_image.encode8(image, raw, bgra), np.uint8),
             reference)
    # a NaN must land on zero, as NumPy's byte cast does
    image = np.zeros((4, 4, 3), np.float32)
    image[0, 0, 0] = np.nan
    raw = bytes(4 * 4 * 4)
    with numpy_only():
        reference = np.frombuffer(nr_daemon.encode(image, raw, 44), np.uint8)
    same("encode carries NaN to zero the way NumPy does",
         np.frombuffer(nr_image.encode8(image, raw, 1), np.uint8), reference)


def resize_checks(rng):
    source = rng.random((57, 91, 3), dtype=np.float32)
    for size in ((31, 50), (120, 200), (57, 33), (57, 91)):
        with numpy_only():
            reference = nr_daemon.resample(source, size)
        same(f"resize to {size[1]}x{size[0]}", nr_image.bilinear(source, size), reference)
    # a reversed view and a padded crop, which the daemon really hands it: the decode is
    # a reversed slice of the wire buffer and the letterbox crop is a padded one
    whole = rng.random((64, 96, 4), dtype=np.float32)
    with numpy_only():
        reference = nr_daemon.resample(whole[..., 2::-1], (20, 30))
    same("resize a reversed view", nr_image.bilinear(whole[..., 2::-1], (20, 30)), reference)
    # Not a whole factor: `resample` takes the area mean when the extent divides evenly,
    # which is a different filter and deliberately so — it is what keeps aliasing out of
    # the network's input. The native path must never stand in for that branch, and the
    # daemon is what keeps them apart.
    crop = whole[8:56, 4:92, :3]
    with numpy_only():
        reference = nr_daemon.resample(crop, (25, 41))
        averaged = nr_daemon.resample(crop, (24, 44))
    same("resize a padded crop", nr_image.bilinear(crop, (25, 41)), reference)
    check("the area mean is not bilinear, and stays NumPy's",
          not np.array_equal(nr_image.bilinear(crop, (24, 44)), averaged),
          "48x88 -> 24x44 divides evenly, so `resample` averages instead of sampling")


def feature_checks(rng):
    for width, height in ((64, 48), (91, 57)):
        colour = rng.random((height, width, 3), dtype=np.float32)
        history = rng.random((height, width, 3), dtype=np.float32)
        geometry = nr_frame.NetworkGeometry.vendor_aligned(width, height)
        rows, columns = geometry.source_rows(), geometry.source_columns()
        noise = nr_frame.deterministic_noise(geometry.network_height,
                                             geometry.network_width, 0)
        controls = np.array([0.0, 1.0, 1.0, -1.0, -1.0], np.float32)
        with numpy_only():
            reference = nr_frame.make_features(colour, geometry=geometry,
                                               **nr_frame.PROFILES["standard"])
        same(f"features {width}x{height} -> {geometry.network_width}x{geometry.network_height}",
             nr_image.features(colour, rows, columns, noise, controls), reference)
        nr_frame.apply_history(reference, history, geometry)     # NumPy either way
        same("features with a history in channels 7-9",
             nr_image.features(colour, rows, columns, noise, controls, history=history),
             reference)


def compose_checks(rng):
    height, width = 48, 80
    colour = rng.random((height, width, 3), dtype=np.float32)
    history = rng.random((height, width, 3), dtype=np.float32)
    previous = colour.copy()
    previous[10:20] = rng.random((10, width, 3), dtype=np.float32)   # a moving band
    previous[20:24] += np.float32(1.0 / 255)                         # one level of drift
    head = (rng.random((height, width, 4), dtype=np.float32) - 0.5).astype(np.float32) * 2
    mask = np.ones((height, width, 3), np.float32)
    mask[30:40, 10:30, 0] = 0.0

    for intensity in (1.0, 0.6, 1.66):
        with numpy_only():
            reference = nr_frame.compose(head, colour, intensity=intensity)
        same(f"still composition, intensity {intensity}",
             nr_image.compose(head, colour, intensity), reference)

    scale = np.float32(nr_frame.BLEND_SCALE)
    for hold, with_previous, control in ((1.0, True, None), (0.0, False, None),
                                         (0.5, True, mask), (1.0, True, mask)):
        floor = (nr_daemon.hold_floor(colour, previous, hold)
                 if with_previous and hold > 0 else None)
        with numpy_only():
            reference = nr_frame.compose(head, colour, intensity=1.0, history=history,
                                         history_floor=floor, control_mask=control)
        gate = nr_frame.history_weight(head)
        slope = np.float32(-255.0 * hold / nr_daemon.HOLD_RAMP)
        same(f"temporal composition, hold {hold}"
             f"{', masked' if control is not None else ''}"
             f"{'' if with_previous else ', no previous frame'}",
             nr_image.compose_temporal(head, colour, history,
                                       previous if with_previous and hold > 0 else None,
                                       gate, control, intensity=1.0,
                                       blend_scale=float(scale), hold=hold,
                                       slope=float(slope)),
             reference)


def main():
    if nr_image.library() is None:
        print("  no work/libnr_image.so (.dylib on macOS) — `make` builds it; the NumPy path still runs")
        return 0
    rng = np.random.default_rng(17)
    codec_checks(rng)
    resize_checks(rng)
    feature_checks(rng)
    compose_checks(rng)
    if FAILURES:
        print(f"\n{len(FAILURES)} FAILED: " + ", ".join(FAILURES), flush=True)
        return 1
    print("\nthe native passes are byte-identical to the NumPy they replace", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""The full-resolution glue in fewer passes, against the passes it replaces.

`upsample_merge` against upsample2, scale_channel, residual and to_half; `gemm_dual`
against a GEMM and a to_half of its output. Both outputs of each must match byte for
byte, and the guard values past them must survive.
"""
import numpy as np
import xmxres as X

GUARD = 64
# NaNs with their own payloads: no pass fed finite values produces one, so a fill that
# survives in the payload is an element nobody wrote, and one past it a guard intact
FILL16 = np.array([0x7E5A], np.uint16).view(np.float16)[0]
FILL32 = np.array([0x7FC0DEAD], np.uint32).view(np.float32)[0]


def fill(dtype):
    return FILL16 if np.dtype(dtype) == np.float16 else FILL32


def filled(values, dtype):
    """Which elements still hold the fill, compared by bits since the fill is a NaN."""
    bits = np.uint16 if np.dtype(dtype) == np.float16 else np.uint32
    return np.asarray(values).view(bits) == np.asarray(fill(dtype)).view(bits)


def check(name, got, want):
    if not np.array_equal(got.view(np.uint8), want.view(np.uint8)):
        bad = np.flatnonzero(got.reshape(-1) != want.reshape(-1))
        raise AssertionError(f"{name}: {bad.size} of {got.size} differ, first at "
                             f"{bad[0] if bad.size else '?'}")


def upsample_cases(rt, rng):
    cases = 0
    channels = 32
    for height, width in ((8, 8), (16, 24), (13, 21), (64, 40)):
        sh, sw = -(-height // 2), -(-width // 2)
        count = height * width * channels
        buffers = []

        def alloc(n, dtype):
            b = rt.buffer(n, dtype)
            buffers.append(b)
            return b
        try:
            source = alloc(sh * sw * channels, np.float16)
            skip = alloc(count, np.float16)
            sin = alloc(channels, np.float32)
            cos = alloc(channels, np.float32)
            sincos = alloc(2 * channels, np.float32)
            upsampled, merged_ref = alloc(count + GUARD, np.float32), alloc(count + GUARD, np.float32)
            half_ref = alloc(count + GUARD, np.float16)
            merged, half = alloc(count + GUARD, np.float32), alloc(count + GUARD, np.float16)
            for spread in (0.01, 1.0, 300.0):
                src = rng.normal(0, spread, sh * sw * channels).astype(np.float16)
                src[:4] = [0, -0.0, 65504, -65504]
                sk = rng.normal(0, spread, count).astype(np.float16)
                sk[:2] = [-0.0, 2 ** -24]
                s = rng.uniform(-2, 2, channels).astype(np.float32)
                c = rng.uniform(-2, 2, channels).astype(np.float32)
                s[0], c[0] = 0.0, -0.0
                X.host_write(source, src)
                X.host_write(skip, sk)
                X.host_write(sin, s)
                X.host_write(cos, c)
                X.host_write(sincos, np.concatenate([s, c]))
                for mask in (0, 7):
                    rt.specialize(mask)
                    for b, dtype in ((merged_ref, np.float32), (merged, np.float32),
                                     (half_ref, np.float16), (half, np.float16)):
                        X.host_write(b, np.full(count + GUARD, fill(dtype), dtype))
                    rt.begin()
                    rt.upsample2(source, upsampled, sw, height, width, channels, a_half=True)
                    rt.scale_channel(upsampled, sin, merged_ref, count, channels)
                    rt.residual(merged_ref, skip, cos, merged_ref, count, channels, b_half=True)
                    rt.to_half(merged_ref, half_ref, count)
                    rt.upsample_merge(source, skip, sincos, merged, half, height, width, sw,
                                      channels)
                    rt.submit()
                    check(f"merged {height}x{width} spread {spread} mask {mask}",
                          X.host_view(merged), X.host_view(merged_ref))
                    check(f"half {height}x{width} spread {spread} mask {mask}",
                          X.host_view(half, np.float16), X.host_view(half_ref, np.float16))
                    assert filled(X.host_view(merged)[count:], np.float32).all()
                    assert filled(X.host_view(half, np.float16)[count:], np.float16).all()
                    assert not filled(X.host_view(merged)[:count], np.float32).any()
                    assert not filled(X.host_view(half, np.float16)[:count], np.float16).any()
                    cases += 1
        finally:
            for b in buffers:
                b.free()
    return cases


def gemm_cases(rt, rng):
    cases = 0
    for rows, cols, inner in ((1024, 32, 16), (40, 32, 16), (64, 48, 32)):
        buffers = []

        def alloc(n, dtype):
            b = rt.buffer(n, dtype)
            buffers.append(b)
            return b
        try:
            a = alloc(rows * inner, np.float16)
            b = alloc(inner * cols, np.float16)
            out_ref, out = alloc(rows * cols + GUARD, np.float32), alloc(rows * cols + GUARD, np.float32)
            half_ref, half = alloc(rows * cols + GUARD, np.float16), alloc(rows * cols + GUARD, np.float16)
            for spread in (0.01, 1.0, 60.0):
                X.host_write(a, rng.normal(0, spread, rows * inner).astype(np.float16))
                X.host_write(b, rng.normal(0, 0.5, inner * cols).astype(np.float16))
                for mask in (0, 7):
                    rt.specialize(mask)
                    for buf, dtype in ((out_ref, np.float32), (out, np.float32),
                                       (half_ref, np.float16), (half, np.float16)):
                        X.host_write(buf, np.full(rows * cols + GUARD, fill(dtype), dtype))
                    rt.begin()
                    rt.gemm(a, b, out_ref, rows, cols, inner)
                    rt.to_half(out_ref, half_ref, rows * cols)
                    rt.gemm_dual(a, b, out, half, rows, cols, inner)
                    rt.submit()
                    check(f"gemm {rows}x{cols}x{inner} float32", X.host_view(out), X.host_view(out_ref))
                    check(f"gemm {rows}x{cols}x{inner} half",
                          X.host_view(half, np.float16), X.host_view(half_ref, np.float16))
                    assert filled(X.host_view(half, np.float16)[rows * cols:], np.float16).all()
                    assert not filled(X.host_view(half, np.float16)[:rows * cols], np.float16).any()
                    assert not filled(X.host_view(out)[:rows * cols], np.float32).any()
                    cases += 1
            for args in ((a, b, out, out, rows, cols, inner), (a, b, out, half, rows + 1, cols, inner)):
                try:
                    rt.gemm_dual(*args)
                except ValueError:
                    pass
                else:
                    raise AssertionError("invalid GEMM half copy accepted")
        finally:
            for buf in buffers:
                buf.free()
    return cases


def main():
    rt = X.Runtime()
    rng = np.random.default_rng(424242)
    merges = upsample_cases(rt, rng)
    gemms = gemm_cases(rt, rng)
    before = rt.graph_key()
    rt.fuse_glue = not rt.fuse_glue
    assert rt.graph_key() != before
    print(f"glue: {merges} upsample-merge and {gemms} GEMM half-copy cases bit-exact, "
          f"guards and graph key OK; staging={rt.staging}")


if __name__ == "__main__":
    main()

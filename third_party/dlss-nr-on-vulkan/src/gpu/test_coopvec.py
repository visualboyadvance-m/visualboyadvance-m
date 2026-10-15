#!/usr/bin/env python3
"""The vector-matrix GEMM on Direct3D 12 (gemm_coopvec.hlsl, notes/phase77): right, routed,
and no further from numpy than the multiply-add kernels are.

libd3dmx runs a GEMM on Shader Model 6.10's thread-scope vector-matrix multiply wherever the
device offers it and the matrix slices it would load start 128-byte aligned (`coopvec_fits`);
everything else stays on the multiply-add kernels. That product's order of summation is the
device's, so this does not ask for the portable kernels' bits. It asks that every routed
shape — plain, transposed, batched, strided and offset, every epilogue, half out, the
residual with a float32 and a half skip, the window residual, the compact head, the half copy,
the QKV epilogue plain and window-gathered — comes out within a tolerance of numpy's float32
product of the same halves (or, for Q, K and V, of the multiply-add path's, which
`test_gemm_qkv.py` holds to the separate passes bit for bit); that the misaligned shapes (a
32-byte B offset, a 48-column width, a transposed K of 48, an 88-byte stride) are refused by
the routing and still right on the multiply-add kernels; and that `xmx_coopvec_calls()` and
the profiler's families say which kernel ran each one. The shapes run twice, in a process of
their own each time: once as the runtime routes them, once under XMX_PORTABLE=1.

Without the path — another runtime, a device or runtime without Shader Model 6.10, the kernel
not compiled in — the first worker exits 77, this prints why and passes: a skip is not a pass,
and the line says so.
"""
import os
from pathlib import Path
import subprocess
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "src"))
sys.path.insert(0, str(ROOT / "src" / "gpu"))
sys.path.insert(0, str(ROOT / "src" / "ref"))

PK_STAGED = 2            # the family the vector-matrix kernel is stamped as (libd3dmx.c)
SKIP = 77
# One product of K halves in float32 is exact to about K ulps however it is ordered; a
# device that sums a 128-deep chunk in fp16 (the f16 builds, or the EMULATED flags) is within
# about 2^-11 of the magnitude. The bound takes the second; the printout shows the first.
RELATIVE = 2e-3


def reference(A, B, epilogue=0, narrow=False):
    import xmxres
    import nr_model
    out = A.astype(np.float32) @ B.astype(np.float32)
    if epilogue in (xmxres.EPI_GATE, xmxres.EPI_GATE_E4M3):
        out = nr_model.quadratic_gate_activation(out)
    if epilogue in (xmxres.EPI_E4M3, xmxres.EPI_GATE_E4M3):
        out = nr_model.e4m3(out)
    if epilogue == xmxres.EPI_HALF or narrow:
        out = nr_model._half_rounded(out)
    return out


def gathered_rows(rt, height, width, origin, image):
    """The window-ordered rows of an image, zero outside it, as the partition writes them."""
    ph, pw, (top, left) = rt.window_extent(height, width, origin, 8)
    rows = ph * pw
    out = np.zeros((rows, image.shape[1]), np.float32)
    inside = np.zeros(rows, bool)
    for r in range(rows):
        window, token = divmod(r, 64)
        y = (window // (pw // 8)) * 8 + token // 8 - top
        x = (window % (pw // 8)) * 8 + token % 8 - left
        if 0 <= y < height and 0 <= x < width:
            out[r] = image[y * width + x]
            inside[r] = True
    return out, inside, rows, pw


def worker(destination, want_path):
    """Every shape, in this process's routing; the outputs, the families that ran them and
    the routed counts go to `destination`."""
    import xmx
    import xmxres as X
    import nr_model
    rt = X.Runtime()
    lib = rt.lib
    on = int(lib.xmx_coopvec()) if hasattr(lib, "xmx_coopvec") else 0
    print("GEMMs on: %s" % xmx.path_note())
    if want_path and on != 1:
        raise SystemExit(SKIP)
    if not want_path and on != 0:
        raise SystemExit("XMX_PORTABLE=1 was set but the runtime kept the vector-matrix path")
    X.profile(True)
    rng = np.random.default_rng(770915)
    results, families, routed, fits = {}, {}, {}, {}

    def run(key, record, c, dtype, shape, fit=True):
        X.profile_reset()
        before = int(lib.xmx_coopvec_calls())
        rt.begin()
        record()
        rt.submit()
        results[key] = X.host_view(c, dtype, shape).astype(np.float32).copy()
        families[key] = sorted({int(k) // 32 for k in X.profile_each_kinds()})
        routed[key] = int(lib.xmx_coopvec_calls()) - before
        fits[key] = int(fit)
        return results[key]

    # 1. the plain contract: shapes that take every slice width (64, 32, 16) and every K chunk
    # (128, 64, 32, 16), batched and transposed; and four the routing must refuse
    cases = (
        # rows, cols, inner, batch, transposed, leading, offsets, fits
        (64, 64, 128, 1, False, None, (0, 0, 0), True),
        (40, 96, 64, 1, False, None, (0, 0, 0), True),          # 64 + 32 columns, 32 rows a group
        (32, 16, 16, 1, False, None, (0, 0, 0), True),          # one 16-slice, one 16-chunk
        (16, 80, 144, 1, False, None, (0, 0, 0), True),         # 64 + 16 wide; 128 + 16 deep
        (64, 128, 4096, 1, False, None, (0, 0, 0), True),       # the bottleneck's depth
        (48, 64, 128, 3, False, None, (0, 0, 0), True),         # a batch, its B stride 16 KB
        (32, 64, 64, 2, True, None, (0, 0, 0), True),           # B stored (N, K)
        (32, 128, 32, 1, True, None, (0, 0, 0), True),          # transposed, one 32-chunk
        (16, 32, 32, 1, False, (48, 80, 96), (8, 64, 32), True),    # strides and 128-byte offsets
        (16, 32, 32, 1, False, (48, 80, 96), (8, 16, 32), False),   # a 32-byte B offset: refused
        (24, 48, 32, 1, False, None, (0, 0, 0), False),         # 48 wide: the 16-slice at 64 bytes
        (32, 64, 48, 1, True, None, (0, 0, 0), False),          # transposed K = 32 + 16: 64 bytes
        (16, 32, 32, 1, False, (48, 44, 96), (0, 0, 0), False),     # ldb 44: an 88-byte stride
    )
    for rows, cols, inner, batch, transposed, leading, offsets, fit in cases:
        lda, ldb, ldc = leading or (inner, inner if transposed else cols, cols)
        brows = cols if transposed else inner
        wa = offsets[0] + rows * lda
        wb = offsets[1] + brows * ldb
        wc = offsets[2] + rows * ldc
        A = (rng.standard_normal((batch, wa)) * 0.5).astype(np.float16)
        B = (rng.standard_normal((batch, wb)) * 0.5).astype(np.float16)
        a = rt.buffer_from(A, np.float16)
        b = rt.buffer_from(B, np.float16)
        c = rt.buffer(batch * wc)
        c.zero()
        key = "plain %dx%dx%d b%d%s%s%s" % (rows, cols, inner, batch, " B^T" if transposed else "",
                                           " strided" if leading else "", "" if fit else " (refused)")
        run(key, lambda: rt.gemm(a, b, c, rows, cols, inner, batch=batch, transpose_b=transposed,
                                 strides=(wa, wb, wc), leading=leading, offsets=offsets),
            c, np.float32, (batch, wc), fit)
        want = np.zeros((batch, wc), np.float32)
        for i in range(batch):
            Ai = A[i, offsets[0]:offsets[0] + rows * lda].reshape(rows, lda)[:, :inner]
            Bi = B[i, offsets[1]:offsets[1] + brows * ldb].reshape(brows, ldb)
            Bi = Bi[:, :inner].T if transposed else Bi[:, :cols]
            want[i, offsets[2]:offsets[2] + rows * ldc].reshape(rows, ldc)[:, :cols] = reference(Ai, Bi)
        results[key + " want"] = want
        for buf in (a, b, c):
            buf.free()

    # 2. every epilogue, wide and narrow, and the residual, on a shape the path takes
    rows, cols, inner = 64, 96, 64
    A = (rng.standard_normal((rows, inner)) * 0.3).astype(np.float16)
    B = (rng.standard_normal((inner, cols)) * 0.3).astype(np.float16)
    a = rt.buffer_from(A, np.float16)
    b = rt.buffer_from(B, np.float16)
    for epilogue, label in ((X.EPI_NONE, "none"), (X.EPI_E4M3, "e4m3"), (X.EPI_GATE, "gate"),
                            (X.EPI_GATE_E4M3, "gate+e4m3"), (X.EPI_HALF, "half")):
        for narrow in (False, True):
            dtype = np.float16 if narrow else np.float32
            c = rt.buffer(rows * cols, dtype)
            key = "epilogue %s %s" % (label, "half out" if narrow else "float out")
            run(key, lambda: rt.gemm(a, b, c, rows, cols, inner, epilogue=epilogue, narrow=narrow),
                c, dtype, (rows, cols))
            results[key + " want"] = reference(A, B, epilogue, narrow)
            c.free()
    cosine = rng.uniform(-1.5, 1.5, cols).astype(np.float32)
    cos = rt.buffer_from(cosine)
    for skip_half in (False, True):
        S = (rng.standard_normal((rows, cols)) * 0.5).astype(np.float16 if skip_half else np.float32)
        skip = rt.buffer_from(S, np.float16 if skip_half else np.float32)
        for epilogue in (X.EPI_NONE, X.EPI_E4M3):
            c = rt.buffer(rows * cols)
            key = "residual %s skip, epilogue %d" % ("half" if skip_half else "float", epilogue)
            run(key, lambda: rt.gemm_residual(a, b, skip, cos, c, rows, cols, inner,
                                              epilogue=epilogue, skip_half=skip_half),
                c, np.float32, (rows, cols))
            want = A.astype(np.float32) @ B.astype(np.float32) + S.astype(np.float32) * cosine[None, :]
            results[key + " want"] = nr_model.e4m3(want) if epilogue == X.EPI_E4M3 else want
            c.free()
        skip.free()
    for buf in (a, b, cos):
        buf.free()

    # 3. the window residual written back into the unpadded image: a 10x12 image of 32
    # channels, windows of 8 from an origin of (-2, -3), the padding never stored
    height, width, channels, origin = 10, 12, 32, (-2, -3)
    _, inside, wrows, _ = gathered_rows(rt, height, width, origin, np.zeros((height * width, channels)))
    A = (rng.standard_normal((wrows, channels)) * 0.3).astype(np.float16)
    B = (rng.standard_normal((channels, channels)) * 0.3).astype(np.float16)
    S = (rng.standard_normal((height * width, channels)) * 0.5).astype(np.float32)
    cosine = rng.uniform(-1.5, 1.5, channels).astype(np.float32)
    a = rt.buffer_from(A, np.float16)
    b = rt.buffer_from(B, np.float16)
    skip = rt.buffer_from(S)
    cos = rt.buffer_from(cosine)
    c = rt.buffer(height * width * channels)
    c.zero()
    key = "window residual %dx%d" % (height, width)
    run(key, lambda: rt.gemm_residual(a, b, skip, cos, c, wrows, channels, channels,
                                      reverse=(height, width, 8, origin)),
        c, np.float32, (height * width, channels))
    branch = A.astype(np.float32) @ B.astype(np.float32)
    # the image's rows gathered back: each inside token's row is one pixel
    index, _, _, _ = gathered_rows(rt, height, width, origin, np.arange(height * width, dtype=np.float32)[:, None])
    want = np.zeros((height * width, channels), np.float32)
    for r in np.flatnonzero(inside):
        pixel = int(index[r, 0])
        want[pixel] = branch[r] + S[pixel] * cosine
    results[key + " want"] = want
    for buf in (a, b, skip, cos, c):
        buf.free()

    # 4. the compact head (N = 16, ldc = 4) and the half copy
    rows, k = 64, 32
    A = (rng.standard_normal((rows, k)) * 0.5).astype(np.float16)
    B = (rng.standard_normal((k, 16)) * 0.5).astype(np.float16)
    a = rt.buffer_from(A, np.float16)
    b = rt.buffer_from(B, np.float16)
    c = rt.buffer(rows * 4)
    key = "compact head"
    run(key, lambda: rt.gemm(a, b, c, rows, 16, k, compact_output=True), c, np.float32, (rows, 4))
    results[key + " want"] = reference(A, B)[:, :4]
    b.free(); c.free()
    B = (rng.standard_normal((k, 64)) * 0.5).astype(np.float16)
    b = rt.buffer_from(B, np.float16)
    c = rt.buffer(rows * 64)
    half = rt.buffer(rows * 64, np.float16)
    key = "half copy"
    run(key, lambda: rt.gemm_dual(a, b, c, half, rows, 64, k), c, np.float32, (rows, 64))
    results[key + " want"] = reference(A, B)
    results["half copy published"] = X.host_view(half, np.float16, (rows, 64)).astype(np.float32).copy()
    results["half copy published want"] = nr_model.e4m3(reference(A, B))
    for buf in (a, b, c, half):
        buf.free()

    # 5. the QKV epilogue, plain and window-gathered: 3 heads, so a 64-wide slice straddles
    # Q and K. Q, K and V are compared with the multiply-add path's in `main`.
    heads, tokens, windows = 3, 64, 4
    channels = heads * 32
    rows = windows * tokens
    A = (rng.standard_normal((rows, channels)) * 0.3).astype(np.float16)
    W = (rng.standard_normal((channels, 3 * channels)) * 0.2).astype(np.float16)
    scale = rng.uniform(0.5, 1.5, heads).astype(np.float32)
    a = rt.buffer_from(A, np.float16)
    w = rt.buffer_from(W, np.float16)
    sc = rt.buffer_from(scale)
    q, kb, v = (rt.buffer(rows * channels, np.float16) for _ in range(3))
    key = "qkv plain"
    run(key, lambda: rt.gemm_qkv(a, w, q, kb, v, sc, rows, channels, heads, tokens), q, np.float16,
        (windows, heads, tokens, 32))
    results[key + " k"] = X.host_view(kb, np.float16, (windows, heads, tokens, 32)).astype(np.float32).copy()
    results[key + " v"] = X.host_view(v, np.float16, (windows, heads, tokens, 32)).astype(np.float32).copy()
    for buf in (a, w, sc, q, kb, v):
        buf.free()
    # gathered from a 20x24 float32 image, origin (-2, -3): 3x4 windows, the padding zero
    height, width, origin = 20, 24, (-2, -3)
    image = (rng.standard_normal((height * width, channels)) * 0.3).astype(np.float32)
    _, _, rows, _ = gathered_rows(rt, height, width, origin, image)
    windows = rows // 64
    a = rt.buffer_from(image)
    w = rt.buffer_from(W, np.float16)
    sc = rt.buffer_from(scale)
    q, kb, v = (rt.buffer(rows * channels, np.float16) for _ in range(3))
    key = "qkv gathered"
    run(key, lambda: rt.gemm_qkv(a, w, q, kb, v, sc, rows, channels, heads, tokens,
                                 window=(height, width, origin)),
        q, np.float16, (windows, heads, tokens, 32))
    results[key + " k"] = X.host_view(kb, np.float16, (windows, heads, tokens, 32)).astype(np.float32).copy()
    results[key + " v"] = X.host_view(v, np.float16, (windows, heads, tokens, 32)).astype(np.float32).copy()
    for buf in (a, w, sc, q, kb, v):
        buf.free()

    np.savez(destination, **results,
             **{"families " + k: np.array(v) for k, v in families.items()},
             **{"routed " + k: np.array([v]) for k, v in routed.items()},
             **{"fits " + k: np.array([v]) for k, v in fits.items()})


def compare(label, got, want, tol, failures):
    d = float(np.abs(got.astype(np.float64) - want.astype(np.float64)).max())
    ok = d <= tol
    print("  %-52s max|d| %.3e  (tol %.1e)  %s" % (label, d, tol, "OK" if ok else "FAIL"))
    if not ok:
        failures.append(label)


def main():
    if len(sys.argv) >= 4 and sys.argv[1] == "--worker":
        worker(sys.argv[2], int(sys.argv[3]))
        return
    import nr_build
    tmp = Path(nr_build.BUILD_DIR) / "test_coopvec"
    tmp.mkdir(parents=True, exist_ok=True)
    outputs = {}
    for name, want_path, env in (("vector", 1, {}), ("portable", 0, {"XMX_PORTABLE": "1"})):
        dest = tmp / (name + ".npz")
        done = subprocess.run([sys.executable, __file__, "--worker", str(dest), str(want_path)],
                              env=dict(os.environ, **env))
        if done.returncode == SKIP:
            print("the vector-matrix GEMM: skipped (this runtime or device has no such path, or it "
                  "is off) — a skip is not a pass")
            return
        if done.returncode:
            raise SystemExit("the %s worker failed (%d)" % (name, done.returncode))
        outputs[name] = np.load(dest)
    vec, port = outputs["vector"], outputs["portable"]
    failures = []
    runs = [k[len("routed "):] for k in vec.files if k.startswith("routed ")]
    print("against numpy, the multiply-add kernels' own distance beside it, and against each other:")
    for key in runs:
        tol = RELATIVE
        if key + " want" in vec.files:
            want = vec[key + " want"]
            tol = RELATIVE * max(1.0, float(np.abs(want).max()))
            compare(key, vec[key], want, tol, failures)
            dp = float(np.abs(port[key].astype(np.float64) - want.astype(np.float64)).max())
            print("  %-52s multiply-add max|d| %.3e" % ("", dp))
        else:
            tol = RELATIVE * max(1.0, float(np.abs(port[key]).max()))
        compare(key + " (vector vs multiply-add)", vec[key], port[key], tol, failures)
        fit, routed, fams = int(vec["fits " + key][0]), int(vec["routed " + key][0]), list(vec["families " + key])
        if fit and (routed != 1 or fams != [PK_STAGED]):
            failures.append(key + " routing")
            print("  %-52s FAIL: expected the vector-matrix kernel, got routed=%d families=%s" % (key, routed, fams))
        if not fit and (routed != 0 or PK_STAGED in fams):
            failures.append(key + " routing")
            print("  %-52s FAIL: expected the multiply-add kernels, got routed=%d families=%s" % (key, routed, fams))
        if int(port["routed " + key][0]) != 0 or PK_STAGED in list(port["families " + key]):
            failures.append(key + " portable routing")
            print("  %-52s FAIL: XMX_PORTABLE=1 still routed to the vector-matrix kernel" % key)
    for key in ("qkv plain k", "qkv plain v", "qkv gathered k", "qkv gathered v"):
        compare(key + " (vector vs multiply-add)", vec[key], port[key],
                RELATIVE * max(1.0, float(np.abs(port[key]).max())), failures)
    compare("half copy published", vec["half copy published"], vec["half copy published want"],
            RELATIVE * max(1.0, float(np.abs(vec["half copy published want"]).max())), failures)
    if failures:
        raise SystemExit("FAILED: " + ", ".join(failures))
    print("vector-matrix GEMM: every routed shape within %.0e of numpy and of the multiply-add kernels, "
          "the refused ones on the multiply-add kernels and right" % RELATIVE)


if __name__ == "__main__":
    main()

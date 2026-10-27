# Phase 79 — the GEMM block on an M6's matrix units, and their own summation order

**2026-10-27, on an Apple M6 Mac mini (macOS 27.0.1, 12-core GPU, Apple11 and Metal4
families) over ssh, with the M1 of phase 78 alongside.** The first run of this tree on
a GPU with matrix units for `mpp::tensor_ops::matmul2d`. The question was which block
the GEMMs should use there. The answer: 16 x 64 on one simdgroup, operands straight from
device memory, never staged through threadgroup memory. Two other findings came out of
it. The M6 sums in an order no M1-M4 does. And the feed-forward's general shapes had
to move onto matmul2d.

## The sweep

A probe ran every product shape the frame's GEMMs use through matmul2d with 80 block
configurations. Block M ran over 8-128, block N over 16-128, on 1-8 simdgroups, with
operands from device memory. A second probe ran the staged layouts, which copy A and B
into threadgroup memory before matmul2d reads them. Results are a fraction of the best
configuration for each shape:

| shape (M x N x K) | 16x32/1 | 16x64/1 | 16x128/2 | 32x32/1 | staged 64x32/4 | best staged |
| --- | --- | --- | --- | --- | --- | --- |
| M6 24576 x 256 x 256 | 0.93 | **0.99** | 0.99 | 0.93 | 0.44 | 0.71 |
| M6 6144 x 512 x 128 | 0.87 | **0.99** | 1.00 | 0.86 | 0.51 | 0.64 |
| M6 98304 x 128 x 64 | 0.96 | 0.95 | 0.96 | 0.93 | 0.86 | 0.97 |
| M6 98304 x 32 x 32 | 0.97 | 0.95 | 0.83 | 0.99 | — | — |
| M1 24576 x 256 x 256 | 0.97 | 0.96 | 0.91 | 1.00 | 0.70 | 0.70 |
| M1 6144 x 512 x 128 | 0.95 | 0.94 | 0.89 | 1.00 | 0.62 | 0.64 |

The deep shapes (K 128-256) are where the time goes. On the M6 they want a wide block:
16 x 64 on one simdgroup or 16 x 128 on two. The 16 x 32 that libmetalmx's direct
kernel uses reaches 0.87-0.93. The staged kernel that took those shapes (K >= 128,
64 x 32 on four simdgroups) reaches 0.44-0.51, and no staged layout passes 0.71. On the
M6's matrix units, matmul2d fetching its own operands beats a threadgroup copy.

The shallow shapes (K 32-64, and the batched window shapes) are bound by memory
bandwidth, and every block lands within a few percent. The best matmul2d block ran
the deep shapes at 14-15.7 TFLOP/s on the M6, against 2.3-2.4 on the M1.

On the M1 the deep shapes want 32 x 32 or 16 x 32. 16 x 64 is at 0.94-0.96 there, and
staging loses less (0.62-0.76).

## What changed

- **`libmetalmx.m`**: `g.matrix_units` is set for a Metal 4 device in
  `MTLGPUFamilyApple10` (1010, the M5 and later; the M6 reports Apple11).
  - On matrix units the default staging threshold is `UINT_MAX`, so every GEMM goes to
    a direct kernel. The pool still takes the staged kernel, as it must. `XMX_STAGE_K`
    still overrides the threshold.
  - On matrix units, `gemm_wide` (family 12, the wide route already used by the
    portable path) handles N a multiple of 64. It is the simdgroup 16 x 64 block.
    `XMX_METAL_WIDE=0` turns it off, and `=1` turns it on for any simdgroup path.
- **`gemm_simd.metal`**: `gemm_wide`, `gemm_resident_t<2, 4>`, in both libraries.
- **`ffn_fused.metal`**: the general (non-staged) shapes take their products on
  matmul2d in the Metal 4 library (below).
- **`test_staged_partial.py`** sets `XMX_STAGE_K=128` on the Metal backend. It tests
  the staged kernel, which the M6's routing no longer reaches on its own.
- **`CMakeLists.txt`**: `prepare_weights` is defined only when `nr_frame_cli` is. A
  configure without libpng (an arm64 cross-build from an Intel Mac) failed on the
  generator expression.

## Numbers, the M6

These are `nr_frame`'s features and network pass, 720 x 480 (the network extent is
832 x 512), from a driver that runs the pass several times in one process:

| build | graph time |
| --- | --- |
| Metal 4, routing before | 31.3 ms |
| Metal 4, `XMX_STAGE_K=1000000` | 28.2 ms |
| Metal 4, the new defaults (no staging, `gemm_wide`) | **28.0 ms** |
| Metal 3.1 library | 126.7 ms |

At 1280 x 720 the time went from 74.0 to 66.7 ms. The output hash was the same in every
Metal 4 row. The 4.5x between the two libraries is the matrix units.

## Numbers, the M1

The M1's defaults are unchanged: it has no Apple10 family, so it keeps K >= 128 staged
and the 16 x 32 block. Its graph is 174-175 ms on the Metal 4 library and 369-375 ms on
Metal 3.1, as before. The head hash matches HEAD's build, with and without
`XMX_METAL_WIDE=1` and `XMX_STAGE_K=1000000`, on both libraries.

The routing the M6 now uses was tried on the M1 too, and it was neither faster nor
slower (192-197 ms in that session either way). It was not worth changing a device
nobody had measured badly.

## The M6 sums in its own order

On the M1, all 80 matmul2d configurations gave exactly the hand-written 8 x 8 loops'
bytes on every shape, as phase 78 found. On the M6, none of the 80 did, on any shape.
The worst relative difference against the loops was 6e-4 to 4e-3. All 80 configurations
did agree with each other bit for bit, and so did the staged layouts at every K step.
The matrix units have one summation order, and it is not eight K at a time from zero.

So phase 78's "the two libraries give the same bytes" holds on an M1-M4 and not on an
M5 or later. There the Metal 3.1 and Metal 4 libraries differ in the last bit. Each
still equals itself, fused kernel against unfused passes, as long as both sides take
their products on matmul2d.

One kernel did not. `ffn_fused`'s general shapes (everything but the staged shape
whose 16 x 16 x 32 stem phase 78 converted) still ran the loops. Its unfused passes, the GEMMs, run on matmul2d.
`test_ffn_fused` failed on the M6 at 1024 x 64 (1092 bytes off). Those shapes now run
on matmul2d too: the expand as a 16 x 32 x dynamic product a 32-wide chunk at a time,
the gate and publish into the hidden stage, and the projection accumulated over the
chunks in a cooperative tensor. After that it passes on the M6. On the M1 it passes
on both libraries, and the frame's PNG hash is unchanged (`1276fbfb1ab3c796`).

The comments that asserted byte identity between the libraries now say where it holds:
`libmetalmx.m`, `gemm_simd.metal` and `ffn_fused.metal`.

## Tests

On the M6 with the new defaults these pass: `test_portable`, `test_gemm_residual`,
`test_staged32`, `test_staged_partial`, `test_window_residual`, `test_glue`,
`test_ffn_fused`, `test_window_block`, `test_qkv_fusion`, `test_epilogue`,
`test_graph`, `test_window_attention`, `test_global_attention` and `test_ffn_batch`.

On the M1, `test_gemm_residual`, `test_staged32`, `test_staged_partial`,
`test_portable`, `test_epilogue`, `test_ffn_fused`, `test_glue`, `test_graph`,
`test_window_residual` and `test_qkv_fusion` pass on both libraries, with and without
`XMX_METAL_WIDE=1`. `test_resident` passes with `XMX_METAL_WIDE=1` on both.

## Not done

- Only an M6 was measured. The M5 is assumed to behave the same because it is in the
  same family and has the same matrix units, but it has not been run.
- The window-gathered A (0x400000) still runs the hand-written loop in the Metal 4
  library, so on an M6 it sums in the loops' order while the other GEMMs use
  matmul2d's. `test_window_residual` and `test_staged_partial` pass on the M6, but
  any new fused pass that must equal a window-gathered GEMM has to sum the same way.
- 16 x 128 on two simdgroups was as good as 16 x 64 on one. It was not built, because
  the 16 x 64 needed only a template instance of the kernel that was already there.

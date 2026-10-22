# Phase 78 — the fused kernels' products on matmul2d, and a race on the M1

**2026-10-22, on an Apple M1 (macOS 27.2) over ssh; no M5 to hand.** Apple's M5 adds
matrix units to every GPU core, and the way a Metal kernel reaches them is Metal 4's
Metal Performance Primitives: `mpp::tensor_ops::matmul2d`. libmetalmx already carried a
Metal 4 library for that (`NR_METAL4`, `nr_shaders4.metallib`, loaded on a device and OS in
`MTLGPUFamilyMetal4`), but only the GEMM kernels' K loop used it. The fused kernels — the
window block, window attention, global attention and the feed-forward's stem, every one on
by default in the frame VBA-M runs — still took their products on hand-written 8x8
`simdgroup_matrix` loops. Now the Metal 4 build takes those on matmul2d too, and the
Metal 4 library is built only where it can run.

## What changed

- **`nr_epilogue.h`**: `nr_view` (a fixed-size tensor over a pointer in any address space)
  and `nr_mm<M, K, N, TRANS>` (one simdgroup's M x K by K x N product, every operand where
  it already is), under `NR_METAL4`.
- **`window_block.metal`**: the three QKV parts, QK^T, PV, the output projection and block
  70's head. QK^T runs both halves of the keys into register tensors before either is
  staged, because the first half's weights overwrite Q, which the second half reads.
- **`window_attention.metal`**: QK^T a half of the keys at a time into the stage, PV
  straight to the float32 output, or through a register tensor for the merged layout (its
  stage is the probabilities' bytes).
- **`global_attention.metal`**: QK^T as a transposed-B product into the stage (a half past
  the block's keys zeroed, as the loop's untouched accumulators were), PV accumulating over
  every 64-key block in a register tensor, sixteen keys a step.
- **`ffn_fused.metal`**: the stem's 16 x 16 x 32 product. The general (non-staged) shapes'
  loops are still hand-written.
- **`CMakeLists.txt`**: `NR_METAL4` defaults on only for a build that includes arm64
  (`CMAKE_OSX_ARCHITECTURES`, else the target processor), and is turned off for an x86_64
  one even when cached on. No Intel Mac's GPU is in `MTLGPUFamilyMetal4`; an x86_64 build
  carried a library it could never load, and needed Xcode 26 to build it.

The int8 GEMM (`gemm_int8.metal`) was left alone: the frame library never calls it.

## Same bytes

matmul2d sums each output in the order the 8x8 loops do, eight K at a time from zero. A
probe on the M1 ran every product shape the fused kernels use — 8 x 32 x 32, 8 x 32 x 64,
8 x 64 x 32, 8 x 32 x 16, 8 x 32 x 96, on device operands — both ways over 4 to 25 million
outputs each: no output differed. On real kernels, every fused kernel's output from the
Metal 4 library equals the Metal 3.1 library's byte for byte (64 windows of window
attention in both layouts, a 40 x 56 window block float and pooled, 256-row global
attention over four heads, the feed-forward at 128 and 64 hidden), and a 720 x 480 frame
through `nr_frame` gives the same PNG on both libraries. `test_window_attention`,
`test_window_block`, `test_global_attention`, `test_ffn_fused`, `test_ffn_batch`,
`test_glue`, `test_portable`, `test_window_residual`, `test_epilogue` and
`test_qkv_fusion` pass on both.

## A race in window_attention, on the M1 only

Before any of this, `test_window_attention` and `test_window_block` failed on the M1 with
the Metal 3.1 library, and differently on every run: a whole token's 32 values at a time,
sometimes by half their magnitude. Twenty runs of the same input gave twenty different
outputs. The window block itself was stable; its three-pass reference was not, and the
pass that varied was `window_attention`, alone, on fixed inputs in a submission of its own
(one window: six distinct outputs in twenty runs; 64 windows: twenty).

Turning each of the kernel's five `simdgroup_barrier`s into a `threadgroup_barrier` one at a
time found the one: after QK^T's logits are stored into a simdgroup's own stage and before
its lanes read them back. With a threadgroup barrier there every run gives the same bytes,
and the frame costs the same (Metal 3.1: 396-398 ms against 396-399 at 720 x 480). The
same store-then-read through a simdgroup barrier in window_block and global_attention was
stable over twenty runs each, as it is everywhere on the M3, and why this one is not on
Apple7 is not understood; the comment at the barrier says what was measured, not why.
Every fused kernel now gives one output in twenty runs on both libraries.

## Numbers, M1, 720 x 480 (the network extent 832 x 512)

| build | graph time, five runs |
| --- | --- |
| Metal 4 library, before | 193.1-197.2 ms (253.7 cold) |
| Metal 4 library, fused kernels on matmul2d | 194.0-197.3 ms (221.1 cold) |
| Metal 3.1 library | 395.6-399.2 ms |

The M1 has no matrix units for matmul2d to reach, so the fused kernels cost the same either
way there; the Metal 4 library's GEMMs were already the 2x. On the product shapes in
isolation matmul2d ran at 0.9-1.45x the loops' speed on the M1.

## Not done

- **Nothing has run on an M5.** Whether these products reach its matrix units, and at what
  speed, is unmeasured, and so is the claim that matmul2d keeps the loops' summation order
  there: if it does not, the fused kernels and their unfused passes may disagree in the
  last bit on an M5, which the tests above would show.
- The feed-forward's general shapes and the int8 GEMM keep their hand-written loops.
- `test_joint_qkv` needs an MLX checkout and was not run.

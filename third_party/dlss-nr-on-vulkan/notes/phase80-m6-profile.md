# Phase 80 — where an M6 frame goes, and three things that did not help

**2026-10-27, the M6 Mac mini of phase 79, after its routing change.** A per-pass profile
of the frame on the M6. Then three attempts at the two largest costs left outside the
direct GEMMs: the window-gathered QKV GEMMs and the window block. None of them gained
anything, and nothing from them is in the tree. This note records what was tried, so
it is not tried again blind.

## The frame

`src/bench/frame_profile.py --size 512 832` (a 720 x 480 frame's network extent), five
warm runs, Metal 4 library, phase 79's defaults:

| pass | ms | share | passes |
| --- | --- | --- | --- |
| direct (tiled and wide) GEMMs | 9.9 | 36 % | 266 |
| window block | 5.7 | 20 % | 10 |
| staged GEMMs (the window-gathered QKV) | 5.5 | 20 % | 52 |
| window attention | 2.8 | 10 % | 52 |
| fused feed-forward | 2.8 | 10 % | 10 |
| the rest | 1.0 | 4 % | |
| device total | 27.7 | | |

The deep direct GEMMs run at 12.6-15.6 TFLOP/s, about the best any block reached in
phase 79's sweep. Most of the other direct GEMMs are N = 32 shapes (26624 x 32 x 128 and
the like), and those are bound by memory bandwidth at about 130 GB/s.

The window-gathered QKV GEMMs run at 7-10.6 TFLOP/s. They are already on matmul2d: the
staged kernel gathers each K step's rows into threadgroup memory and runs matmul2d on
them. (Phase 79 first said they kept the hand loops; it was corrected.)

On the Metal 3.1 library the same frame is 126.6 ms. Its staged GEMMs alone take 96 ms
there, and the window block 8.7 ms against 5.7.

## Tried on the gathered QKV GEMMs

- **A 64-deep K step** (`gemm_staged_t<64, 64>`) for staged GEMMs at K a multiple of 64.
  That halves the gathers and barriers a block. The frame was 28.3-28.4 ms against
  28.2-28.3 without it, with the same bytes. The routing was confirmed: every staged
  specialisation was built as the deep kernel.
- **Two or four 32-column blocks a threadgroup.** The A tile is gathered once per K
  step for 2 or 4 neighbouring heads, with one cooperative accumulator each, then the
  QKV epilogue runs on each block in turn through the stage. This cuts the gathering
  2-4x and gives the same bytes. Two blocks measured 27.7-28.2 ms against 28.1, which is
  noise; four measured 28.7-28.9, slower, presumably from fewer threadgroups.

So these GEMMs are limited neither by the gather nor by the K loop's barriers. They are
at 0.7-0.75 of the best direct rate, so even a perfect fix is worth 1-1.5 ms a frame.

Two compiler notes for anyone writing more MPP code:
- An array of cooperative tensors (`Acc accs[N]`) crashed the Metal compiler (exit 139).
  Separate variables work.
- `matmul2d::run` wants its operands as lvalues, so a `tensor.slice(...)` has to be
  named first.

## Tried on the window block

`window_block` was timed alone over a 512 x 832 image (6656 windows, the frame's
largest call, 1.38-1.43 ms). The variants were built with one part of the kernel's
Metal 4 path switched off at a time, and loaded with `XMX_METALLIB`. Their outputs are
wrong; only the timings mean anything. Minimum of 30 runs; the runs are noisy:

| part switched off | ms saved |
| --- | --- |
| the Q, K, V publishes (E4M3 and stores, K transposed) | 0.24 |
| the QKV products | 0.19 |
| the row sums and the probabilities' scaling | 0.17 |
| QK^T | 0.16 |
| PV | 0.15 |
| the attention output's publish and the output projection | 0.13 |
| the softmax weights (bias and transform) | 0.11 |
| the Q and K normalisation | 0.08 |
| the gather, the pool | 0.05 each |
| everything above | 1.03 (0.35 ms left) |

The four products are about 0.6 ms of the 1.38. The rest is spread over about ten
short phases, each ended by a barrier, and they add up. There is no single expensive
part.

A variant with the simdgroups' areas overlapped (24.25 KB of threadgroup memory down to
17.25 KB, again timing only) measured the same, 1.30 against 1.31 ms. So the footprint
is not what limits how many windows a core holds.

What might still pay, unmeasured:
- Doing the publishes on cooperative tensors' elements in registers, rather than
  through the float stage and a barrier.
- Fewer, larger products: 16 rows a simdgroup, or the window's QKV as one product over
  all eight simdgroups. On the M6 and the M1 every matmul2d block gives the same bytes,
  so this is bit-safe.

Either is a rewrite of a 540-line kernel that must stay byte-identical to its three
unfused passes, for perhaps 10-20 % of the kernel: 0.6-1.1 ms of a 28 ms frame.

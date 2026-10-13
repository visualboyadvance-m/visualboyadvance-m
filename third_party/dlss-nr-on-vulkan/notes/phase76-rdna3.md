# Phase 76 — AMD RDNA3 under RADV: the 16x16x16 matrix path, and three RADV rounding traps

2026-10-12. Radeon 780M (Phoenix, RDNA3 iGPU), Mesa 26.2.4 RADV, Fedora, through VBA-M's
DLSS NR layer and this tree's tests. The first AMD GPU on the Vulkan matrix path.

## What the device has

`vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR` on RADV lists 14 configurations, every one
**M=16 N=16 K=16**, subgroup scope: `fp16 x fp16 -> fp16`, `fp16 x fp16 -> fp32`, and the
int8/uint8 pairs into int32. No 8x16 shape at all. Subgroup 32 or 64 (size control present),
`VK_KHR_workgroup_memory_explicit_layout` present with all four features.

## The bug that started it: rainbow bands

libxmx enabled the matrix path wherever `VK_KHR_cooperative_matrix` existed and never asked
which shapes the device runs. Every matrix kernel is fixed to Xe's 8x16x16. RADV compiled and
ran them; a shape the device does not list is undefined, and here the head came out at
mean 6.5e33, sd 1.9e36, different on every replay — each channel then clips to 0 or 1, which
in VBA-M was bands of saturated colour. `test_dlssnr`: 14 of 49 failed.

`coopmat_shapes()` now reads the list, and `choose_matrix_path()` takes the 8x16 kernels where
their shape is listed, the `_m16` builds where 16x16x16 fp16 -> fp32 is, the portable path
otherwise (Arc A-series' 8x8x16 lands there now, rather than on kernels it cannot run).
`xmx_path()` says which and why.

## The `_m16` builds

libxmx swaps names in `shader_code()` when `g.m16`, so no caller (C, Python, VBA-M) changes:

| caller hands | runs | how |
|---|---|---|
| `gemm_staged*.spv` | `gemm_staged*_m16.spv` | `-DTM16`; a 64-row block keeps four subgroups along M (one 16x32 slice each); a 32-row block splits them 2x2 (16x16 each) |
| `gemm_tiled.spv` | `gemm_tiled_m16.spv` | `-DTM16 -DRM=1 -DRN=2`: the same 16x32 block as one tile row |
| `gemm_resident.spv` | `gemm_resident_m16.spv` | `-DTM16 -DHALF_TILE`: the 8-row block on a half-used 16-row tile; A straight from memory where 16 rows are in the matrix, else through a zero-padded shared copy; every output through the stage |
| `ffn_fused.spv` | `ffn_fused_m16.spv` | a subgroup's 16 rows one tile instead of two |
| `window_attention.spv` | `window_attention_m16.spv` | 4 subgroups x 16 queries, 2 KB scratch each, same 16 KB, 128 lanes |
| `global_attention.spv` | `global_attention_m16.spv` | the same arrangement |
| `window_block.spv` | `window_block_m16.spv` | its own file: a subgroup owns two window rows, so every lane mapping (gather, QKV epilogue, residual, head) changes; the token-major scratch keeps every other address, the pool's included |
| `gemm_coopmat.spv`, `gemm_batched.spv` | their `gemm_portable_desc` twins | descriptor-bound benchmark kernels only |

Every block size and dispatch is the 8-row build's. The 8-row SPIR-V of the GEMMs and of
the three fused kernels converted by macros is byte-identical to before (checked against a
build of the stashed tree); the changes below that are not specific to `TM16` move it.

A row's result on RDNA3 does not depend on the other rows of its tile (checked: zero, random
and 40-sigma bottom rows leave the top eight bit-identical), which is what makes the half
tile exact.

## RDNA3's matrix units are not IEEE-exact

One 16x16x16 tile, zero accumulator, operands on a 1/256 grid whose every sum is exact in
float32: 100 of 256 results a float32 ulp or two off. An identity times -0.5 gives
-0.50000006; small integers give -9.999998 for -10. Same-sign sums of equal terms come out
exact. Whether it is the hardware or RADV's lowering is not settled. Consequence: the
checks that demand exact sums fail on the matrix path by design —
`test_gemm_contract`'s dyadic epilogue cases (8), `test_portable` on the default path (5),
`test_resident`'s context / merged publish / attention output. All three pass with
`XMX_PORTABLE=1`. Fused-against-unfused checks are unaffected: the same tile operations give
the same bits.

## Three RADV rounding traps, all fixed, all affecting the portable path too

1. **`half_round` was a no-op.** `src/bench/half_probe.py`: RADV's packHalf2x16 round trip
   truncates (183147 of 360704 values below numpy, AMD's own drivers' count) and the cast is
   folded to x (360428 of 360704 unrounded); the bits are exact. libxmx chose the round trip
   on Mesa, so on RADV no vendor rounding point ever happened. `detect_driver_modes()` now
   takes the bits on `VK_DRIVER_ID_MESA_RADV`; `xmx_half_bits()` reports it and the daemon's
   probe checks the bit-twiddled spelling then.
2. **The softmax exponent trick packed by truncation.** `(packHalf2x16(affine) << 5) +
   0x7ff88000u` assumes the pack rounds to nearest even. `pack_half_rte()` (publish.glsl):
   with HALF_ROUND == 2 the values are rounded by `half_round_bits` first and the
   instruction packs them — a half value packs the same under any rounding, and the
   instruction still does the packing (the B580 lesson stands). The ViT's `vit_weight`
   already packs a `half_round`ed value and needed nothing.
3. **An fma fused into its conversion to half.** Where the only use of a float32 fma is its
   conversion to half — specialised pipelines, a narrow store with the identity publish —
   RADV emits one mixed-precision fma rounded straight to half; `precise` does not stop it.
   On a value that lands on a half tie after the float32 rounding, the answer is a half step
   off (0x3ef7f000 and 0x3f2a5000 were the cases: test_window_block 1 in 12288, on the fused
   kernel; test_gemm_residual 1 in 4128, on the *reference* residual pass). `to_half()` /
   `to_half4()` (publish.glsl) convert through `half_round_bits` when HALF_ROUND == 2, and
   the narrow stores that can receive an fma use them: the GEMMs' residual paths, the fused
   feed-forward, the window blocks, `resident.comp`'s `store()`. The plain conversion
   elsewhere.

And `add_gemm_residual` and `resident.comp`'s residual are written as the `precise fma()`
the fused kernels already wrote, rather than leaving `branch + skip * cos` to the compiler's
contraction (an uncontracted window block fails 547 of 2048 on RADV, so RADV contracts it;
Mesa ANV and Intel's Windows compiler contracted it too, so this is the same there).

## Measured

Frames (`nr_frame_rates`, daemon's path, 6 frames after 2):

| case | matrix `_m16` | portable (rounding fixed) | portable before the fixes |
|---|---:|---:|---:|
| 480x320 @1.0 | 47.4 ms | 149.7 | 113.7 |
| 640x360 @0.5 | 34.5 | 109.9 | 85.8 |
| 1280x720 @0.55 | 87.6 | 266.9 | 202.4 |
| 1920x1080 @0.55 | 185.5 | 565.1 | — |

The portable path got slower because its rounding now happens (integer arithmetic). Device
time at 640x384 (`frame_profile.py`): 151 ms portable -> 65 ms; staged GEMM 37 ms of it,
window block 11.5, fused FFN 10, window attention 5. The staged GEMM's tiling for 16-row
tiles is the next lever; untried.

Picture, two GBA frames (Mario 3, Advance Wars title) at 4x nearest, `nr_frame`, against the
NumPy CPU reference (`src/ref/nr_frame.py`): matrix 0.92 / 1.10 levels of 255 mean,
portable 0.80 / 1.35 — the pass moves the frames 7-8 levels. Matrix and portable differ from
each other by 0.78 / 2.00, the two deviations not in the same direction.

Tests on the matrix path: every fused kernel bit-identical to its passes (window attention
48, FFN 168, window block 240+32+64, global attention 32, glue, QKV, residuals 120 and 192,
staged 32/partial/packed); `test_nr_frame_c`'s head bit-identical to the Python resident
path at three extents; the frame-level tests with MLX-DLSS (compact head, frame execution,
input FP16, joint QKV, scratch arena) green; `test_dlssnr` 49/49. Failing by design: the
exact-sum checks above. Skipped honestly: the int8 kernel tests (`gemm_runner` exits 77
where the 8x16x32 integer shape is missing). Unrelated and known: `test_nr_frame_c`'s three
`compose_encode, history` checks fail on this x86-64 CPU on every path (HANDOFF, 2026-10-06).

## The shared device was never pinned, and on RADV that is every frame wrong

In VBA-M the Vulkan panel lends its device (`xmx_adopt`), and libxmx pinned the subgroup
width to 32 only on a device of its own: the panel never enabled Vulkan 1.3's
`subgroupSizeControl`, so no pipeline could ask. RADV computes in 64 lanes by default, and
every kernel here is written for 32 — with the pin removed the head is inf and NaN across the
frame (the owner: "rainbow banding across the whole frame", worse than the 8x16 garbage). Now:

- `XMX_ADOPT_SUBGROUP_SIZE` (4) and `XMX_ADOPT_FULL_SUBGROUPS` (8) say the host enabled them,
  and libxmx pins the lent device as its own;
- a lent device that could pin, whose width varies, and was not set up to, is **refused** at
  the open, so the host falls back to libxmx's own device, which pins;
- the wx and Qt Vulkan panels enable both features where the device has them, and
  `dlssnr::VulkanShare` carries them.

Checked with a harness that makes the panel's device and lends it (`nr_frame_adopt_vulkan`):
enabled, the shared device runs the `_m16` path with a finite head; not enabled, the share is
refused with the message and libxmx's own device gives the same head to four digits.

## Load on a laptop

The owner's report after the fix: 50 fps once the laptop warms up, 45 on still scenes, under
40 at worst. The pipeline runs a pass as soon as the one before lets it, so the GPU is busy
whatever the picture does: 19 passes a second at 480x320 (27 at 240x160, padded to 320), the
GPU 87-91% busy, the APU's package (amdgpu's PPT) at 42-50 W against 14 W idle, Tctl from 47
to 77 C in half a minute.

- libxmx's own device took the first compute family, the graphics one on RADV, and a pass
  held a renderer's frame for its whole length: a 60 Hz graphics submit waited 45 ms twenty
  times a second. It now takes a compute-only family where there is one (graphics waits
  0.4 ms, passes 19/s as before). VBA-M's Vulkan panel already lent family 1.
- On a still picture a pass is a fixed point after about five: the same frame and settings
  give the same picture and the same float history. VBA-M's filter now queues no pass
  once one comes back identical to the one before, until the frame or a setting changes;
  the GPU drops to idle and the picture is byte-identical to before (four frames, two games).
- In motion nothing is skipped, so VBA-M's filter has an adaptive frame rate (the
  `Display/DlssNrAdaptiveFrameRate` option, on by default): it counts the frames the host
  draws, holds them against 60 x the throttle, and spaces the passes out while they fall
  short, remembering the spacing that fell short so it does not walk back over it. A
  real-time harness (the core, xbrz2x, the filter, and 7 ms of fixed CPU work standing in
  for the GUI) over five minutes on the 780M, second half: off 54.0 fps, 84 of 150 seconds
  under 93%; on 60.0 fps, 1 of 150, at 13 passes a second. At 150% it held 90 at 8.
- Where even a pass a second leaves the rate short, it tests whether the spacing helps at
  all: two seconds without it, against the two before, cut to one where the first is
  clearly worse. Not 2% of the target better with it, it stands down (the passes run
  freely) for a minute, or until the rate falls under what the test saw; better, it keeps
  the spacing and tests again after 2, 4, 8, then every 16 minutes. It mostly does help: xBRZ 6x with the layer after the filter, 7 ms of
  stand-in GUI work, hot: 38 fps unspaced, 57 at a pass a second -- the filter's own work
  on the CPU each frame (lining the pass up with the live 1440x960 picture) goes from 6 to
  13 ms when passes land often. The layer before the filter does the same at 240x160:
  0.5 ms a frame, 60 fps at 20 passes a second.

## Not done

- Confirmed in VBA-M's GUI by the owner (wx, Vulkan renderer, 2026-10-12): the bands are
  gone. The Qt panel's change is built, not run.
- The Intel 8-row kernels whose sources changed outside `TM16` (`to_half4`, `pack_half_rte`,
  the explicit residual fma) were not run on Xe2: they are the same arithmetic there by the
  constant (HALF_ROUND is not 2 on ANV) and by the contraction both Intel compilers made.
- Is RDNA3's inexact sum the hardware or RADV? A reproducer for Mesa is three files in a
  scratch directory (a 16x16 copy of `gemm_coopmat.comp` through `gemm_runner`); not filed.

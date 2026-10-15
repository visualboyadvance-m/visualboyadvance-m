# Phase 77 — the vector-matrix path on Direct3D 12: `gemm_coopvec`

**2026-10-15, written and cross-built on the owner's Apple M3, not yet run on any Direct3D 12
device.** `phase75` gave libd3dmx no matrix path: HLSL, it said, ships no matrix-matrix
operation, and Shader Model 6.9's `linalg` is matrix-vector. This phase takes the matrix-vector
operation at its word. A GEMM is M rows of (vector times matrix), and that is exactly what
Direct3D's *cooperative vector* — a per-thread vector multiplied by a thread-scope matrix,
loaded straight from a buffer — computes on a device's matrix units. `gemm_coopvec.hlsl` is
the resident GEMM written that way, libd3dmx routes to it wherever the device and the GEMM's
shape allow, and the multiply-add kernels remain for everything else. `xmx_coopvec()` says
whether the path runs; `xmx_path()` says why or why not.

## What the API is, as of 2026-10

Three things settled before a line was written, each from the sources on this machine:

- **Shader Model 6.9's cooperative vectors are gone from the compiler.** They were an
  experimental feature of dxc 1.8.2505 (`dx/coopvec.h`, `__builtin_MatVecMul`); commit
  `67a8e24c8` "Remove experimental Cooperative Vector feature" took them out of `main`, and the
  dxc on this Mac (1.10, built from `084c3a4d`, 2026-09-21) has only the successor.
- **The successor is Shader Model 6.10's linear algebra, `dx/linalg.h`** — one header, three
  scopes: `Thread` (a matrix times a vector, "the Cooperative Vector Replacement API" in the
  header's own words), `Wave` and `ThreadGroup` (matrix times matrix, the cooperative-matrix
  shape of thing). The thread-scope multiply, `Multiply<float>(Matrix<F16, M, K, A, Thread>,
  vector<half, K>)`, lowers to `dx.op.linAlgMatVecMul` (opcode 2147483673), which the DXIL
  tables say needs no wave-uniform inputs. Its contract (hlsl-specs proposal 0035): K in
  [4, 128], a vector of at most 1024 elements, the matrix loaded from a **ByteAddressBuffer**
  (never an RW one) at a **128-byte aligned** offset with a **16-byte aligned** stride, in
  row-major or column-major layout with no conversion, or an "optimal" layout the runtime
  converts into. The order and width of the accumulation inside one product are the device's.
- **The runtime side is the preview Agility SDK, 1.721-preview**, and no released header has
  it: `D3D12_FEATURE_LINEAR_ALGEBRA_SUPPORT` (77) answers a tier, `D3D12_FEATURE_LINEAR_ALGEBRA_
  LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT` (78) answers per operation, and for
  `THREAD_VECTOR_MATRIX_MULTIPLY` the vector, matrix, bias and result types with support
  flags. Note the data-type enumeration there differs from DXIL's: FLOAT16 is 7 and FLOAT32 is
  8 in the D3D12 enum, 8 and 9 in DXIL. Tier 1 requires fp16 x fp16 -> fp16 for the vector-
  matrix multiply and lets fp32 results be optional, so libd3dmx asks for fp32 first and fp16
  after. `D3D_SHADER_MODEL_6_10` is `0x6a`. The retail DirectX-Headers (`main`) skip the
  enumeration values 55, 58-60, 62-63 and 66 up: that is where the preview lives.
  The runtime Windows ships does not know Shader Model 6.10; the SDK's `D3D12Core.dll` has to
  be loaded, and a library — not the executable, which exports `D3D12SDKVersion` — does that
  through `ID3D12SDKConfiguration1::CreateDeviceFactory`. The preview shader model is behind
  `D3D12ExperimentalShaderModels`, which the factory enables for its own devices and which
  needs developer mode.

## The kernel: `src/gpu/d3d12/gemm_coopvec.hlsl`

A group is 32 threads, 32 consecutive rows of C (`SV_GroupID.y`, in pieces of 65535 through
`spare`, as every GEMM here), on one column slice (`SV_GroupID.x`), for one batch item (`.z`).
A thread:

1. takes its row of A as a long vector, `bufA.Load<vector<half, KC>>`, KC = 128 while 128 or
   more of K remain, then 64, 32, 16 for the rest (every K in the graph is a multiple of 16);
2. loads the NC x KC slice of B it faces as a thread-scope matrix, from the weight buffer as
   it is: a row-major (K, N) B read **column-major** with the row stride as its column stride,
   a transposed (N, K) B read **row-major**. No copy, no conversion, no staging;
3. asks for the product, `Multiply<float>(matrix, vector)` — `Multiply<half>` under
   `-DRESULT16` — and sums it into a `vector<float, NC>` accumulator;
4. finishes with the epilogues every GEMM here has, on its own NC outputs: the residual
   `+ skip * cosine` (float32 or half skip, 0x80000 through the window layout back into the
   unpadded image via `residual_output_index`), the publish, the half output, the compact head
   (0x10000, N = 16), the published half copy (0x200000), and the QKV epilogue (0x100000) a
   head at a time — a 64-wide slice may hold a head of Q and a head of K, so each 32-wide half
   decides its part, normalises through the same `cosine_reciprocal` / `vit_reciprocal` and
   stores published to its own target. The pooled window residual (0x800000) is not here: it
   is the 16x32 block's own and stays on the tiled portable kernel.

NC is 64 while 64 or more columns remain, then 32, then 16 — so every slice but the last two
starts on a 64-column boundary, 128 bytes of half, which is what the thread-scope matrix's
alignment demands of the plain form. The kernel and the runtime walk the same two rules
(`main` in the HLSL, `coopvec_slices` / `coopvec_fits` in libd3dmx).

**Four builds.** Plain, and `-DGATHER` for a window-gathered A (0x400000: the row made from
the image, zero outside it, rounded to half or published to E4M3 as it is read, four halves
or four floats at a time); each with `-DRESULT16`. The gather is its own build because it
unrolls a conversion per element for every (NC, KC) pair: plain 220 KB of DXIL, 7 354
operations, 32 `linAlgMatVecMul` sites and 44 matrix loads (12 shapes, two layouts each);
gather 495 KB, 14 988 operations. Compiled with `-T cs_6_10 -enable-16bit-types -HV 2021`
and the project's other flags, no warnings, by the dxc above; the DXIL carries the
`dx.types.LinAlgMatrixC8M{16,32,64}N{16,32,64,128}U0S0` types and the alignment immediate
128 on every matrix load.

## The runtime: libd3dmx

- **The device is asked three questions** (`check_device`): the highest shader model, asked
  from 6.10 down because a runtime refuses an enumeration value it has no name for with
  E_INVALIDARG; the linear algebra tier (77); and whether the fp16 x fp16 thread vector-matrix
  multiply has fp32 results, else fp16 (78). `coopvec_support` is 0, 1 or 2, and the support
  flags (EMULATED_INPUTS / EMULATED_OUTPUTS) are kept for the note. The structures are spelled
  in libd3dmx.c under names of their own, with a static assertion on the 56-byte size the
  runtime checks, so neither MinGW's nor the Windows SDK's d3d12.h needs the preview.
- **An Agility SDK is loaded through a device factory** (`agility_factory`): `D3D12GetInterface`
  for `ID3D12SDKConfiguration1`, `CreateDeviceFactory(version, path)`, the factory's
  `EnableExperimentalFeatures(D3D12ExperimentalShaderModels)` when the modules are unsigned or
  the vector-matrix module is compiled in, and the factory's `CreateDevice` on each adapter
  tried. Where to look: `XMX_D3D12_SDK_PATH` (`XMX_D3D12_SDK_VERSION`, else the version is read
  off `D3D12Core.dll`'s `D3D12SDKVersion` export, mapped without running it), then `D3D12\`
  under the executable (passed as the documented relative `.\D3D12\`), then `D3D12\` under
  libd3dmx itself. `XMX_D3D12_SDK=0` keeps the system runtime. Whatever fails is one line on
  stderr and the system runtime serves as before; an adopted device (VBA-M's) is the host's and
  never goes through this — a host that wants the path must be on the SDK itself.
- **The pipelines** are built in `xmx_res_init`, after the portable ones: `gemm_coopvec` and
  `gemm_coopvec_gather` (or their `_f16` builds), unless `XMX_PORTABLE=1` or
  `XMX_D3D12_COOPVEC=0`. A build that fails — the module not compiled in, the runtime
  refusing the shader model, a driver that reports the feature and then rejects the DXIL — is
  one line on stderr, `xmx_path()` carries the reason, and nothing else changes.
- **B is bound twice.** A thread-scope matrix loads from a `ByteAddressBuffer`, so the root
  signature has a tenth parameter, a root SRV at t0 (50 of the 64 DWORDs), which every
  dispatch sets (the dummy elsewhere); for a vector-matrix pass the second operand goes
  there, is transitioned to `NON_PIXEL_SHADER_RESOURCE`, and the dummy takes its UAV slot.
  The recording's state tracking transitions it back the next time a pass reads it as a UAV
  (the fused feed-forward does), which on a shared weight buffer is a barrier each way, where
  a barrier already stood.
- **Routing** (`record_gemm`, before the wide/tiled/base choice): `coopvec_fits` — N and K in
  whole 16s, no pooled residual, a stride of whole 8 halves, a 128-byte aligned B offset and
  batch stride, a gathered A's offset in whole 8 bytes, and then every slice start, for every
  K chunk of every column slice, on a 128-byte boundary. Weights are one buffer a tensor, so
  the graph's GEMMs fit; a slice of a wider weight (the branched feed-forward's heads, through
  element offsets) fits when the offset is 64 halves. The dispatch is `coopvec_slices(N)` x
  `ceil(M / 32)` x batch, stamped as the **staged family** — the slot the staged kernel has on
  the other runtimes, which no pass takes here — so `frame_profile.py` shows the vector-matrix
  GEMMs as `gemm staged`, and `xmx_coopvec_calls()` counts them.
- **`xmx_portable()` stays 1** on Direct3D 12 with the path on. Callers choose the fused
  kernels' names by it, and the fused kernels here are the multiply-add twins either way; the
  staged-kernel tests (`test_staged32`, `test_staged_partial`, `test_staged_packed`) keep
  skipping by it. `xmx_coopmat()` stays 0: there is no cooperative matrix here. The new
  answers are `xmx_coopvec()` and `xmx_coopvec_calls()`, exported by all three runtimes (0 on
  Vulkan and Metal), `nr_frame_coopvec()` in the C frame API, `NativeFrame.coopvec` in Python.

## The numerics, and what the tests mean on this path

A device sums one product in its own order and, with the fp16 builds or the EMULATED flags, at
its own width. So the vector-matrix GEMM is **not bit-identical** to the multiply-add kernels
— as RADV's cooperative-matrix path is not (HANDOFF, 2026-10-12) — and the fused passes, which
are the multiply-add twins here, are no longer bit-identical to the unfused GEMMs they
replace. The checks that compare a fused pass with its passes byte for byte (`test_glue`,
`test_ffn_fused`, `test_window_block`, `test_global_attention`, `test_window_attention`,
`test_gemm_qkv`, `test_gemm_residual`, and `test_gemm_contract`'s exactness) hold on this path
only under `XMX_PORTABLE=1`, which is what the `d3d12_portable_*` ctests set. The graph's
head moves accordingly: a frame on the path is a different (correct) picture, and the
`nr_frame_reference_d3d12.bin` the C frame test writes is the path's own.

`src/gpu/test_coopvec.py` (ctest `d3d12_test_coopvec`) is the path's own check: in one process
as the runtime routes, in another under `XMX_PORTABLE=1`, thirteen plain shapes (every slice
width and K chunk, a batch, a transposed B, strides and offsets, four the routing must refuse),
every epilogue wide and narrow, the residual with both skips, the window residual, the compact
head, the half copy and the QKV epilogue plain and window-gathered — each within 2e-3 of the
magnitude of numpy's float32 product of the same halves (the multiply-add kernels' own distance
printed beside it, as the floor), the two paths within that of each other, and the routing
checked by `xmx_coopvec_calls()` and the profiler's families. Without the path it exits with
"skipped" and says so.

## What was verified on the M3

- The four builds compile (sizes and counts above); the probe that found the API (`Multiply`
  of a 32x64 matrix and a 64-vector) compiled to `dx.op.linAlgMatVecMul` first.
- libd3dmx.c compiles under `x86_64-pc-mingw32-gcc` 16.2 with `-Wall -Wextra`, no warnings,
  and links as a DLL exporting 87 `xmx_*` entry points, the two new ones among them;
  nr_frame.c, test_nr_frame.c and libxmx.c (against the MinGW Vulkan headers) likewise;
  nr_frame.c and test_nr_frame.c also under the Mac's clang.
- CMake configures on macOS with the new function and macro; the MinGW cross configure with
  `-DNR_BUILD_D3D12=ON -DNR_DLSSNR_D3D12=ON` finds `cs_6_10` in this dxc, turns
  `NR_D3D12_COOPVEC` on, compiles the four modules and embeds them (see the HANDOFF entry for
  what the cross build printed).

## What is not done

- **No device has run it.** The first run is `set NR_GPU_BACKEND=d3d12` and
  `python src\gpu\test_coopvec.py` with a preview Agility SDK's `D3D12Core.dll` in `D3D12\`
  beside `python.exe` or named by `XMX_D3D12_SDK_PATH`, developer mode on, and a driver that
  reports the feature — then `ctest -R d3d12_`. The stderr line `libd3dmx: Agility SDK 721
  from ...` says the SDK loaded; `xmx_path()` says the rest. `XMX_D3D12_STEP=1` and
  `XMX_D3D12_DEBUG=1` are there as before.
- **Speed is unmeasured.** The design puts the whole weight slice through the device's matrix
  path with no staging, and the A row read once per 64 columns; whether that beats the
  multiply-add kernels — and by how much on which vendor — is the first run's to say.
- **The optimal layout is not used.** `ConvertLinearAlgebraMatrix` (a preview command-list
  method) would re-lay the weights for the device once at upload; the row/column-major loads
  are the always-supported form and may be the slower one.
- **The fused passes stay on the multiply-add kernels.** The fused feed-forward is the
  textbook cooperative-vector use (a row through expand, gate, project without leaving the
  thread) and the natural next kernel; the window block and the global attention want the
  wave-scope matrix-matrix multiply, which the same header offers and which would be the true
  cooperative-matrix path on this runtime.
- An adopted device (VBA-M's Direct3D 12 renderer, once it shares) takes the path only if the
  host's runtime is the SDK's; libd3dmx cannot load an SDK into a host's device.

# HANDOFF — read this first

State of the DLSS-NR on Intel Xe2 project as of **2026-09-22**. notes/CLAUDE.md holds the
State of the DLSS-NR on Intel Xe2 project as of **2026-09-24**. notes/CLAUDE.md holds the
original brief; **this file overrides it wherever they disagree**, and after
2026-09-09 they disagree about something foundational.

`notes/INDEX.md` says what each phase note settles — go there when
you need the evidence behind a line in this file, rather than reading them in order.

---

## Latest: five more dlss-nr-on-intel commits, on every runtime (2026-09-24, night)

`3a79933`..`064ffad` of `uzbekunknown/dlss-nr-on-intel` are in: the staged GEMM's partial last
64-row block, the Tekken 7 25 fps record, the temporal gate and the hold floor inside the native
composition, the host passes on every core, and their notes. Carried past libxmx:

- **The partial block on Metal**: `gemm_staged` in `metal/gemm_simd.metal` clamps a partial
  block's A rows to the last real one and `store_staged` skips rows past M; `libmetalmx` routes
  M that is not whole 64-row blocks to it under the same `XMX_STAGED_PARTIAL` (default 1) and
  `xmx_staged_partial()`. **Direct3D 12** has no staged kernel, so `xmx_staged_partial()` there
  accepts the switch and changes nothing. `test_staged_partial.py` skips on a runtime or device
  without a staged kernel (portable Vulkan, MoltenVK, D3D12) rather than failing.
- **The gate table in the C frame library**: `nr_frame.c` builds the 65536-entry table once per
  blend scale (with `expf`, as its per-pixel gate did — still the one place it can differ from
  NumPy by a last bit) and hands it to `nr_compose_temporal`'s new `table, confidence`
  arguments, which also clip the floor to 1 as upstream now does.
- **The threads are not OpenMP here.** Apple's clang has none, MSVC's is 2.0, and `nr_image.c`
  is linked into libdlssnr and so into VBA-M's sandboxed `.app`. `nr_image.c` has its own pool
  (pthreads; Win32 condition variables from Vista on; serial where neither exists): contiguous
  bands of rows as OpenMP's static schedule gives them, passive waits, a job already running
  sends a second caller serial. `NR_HOST_THREADS` (else `OMP_NUM_THREADS`) sets the count.
  `nr_parallel_rows()` is public in `nr_image.h`, and `nr_frame.c` puts its own row loops on it
  too — the detail blur, the masked composition, the noise. ctest runs `test_native_image` at
  1 and 3 threads beside the default; the Makefile does the same.

Verified on the M3 (a scratch copy with MLX-DLSS cloned and the safetensors rebuilt from
`weights/`): `test_staged_partial` 34 cases bit-exact on Metal-simdgroup, unmapped memory too
(skipped on MoltenVK, which is portable); the C frame pair and `test_dlssnr` 36/36 on Vulkan and
Metal, also at `NR_HOST_THREADS=1`; `test_native_image` at 1, 3 and 8 threads; temporal controls,
scratch arena, frame execution, input FP16, compact head and joint QKV on both runtimes. At
1920x1080, 1 thread against 8, same bytes: temporal composition 14.0-14.3 -> 5.3-7.5 ms,
composition 7.0-8.2 -> 2.5-3.4, encode 4.6-6.9 -> 1.5-2.4. The Win32 pool and libd3dmx compile
with MinGW-w64, the pool and nr_frame.c with the NDK for arm64-v8a and armeabi-v7a; none of that
has run. `publish_check` still fails on the committed `weights/*.h`, as before this change.

## The dlss-nr-on-intel fusions on every runtime (2026-09-24)

The 36 commits of `uzbekunknown/dlss-nr-on-intel` from 2026-09-19 to 09-24 are in this tree
(35 rebased; the 36th is a merge with no changes of its own): the batched FFN groups, the
compact input and output, joint QKV, the present fences, the int8 kernel and its contract,
the residual and QKV epilogues, the window partition folded into the QKV loads, one-pass
window attention, the fused feed-forward, the full-resolution glue, the staged GEMM's shared
bytes, and their notes (`notes/improve-*.md`, `phase68`, `phase69`). **Every fusion now runs on
all three runtimes and in the C frame library**, not only on libxmx's matrix path:

- **Vulkan without matrix units** (MoltenVK, phones, `XMX_PORTABLE=1`): `gemm_portable.comp`
  takes every fused flag, and `window_attention_portable.comp` / `ffn_fused_portable.comp` are
  the fused passes summed in the portable GEMM's order. `xmxres.fused_shader` picks the twin.
- **Metal**: `libmetalmx` and `metal/*` carry the same flags and passes on the simdgroup and
  portable paths (`window_attention.metal`, `ffn_fused.metal`, `nr_epilogue.h`).
- **Direct3D 12**: `libd3dmx` and `d3d12/*` carry them on its portable GEMM, with a 128-byte
  push block and six root UAVs. Compiled with dxc and MinGW; **not run**, as before.
- **`nr_frame.c`** records the new graph call for call, all 14 `NR_*` switches included
  (`NR_JOINT_QKV`, `NR_INPUT_FP16`, `NR_COMPACT_HEAD` too), and stays bit-identical to the
  Python head in every combination tried.

Verified on the M3: every fusion test bit-exact on Vulkan-portable, Metal-simdgroup and
Metal-portable, the C frame test pair on Vulkan and Metal, `XMX_STAGING=1` on the frame and
QKV tests; the full ctest suite green but `publish_check` (scratch `.DS_Store` only). The int8
tests skip without cooperative matrix. **No Xe2 run**: the matrix-path GLSL is upstream's.

Three things not to undo. **The staged GEMM now needs `VK_KHR_workgroup_memory_explicit_layout`**
(its tiles and stage alias): libxmx enables it when present, and an adopting host passes flag 2
(`XMX_ADOPT_EXPLICIT_LAYOUT`) — VBA-M's wx and Qt Vulkan panels now do. Without it the staged
kernel is not built, and **`xmx_window_gather()`** is 0 on a matrix device, so the Python and C
record the partition as its own pass. **The glue's merge is `fma(skip, cos, scale)` with a plain
multiply for the scale, never `precise`**: SPIRV-Cross spells a precise multiply
`fma(l, r, 0.0)`, which turns `-0` into `+0` and broke bit-identity on MoltenVK. And **a fused
pass must equal its own backend's reference**, so each portable twin copies the portable GEMM's
lane layout and summation order, not the matrix kernel's.

## Latest: the runtime on Direct3D 12 — libd3dmx (2026-09-22; one run, the graph did not come back)

**There is a third compute runtime, Windows only, and it has run once.** The run opened the
device, built the nine pipelines, uploaded the weights, recorded and captured the graph — and
the graph's list had not signalled its fence after 60 s, with the device *not* removed, so the
old fixed timeout turned what is most likely a slow adapter (16.6 s of setup where the M3 takes
one) into an error that named nothing. The wait now has no timeout by default (one-second slices,
device-removed checked in each, a stderr note from 10 s on; `XMX_D3D12_TIMEOUT=N` for a limit),
`XMX_D3D12_STEP=1` runs the recording pass by pass with a name and a time on stderr, the debug
layer's messages are drained to stderr, and a real bug went with it: `cmd_begin` reset the
recording's resource-state tracking from the transfer list too. A second attempt then failed
at `D3D12CreateDevice (0x887a0004)` on the first-listed adapter, where VBA-M's D3D12 panel opens
a device: adapter selection now *probes* every hardware adapter (device + SM 6.2 + 16-bit ops)
and falls back to WARP, as `panel.cpp` does. `notes/phase75`, "The first run". The next run is the one with `-v`, then `XMX_D3D12_STEP=1 XMX_D3D12_DEBUG=1`.

`src/gpu/libd3dmx.c`
implements every `xmx_*` entry point of `xmx.h` on Direct3D 12 in C (COM through `lpVtbl`,
`d3d12.dll` and `dxgi.dll` loaded by name, nothing linked), and carries the kernels rewritten in
HLSL under `src/gpu/d3d12/` — nine DXIL modules from five sources, compiled by dxc and embedded
through bin2c, serving the fourteen names every caller uses. **`NR_GPU_BACKEND=d3d12`** makes
`nr_build.library("xmx")` and `nr_frame.c` load it; CMake builds it under `NR_BUILD_D3D12` (on by
default for every Windows target but 32-bit x86, when dxc is found) and `NR_DLSSNR_D3D12`, which
follows it, builds libdlssnr from it — a Windows x64 or ARM64 host gets the Direct3D 12 archive
unless it passes `-DNR_DLSSNR_D3D12=OFF` — with `nr_frame_adopt_d3d12` for a host that wants to
share its device (VBA-M's filter skips its Vulkan share when `nr_frame_runtime()` is not
`vulkan`, and says so in `Device()`). Verified on the M3: the nine
modules compile without a warning, the disassembly carries the `precise` marks and the
round-to-even the E4M3 quantiser needs, the DLL cross-compiles with MinGW exporting all 49
entry points, and the CMake cross build makes the DLL, the archive and its test. **No kernel
has been seen to finish on a Direct3D 12 device**; `notes/phase75` says how to make the next run
(`-v` for the adapter's name, `XMX_D3D12_STEP=1 XMX_D3D12_DEBUG=1` for the pass list and the
validation messages, and `XMX_D3D12_WARP=1` for the software adapter).

Five things a next reader must not undo. **There is no matrix path**: HLSL has no shipped
matrix-matrix operation (the SM 6.8 WaveMatrix preview was withdrawn, SM 6.9's `linalg` is
matrix-vector), so every GEMM is the multiply-add kernel and `gemm_resident` / `gemm_tiled` /
`gemm_coopmat` / `gemm_batched` / `gemm_staged` resolve to it (`kernel_alias`). **An operand is a
root UAV plus a byte offset in the push block's address slot**, because HLSL cannot dereference
a pointer; the GLSL's alignment tests on the address hold on the offset since a resource is 64 KB
aligned. **A dispatch is split at 65535 groups per axis** and the kernels add the push block's
`spare` word to their group id — the graph's wide passes exceed the limit by 2x, and Vulkan
never enforced one. **Narrow stores go through `f32tof16`**, so a half buffer holds exactly what
`half_round` gives, independent of a driver's `fptrunc`. And **specialization does not exist
there**: the flags come from the push block and `xmx_specialized_count()` is 0. Also: a dxc
without `dxil.dll` beside it (every Linux and macOS dxc) writes unsigned DXIL, which the runtime
takes only with developer mode on; libd3dmx asks for the experimental shader models when it
finds its modules unsigned and says so in the pipeline error.

## Latest: the runtime on Metal — libmetalmx (2026-09-21, later)

**There is a second compute runtime, Apple only.** `src/gpu/libmetalmx.m` implements every
`xmx_*` entry point of `xmx.h` on Metal directly — no MoltenVK, no SPIRV-Cross — and carries
the fourteen kernels rewritten in the Metal Shading Language (`src/gpu/metal/`), compiled by
Apple's `metal` into one `nr_shaders.metallib` that bin2c puts inside the library.
**`NR_GPU_BACKEND=metal`** makes `nr_build.library("xmx")` and `nr_frame.c` load
`libmetalmx` instead of `libxmx`; nothing else changes, the shader names included
(`gemm_coopmat.spv` selects the kernel `gemm_coopmat`). Built by `make` under Darwin and by
CMake under `NR_BUILD_METAL` (on by default on Apple, a hard error anywhere else); `make
test-metal` and the `metal_*` ctests run the GPU suite on it. The matrix path is
`simdgroup_matrix` with half operands into a float accumulator — the GEMM contract is
**exact** on it, every epilogue and slice at max |d| 0 — and a real 720p frame through the C
library takes **735-746 ms against 938-948 ms through MoltenVK** on the same M3, the two
outputs at correlation 0.99987 (mean 0.28 levels, max 5). `notes/phase74`.

Four things the next reader must not undo, three of them learned the hard way in one session:
the buffer addresses in the push block come from an **`MTLArgumentEncoder`** over one pointer
argument, not from `[MTLBuffer gpuAddress]` — that property is macOS 13, VBA-M's deployment
target is 11.0 and warned on it, and the encoder writes the identical value on every buffer
of both storage modes (400 checked); the Metal compiler runs with **`-fno-fast-math -ffp-contract=off`** (the `phase67` FMA trap,
now at the compiler rather than in MoltenVK's config); consecutive encoders are ordered by an
**`MTLFence`**, because the graph's buffers are untracked and without it the skip copies ran
before the blocks that fed them — every block bit-exact, the whole graph at correlation 0.25;
and the GEMM epilogue goes **through threadgroup memory, never over `thread_elements()`**,
which is a 64-wide vector per thread and cost 27x when looped over. **`libdlssnr` on Apple
is now built from libmetalmx** — no Vulkan symbol in the archive, `-framework Metal` its
public link interface, `test_dlssnr` bit-identical to the Python path — so a host there
must not call `nr_frame_adopt_vulkan` (refused: nothing Vulkan to adopt) and can ask
`nr_frame_runtime()` which runtime it has; `-DNR_BUILD_METAL=OFF` gives the Vulkan archive.
The Vulkan layer is unchanged and serves a MoltenVK game from either runtime through the
daemon, untested here. Also fixed: `test_softmax_pack.py` lacked `import sys` and failed on every
backend.

## Latest: it builds for Android (2026-09-21)

The CMake build goes through the Android NDK — `-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake
-DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=28` standalone, or inside VBA-M's
`tools/android/build-android-qt.sh`, which already passes `ENABLE_VULKAN=ON` and now links
`libdlssnr.a` into the Qt APK. `find_package(Vulkan)` finds the NDK's `libvulkan.so` by itself;
`bin2c` and `slice` are built for the build machine (VBA-M's `host_compile()`, or this tree's
own `NR_HOST_CC` fallback, which refuses the cross toolchain's directory); the weights compile
under the same two-job pool. Built and linked for arm64-v8a and armeabi-v7a; `notes/phase72`. **It has now run on one device**, a
Mali-G57 MC2 phone, after one fix: the row pass fenced its shared-memory staging with a
subgroup-scope barrier that Mali's compiler aborts on (and that would fence only half of a
32-wide workgroup on a 16-lane subgroup); it is a `barrier()` now, all 34 C checks pass on
the phone, at 6.7 s a pass. `notes/phase73`.

Three things a next reader needs. **The layer defaults off on Android** and its X11 define is
now `__linux__ && !__ANDROID__` — Android is `__linux__` with no `X11/Xlib.h`, which was the
one compile failure. **VBA-M's Release build hands `-ffast-math` to every target here**, and
has since the archive was added: the exact flags come later on the line and win — verified on
the NDK's clang 21, no `fmadd` and no reassociation — so it is a warning, now silenced, not a
numerics change. And **a phone takes the portable GEMM path** with no cooperative matrix, on a
subgroup width nobody has measured (Adreno 64, Mali 16, against the 32 of Xe2 and Apple); the
device must be Vulkan 1.3 with `shaderFloat16`, `storageBuffer16BitAccess`,
`bufferDeviceAddress`, the memory model and `scalarBlockLayout`, or `xmx_open` fails and the
filter drops to none. `nr_temp_dir()` now knows `/data/local/tmp`.

## The weights compiled into libnr_frame (2026-09-20, later)

The CMake build now embeds `dlssnr-logical.safetensors` — found in the source root or
`work/mlxw/`, or named with `-DNR_WEIGHTS_FILE=` — into `libnr_frame` through **bin2c**
(`src/tools/bin2c.c`, Rafael Kitover's, unchanged), and `nr_frame_open(NULL)` reads the
safetensors from the compiled-in bytes. The `nr_frame` command without `--weights`,
`test_nr_frame`, and `NativeFrame()` with no argument all default to that. **Same head bytes
as the file** (`bb94971d4937ef64` on a 512x288 random frame, both ways), 34 C checks green,
the library 294 MB.

**The trap is the compiler, not the file.** A byte-array initializer costs clang ~90 bytes of
memory per byte: 1.44 GB for a 16 MB slice, ~25 GB for the whole file on an 8 GB machine. So
`src/tools/slice.c` cuts the file into 8 MB pieces, bin2c writes one header per piece,
each is its own translation unit under a two-job Ninja pool (`NR_EMBED_CHUNK_MB`,
`NR_EMBED_JOBS`), and the whole build is 60 s at 0.89 GB peak. Do not "simplify" it back to
one array. The generated C lives in `build/weights/` and `*.safetensors` is ignored, so
nothing derived from the DLL is committed. The Makefile embeds nothing and its library
refuses a NULL path with a message. The Python graph still reads the file. `notes/phase71`.

**The fifteen SPIR-V modules are compiled in too**, into libxmx and gemm_runner, by the same
bin2c (`NR_EMBED_SHADERS`, on). A shader is now *named*: `xmx_init("gemm_coopmat.spv")` takes
the embedded module, a path still opens a file (the `XMX_*_SPV` overrides), and a path whose
file is missing falls back to the embedded one. `nr_frame.c` and `nr_build.shader_arg()`
hand over bare names when the runtime has them. Verified with every `.spv` moved out of
`build/`: the C test, the Python GEMM test and the frame all run, same bytes. The files are
still written for the benches. The build's tool targets are `bin2c_dlss` / `slice_dlss`
because VBA-M adds this tree as a subdirectory beside its own `bin2c`.

## The CMake build lands in its own directory (2026-09-20)

`cmake -S . -B build && cmake --build build` now puts every library, executable, `.spv` and the
ICD manifest in **`build/`** — flat, the Makefile's names — and leaves `work/` to the inputs:
the weights and the headers clone. What made that possible is **`src/nr_build.py`**, the one
place the Python now asks where a build put things: `NR_BUILD_DIR` in the environment first
(every ctest gets it; `make test` pins `work/`), else the newest of `work/` and `build*/` by
`libxmx`'s mtime. Sixteen files spelled `ROOT / "work"` before; none of the *output* users do
now. The C command finds the weights from `build/` too (`../work/mlxw`). `-DNR_OUTPUT_DIR=work`
gives the old layout. **The one trap: two builds, no `NR_BUILD_DIR`, and the newest wins** —
`python3 src/nr_build.py` prints which one that is. Verified on the M3: a full CMake build into
`build/` with `work/` untouched, and the ctest subset named in the note. `notes/phase70`.

**The embedded weights come from `weights/`, not from bin2c at build time.** The directory holds
the logical safetensors as C — 35 bin2c slices of 8 MB and their table — and `NR_EMBED_WEIGHTS`
(ON) compiles it into `libnr_frame`. `NR_BIN2C_WEIGHTS` (**OFF**) regenerates the directory
first from `dlssnr-logical.safetensors` when the file is found, and says so and skips when it
is not. Regeneration was checked byte-identical to the checked-in slices and table. The `.bin`
and `.c.in` intermediates now go to the build tree; the copies left in `weights/` from the
earlier scheme are dead weight (280 MB) and can go. `notes/phase70`.

## CMake for three platforms, and half precision without `_Float16` (2026-09-19, late)

`CMakeLists.txt` builds what the Makefile builds, into the same `work/` with the same names,
on Linux and macOS as run here and on Windows as written — MSVC or clang-cl with the Vulkan
SDK, libpng from vcpkg, the layer off because it is POSIX. `ctest` runs the suite. **The Windows build
now links** — a MinGW-w64 cross-compile from the M3 (`buildmingw-x64/`, ignored by git along with
every `build*/`) produces every DLL, exe and shader into `work/`; **nobody has run those
binaries yet** (no Wine here, no Windows machine). Three things stood in the way and are fixed:
`-march=native` handed to a cross-compiler (CMake now probes for it), a `dlfcn.h` include and a
second `clock_gettime` in the command, and a Windows `BIND` in the library that never compiled.
`src/ref/nr_portable.h` holds the platform seams — `nr_dl_open`, `nr_dl_sym`, `nr_dl_self_dir`,
`nr_now` — and both C files now go through them instead of carrying their own `#ifdef _WIN32`
copies; its software half conversion for MSVC is **exhaustively bit-exact** against `_Float16`
over all 2^32 floats and all 65 536 halves, NaN payloads aside. One MSVC trap stays: the
`sprintf_s` blocks abort on truncation where `snprintf` clips, and MinGW never takes them.
`notes/phase69`.

## The frame path is a C library, and a real frame rendered on the M3 (2026-09-19, night)

`src/ref/nr_frame.c` is `nr_frame.py` in C — the safetensors reader, the 71-block graph
recorded against libxmx **call for call** as `nr_frame_resident.py` and `nr_resident.py`
record it, the features and the composition on `nr_image.c` — built as
`work/libnr_frame.so`, with `work/nr_frame` — the `nr_frame.py` command itself in C, every
flag, PNG through libpng and no ImageMagick — and `nr_frame_native.py` to
drive the library from NumPy. **The head is bit-identical to
the Python resident path** on the same features, checked on the real weights by
`test_nr_frame_c.py` (20 checks, in `make test`). Where C and NumPy part it is by a last
bit of a transcendental — five noise values in 307 200, the blur kernel's `expf`, the
gate's — and the note says so. Reproduce the picture with the features held fixed, not
with the noise regenerated on each side. `notes/phase68`.

**The weights are on the Mac now** (`work/mlxw/`, extracted by the owner during the
session), so every test that skipped runs there, and a **real 1280x720 frame renders on
Apple silicon: 0.90 s** through the C library, 888 ms of it the graph on the portable GEMM.
The `phase67` estimate held.

**`make test` fails at `publish_check`, and the code is not why.** Two commits made on the
Mac outside the session dropped `/work/` and `/ref/` from `.gitignore` and committed 205
files under `work/` — the MLX-DLSS clone, manifests, six test binaries, a log. No weights,
no DLL. The check reads `git ls-files`, so it now fails on the binaries and will fail on
every build. Restore the ignore and `git rm -r --cached work`, or exempt the build
directory; the owner decides.

## It builds and passes on an Apple M3 through MoltenVK (2026-09-19, evening)

MoltenVK has **no `VK_KHR_cooperative_matrix`**, and SPIRV-Cross cannot translate the matrix
opcodes at all, so every GEMM here failed to load on the owner's Mac. Now the runtime asks
the device: with the extension it runs the kernels it always ran; without it every GEMM
goes to `src/gpu/gemm_portable.comp` — the same push constants, flags, strides, batch,
epilogues and store, computed with plain FP32 multiply-adds — behind the same dispatches.
`XMX_PORTABLE=1` forces that path on Xe2, and `make test` runs `test_portable.py` both ways.
The staged 64x32 kernel has no portable twin and is never selected without matrix units.

On the M3: **`make test` green, 117 checks**, five skips for the absent weights; the
portable kernel does **480-570 GFLOP/s** against the matrix kernel's 1348-3828 on Xe2. **No
frame was rendered** — no DLL on that machine — so the graph on Apple silicon is a claim
nobody has made yet. Estimate from the kernel rate: about a second a 720p frame.

**Two things a next reader must not undo.** `xmx.py` sets `MVK_CONFIG_FAST_MATH_ENABLED=0`
before the library loads: Metal's fast math contracts the gate's multiply and add into an
FMA and skips the half rounding between them, which moved the gate epilogue 3e-03 and every
E4M3 publish a quantum, and a frame rendered that way would look right and match nothing.
And on macOS `libxmx.dylib` — every library there carries `.dylib`, the Makefile, the loaders and the layer manifest agreeing — links **MoltenVK directly** while the layer and its tests link the
**loader** — the two are both called libvulkan on that machine and only one knows what a
layer is. `notes/phase67`.
## Latest: over a third of the frame was passes that need not exist (2026-09-23)
## Latest: 46 % of the frame was passes, shared memory and pads (2026-09-24)
## Latest: 48 % of the frame was passes, shared memory and pads (2026-09-24)
## The host passes on every core, and a lead in the CPU's idle state (2026-09-24, evening)

**The passes around the network, at the output's resolution, are cheaper.** The temporal gate
runs natively after all — its logit is half, so a 65536-entry table of NumPy's own sigmoid is
exact (`phase57`) — and every native pass is split by rows across the eight cores (upstream
with OpenMP; this tree with `nr_image.c`'s own pool, see the top of this file), which cannot
change a byte (tested at 1, 3 and 8 threads). The log's change figure is taken on
every fourth row: over the whole frame it was 5 ms of a 1080p frame. On the daemon's own path,
answers byte-identical: **1280x720 at 0.35, 70-73 -> 60-61 ms with the gate and -> 53 with the
cores; 1920x1080 at 0.3, 106-122 -> 77-82 with the cores**; 512x288 and 640x360 unchanged,
because there the graph is 33 ms of 36-39. README table
re-measured: 1024x768 at 0.55 84 -> 75 ms, 1920x1080 at 0.55 196 -> 171.

**A lead that needs root.** The 320x320 graph runs **32.1-32.7 ms with every core idle and
27.3 with any process spinning on a P-core** — a bare `pause` loop does it — and 28.5-29.5 on
an LP E-core, at the same 1950 MHz GPU clock throughout. So it is the package's idle state, not
GPU clocks and not work the core does. Polling the fence from the waiting thread gets only
0.5-1 ms of it, and spinning a core for the length of every graph is not a trade for a laptop,
so nothing is kept. The next test needs root: hold a PM QoS latency limit open while the graph
runs, and read the uncore frequency, which is 0400 here —

```
sudo python3 -c "import os,struct,time; f=os.open('/dev/cpu_dma_latency',os.O_WRONLY); os.write(f,struct.pack('i',50)); time.sleep(600)"
sudo cat /sys/devices/system/cpu/intel_uncore_frequency/package_00_die_00/current_freq_khz
```

If a 50 us limit is worth the 15 % without a spinning core, the daemon can hold one when it
is allowed to. Tried and not kept, in `improve-fusions.md`: weights stored as their E4M3 bytes
(exact, but a proxy put the prize at 0.14 ms a frame) and, again, register prefetch.

## The staged GEMM was on half its threads (2026-09-24, later)

The owner asked why window attention has exactly 2 KB of shared memory and what 1 KB or
512 B would do. Two separate answers, and only the second cost anything here.

**A driver quirk.** Mesa sizes a core's shared-memory partition as *workgroups its threads
hold* x **the declared bytes**, but gives each workgroup its declaration **rounded up** — 1 KB
at least, then powers of two to 16 KB. So declaring less can be slower: 256 B puts a 32-lane
kernel on a quarter of the threads, 512 B on half, 1.25-1.5 KB is 29 % slower than 2 KB.
Verified in the source (26.2.2, `genX_shader.c:1183`; unchanged in 26.2.3 and `main`), in
the driver's own decoded dispatches (`INTEL_DEBUG=bat`: the preferred partition is the only
field that differs between a 256 B and a 1 KB pipeline) and at fourteen sizes on the
hardware. **It costs this frame nothing measurable** — the base GEMM at 512 B, the one kernel
it touches, is no faster padded to 1 KB.

**The cap.** 128 KB between a core's workgroups, on any driver. `gemm_staged.comp` is 128
lanes and declared 15.5 KB: eight workgroups a core, half the threads. **Declare exactly an
allocation size, and keep (workgroups a core holds) x size <= 128 KB.**
`notes/improve-shared-memory.md`. Its operand tiles and its stage
are never live at once, so they now alias as two `shared` blocks
(`VK_KHR_workgroup_memory_explicit_layout`, enabled in libxmx): 8 KB, sixteen workgroups.
**Staged GEMM 90.5 -> 70.9 ms at 720p, device total 219 -> 198**, bit-identical, `make test`
green in both memory modes. The one new barrier is load-bearing — without it all three head
hashes change.

Curve **9 ms + 205 ms per megapixel**. Live, re-measured with it and with scale 0.5's area
mean out of NumPy's multi-axis reduction (16.5 -> 1.9 ms at 1024x768): 512x288 at 0.35 is
**42.7 ms**, 1920x1080 at 0.55 **205.5**. At 512x288 every scale up to 0.62 runs the same
320x320 network, and 0.62 measured 42-43 ms against 41-42 at 0.35 — three times the real
pixels for a millisecond.

Measured and not kept: the softmax pipeline at 1 KB (0.4 ms), the base GEMM padded to 1 KB
(nothing), the fused FFN aliased to 1 KB to win the L1 back (1 %, noise). The L1 is real —
it and shared memory are one array, and at 2 KB x 64 the partition is all of it — but none
of these kernels lives on it.

**And the staged GEMM's loader was waiting on its own loads.** At the live extent the deep
GEMMs are neither short of blocks (a 64x16 build doubled them: no change) nor of K steps
(BK = 64: slower): each load sat in its own branch and was waited for before the next. Issuing
the step's loads together takes the bottleneck's 64x1024x4096 from 0.27 to 0.22 ms and a frame
1 ms faster at 320x320, 3 ms at 720p, bit-identical. `notes/improve-fusions.md`.

**And the staged kernel now takes a partial last block**, so the deeper levels' GEMMs over 144
or 400 rows leave the tiled kernel, which ran them at half the speed. Bit-identical
(`XMX_STAGED_PARTIAL=0` to compare), and it pays where a level is not whole 64-row blocks:
**live 512x288 at 0.35 42.7 -> 36.7 ms (27 fps)**, the 320x320 graph 36.5 -> 32.7, 1920x1088
444.6 -> 422.6, and nothing at 1280x768, whose levels all are. Curve **9.4 ms + 196 ms per
megapixel**; the README table is re-measured with it.

**In a game it is 25 fps.** Tekken 7, live, every present through the network, the owner
playing: **25 fps at 640x360** at scale 0.35, 0.5 and 0.6 alike (a 320x320 or 384x320 network,
40 ms a frame, 30-33 ms of it the graph), and 800x450 at 0.35 the same — 10.5 fps on
2026-09-16. `notes/phase59`.

**The fix is built and tested, not filed.** Mesa 26.2.3 rebuilt with the one line
(`work/mesa-26.2.3/`, loaded through `VK_DRIVER_FILES`, system driver untouched): every size
up to 2 KB at the full rate, this project unchanged and green, and one cost measured — a
256 B pointer chase loses the L1 the small partition had left it, 2.33 -> 3.62 ms. The
issue draft and the standalone reproducer (`src/probe/slm_occupancy.*`) are ready; filing
needs the owner's account.

## 48 % of the frame was passes, shared memory and pads (2026-09-24)

A second day of the same kind of work, driven by a new profile per call site —
`python3 src/bench/frame_profile.py --calls N`, each pass labelled by the entry point that
recorded it and its shape. Every change bit-identical, checked on kernel tests, three whole
frames (one a real game frame) and both memory modes:

- **window attention in exactly 2 KB of shared memory**: 56 -> 46 ms at 1280x768. Padded to
  8 KB the same shader ran 76 % slower — occupancy is its bound;
- **block 70 stores half for the head directly**, one `to_half` fewer;
- **the full-resolution glue** (`NR_FUSE_GLUE`): block 70's input in one pass instead of
  four, the stem's GEMM storing its own half copy; 8 ms at 720p. The residual pass compiles
  to FMA — measured on crafted inputs — so the merged pass writes `fma()` explicitly;
- **the narrow blocks' whole feed-forward in one kernel** (`NR_FUSE_FFN`, `ffn_fused.comp`):
  the 128-wide hidden layer never leaves the chip; 33.6 -> 21.7 ms of GPU time at 720p;
- **the bottleneck padded to 64-row blocks** when it costs under an eighth more rows, putting
  its K=4096 GEMMs on the staged kernel: 240.6 -> 235.4 ms at 720p;
- the branched blocks' published FFN output stored as half (1.3 ms);
- **the window partition folded into the QKV projection** (`NR_FUSE_PARTITION`): the staged
  GEMM's A loader gathers the window rows from the image itself, zero outside it, rounding to
  half on the way in — 62 passes fewer, 8.7 ms at 720p;
- **still frames composed natively**: `nr_frame.compose` only reached the C `nr_compose`
  above intensity 1 or with history, so photo mode and every cut paid 3.9 ms of NumPy at
  512x288 for the same bytes.

All eight switches off against on, paired: **1280x720 445 -> 231 ms, 1920x1080 968 -> 507,
320x320 62 -> 41.** Graph curve **10 ms + 230 ms per megapixel**. Live: 512x288 at 0.35 is
**45.1 ms (22.2 fps)**, and every live size up to 640x360 runs the network at 320x320, where
the graph is ~39 ms of the round trip.

Measured and **not** kept, so nobody tries them again:

- 16-column chunks in the fused FFN: no spills at all, and 9 % slower than 32 columns with
  some. The spill count is a symptom to read, not a target;
- the same kernel for the branched blocks (`NR_FUSE_BRANCHED_FFN`, off): −2.7 ms at 720p,
  +2.6 at 384x384. The deeper levels are arithmetic, where the staged GEMM does better;
- register-prefetch pipelining in the staged GEMM: 30 % slower (84 -> 111 ms of staged GEMM
  at 720p), as phases 21 and 26 found for the K loop before;
- the tiled or base kernel for the small-M, deep-K GEMMs of the deep levels at 320x320: both
  slower than staged (43 -> 53 ms for the frame).

What is left at the live extent (320x320): GEMMs of the deep levels with M of 64-576 and K up
to 4096, latency-bound on 32-200 workgroups; split-K would parallelise them and is ruled out
because it changes the order of summation.

## Over a third of the frame was passes that need not exist (2026-09-23)

**"Performance inside the graph is finished" was wrong.** Every pass that only moves data is
at the memory ceiling — `phase45` measured that correctly — but a pass at the ceiling that
need not exist is all waste, and a per-pass profile never asks whether a pass should be
there. Five fusions, each bit-identical and each behind its own switch:

- the residual in the projection's epilogue, twice, and attention in one pass (Codex's
  phases 38, 39 and 42, ported: `notes/improve-fusions.md`);
- the head merge in the fused attention's store (`NR_FUSE_ATTENTION_MERGE`, 4 %);
- **Q and K normalised and V published in the QKV projection's own epilogue**
  (`NR_QKV_EPILOGUE`, 22 %): the float32 projection — 377 MB at block 0 of a 720p frame,
  written once and read three times — no longer exists. `notes/improve-qkv-epilogue.md`.

All five off against on, paired in one process on a freshly booted machine: **1280x720
458 -> 284 ms, 1920x1080 980 -> 600, 384x384 80 -> 52**, dispatches 1128 -> 592. The extent
curve is now **10 ms + 274 ms per megapixel**; live, 512x288 at scale 0.35 is 53.9 ms,
18.6 fps, and 1920x1080 at 0.55 is 280 ms. README table and `nr_knobs.RATES` re-measured
with it.

Four things to carry:

- **Measure with empty swap.** Before a reboot, with 5.5 GiB in zram, 1920x1080 live ran
  322-463 ms from run to run; after it, 278-283. The small extents barely moved. Check
  `swapon --show` and `/proc/pressure/memory` first; `phase51` was bitten by the same thing.

- **Workgroup shared memory comes in powers of two.** A 64-byte array beside a `stage`
  of exactly 2 KB took every tiled GEMM's workgroup to 4 KB and cost 23 ms of a 720p frame —
  in every tiled GEMM, used or not, with the compiled code identical to the instruction.
  Found only by profiling with the new feature *off*. Check the total before adding any.
- **A fusion moves a write into an earlier dispatch, so it has to re-check the scratch
  arena.** `k16` shares a role with the QKV projection's own input; the epilogue writes K
  while other workgroups still read that input, so K goes to `key16` in that mode. The role
  table was built for passes that finish before the next starts.
- **`ffn_batch.py`'s host-read column is not to be trusted between modes.** Twice it showed
  the "on" mode reading the head up to 2x slower; twice a direct probe found 7.49 against
  7.45 ms. The graph time is the measurement.

Left: window attention (57 ms at 1280x768) is now the largest pass that is not a GEMM;
`partition` (17) and `to_half` (10) could be second outputs of the epilogues before them; the
QKV epilogue's reduction runs on half the lanes.

## The present has a test, and `NR_LAYER_SYNC=semaphore` (2026-09-19, later)

> **Superseded 2026-09-22** (`d22ed9d`): there is one path now. The layer waits on the
> present's own semaphores, finishes its copies on private fences and never drains a queue;
> `NR_LAYER_SYNC` is accepted and ignored. `notes/improve-present-fences.md`; the stand that
> found the old default reading unfinished images is `phase69`.

The layer's two `vkQueueWaitIdle` calls per present are now optional. `NR_LAYER_SYNC=semaphore`
waits on the semaphores the present brought, signals one of a ring of four, and redirects the
present onto it; only the readback stalls, on its own fence. **Default is still `idle`** until
somebody runs the other one through a real game.

**What matters more than the switch: the present path has a test at last.**
`VK_EXT_headless_surface` gives a swapchain with no screen, so `src/layer/test_present.c` plus
`test_present.py` drive real presents through the layer against a stand-in daemon and check
both directions — what the game drew reaches the daemon, and what the daemon answered is in
the image next time round. In `make test`, both sync modes, both memory modes.

Its first run caught `present_now` calling itself, added an hour earlier: every application
would have died on its first present. It also cost four experiments aimed at Mesa before the
obvious check — **build the old code and run the new test against it** — put the blame back
where it belonged. `notes/phase66`.

The test does **not** prove the wait is load-bearing: removing the wait on the game's
semaphores leaves every check passing, because on this machine the clear finishes long before
the copy is submitted. That needs a game, or a card fast enough to lose the race.

## The graph runs in memory the host cannot address (2026-09-19)

The rest of the discrete-card answer. `phase63` put the operands in the card's memory; that
still needed the card's memory to be *mappable*, and without resizable BAR the window is
256 MB, so the code fell back to system RAM — the 50x trap again. Now the graph's buffers can
be device-local and **unmapped**: the host reaches them through explicit copies on their own
command buffer, and `Buffer.view()` on one raises instead of handing back a shadow.

**`XMX_STAGING=1` forces that path on this iGPU**, which is how it is tested: where memory
lives cannot change what the graph computes, so every test becomes a test of the discrete
path. The head is bit-identical (`9e1e37d981fbcf01` either way) and `make test` is green in
both modes, 181 checks each.

Two more from the same work: the daemon's frame line ends with `gpu <in>+<run>+<out>ms`,
because on a card the outer two are PCIe and one total cannot tell them apart; and the
weights moved above the frames, so changing the render scale no longer re-uploads 292 MB
(first frame at a second extent: 335 ms -> 78). `notes/phase65`.

~~**Still `vkQueueWaitIdle` twice a frame in the layer.**~~ **Gone since 2026-09-22**
(`d22ed9d`, `notes/improve-present-fences.md`): no queue is drained; the layer waits on the
present's semaphores and on its own fences. The cross-queue case this paragraph feared was
real — `phase69` caught the old default copying an unfinished image.

## Somebody else ran it, on a discrete GPU (2026-09-18)

An **Arc B580** — discrete Battlemage, same cooperative-matrix table as this iGPU, config
for config — measured **50x slower** than the Arc 140V on the same GEMM shapes. Cause:
`memtype()` in `libxmx.c` preferred `HOST_CACHED` for every buffer, which is right on this
shared-memory APU (`phase8`: 80 MB/s readback from the uncached type) and means *system RAM*
on a card with its own memory. Every operand crossed PCIe; 12 GB of VRAM sat idle. The
preference now follows `deviceType`, with a 1 GiB heap floor so a card without resizable BAR
falls back instead of failing to allocate. **Untested — there is no discrete GPU here**; this
machine takes the same branch as before and is unchanged.

Two traps came with it. `bench.py` never checked what `xmx_gemm` returned, so a call that
failed instantly printed **495 058 GFLOP/s** and hid the one interesting event in their log.
And their `make test` failure (`test_ui_mask`, empty answer) carries no diagnosis, because
the harness only prints the daemon's stdout if the daemon exits; `nr_frame.py --resident` is
the reproducer that shows the error. `notes/phase63`.

## A D3D12 game with a picture — Mortal Kombat 1 (2026-09-16)

The first D3D12 title with a picture, not just an attach. Run off the BitLocker Windows
partition: `steamapps/compatdata` symlinked to Linux, Windows' own `shadercache` left alone,
VKD3D/DXVK cache paths set explicitly. **Render scale 1.0 at 1920x1200 gets the game
OOM-killed** — its D3D12 buffers and the model's do not fit in 15 GiB together; 0.55 live was
fine. And the pass behaves unlike Tekken and DoA5: brightness unchanged, fine detail on the
faces *down* 14-24 %, the visible change the warm grade and skin glow coming out. Not film
grain — measured. `notes/phase62`.

## The parameter count was wrong, and so were two published claims (2026-09-16)

**145 755 123 parameters, not 73 841 889.** The large matrices are FP8 E4M3, one byte each;
73 841 889 was the weight section's bytes divided by two, the dense-FP16 misreading withdrawn
as an encoding on 2026-09-08 and never re-checked as a count. The "~148 M FP8" press figure
was right. Fallen with it, from the published `docs/ARCHITECTURE.md`: **GQA 4:1** (`qkv` is
`(C, 3C)`, full MHA — this file had already withdrawn it) and **27 % subnormals** (the real
weights hold 7). Corrected in README, the architecture and the brief. **Before quoting any
number about the weights, sum the logical shapes.** `notes/phase61`.

## The performance mode buys nothing (2026-09-16, later)

Switched to performance in Windows (a firmware setting Linux cannot reach: RAPL PL1 35 W,
PL2 37 W) and in Linux. **The graph did not get faster.** It sat at 1950 MHz — the GPU's
hardware ceiling, `rp0` — for 89 % of the run, and a package power limit cannot lift a
frequency ceiling. Re-measured idle in three processes: **15 ms + 449 ms per megapixel**, and the
whole frame at **209-210 ms against 214 on power-saver with the same code — 2 %, noise**.
The first run had Steam recompiling Counter-Strike 2's shaders on two cores unnoticed; the
graph did not notice (GPU-bound, within 1-5 ms), one whole-frame time did (245 ms). Check
`ps` for `fossilize_replay` before any benchmark on this machine. **Do not record the power profile as a variable against frame
time; record that the GPU was at its ceiling.** The lever is still the extent.
`notes/phase60`.

## A third kind of client, and 10.5 fps (2026-09-16)

**Tekken 7** — Unreal Engine 4, **64-bit D3D11 through DXVK** — runs live at **10.5 fps at
640x360**, 210 frames measured over 20 seconds, 91 ms each. Nothing was changed to make it
work. That is the third kind of Vulkan client: 32-bit D3D9/DXVK (`phase34`), 64-bit
D3D12/VKD3D (`phase41`), and now this. `notes/phase59`.

Two things worth carrying:

- **The history gate reads 0.573**, the highest yet, against 0.38-0.54 in a DoA5 fight and
  0.12 on the replay `phase54` was measured on. The gate is global — it reports how much of
  the *whole frame* agrees with its history — and Tekken's camera barely moves. 86 % of the
  frame also takes the floor. The most stable live picture so far, for a reason that
  belongs to the game rather than to anything here.
- **UE4 hands over a swapchain the size of the window**, so `Letterbox` finds nothing and
  costs its eight lines. DoA5's quarter-frame bars are not universal.

Do not take a game's API from its executable: UE4 carries `D3D11RHI`, `D3D12RHI`,
`VulkanRHI` and `OpenGLDrv` in the string table of every build, and the configuration is
inside the pak files. `/proc/<pid>/maps` on the running process settles it — and shows
whether the layer attached, in the same line.

## The host passes are in C now (2026-09-12)

The lever this file has pointed at for two days — the full-frame passes *around* the
network, which run at the output resolution and do not shrink with the render scale — is
taken. `ProjectsCodex` wrote them in C (their phase36); `src/ref/nr_image.c` is that
library, extended here for the history in the feature channels and the temporal
composition, which that tree does not have. **Host passes 74 -> 28 ms, output
byte-identical**, checked by `src/ref/test_native_image.py` over reversed views, padded
crops and a mirrored network extent. The frame is 259 -> 214 ms at 1024x768 / scale 0.55,
because the graph is 185 ms of it and untouched. `notes/phase57`.

Three things worth carrying forward:

- **The floor's constant stays in NumPy**: `clip(1 - moved * 255 / ramp, 0, 1) * hold` is
  the same value as `moved * slope + hold` by algebra and a different one in float32. The
  contract is byte-identical, not nearly. *The gate stayed there too, for `expf`, until
  2026-09-24: its logit is half, so a 65536-entry table of NumPy's own sigmoid runs it in C
  bit-exactly (`phase57`).*
- **`active_region` then became the largest host pass** — 21 ms, reducing the whole frame
  twice to find bars that never move. `Letterbox` finds them once and afterwards checks
  eight lines. 0.2 ms.
- **Their 40 % is our 17 %, and that is the honest reading.** Their baseline feature
  assembly was 146 ms against our 14.5: most of their headline was ground `phase47`,
  `phase48` and `phase51` had already covered here. The new part is a factor of three.

Measured on **power-saver at 1.2 GHz** — the pair is comparable, the absolutes are not
comparable with earlier sessions.

## It stops flickering (2026-09-11)

The picture in live mode shimmered, and the cause was not noise in the input. Measured on
22 consecutive presents of a real game: **2.3 % of a frame is byte-identical between two
presents, and the network moved those pixels 3.25 levels of 255 anyway**, while pixels
that actually moved came back amplified 1.01x. The graph is global — five downsamples into
a ViT-1D bottleneck whose attention sees the whole frame — so a fighter moving in the
middle moves the decoder's answer over a crowd that did not move at all. Vendor stability
comes from the temporal path, not from the network (`notes/phase53`).

**The temporal path now runs in live mode, and the flicker is 3.7x smaller for 3.7 % of
the frame time** (215 -> 224 ms; static-pixel invention 3.25 -> 0.87 levels; moving pixels
untouched at 0.98x). `notes/phase54`. Three things a next reader needs:

**1. Identity reprojection is free and exact.** A `vkQueuePresentKHR` layer has no motion
vectors, so the previous output goes into feature channels 7-9 where it sits.
`sample_history` at pixel centres is **bit-identical** to the history — the five-tap
Catmull-Rom collapses to its middle tap — so there is no gather and no blur, and
`nr_frame.apply_history` matches MLX-DLSS's `make_temporal_features` bit for bit.

**2. The learned gate is not local, and this is the surprise.** `phase12` measured it at
**0.705** with correct history. In a fight it reads **0.12** over pixels that did not move
and 0.014 over pixels that did. It discriminates 10x, but the whole scale is down 5x,
because most of the frame moved and a global network distrusts the history everywhere.
**Optical flow does not fix it** — measured, not assumed: DIS costs 7 ms, raises the
whole-frame gate 0.076 -> 0.111, and nearly all of that lands on *moving* pixels, which is
the ghosting case rather than the flicker one.

**3. So there is a floor under the gate, and it is ours, not the vendor's.**
`history_confidence` can only scale down. What a present-time layer has instead is the
game's own frame: where the game handed back the same pixel, the previous output is right
for that pixel by construction. `nr_daemon.hold_floor` turns that into a per-pixel lower
bound, full at zero change and gone by four levels of 255, never above `blend_scale`. It
cannot ghost — the frame that changes a pixel is the frame that releases it — and it costs
**2.2 ms**. Knobs: `temporal`, `hold`, `cut_limit`, all live through `nr-ctl`.

**The daemon is now stateful across frames.** Any test that sends a sequence and expects
each frame to stand alone has to pass `--temporal 0`; `test_ui_mask.py` does.

**And there is a switch on a key** — `src/layer/nr-toggle`, a flip plus a notification,
and turning it on brings the daemon up so one press really is one press. Bind it in System
Settings; `nr-toggle install` prints the two commands and opens that page.

**There is a panel, and one table behind everything.** `src/layer/nr-panel` puts all
eight knobs on one screen with what each does and a status line read from the daemon's own
log; `src/layer/nr_knobs.py` is the single definition that `nr-ctl`, the panel and
`README.md` all render, checked by `make test` so a knob cannot exist in one and be
missing from another — which had already happened with `hold`. `README.md` is new and is
the front door for anyone arriving cold. `notes/phase56`.

**Do not make this tool install its own shortcut. It did, and it crashed KWin on the first
keypress.** In Plasma 6.7 the shortcut registry lives *inside* KWin —
`org.kde.kglobalaccel` is owned by `kwin_wayland` and `plasma-kglobalaccel.service` is
**inactive** — so editing `kglobalshortcutsrc`, deleting the launcher and restarting that
unit all work behind the back of the live registry. It kept the component, the file under
it was gone, and the next key went `Component::uniqueName()` ->
`GlobalShortcutsRegistry::processKey` -> SIGSEGV. Firing it with `invokeShortcut` had
passed, because that dispatches by name and never walks the key map: **a test that
exercises the mechanism around the thing under test proves nothing about it.**
`notes/phase55`.

## It runs in a game, live, at 10 fps (2026-09-10 evening)

Ten phases in one session, `notes/phase39` through `phase48`. The three things a next
reader most needs:

**1. The frame is fully accounted for, and performance really is finished — inside the
graph.** `xmx_profile()` puts a GPU timestamp after every recorded pass, so there is now
a per-pass breakdown instead of two ablations (`src/bench/frame_profile.py`,
`notes/phase45`). GEMM is 216 ms of 488 at 720p; of the other 272, **every pass that only
moves data is at the memory ceiling** (residual 104 GB/s, merge heads 84, partition 69,
split heads 65, to-half 61, against the machine's 70-91). Only softmax and cosine publish
sit below it, at ~45 %, and they are arithmetic — that is what an arithmetic pass looks
like measured with a bandwidth ruler. GEMM is register-bound (`phase26`). There is nothing
left to win inside the graph.

**2. The bottleneck moved out of the graph.** Shrink the extent and the network stops
being the frame. What is left is a stack of independent full-frame passes over the
*output* resolution — feature assembly, composition, the head upscale, the detail blur —
each reading and writing the whole picture, and none of which shrinks with the render
scale. Two of them were pure waste and are fixed (`phase47`, `phase48`): a resample
written in the obvious 2-D form cost **193 ms of a 350 ms frame** until the axes were
separated, and the diffusion noise was rebuilt from four transcendentals a pixel every
frame despite depending only on the extent and a frame index nobody sets.
**This is where the next work is.**

**3. There is a live mode and a control tool.** `NR_LAYER_LIVE=N` in the layer,
`--render-scale` in the daemon, `src/layer/nr-ctl` to drive both without reloading the
model. Measured end to end: **512x288 at scale 0.35 is 94 ms, 10.6 fps**; 640x360 is 9.5,
854x480 is 5.4. Set the *game* to that size and the compositor does the stretch for free.
The network runs on the reduced frame but the **head** is scaled back and composed against
the full-resolution original, so the game's own pixels are never resampled.

**4. Read the parallel tree before assuming this one is ahead.** `~/ProjectsCodex` is
driven by a different model on the same problem. Twice now it has held something this
tree lacked: `.gitignore` hiding the entire CPU reference from thirteen commits, and —
this session — seven Vulkan-layer guards plus a real bug, the interface mask silently
failing whenever a strength knob moved (`phase49`). Its git history being behind ours
says nothing about its working tree.

Also this session: the interface mask proven in a live fight (**ten times less HUD
damage**) and then caught making a *worse* artefact on a near-static frame, diagnosed and
fixed (`phase42`, `phase43`); the profiles measured as a real trade-off — everything added
to skin texture comes out of speculars and colour, and `--colour-strength` runs *opposite*
to its name (`phase44`); the layer proven under **VKD3D-Proton** on a 64-bit D3D12 game
(`phase41`); and four busy E-cores measured to cost the GPU **7 %** for a theoretical
gain of under 2 % (`phase46`).

## Earlier entries, now folded into the notes

softmax native packing (`phase29`), captured frame replay (`phase28`), pipeline
specialization (`phase27`). All three are in effect and none is contested; the numbers
live in their notes.

## IT RENDERS (2026-09-09, later)

A frame goes in and a neurally-rendered frame comes out, with the real effect:
eyelashes and eyebrow hairs resolved out of a smeared input, skin pores synthesised,
iris and eyeliner sharpened. `notes/phase7-first-render.md`.

```
make                                                         # Vulkan runtime, layer, shaders
python3 src/ref/nr_frame.py IN.png OUT.png --resident        # the fast path
python3 src/ref/nr_temporal.py IN.png OUT --resident --pan 6,0 --frames 5
work/venv/bin/python src/ref/nr_frame.py IN.png OUT.png --accel   # 10 s, best on CPU
python3 src/ref/nr_frame.py IN.png OUT.png                   # 38 s, netlib reference
```

Before phase27, a full **1280x720** frame rendered in **0.59-0.72 s** (network extent
1280x768), with a **2.9 s** 1080p record. Current paired results are above. Quote the
band, not a single number. Frames inside one process were steady to 2 %, but the same binary spread ten
percent either side of a ~625 ms median *between* processes — buffer placement, not
clocks; the GPU holds 1950 MHz at 43-45 C throughout. A first frame after an idle costs
two to four times the rest while the clock ramps.

- `src/ref/nr_model.py` — the recovered 71-block graph in numpy (a port of MLX-DLSS's
  PyTorch `model.py`, Apache-2.0; no torch on this machine). One GEMM entry point,
  `nr_model.MATMUL`, for the XMX swap.
- `src/ref/nr_frame.py` — frame in / frame out, using MLX-DLSS's pure-numpy
  `features.py` and `composition.py` loaded by path from `work/mlx-dlss`.
- Our 649 logical tensors match their `weight_spec.json` **exactly** — 0 missing,
  0 extra, 0 shape mismatches. Two independent extractions of the same DLL agree.
- Two controls passed. Shuffled weights give a flat red tint with no pores and no
  lashes (structure-blind, ratio 0.98 vs the trained 1.18). The recovered control
  profiles work: `neutral` switches the network off by **37x** (change 0.00071 vs
  0.02610), and `natural` / `cinematic` are distinct styles.
- **`src/ref/{hnet_model,hnet_ops,hnet_ref,forward,run_frame}.py` are superseded.**
  They decode the packed container as dense FP16 and guess the block layout. Keep them
  for the PTX-derived findings they encode; do not build on them.
- **Phase 4 is done**: `src/gpu/nr_xmx.py` puts every GEMM on the XMX units, the
  batched attention included. `notes/phase8-xmx-graph.md`.
- **`src/gpu/nr_xmx.py` (the GEMM-hook path) is superseded by residency** and kept
  only for comparison. It lost to a good CPU BLAS, because a dispatch round-trips its
  activation through host memory; that is what made residency the precondition rather
  than an optimisation. The CPU baselines are worth remembering: the system numpy is
  netlib reference at ~3 GFLOP/s, a pip numpy is OpenBLAS at 214, and on the 384 face
  netlib CPU is 38.0 s against OpenBLAS CPU 17.5 s. `notes/phase13-torch-and-blas.md`.
- **The numpy port is bit-identical to MLX-DLSS's PyTorch original** — every primitive,
  every layout operator, all four block families on real weights, and the whole
  71-block forward, once the same GEMM is given to both.
  `python3 src/ref/test_against_torch.py` under `work/venv`.
- **Per-element agreement is not a property a port can have.** Each precision regime
  annihilates perturbations below its own working precision *exactly* and jumps
  straight to 9-12 % of the head's sd above it — float32 flips between 1e-09 and
  1e-07, half between 1e-05 and 1e-03. numpy's own float32 GEMM carries 5.2e-07 of
  error, above the float32 threshold, so **any** correct float32 implementation would
  diverge from this reference by the whole floor. Judge on the composed image and on
  the controls, never per-element. `notes/phase9-numerics.md`.

---

- **The NPU is not worth using.** Present and driver-ready (`/dev/accel/accel0`,
  `intel_vpu`), but the GPU is the faster engine in this SoC (67 TOPS against 48), our
  own shader runs at 4 % of it, all three engines share one memory pool so the actual
  bottleneck does not move, and the graph's bit-level ops do not fit an NPU compiler's
  operator set. `notes/phase14-npu-and-rounding.md`.

---

## 0. The one thing to know

**The weight container does not hold plain dense FP16, and almost every dead end in
this project traces to assuming it does.**

`iamwavecut/MLX-DLSS` ships working extraction code for the same DLL. Run on ours it
turns the 153 packed tensors into **649 named, shaped, logical tensors** — none
unsupported, none opaque. The payload is packed into `mma` fragment order and partly
E4M3; our reader was interpreting those bytes as dense FP16.

Proof it matters: `block0.layer0.input_adapter_weight` is `(16, 32)`, exactly the
512-element region we had located at the front of block0. Its logical values have
**correlation −0.02** with our raw FP16 read of the same bytes, and sd **0.2489**
against our 0.0306 — the `1/sqrt(16) = 0.25` that `notes/phase5-stem.md` predicted a
16-input projection should have. Right location, right shape, wrong decode.

So: **start from `work/mlxw/dlssnr-logical.safetensors`, not from
`src/tools/hnet_weights.py`.** Everything we derived from the *kernels* survives;
everything we derived from container *bytes* has to be re-checked against the logical
tensors.

```
git clone --depth 1 https://github.com/iamwavecut/MLX-DLSS work/mlx-dlss
T=work/mlx-dlss/python/mlxdlss/tools
PYTHONPATH=work/shim python3 $T/extract_dlssnr_weights.py work/dl/nvngx_dlssnr.dll work/mlxw/dlssnr-packed.safetensors
PYTHONPATH=work/shim python3 $T/unpack_dlssnr_weights.py work/mlxw/dlssnr-packed.safetensors work/mlxw/dlssnr-logical.safetensors
```

`work/shim/safetensors/` is a 60-line format-compatible shim — this machine has no pip
and the real package needs sudo. Round-trip verified. Both files already exist.

---

## 1. Do this first

The graph runs, on CPU and on XMX, and the kernel work is finished. **720p is 495 ms
and dead stable — 494/494/494 across three processes.** What is left, in order of
value:

1. ~~**A frame worth looking at, from a real game.**~~ **Done** —
   `notes/phase34-doa5.md`. Dead or Alive 5 (appid 311730, 32-bit D3D9 through DXVK,
   not D3D11 as written here before) was driven live and the pass ran on real faces:
   lashes and skin resolved in the game's own swapchain image.
   `src/layer/nr-photo --proton <appid> <exe>` is the way in.
   What is *not* done is the interface mask on a live HUD. It was written and it was
   broken in the one place tests did not reach — `exchange()` used one size for the
   request and the answer, so every masked frame came back unchanged
   (`notes/phase39-layer-review.md`). Fixed and covered by a test that fails on the old
   code, but **never yet run in a game**: that needs a restart with `NR_UI_MASK=1`.

2. ~~**The Mesa/ANV cooperative-matrix store bug.**~~ **It does not exist** —
   `notes/phase38-there-was-no-bug.md`. The reproducer written to file it found that the
   last surviving variant was our own invalid shader: a float16 matrix stored into a
   `float[]`, where the component type does not match the destination. Given a matching
   destination every variant is exact. Three phases of design rested on it. What is left
   upstream is llama.cpp issue #13530
   has coopmat disabled for all Intel on the strength of an Alchemist regression, and
   its only Xe2 rebuttal is a discrete B580 with GDDR6; Arc 140V on a UMA LPDDR5X pool
   is unmeasured in public and this project has the numbers.

3. **A live mode exists and reaches 10 fps.** `notes/phase47-live-mode.md`:
   `NR_LAYER_LIVE=N` in the layer, `--render-scale` in the daemon, `src/layer/nr-ctl` to
   drive both without restarting the model. Measured end to end, **512x288 at scale 0.35
   is 98.6 ms, 10.15 fps**; 640x360 is 8.6 and 854x480 is 5.7. Set the *game* to that
   size and the compositor does the stretch for nothing. Note what this changed: below
   about 640x360 the network is no longer the frame — the numpy at output resolution
   (composition, feature assembly) is, and it does not shrink with the render scale.

4. ~~**If more speed is wanted, measure before choosing.**~~ **Measured, and there is
   nothing left inside the frame.** `notes/phase45-frame-profile.md`: every pass now has
   a GPU timestamp (`xmx_profile`, `src/bench/frame_profile.py`), not an ablation. The
   split is **216 ms of GEMM against 272 ms of everything else** — the old ablation said
   221/290, so it was right. What is new is the inside of that 272:

   | pass | ms | GB/s | of the machine's ~80 |
   | --- | --- | --- | --- |
   | softmax | 74.4 | 36 | 44 % — arithmetic |
   | cosine publish | 67.9 | 38 | 48 % — arithmetic |
   | residual | 51.3 | 104 | **130 %** |
   | split heads / partition / merge heads | 52.9 | 65-84 | **81-105 %** |
   | to half | 11.3 | 61 | **77 %** |

   **Every pass that only moves data is already at the memory ceiling.** The two below it
   do per-element arithmetic, so 45 % of bandwidth is what that looks like, not a
   deficiency — and softmax has already had one round of exactly this work for 1.1 % of
   the frame (phase 29). GEMM is register-bound (phase 26). The frame is fully accounted
   for. Also killed, with a probe kept at `src/bench/bank_probe.*`: the 32-way shared
   memory bank conflict in `attention.comp` is real and costs **1.11x**, not 32x.

   The extent curve is **17 ms + 488 ms per megapixel** — down from 20 + 632 before
   specialization and replay. Things already tried, with numbers, that should not be
   repeated: shared-memory operand staging (phase 22), integer weights (phase 23),
   register blocks past 16x32 and software pipelining the K loop (phases 21 and 26),
   and storing published buffers as float16 (phase 22 — correct, and no faster).
   ~~The open ones: attention layout/conversion fusion, and OpenCL.~~ Both are now
   closed. Attention Q/K fusion is **done** (phase 32: 1804 -> 1664 dispatches, 4 %).
   **OpenCL is measured and is not the lever** — phase 33: its DPAS path reaches
   3533 GFLOP/s against Vulkan's 3828 on the same shape, peaks at the same 16x32 block
   and collapses beyond it the same way. Two APIs, two subgroup widths, one curve.
   `cl_intel_subgroup_2d_block_io` is present and untried, and would have to buy more
   than 8 % just to reach parity.

**Memory is no longer the constraint it was**: the shared scratch arena took 720p from
5041 to 2303 MiB and 1080p now fits without swapping (phase 32).

**Real time is still not on the table.** At `17 ms + 488 ms/Mpixel`, 30 fps needs about
a 243x137 extent and 15 fps about 425x239. On this hardware with this graph, DLSS-NR is
a photo mode — which is what the Vulkan layer delivers.

---

## 2. What is solid — measured from the kernels, unaffected by the packing

| Fact | Where |
|---|---|
| Attention **is** softmax: logits hard-clamped (Swin ±6, ViT ±3), `exp` hand-rolled in f16x2 bit arithmetic, **no max subtraction**. Constants exact, verified bit-exact | `notes/phase5-softmax-found.md` |
| The gate multiplies the **skip**, fused into the `mma` C operand: `D = A·B + gate⊙x`; the branch is added unscaled | `notes/phase5-gate-on-skip.md` |
| `attn_scale` is **FP32 per head** (`ld.global.b32` + `cvt.rn.f16.f32`, stride 4, indexed `head = 4·ctaid.z + tid.y`) | `notes/phase5-attn-scale-fp32.md` |
| **head_dim = 32 at every width**, from the QK-norm reduction (4 lanes × 4 squares × 2). Head count = C/32 | `notes/phase5-narrow-blocks.md` |
| Kernel parameter block: `+0` input, `+8` output, `+16` weight arena, `+24/+32` dims. I/O kernels take descriptor tables | `notes/phase5-io-structs.md` |
| **Input is five optional 2D textures**, `tex.2d.v4.f32`; only colour is required | `notes/phase5-input-contract.md` |
| **16-channel packing order**: ch4-6 colour, ch7-9 reprojected history (same affine `(x−a)·b`), ch12-14 sign-encoded validity. MLX-DLSS agrees independently | `notes/phase5-channel-order.md` |
| Output head is **32 → 4**; three channels become display RGB | `notes/phase5-output-head.md` |
| XMX flushes subnormal FP16 to zero; fixed by a per-tensor 2^k rescale | `notes/phase4-subnormal-flush.md` |
| **Each precision regime has a sharp threshold**: below it a perturbation is annihilated exactly, above it the head jumps to 9-12 % of its sd. float32 ~1e-07, half ~1e-04. The half path is the **more stable** of the two | `notes/phase9-numerics.md` |
| The whole CPU/XMX gap is the FP16 rounding of GEMM *activations*, and **97.3 % of them are already half-valued** — only 186 of 6987 calls are touched. Weights change nothing: 579 of 649 tensors are stored F16, the other 70 are `attn_scale`, not a GEMM operand | `notes/phase9-numerics.md` |
| Batched attention and the folded branched FFN are **bit-identical** to the plain GEMM hook; the two 720p renders are pixel-identical | `notes/phase9-numerics.md` |
| ~~CPU and XMX agree to 9.7e-07 over 4.2M elements~~ — measured on a pass-through with no E4M3 publishes in it; does not transfer | `notes/phase4-end-to-end.md` |

---

## 3. Withdrawn — do not resurrect

Today alone: **eight**. This is the normal rate here; assume the next confident claim is
also wrong until it survives an adversarial test.

- **"The weights are plain FP16, used exactly as stored."** Section 0.
- **"GQA 4:1, Q=C², K=V=C²/4."** `qkv_weight` is `(C, 3C)` — **full MHA, no GQA**. The
  narrow-block Q/K/V split that resisted three methods does not exist.
- **"DLSS-NR does not use softmax."** The `ex2` census was right, the inference wrong;
  `ptx_trace.py` was dropping every `{ … }` scope, which is where the f16x2 exp lives.
- **"3C² means K and V are pre-replicated."** The factor 2 is uniform across all
  matrices — it is the packing, not GQA.
- **"The 128C region is an attention mask."** `max = 0.00` was a rounded print; the
  fraction of exact zeros is 0.0000.
- **"`attn_scale` is stored as a log."** Right symptom, wrong mechanism — it is FP32.
- **"The attention branch helps, 1.39x swing."** Reverses on a structurally different
  image and on two real Cyberpunk frames, where the branch **costs 1.37x**. The swing
  was the branch repairing damage the bilinear scaffolding does to `test_pattern`'s
  fixed-period lines. `notes/phase5-input-dependence.md`
- **"1.02x better than the input."** True only at sigma 0.06 on one synthetic pattern.
  Swept: the network adds a fixed distortion of 0.0163 and removes a constant 6.5 % of
  noise — constancy being the signature of a linear filter, not a denoiser.

Older, still withdrawn: the "missing 2^8.5 factor", the "~450x branch/skip factor" (it
followed from the wrong gate form), the leading-region projection, and "1.01x parity"
(a pass-through).

---

## 4. Traps this project keeps falling into

- **Tools that skip input silently.** `ptx_trace.py` dropped brace scopes and hid 120k
  instructions. Always check coverage: parsed statements vs `;` count, and whether the
  dataflow graph is connected.
- **PTX is not SSA, and it has loops.** "Last definition before the use" is only valid
  in straight-line code. The pre-block's store tail sits *textually before* the texture
  reads it consumes, because it is a loop body. A def/use map over all definitions
  invents edges; program order alone is not enough either.
- **The metric has had five holes.** Correlation rewards inaction; dividing by a fitted
  gain is 0/0 on collapse; a pass-through scores 1.00x; the score measured the
  scaffolding not the model for a whole phase; and results depended on the test image
  and the noise level. `run_frame.py` now reports COLLAPSED, PASS-THROUGH and
  NETWORK-INERT — **and `--no-attend` is the control to run whenever a score moves.**
- **Statistical segmentation cannot find tensor boundaries.** Three methods failed
  calibration at C=512 where the answer was known. The notes said so; I re-ran one of
  them anyway. Read section 3 before trying a fourth.
- **A lookup table written from memory mislabels everything downstream.**
  `frame_profile.py` shipped a hand-written map of unary kind numbers to names. It was
  wrong, and under the wrong names one pass appeared to run at a fifth of memory speed,
  which produced a confident and entirely false diagnosis — with a proposed fix — before
  anyone noticed the table. The 50 ms belonged to the one pass that needed nothing.
  **Generate name tables from the source they name.** The file now regexes them out of
  `resident.comp` and `attention.comp` at import. `notes/phase45`.
- **A textbook mechanism is a hypothesis, not a finding.** `attention.comp` gives each of
  a subgroup's 32 lanes one row of a stride-64 shared tile: bank `i mod 32` for all 32
  lanes, a perfect 32-way conflict, arithmetic beyond dispute. Measured on Xe2 with a
  30-line probe it costs **1.11x**, not 32x, and the `gather`/`scatter` surgery it would
  have justified was not worth doing. `src/bench/bank_probe.*`, `notes/phase45`.
- **On this model, looking at the picture disagrees with measuring it.** The pass moves
  the *level* — it pulls back blown-out skin by 30-40 % — and that shift dominates every
  raw statistic and every impression. Twice in one session a confident visual reading was
  wrong: "more pores" where fine texture had *fallen* 16 % (it rose 31 % once normalised
  for level), and "the irises turned brown" where the hue moved 18° to 21° and had been
  brown all along. **Normalise for level, and sample the exact pixels you are claiming
  about.** `notes/phase42`, `phase44`.
- **One frame is not a recommendation.** `cinematic` was declared the defensible default
  for faces on the strength of a single cutscene where it landed slightly positive. Three
  frames later it was removing detail — on a bright scene it *smooths*. Any statement of
  the form "profile X is right for Y" needs several scenes. `notes/phase44`.
- **`pgrep -f` and `pkill -f` match the shell that runs them.** Four times now. The
  `[n]ame` bracket trick fixes the pattern but not the case where the string also appears
  elsewhere in your own command line — a heredoc, a comment, an echo. `pkill -f
  nr_daemon.py` killed the very shell launching the daemon. **List with
  `ps -eo pid,args | awk '/pattern/ && !/awk/'` and kill by explicit PID.**
- **The test suite and a loaded daemon do not fit together.** 15 GiB shared with the iGPU;
  a resident daemon holds the model, `make test` allocates its own device buffers, and the
  run gets OOM-killed. Stop the daemon before the suite, not after.
- **Steam compiles shaders behind your back, and it looks like a slow machine.** After a
  Vulkan driver update it replays each game's pipeline cache — Counter-Strike 2's was
  6.5 GB — in `fossilize_replay` workers that hold whole cores for many minutes. A
  benchmark run alongside is measuring a machine with cores missing. `phase60` was.
  **`ps -eo pcpu,comm --sort=-pcpu | head` before any timing run.**
- **Numbers in a published page rot silently, and the page keeps being read.** The
  README's frame-time table was measured on 2026-09-10 and survived the host passes moving
  to C two days later: for a week it understated this machine by a third, and at 1080p by
  half, on the same page that quoted Tekken's 10.5 fps and contradicted it. It is now
  generated from `nr_knobs.RATES` by `src/tools/knob_doc.py`, with `src/bench/live_rates.py`
  behind the numbers, and `make test` fails if the page and the table disagree. Sibling
  check: `src/tools/claims_check.py` fails the suite if a withdrawn claim appears outside
  the notes that withdrew it — it found eight on the day it was written. `notes/phase64`.
- **A pinned dependency nobody re-clones is a dead pin.** The README's Vulkan-Headers
  commit did not exist in KhronosGroup/Vulkan-Headers at all, so the build's second line
  failed for every reader from a clean checkout; the local copy has no `.git` and could
  never have shown it. Pin by tag (`--branch v1.4.321`), which fails loudly at clone time.
- **External write-ups are summaries, not sources.** A WebFetch of `weight_spec.json`
  returned plausible-looking shapes with a confabulated label (`block31` as "final
  output stage"). Clone the repo and read the file.
- **A reset to the remote drops whatever was never pushed.** On 2026-09-23 `master` was
  reset to `origin/master`, and three commits kept on purpose the day before went with it;
  `test_present.c` cited two notes that no longer existed until they were restored on
  09-24. Before resetting a branch to its remote, `git log origin/<branch>..<branch>` — and
  leave a backup ref.

---

## 5. Layout, commands, repo

```
ref/        the DLL (0444) + sha256      NEVER modify, NEVER commit
work/       weights, PTX modules, mlx-dlss clone, mlxw/*.safetensors, shim, builds
notes/      24 findings documents
src/tools/  PE/resource readers, the weight reader (now superseded), model_spec,
            and the PTX analysis tools: ptx_trace (dataflow), ptx_addrform
            (address → linear form), ptx_chains (accumulator chains)
src/ref/    nr_model, nr_frame, nr_temporal, nr_display, nr_accel, image_io
src/gpu/    xmxres, nr_resident, nr_frame_resident   <- the device path
src/layer/  nr_layer.c, nr_daemon.py, nr-photo       <- into a game
            hnet_*, forward, run_frame            <- superseded, kept for their findings
src/gpu/    gemm_coopmat*.comp, libxmx.c, xmx.py, tests
```

Git repo initialised 2026-09-08, three commits, **code only** — `.gitignore` keeps the
DLL, the weights, the driver payload and the game screenshots out. `/ultrareview` is
ready to run (user-triggered; the model cannot launch it).

Regression, all should exit 0:

```
python3 src/ref/test_nr_model.py                  # primitives, codec, temporal, graph
work/venv/bin/python src/ref/test_against_torch.py # bit-identical to the original
python3 src/gpu/test_resident.py                  # the device path, ~2 min
python3 src/gpu/test_gemm.py                      # worst rel 2.4e-06
python3 src/ref/nr_frame.py IN.png OUT.png --gpu  # the visual check
python3 src/ref/nr_frame.py IN.png OUT.png --profile neutral   # the control: ~37x smaller
```

`nr_frame.py` flags: `--gpu --size HxW --profile {standard,neutral,natural,cinematic}
--intensity F --detail-strength F --colour-strength F --frame-index N -v`.

**Controls** (`notes/phase10-controls.md`). Free, post-network — one pass covers the
whole range: `--intensity` (exactly linear, 0 an exact no-op, clamped to [0,1]),
`--detail-strength` / `--colour-strength` (the change is 0.0049 high frequency against
0.0249 low, so `--colour-strength 0` is detail with no tonal shift; past
`--detail-strength 2` it over-sharpens), `--intensity-ladder` renders once and writes
one file per value. Costing a pass each: `--style-index` (a different character, corr
0.50 with style 0 at index 1 — but only 0-8 are sane, 64 gives a magenta cast),
`--local-tone` / `--local-structure` (smooth monotone gains, colour-neutral and safe to
over-drive: tone reaches 1.24x at 2.0, structure peaks near 1.5), `--skin-structure`,
`--auto-mask`, `--control-mask`. Model A/B/C is **not** reproducible — the shipped
weights prove only slot 0.

`nr_xmx.install(fuse_branched=True, exact=False)`. `exact=True` carries activations
half cannot hold as a sum of two halves — more accurate than the reference's own
float32 GEMM — for 16.8 s -> 21.2 s. It moves the head gap from 0.0174 to 0.0124 and
no further; see `notes/phase9-numerics.md` for why zero is not reachable.

The old harness (`src/tools/model_spec.py`, `src/ref/hnet_*.py`, `run_frame.py`) still
runs but is built on the wrong weight decode; its scores measure scaffolding.

---

## 6. Honest standing

**The prototype works.** A frame goes in, a neurally-rendered frame comes out, on
NVIDIA's own weights, with the effect the feature is sold on. Two adversarial controls
pass (`notes/phase7-first-render.md`). The previous entry here — "no demonstrated
denoising" — is retired; its cause was the weight decode, exactly as section 0 predicted.

What is *not* claimed:

- **No NVIDIA parity gate.** There is still no NVIDIA GPU here, so there are still no
  reference activations. The graph is MLX-DLSS's recovery from vendor captures, and it
  is validated against their spec and against behaviour, not against the DLL.
- **The whole graph is resident on the GPU**: phase27 warm medians were **0.114 s** at
  384x384, **0.536–0.550 s** at 720p and **1.179 s** at 1080p; since the fusions and the
  shared-memory fix (2026-09-24) 1280x720 is about **0.2 s** of GPU time, on the curve
  `9.4 ms + 196 ms per megapixel`. The optimization is
  bit-identical to the generic GPU path. Earlier comparisons reported head correlation
  0.9918 with the CPU reference and visually indistinguishable pictures.
  **2.3 GiB** of device buffers at 720p since the scratch arena (`phase32`); the 5.6 GB
  this used to say predates it, and 1080p did not fit at all before.
  `src/gpu/nr_frame_resident.py`, `notes/phase15-residency.md`,
  `notes/phase18-fusion.md`, `notes/phase21-fusion-and-tiling.md`.
- **It runs in a real game, in two modes.** A Vulkan layer captures the presented frame,
  a daemon runs the model, and the result goes back into the swapchain. Proven in **Dead
  or Alive 5** (32-bit D3D9 through DXVK) with faces enhanced and measured, and the layer
  proven to attach under **VKD3D-Proton** on a 64-bit D3D12 title. Photo mode is triggered
  by a file; live mode (`NR_LAYER_LIVE=N`) runs continuously: 36.7 ms a frame at 512x288
  for the daemon alone (27 fps, `nr_knobs.RATES`, 2026-09-24). In a game it shares the GPU
  with the game's own rendering — Tekken 7 ran 25 fps at 640x360 on 2026-09-24, against 10.5 on 2026-09-16, before the
  fusions (`phase59`). `src/layer/`, `notes/phase34-doa5.md`, `phase41`, `phase47`.
- **HDR is handled**: `src/ref/nr_display.py`, the recovered display codec — encode a
  linear-HDR frame to an sRGB proxy with a soft knee, run the model, fold it back by
  luminance ratio onto the untouched original. Clamping instead destroys 97 % of the
  highlight structure. `notes/phase16-hdr.md`.
- **The interface mask works and has a failure mode.** In a live fight it cut HUD damage
  **tenfold** (mean |d| 9.69 -> 0.97 on the health bar). On a near-static frame it covered
  the *subject* in a speckle instead, and since the pass moves skin by 20-30 levels the
  interleaving read as a mottled crust — worse than the softened HUD it prevents. Two
  guards now: a majority filter narrowing the mask to solid blocks, and a coverage limit
  that drops it entirely above 55 %. `notes/phase42`, `phase43`.
- **The machine's real limits, measured** (`notes/phase20-machine-limits.md`):
  **70-91 GB/s** of memory bandwidth against 136.5 theoretical, the GPU holding its
  **1950 MHz ceiling** throughout a run at 46-48 C, and our GEMM at **8-12 %** of the
  ~32 TFLOP/s FP16 peak — **4.4 %** averaged over the frame's real shapes **before
  specialization**. The GPU has **64** XMX engines. Phase27 demonstrates that some
  register pressure was avoidable shader code; these older throughput measurements
  do not establish the optimized kernel's ceiling. Earlier notes quoted 23 GB/s,
  which was single-threaded numpy and wrong by 3x. Both GEMM and elementwise graph
  operations now run on the GPU.
- **Temporal processing runs in live mode** since `phase54`: the previous output goes into
  feature channels 7-9 with identity reprojection — a present-time layer has no motion
  vectors, and identity is bit-exact — under the model's learned gate and a hold floor
  where the game handed back the same pixel: 3.7x less flicker. Photo mode stays
  single-frame, and `nr_temporal.py` keeps the motion-vector path for sequences that have
  real motion. `notes/phase53`, `phase54`.
- The graph recovery is **not ours**. Ours is the Xe2 execution path, the numpy
  reference, the independent second extraction that confirms their weight spec, and
  the PTX findings in section 2 that their write-up and ours agree on.

## 7. Hardware (probed, trust it)

Intel Arc 140V-class Xe2, Mesa ANV, Vulkan 1.4.354, subgroup 32. Six cooperative-matrix
configs, all scope=subgroup, all M=8 N=16; the path is **fp16 × fp16 → fp32** (config 1).
`cooperativeMatrixRobustBufferAccess = false`, so edge tiles need explicit padding.
Shared-memory APU: correctness and capacity are unaffected (141 MiB of weights against
an 11.46 GiB heap); it caps **bandwidth**, already the measured ceiling at 0.7–1.9
TFLOP/s with a naive kernel. `libxmx.c` still memcpys into a mapped HOST_VISIBLE buffer
on every dispatch — on an integrated GPU those copies are free to remove.

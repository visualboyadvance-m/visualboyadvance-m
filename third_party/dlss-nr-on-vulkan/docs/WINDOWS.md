# Windows — the port, and where it stands

> **In this tree (andyvand/dlss-nr-on-vulkan, 2026-10-02):** this file and
> `docs/WINDOWS-PORT.md` / `docs/PERF-WINDOWS.md` are `uzbekunknown/dlss-nr-on-intel`'s record of
> its Windows port, carried over verbatim with its 34 commits of 2026-09-28 to 10-02. Where it
> says what the build cannot do, read it as that tree's build: here `CMakeLists.txt` builds the
> compute side on Windows with MSVC, clang-cl or MinGW (the native passes carry halves as
> `uint16_t`, not `_Float16`), builds the layer there too through `src/layer/nr_transport.h` and
> `nr_layer.def`, and has a second runtime on Direct3D 12 (`libd3dmx`, `NR_GPU_BACKEND=d3d12`,
> `notes/phase75`) beside libxmx. The compute findings — the fp16 configuration on Intel's
> driver, the folded `packHalf2x16` round trip and `half_round` chosen per driver,
> `DenormPreserve 16` — hold on both; `tools/build_win.bat` and `tools/deploy.*` are upstream's
> MSVC-into-`work/` route and assume that layout.

**Experimental; first run on Windows on 2026-09-29/30** (Arc 140V, Intel's driver 101.8991,
then 101.9033). The compute side builds and runs there, and since PR #3 joined (2026-10-02) the
layer and the daemon do too, from an MSVC build (`docs/WINDOWS-PORT.md`). What
the machine said is `notes/phase71-intel-windows-driver.md`. The plan below was written on Linux
before that session, and it is corrected where the machine disagreed.

## What is meant to run

| | Linux | Windows, first milestone |
| --- | --- | --- |
| the runtime, `libxmx` | yes | `work/libxmx.dll` |
| the native host passes, `libnr_image` | yes | `work/libnr_image.dll` |
| `coopmat_probe`, the GPU tests | yes | yes |
| photo mode on a still, `src/ref/nr_frame.py` | yes | yes |
| the game layer and live mode | yes | the MSVC build, on a named pipe (`docs/WINDOWS-PORT.md`) |
| `nr-photo`, `nr-toggle` (bash) | yes | no |

The runtime is plain C and Vulkan and the native passes plain C with OpenMP, so the compute side
needs no porting — only a build. The Python side loads `work/lib<name>.dll` there, with its
search path set as Python 3.8+ requires (`src/gpu/xmx.py`, `native_library`).

## The one unknown that decides everything

Every kernel here is written for `VK_KHR_cooperative_matrix` with an `fp16 x fp16 -> fp32`
configuration of **M=8, N=16, K=16** at subgroup scope — what Mesa's ANV offers on Xe2
(`notes/hw-coopmat.md`: six configurations). **Whether Intel's own Windows driver offers the
same is not known here.** `coopmat_probe` answers it in a second, and it is the first thing to
run. If the configuration is missing, nothing else in this file matters until it is solved.

And every speed figure in this repository was measured on Mesa: the shared-memory partitions,
the subgroup width, the specialisation constants and the kernels' shapes were all tuned against
ANV's compiler. Intel's compiler is another one. Expect correct results if the configuration is
there — the tests compare bytes — and expect to measure the speed rather than assume it.

**Answered on 2026-09-29: the configuration is there.** Intel's driver offers four configs, the
fp16 one among them. The compiler turned out to be the difference: it folds
`unpackHalf2x16(packHalf2x16(x))`, which `half_round` relied on, so libxmx now picks the spelling
per driver. The graph runs 1.2-1.4x Mesa's time. The picture was 47-49 dB from Linux's until
libxmx declared `DenormPreserve 16`. Left undeclared, Mesa flushes float16 subnormals in the
GEMMs and Intel's driver keeps them. With the mode declared, the two compute the same network
output bit for bit (phase71).

## Before building

1. **An Intel Xe2 GPU** — Lunar Lake 130V/140V, Battlemage B570/B580 — with Intel's current
   driver.
2. **The Vulkan SDK** (LunarG): the headers, `vulkan-1.lib`, `glslangValidator`. It sets
   `VULKAN_SDK`, which CMake reads.
3. **MSYS2**, in its UCRT64 shell:
   `pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,python,python-numpy}`. GCC 12 or newer —
   the native passes need `_Float16`, which MSVC does not have, and CMake refuses MSVC for that
   reason. OpenMP comes as `libgomp-1.dll`.
   With a python.org Python instead of MSYS2's, set `NR_DLL_PATH` to the UCRT64 `bin` folder,
   so `libnr_image.dll` finds `libgomp-1.dll` and `libwinpthread-1.dll`.
4. **ImageMagick**, with `magick` on the `PATH`, for the still-frame tools.
5. The same checkouts as on Linux — `work/vulkan-headers` is optional with the SDK,
   `work/mlx-dlss` at `06a3e11` is not — and the weights, extracted from your own DLL by the same
   two Python commands as in the README.
6. **A checkout on a path of ASCII characters only**, such as `C:\dlss-nr`. Python hands the
   runtime its shader paths as UTF-8 and the runtime opens them with `fopen`, which reads a path
   in the ANSI code page: a folder named in Cyrillic, as a user profile may be, fails to open.
7. **`PYTHONUTF8=1`** in the environment for anything run by hand. Windows' Python opens text in
   the ANSI code page, and the notes, the README and the knob table are UTF-8; CTest sets it for
   its own tests.

## Git on Windows

The repository's `.gitattributes` keeps every file LF in the checkout and batch files CRLF, so
`core.autocrlf` cannot turn a one-line change into a whole-file rewrite, as it once did. Write
commit messages from a file, or from Git Bash: PowerShell's defaults put a byte-order mark in
front of them.

## Build

```sh
cmake -G Ninja -S . -B work/cmake
cmake --build work/cmake
work/coopmat_probe
ctest --test-dir work/cmake --output-on-failure
```

The layer is off by default in this build on Windows (`NR_BUILD_LAYER`), so CTest registers the
compute tests and the tree's own checks, not the layer's. The layer, the daemon's named pipe and
everything else build with MSVC instead: `tools\build_win.bat`, into the same `work/`, so keep
one build per checkout (`docs/WINDOWS-PORT.md`). Both give the same heads.

On Intel's driver, leave `gpu_window_attention` out: `ctest --test-dir work/cmake -E
gpu_window_attention`. Its unmerged variant, which the graph does not use by default, hangs the
engine from 32 windows up (phase71). Everything else passes, now that libxmx declares
`DenormPreserve 16`; before it, a zero's sign in two GEMMs and 378 values of the ViT attention's
unfused reference differed here.

## The first session, in order

1. **`coopmat_probe`**: the configurations, against `notes/hw-coopmat.md`.
2. **CTest**: every GPU test compares against a reference — bit for bit where the path is meant
   to be exact. A failure here is a driver difference to understand before anything is timed.
3. **Photo mode** on a frame both systems have: `python src/ref/nr_frame.py IN.png OUT.png
   --resident`. The picture should be the Linux one; `src/bench/frame_replay.py` prints the
   head's hash and the warm graph time, the same numbers on both.
4. **Timing** with `src/bench/frame_replay.py --size 320 320` and `--size 720 1280`: the live
   extent and a 720p frame (on its 1344x768 field), against the same commands on Linux. `live_rates.py` speaks `AF_UNIX`,
   which Python has not here; the daemon itself listens on a named pipe.

## The second milestone: live mode in a game

- **Games on Windows present through D3D11 and D3D12, not Vulkan**, and the layer sees only
  Vulkan presents. For D3D9-11 titles — Tekken 7, Dead or Alive 5 — DXVK's DLLs in the game's
  folder put the game on Vulkan, as Proton does, and the layer attaches as it does on Linux. For
  D3D12, and for motion vectors and a frame before the interface, the planned OptiScaler route
  (`notes/HANDOFF.md`, "Planned") is the one.
- **The layer's POSIX parts**, all in `src/layer/nr_layer.c`: a pthread mutex (`pthread_mutex_*`,
  twenty-one uses — an `SRWLOCK`), the Unix socket to the daemon (`socket`, `connect`, `send` with
  `MSG_NOSIGNAL`, `recv`, `close` — TCP on `127.0.0.1` through Winsock, or a named pipe),
  `access` on the trigger file (`_access`), and one `open`/`read`/`write` pair. Then a Windows
  layer manifest, found through `VK_ADD_LAYER_PATH` or the registry
  (`HKCU\Software\Khronos\Vulkan\ImplicitLayers`), and `VK_LAYER_EXPORT` on the entry points.
- **The daemon**: Python has no `AF_UNIX` on Windows, so the same TCP or pipe transport on its
  side. `nr-ctl` and `nr-panel` talk through the settings file, the trigger and the daemon's
  log, and are portable once their paths leave `/tmp`.
- **The launchers** — `nr-photo`, `nr-toggle` — are bash; a Python or PowerShell one replaces
  them.

PR #3, an outside contributor's port of exactly this part — the layer's threads and transport, a
named pipe for the daemon, an MSVC build, deploy scripts — run on a B580, joined the main line on
2026-10-02. What is left of the milestone is a game on this machine: a D3D9-11 one through
DXVK, with the layer from `build_win.bat` and `tools\deploy.bat`.

# Phase 72 — the tree builds for Android

**2026-09-21.** The owner asked for dlss-nr-on-vulkan to be built on Android too. VBA-M's Qt
port already ships an Android APK (`tools/android/build-android-qt.sh`, Qt for Android from
vcpkg, the NDK's own toolchain file) with `ENABLE_VULKAN=ON`, and its Vulkan panel already
carries the DLSS NR share code (`renderers/vulkan-panel.cpp`) with no Android gate — so the
question was only whether this tree's CMake build survives the NDK, and it very nearly did.

## What happened on the first configure

Nothing failed. `find_package(Vulkan)` finds the NDK sysroot's `libvulkan.so` and headers on
its own; VBA-M's `HostCompile` module builds `bin2c` and `slice` for the Mac; glslangValidator
comes from the host vcpkg triplet (`VBAM_VCPKG_HOST_PREFIX`); libpng is vcpkg's static
`arm64-android` one, so even the `nr_frame` command is built. The pre-processed weights in
`weights/` compile with the NDK's clang 21 under the same two-job pool.

The first *build* had one failure and one worry:

- **`nr_layer.c` defined `VK_USE_PLATFORM_XLIB_KHR` under `__linux__`**, and Android is
  `__linux__` without X11 — `fatal error: 'X11/Xlib.h' file not found` from vcpkg's
  `vulkan.h`. Guard is now `__linux__ && !__ANDROID__`; the layer and its two direct-include
  tests then compile for arm64. The layer's *default* is off on Android anyway
  (`NR_LAYER_DEFAULT`): there is no game to hook and no daemon.
- **VBA-M compiles its whole Release build with `-ffast-math`** (`cmake/Toolchain-gcc-clang.cmake`),
  and that reaches these targets, whose contract is NumPy's float32 operation order. Every
  file printed `clang: warning: overriding '-ffast-math' option with '-ffp-contract=off'`.
  Checked on the NDK's clang, not assumed: `-O2 -ffast-math -ffp-contract=off -fno-fast-math`
  on `a*b+c` and `(a+b)+c-a` gives `fmul; fadd` and three separate adds — no `fmadd`, no
  reassociation — the same code as `-O2` alone with contraction off. So the exact flags,
  which come later on the line, win; the warning was noise and is now silenced on clang
  (`-Wno-overriding-option`). This applies to the desktop VBA-M builds as well, which have
  been compiling `libdlssnr` this way since the archive was added.

Two more small things: `nr_temp_dir()` returned `/tmp`, which Android does not have, and now
returns `/data/local/tmp` there (only `test_nr_frame`'s synthetic weights use it); and a
**standalone** cross build had no way to get `bin2c` and `slice` without naming host copies,
so the CMake now does what VBA-M's module does — a C compiler from the build machine's
`PATH`, refusing the cross toolchain's own directory (`NR_HOST_CC`).

## What was built

| | arm64-v8a, inside VBA-M | armeabi-v7a, standalone |
| --- | --- | --- |
| `libdlssnr.a` | 292 598 964 B | 292 546 480 B |
| `test_dlssnr` (PIE, `linker64`, needs `libvulkan.so libdl.so libm.so libc.so`) | 292 576 112 B | 292 363 352 B |
| `libxmx.so` / `libnr_frame.so` / `libnr_image.so` | 528 KB / 292 MB / 47 KB | built |
| `gemm_runner`, `test_nr_frame`, the 15 `.spv` | built | built |
| `nr_frame` command | built (vcpkg libpng) | not built (no libpng) |
| `libnr_layer.so`, `test_settled`, `test_exchange` | compile when forced on | off |

And the whole thing end to end: `build-android-qt.sh`'s `apk` target links
`vbam-components-filters-dlssnr` and `libdlssnr.a` into the Qt app module, so the APK
carries the model:

| | |
| --- | --- |
| `visualboyadvance-m-qt-ARM64.apk` | 398 252 777 B |
| `lib/arm64-v8a/libvisualboyadvance-m-qt_arm64-v8a.so` inside it | 321 411 488 B, 67 `xmx_*` / `nr_frame_*` dynamic symbols |
| next largest, `libQt6Core` | 42 MB |

The weights are the APK. Before this the module was a few tens of MB.

## What is not claimed

**Nothing has run on a device.** There is no Android device attached to this build, and the
owner's phone holds their signed copy that must not be replaced (`memory: reference_android_device_testing`).
What a device will need, and what is unknown:

- The runtime asks for a **Vulkan 1.3** device with `shaderFloat16`, `storageBuffer16BitAccess`,
  `bufferDeviceAddress`, `vulkanMemoryModel` (+DeviceScope) and `scalarBlockLayout`
  (`libxmx.c`, `xmx_open`); the Qt panel enables the same set when it lends its device. Recent
  Adreno and Mali drivers advertise 1.3; older phones will fail `xmx_open` and the filter
  falls back to `kNone` as it does on any failure.
- Without `VK_KHR_cooperative_matrix` — the normal case on a phone — every GEMM runs on
  `gemm_portable*.comp`, the path MoltenVK takes. The shaders are `local_size_x = 32` and
  the resident kernels use `subgroupBarrier()`; they were measured with a subgroup of 32
  (Xe2, Apple). A 64-wide Adreno wave or a 16-wide Mali subgroup is untested. `phase67`
  says what the portable contract is; whether a mobile driver honours it is a measurement
  nobody has made.
- Memory: at the filter's ~320x320 network extent the device buffers are small; the model
  itself is 292 MB of weights mapped from the APK's `.so`, plus the FP16 upload.
- Speed: the M3 does ~230-320 ms a pass through the portable GEMM at 480-570 GFLOP/s. A phone
  GPU is several times slower; expect one pass a second or worse. The filter is asynchronous
  (latest frame wins), so the emulator's own frame rate is unaffected.
- The APK grows by the embedded weights. `-DNR_EMBED_WEIGHTS=OFF` builds an archive whose
  `nr_frame_open(NULL)` refuses, and the filter has no path-loading route, so on Android the
  embedded form is the only one that works today.

#!/usr/bin/env bash
#
# Build the VisualBoyAdvance-M wxWidgets frontend (src/wx, on the wxQt port) for
# Android and package it into an APK with Qt-for-Android + androiddeployqt.
#
# Dependencies come from a vcpkg tree for the Android triplet (Qt 6 for
# Android, a static wxQt, SDL3, ffmpeg, FAudio, Lua ...), with the host Qt
# (androiddeployqt, moc, rcc) taken from the host triplet of the same tree; the
# NDK's own CMake toolchain file is used and the repository's vcpkg glue
# (cmake/Set-Toolchain-vcpkg.cmake) chains it. src/wx/CMakeLists.txt picks the
# triplet's wx-config up by itself (tools/wxwidgets/wx-config).
#
# ENABLE_VULKAN=ON builds the wx Vulkan renderer (VK_KHR_android_surface,
# libvulkan.so resolved at run time) and brings third_party/dlss-nr-on-vulkan
# into the build: libdlssnr.a (the DLSS NR compute runtime, frame library,
# embedded weights and shaders) is linked into the app module, so the APK grows
# by the ~290 MB of weights. Pass -DENABLE_VULKAN=OFF as an extra argument to
# leave both out.
#
# This is the wx counterpart of build-android-qt.sh; override any of the paths
# below via the environment before running, and pass further -D options as
# arguments.

set -euo pipefail

# --- Configuration ----------------------------------------------------------

VCPKG_ROOT="${VCPKG_ROOT:-$HOME/Downloads/vcpkg}"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/29.0.14206865}"

BUILD_TYPE="${BUILD_TYPE:-Release}"
ABI="${ABI:-arm64-v8a}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-28}"

case "$ABI" in
    arm64-v8a)   TRIPLET="${TRIPLET:-arm64-android}" ;;
    armeabi-v7a) TRIPLET="${TRIPLET:-arm-neon-android}" ;;
    x86_64)      TRIPLET="${TRIPLET:-x64-android}" ;;
    x86)         TRIPLET="${TRIPLET:-x86-android}" ;;
    riscv64)     TRIPLET="${TRIPLET:-riscv64-android}" ;;
    *) echo "error: unknown ABI $ABI" >&2; exit 1 ;;
esac

# Host triplet for the Qt host tools (androiddeployqt, moc, rcc ...).
case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)  HOST_TRIPLET="${HOST_TRIPLET:-arm64-osx}" ;;
    Darwin-x86_64) HOST_TRIPLET="${HOST_TRIPLET:-x64-osx}" ;;
    Linux-x86_64)  HOST_TRIPLET="${HOST_TRIPLET:-x64-linux}" ;;
    Linux-aarch64) HOST_TRIPLET="${HOST_TRIPLET:-arm64-linux}" ;;
    *)             HOST_TRIPLET="${HOST_TRIPLET:-arm64-osx}" ;;
esac

# Resolve the repository root (two levels up from tools/android).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$SRC_DIR/build-android-$ABI}"

TARGET_PREFIX="$VCPKG_ROOT/installed/$TRIPLET"
HOST_PREFIX="$VCPKG_ROOT/installed/$HOST_TRIPLET"
NDK_TOOLCHAIN="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"

# --- Sanity checks -----------------------------------------------------------

for path in "$TARGET_PREFIX/share/Qt6/Qt6Config.cmake" \
            "$TARGET_PREFIX/tools/wxwidgets/wx-config" \
            "$HOST_PREFIX/tools/Qt6/bin/androiddeployqt" \
            "$NDK_TOOLCHAIN"; do
    if [ ! -e "$path" ]; then
        echo "error: not found: $path" >&2
        exit 1
    fi
done

export ANDROID_SDK_ROOT ANDROID_NDK_ROOT VCPKG_ROOT

# --- Configure ---------------------------------------------------------------

# The NDK toolchain sets ANDROID and the ABI; the vcpkg triplet tells the
# repository's Set-Toolchain-vcpkg.cmake which installed tree to use (it
# chainloads the NDK file). QT_HOST_PATH points androiddeployqt and the host
# tools at the host Qt of the same vcpkg tree.
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK_TOOLCHAIN" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
    -DNO_VCPKG_UPDATES=ON \
    -DCMAKE_PREFIX_PATH="$TARGET_PREFIX" \
    -DQT_HOST_PATH="$HOST_PREFIX" \
    -DENABLE_WX=ON \
    -DENABLE_QT=OFF \
    -DENABLE_SDL=OFF \
    -DENABLE_LIBRETRO=OFF \
    -DENABLE_SDL3=ON \
    -DENABLE_GLES=ON \
    -DENABLE_VULKAN=ON \
    -DENABLE_AAUDIO=ON \
    -DENABLE_ONLINEUPDATES=OFF \
    -DENABLE_DEBUGGER=OFF \
    -DENABLE_LINK=ON \
    -DBUILD_TESTING=OFF \
    "$@"

# --- Build + package ---------------------------------------------------------

# The 'apk' target is created by Qt's qt_add_executable finalizer; it compiles
# the shared module and runs androiddeployqt to emit the APK, which the
# vbam-android-apk target then copies (or, with UPSTREAM_RELEASE, signs) to the
# top of the build directory as visualboyadvance-m-<ARCH>.apk.
cmake --build "$BUILD_DIR" --target apk -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cmake --build "$BUILD_DIR" --target vbam-android-apk

echo
echo "Done. APK(s) under:"
find "$BUILD_DIR" -maxdepth 1 -name '*.apk' -print 2>/dev/null || true

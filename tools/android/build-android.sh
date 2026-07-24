#!/usr/bin/env bash
#
# Build the VisualBoyAdvance-M wxQt frontend for Android (arm64-v8a) and
# package it into an APK using Qt-for-Android + androiddeployqt.
#
# Prerequisites (already present on the reference machine):
#   - Qt-for-Android with the wxQt (wxWidgets) build installed in its prefix:
#       QT_ANDROID_PREFIX/bin/{qt-cmake,wx-config}
#   - Host Qt (provides androiddeployqt), auto-located by qt-cmake.
#   - Android SDK + NDK. SDL3 is picked up from the NDK sysroot automatically.
#
# Override any of the paths below via the environment before running.

set -euo pipefail

# --- Configuration ----------------------------------------------------------

QT_ANDROID_PREFIX="${QT_ANDROID_PREFIX:-/usr/local/qt-6.9.3-android-arm64}"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"
ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/29.0.14206865}"

BUILD_TYPE="${BUILD_TYPE:-Debug}"

# Resolve the repository root (two levels up from tools/android).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$SRC_DIR/build-android}"

QT_CMAKE="$QT_ANDROID_PREFIX/bin/qt-cmake"
WX_CONFIG="$QT_ANDROID_PREFIX/bin/wx-config"

# --- Sanity checks -----------------------------------------------------------

for path in "$QT_CMAKE" "$WX_CONFIG"; do
    if [ ! -x "$path" ]; then
        echo "error: not found or not executable: $path" >&2
        exit 1
    fi
done

if [ ! -d "$ANDROID_NDK_ROOT" ]; then
    echo "error: ANDROID_NDK_ROOT does not exist: $ANDROID_NDK_ROOT" >&2
    exit 1
fi

export ANDROID_SDK_ROOT ANDROID_NDK_ROOT

# --- Configure ---------------------------------------------------------------

# qt-cmake injects Qt's android toolchain file (which chains the NDK's
# android.toolchain.cmake), so ANDROID and the arm64 ABI are set for us.
"$QT_CMAKE" -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DwxWidgets_CONFIG_EXECUTABLE="$WX_CONFIG" \
    -DENABLE_SDL3=ON \
    -DENABLE_FFMPEG=OFF \
    -DENABLE_LUA=OFF \
    -DENABLE_ONLINEUPDATES=OFF \
    -DENABLE_DEBUGGER=OFF \
    -DENABLE_VULKAN=OFF \
    -DENABLE_LINK=OFF \
    -DBUILD_TESTING=OFF

# --- Build + package ---------------------------------------------------------

# The 'apk' target is created by Qt's qt_add_executable finalizer; it compiles
# the shared library and runs androiddeployqt to emit the APK.
cmake --build "$BUILD_DIR" --target apk -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo
echo "Done. APK(s) under:"
find "$BUILD_DIR" -name '*.apk' -print 2>/dev/null || true

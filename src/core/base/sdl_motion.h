// SDL gamepad motion-sensor → GBA tilt-sensor bridge.
//
// SDL2 (>= 2.0.14) and SDL3 both expose accelerometer and gyroscope
// data for controllers that ship with sensors — DualSense (PS5),
// DualShock 4 (PS4), Joy-Con, Switch Pro Controller, and any HID
// device that SDL's hidapi-backed driver recognizes. This module
// converts that data into the integer ranges the GBA emulator core
// expects from `systemGetSensorX/Y/Z`:
//
//     X tilt:  1897..2197, centered at 2047 (12-bit ADXL193 range)
//     Y tilt:  1897..2197
//     Z (gyro yaw rate): −1800..+1800 (raw before the /10 divide)
//
// X and Y are derived from the accelerometer's gravity vector (the
// real GBA tilt cart used an ADXL accel and exposed the same axes).
// Z is derived from the gyroscope's yaw axis (WarioWare Twisted!'s
// real cart did the same).
//
// The module is intentionally tiny and self-contained — frontends
// (wx, sdl, libretro-with-sdl) attach a single SDL_GameController /
// SDL_Gamepad pointer and then poll once per frame. When no
// sensor-capable pad is attached, IsActive() returns false and the
// frontend should fall back to its existing keyboard / mouse / analog
// stick tilt mapping.
//
// Both SDL2 and SDL3 are supported via the same symbol — the
// `ENABLE_SDL3` define (set by CMake when the SDL3 dev package is
// found) flips the underlying API call, but the public C++ surface
// stays identical so frontend code doesn't need to fork.

#ifndef VBAM_CORE_BASE_SDL_MOTION_H_
#define VBAM_CORE_BASE_SDL_MOTION_H_

#include <cstdint>

namespace vbam {
namespace core {

// Forward-declare so callers don't need to include SDL.h here.
struct SdlMotionImpl;

class SdlMotion {
public:
    // Constants matching the existing tilt-sensor code in
    // wx/sys.cpp / sdl/inputSDL.cpp / libretro/libretro.cpp.
    static constexpr int kTiltCenter = 2047;
    static constexpr int kTiltMin    = 1897;
    static constexpr int kTiltMax    = 2197;
    static constexpr int kGyroMax    =  1800;
    static constexpr int kGyroMin    = -1800;

    SdlMotion();
    ~SdlMotion();

    SdlMotion(const SdlMotion&)            = delete;
    SdlMotion& operator=(const SdlMotion&) = delete;

    static SdlMotion& Instance();

    // Attach an opened gamepad. Pass an `SDL_GameController*` (SDL2)
    // or `SDL_Gamepad*` (SDL3) — accepted as an opaque void* so the
    // header doesn't pull in SDL. If the pad has accel and/or gyro
    // sensors, this enables them and starts caching reads. Calling
    // Attach with a different pointer detaches the previous one;
    // passing nullptr detaches.
    //
    // Returns true if at least one sensor was successfully enabled.
    bool Attach(void* sdl_gamepad);

    // Drop the current pad without enabling another. Idempotent.
    void Detach();

    // Polls sensor data from the attached pad and updates the cached
    // tilt/gyro values. Call once per emulated frame (or whenever
    // systemUpdateMotionSensor is called).
    void Poll();

    // True iff Attach() succeeded *and* at least one sensor (accel or
    // gyro) is producing data. Frontends use this as the "switch"
    // between gyro-tilt and the legacy kb/mouse fallback.
    bool IsActive() const;
    bool HasAccel() const;
    bool HasGyro()  const;

    // Tilt readings in GBA units (matching kTilt* constants).
    int  TiltX() const;
    int  TiltY() const;

    // Gyro yaw-rate reading in GBA units (kGyro* range, *before*
    // libretro/wx's /10 divide for systemGetSensorZ).
    int  GyroZ() const;

    // Sensitivity multipliers (1.0 = default). Frontends can let
    // users adjust how much real-world motion translates into a
    // full-scale GBA tilt — the same idea as libretro's
    // tilt_sensitivity / gyro_sensitivity options.
    void SetTiltSensitivity(float scale);
    void SetGyroSensitivity(float scale);

    // Set a deadband on the accelerometer in m/s² (anything below the
    // threshold reads as "level"). Useful to filter handheld
    // controller jitter when the user is just holding the pad still.
    void SetAccelDeadband(float threshold_m_s2);

private:
    SdlMotionImpl* impl_;
};

}  // namespace core
}  // namespace vbam

#endif  // VBAM_CORE_BASE_SDL_MOTION_H_

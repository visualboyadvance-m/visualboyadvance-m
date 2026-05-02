// SDL gamepad accelerometer/gyro → GBA tilt-sensor bridge.
// See sdl_motion.h for the design overview.

#include "core/base/sdl_motion.h"

#ifdef ENABLE_SDL3
#  include <SDL3/SDL.h>
#else
#  include <SDL.h>
#endif

#include <algorithm>
#include <cmath>

namespace vbam {
namespace core {

namespace {

// SDL exposes accel data in m/s², with gravity ≈ 9.80665 along the
// axis pointing "down" relative to the controller. The GBA's ADXL193
// accelerometer emits 12-bit values centered at 2047, with
// (kTiltMax-kTiltCenter)=150 corresponding to roughly 1g of tilt-axis
// gravity. So 1.0 (m/s²) of gravity → 150 / 9.80665 ≈ 15.3 GBA units.
// We then scale by the user-configurable tilt-sensitivity multiplier.
constexpr float kGravity = 9.80665f;
constexpr float kAccelToTilt =
    static_cast<float>(SdlMotion::kTiltMax - SdlMotion::kTiltCenter) / kGravity;

// SDL gyro data is in rad/s. Real GBA gyro carts (WarioWare Twisted)
// see ±1800 GBA units ≈ ±1500°/s on the cart's gyro. So 1 rad/s ≈
// 1800 / (1500 * π/180) ≈ 68.75 GBA units.
constexpr float kGyroToZ = 1800.0f / (1500.0f * 3.14159265f / 180.0f);

// SDL2 vs SDL3 typedef shim. Both versions of SDL keep the
// accelerometer / gyro enums under the same names; just the gamepad
// pointer type and accessor names differ.
#ifdef ENABLE_SDL3
using SdlPad = SDL_Gamepad;
#else
using SdlPad = SDL_GameController;
#endif

inline bool PadHasSensor(SdlPad* pad, SDL_SensorType type) {
    if (!pad) return false;
#ifdef ENABLE_SDL3
    return SDL_GamepadHasSensor(pad, type);
#else
    return SDL_GameControllerHasSensor(pad, type) == SDL_TRUE;
#endif
}

inline bool PadEnableSensor(SdlPad* pad, SDL_SensorType type, bool on) {
    if (!pad) return false;
#ifdef ENABLE_SDL3
    return SDL_SetGamepadSensorEnabled(pad, type, on);
#else
    return SDL_GameControllerSetSensorEnabled(
               pad, type, on ? SDL_TRUE : SDL_FALSE) == 0;
#endif
}

// Read a 3-axis sample. Returns false if the pad doesn't support the
// sensor or the read failed; out is left untouched in that case.
inline bool PadReadSensor(SdlPad* pad, SDL_SensorType type, float out[3]) {
    if (!pad) return false;
#ifdef ENABLE_SDL3
    return SDL_GetGamepadSensorData(pad, type, out, 3);
#else
    return SDL_GameControllerGetSensorData(pad, type, out, 3) == 0;
#endif
}

// SDL3 dropped the SDL_INIT_GAMECONTROLLER flag in favor of
// SDL_INIT_GAMEPAD; both can be re-initialized cheaply (idempotent).
inline void EnsureSdlSubsystem() {
#ifdef ENABLE_SDL3
    SDL_InitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_SENSOR);
#else
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR);
#endif
}

}  // namespace

struct SdlMotionImpl {
    SdlPad* pad = nullptr;
    bool    has_accel = false;
    bool    has_gyro  = false;

    // Cached, in GBA units.
    int tilt_x = SdlMotion::kTiltCenter;
    int tilt_y = SdlMotion::kTiltCenter;
    int gyro_z = 0;

    // User-tunable. Defaults match the existing kb-tilt feel.
    float tilt_scale     = 1.0f;
    float gyro_scale     = 1.0f;
    float accel_deadband = 0.20f;   // m/s², roughly 0.02 g.
};

SdlMotion::SdlMotion() : impl_(new SdlMotionImpl) {
    EnsureSdlSubsystem();
}

SdlMotion::~SdlMotion() {
    Detach();
    delete impl_;
}

SdlMotion& SdlMotion::Instance() {
    static SdlMotion s_instance;
    return s_instance;
}

bool SdlMotion::Attach(void* sdl_gamepad) {
    if (!impl_) return false;
    if (impl_->pad == sdl_gamepad) return impl_->has_accel || impl_->has_gyro;
    Detach();
    if (!sdl_gamepad) return false;

    SdlPad* pad = static_cast<SdlPad*>(sdl_gamepad);
    impl_->has_accel = PadHasSensor(pad, SDL_SENSOR_ACCEL);
    impl_->has_gyro  = PadHasSensor(pad, SDL_SENSOR_GYRO);
    if (!impl_->has_accel && !impl_->has_gyro) {
        // Pad has no motion sensors — nothing to do.
        return false;
    }
    if (impl_->has_accel) PadEnableSensor(pad, SDL_SENSOR_ACCEL, true);
    if (impl_->has_gyro)  PadEnableSensor(pad, SDL_SENSOR_GYRO,  true);
    impl_->pad = pad;
    // Re-center the cached values; whatever the previous pad reported
    // shouldn't bleed into the new one.
    impl_->tilt_x = kTiltCenter;
    impl_->tilt_y = kTiltCenter;
    impl_->gyro_z = 0;
    return true;
}

void SdlMotion::Detach() {
    if (!impl_ || !impl_->pad) return;
    if (impl_->has_accel) PadEnableSensor(impl_->pad, SDL_SENSOR_ACCEL, false);
    if (impl_->has_gyro)  PadEnableSensor(impl_->pad, SDL_SENSOR_GYRO,  false);
    impl_->pad = nullptr;
    impl_->has_accel = impl_->has_gyro = false;
    impl_->tilt_x = kTiltCenter;
    impl_->tilt_y = kTiltCenter;
    impl_->gyro_z = 0;
}

void SdlMotion::Poll() {
    if (!impl_ || !impl_->pad) return;

    if (impl_->has_accel) {
        float a[3] = { 0, 0, 0 };
        if (PadReadSensor(impl_->pad, SDL_SENSOR_ACCEL, a)) {
            // Apply deadband around 0 on each tilt axis.
            const float dz = impl_->accel_deadband;
            float ax = (std::fabs(a[0]) > dz) ? a[0] : 0.f;
            // SDL's convention for handheld accel: Y is up (+gravity
            // when level, face-up). The GBA's "tilt forward" produces
            // the *same sign* on the cart's vertical axis, so flip Y
            // so a controller tilted forward → GBA tilts forward.
            float ay = (std::fabs(a[2]) > dz) ? a[2] : 0.f;

            // Scale m/s² → GBA tilt units, applying user sensitivity.
            int x = static_cast<int>(ax * kAccelToTilt * impl_->tilt_scale)
                    + kTiltCenter;
            int y = static_cast<int>(ay * kAccelToTilt * impl_->tilt_scale)
                    + kTiltCenter;
            impl_->tilt_x = std::clamp(x, kTiltMin, kTiltMax);
            impl_->tilt_y = std::clamp(y, kTiltMin, kTiltMax);
        }
    }

    if (impl_->has_gyro) {
        float g[3] = { 0, 0, 0 };
        if (PadReadSensor(impl_->pad, SDL_SENSOR_GYRO, g)) {
            // Yaw axis is the controller's "Y" in SDL's convention
            // (the axis pointing up out of the face buttons).
            float gz = g[1] * impl_->gyro_scale * kGyroToZ;
            impl_->gyro_z = std::clamp(static_cast<int>(gz),
                                       kGyroMin, kGyroMax);
        }
    }
}

bool SdlMotion::IsActive() const {
    return impl_ && impl_->pad && (impl_->has_accel || impl_->has_gyro);
}
bool SdlMotion::HasAccel() const { return impl_ && impl_->has_accel; }
bool SdlMotion::HasGyro()  const { return impl_ && impl_->has_gyro;  }
int  SdlMotion::TiltX()    const { return impl_ ? impl_->tilt_x : kTiltCenter; }
int  SdlMotion::TiltY()    const { return impl_ ? impl_->tilt_y : kTiltCenter; }
int  SdlMotion::GyroZ()    const { return impl_ ? impl_->gyro_z : 0; }

void SdlMotion::SetTiltSensitivity(float scale) {
    if (impl_) impl_->tilt_scale = scale > 0.f ? scale : 1.f;
}
void SdlMotion::SetGyroSensitivity(float scale) {
    if (impl_) impl_->gyro_scale = scale > 0.f ? scale : 1.f;
}
void SdlMotion::SetAccelDeadband(float threshold_m_s2) {
    if (impl_) impl_->accel_deadband =
        threshold_m_s2 >= 0.f ? threshold_m_s2 : 0.f;
}

}  // namespace core
}  // namespace vbam

// ======================================================================
// \title  AdcsLogic.hpp
// \brief  ADCS sensor checks, control laws, coil allocation and mode rules
//
// Pure functions with no F Prime dependencies, so every rule can be unit
// tested on its own. AdcsManager owns the I/O and calls these to decide.
// All vectors are in the body frame. Arithmetic is single precision.
// ======================================================================

#ifndef project_AdcsLogic_HPP
#define project_AdcsLogic_HPP

#include <stdint.h>

#include <cmath>

namespace project {
namespace AdcsLogic {

// ----------------------------------------------------------------------
// Types
//
// The enum values are the ones reported in telemetry, so they match the
// FPP enums in AdcsManager.fpp one for one.
// ----------------------------------------------------------------------

struct Vec3 {
    float x;
    float y;
    float z;
};

//! Result of a sensor reading's plausibility check.
enum class Status : uint8_t { OK = 0, GYRO_FAIL = 21, MAG_FAIL = 31 };

//! ADCS mode, from the angular rate.
enum class Mode : uint8_t { TUMBLING = 0, STABLE = 1, SUN_POINTING = 2, ACS_OFF = 3, VF_TUMBLING = 4 };

//! Control law. Sun pointing needs the sun vector, which this task does not
//! compute yet, so only the two detumbling laws are supported.
enum class Controller : uint8_t { BDOT = 0, BCROSS = 1 };

//! Magnetorquer coils, in allocation order: axis n owns coils 2n and 2n+1.
constexpr uint32_t COIL_COUNT = 6;

// ----------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------

//! Detumbling gain for both B-dot and B-cross.
constexpr float DETUMB_GAIN = 1.0e05F;

//! Plausibility limits. The magnetometer saturates near 1.3 mT (X, Y) and
//! 2.5 mT (Z); the field in orbit is about 40 uT. The gyroscope's widest
//! range is 2000 deg/s, so a faster reading is a fault.
constexpr float MIN_MAG_NORM_T = 1.0e-6F;
constexpr float MAX_MAG_NORM_T = 2.5e-3F;
constexpr float MAX_GYRO_NORM_RAD_S = 2.0e3F * 3.14159265358979F / 180.0F;

//! Angular rate thresholds for the mode rules, in rad/s.
constexpr float VF_TUMBLING_TOL_BDOT = 2.62F;  // 150 deg/s
constexpr float VF_TUMBLING_TOL = 3.05F;       // 175 deg/s
constexpr float TUMBLING_TOL = 0.54F;          // 30 deg/s
constexpr float DETUMBLED_TOL_LO = 0.070F;     // 4 deg/s
constexpr float DETUMBLED_TOL_HI = 0.087F;     // 5 deg/s

//! B-dot sample count, and the weights of the least-squares slope through
//! six evenly spaced samples (oldest first), before dividing by the spacing.
constexpr uint32_t BDOT_SAMPLES = 6;
constexpr float BDOT_WEIGHTS[BDOT_SAMPLES] = {-5.0F / 35.0F, -3.0F / 35.0F, -1.0F / 35.0F,
                                              1.0F / 35.0F,  3.0F / 35.0F,  5.0F / 35.0F};

// ----------------------------------------------------------------------
// Vector helpers
// ----------------------------------------------------------------------

inline float norm(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Vec3 scaled(const Vec3& v, float k) {
    return {v.x * k, v.y * k, v.z * k};
}

constexpr Vec3 ZERO = {0.0F, 0.0F, 0.0F};

// ----------------------------------------------------------------------
// Sensor checks
// ----------------------------------------------------------------------

//! Accept a gyroscope reading (rad/s) no faster than MAX_GYRO_NORM_RAD_S.
//! A rejected reading, including NaN, becomes zero with GYRO_FAIL.
inline Status checkGyro(const Vec3& raw, Vec3& out) {
    // Written as !(a <= b) so that NaN is rejected too.
    if (!(norm(raw) <= MAX_GYRO_NORM_RAD_S)) {
        out = ZERO;
        return Status::GYRO_FAIL;
    }
    out = raw;
    return Status::OK;
}

//! Convert a magnetometer reading from uT to T, remove the hard-iron bias
//! and apply the per-axis scale: (raw * 1e-6 - bias) * scale. A result
//! outside MIN_MAG_NORM_T..MAX_MAG_NORM_T becomes zero with MAG_FAIL.
inline Status correctMag(const Vec3& rawUt, const Vec3& biasT, const Vec3& scale, Vec3& outT) {
    const Vec3 field = {(rawUt.x * 1.0e-6F - biasT.x) * scale.x, (rawUt.y * 1.0e-6F - biasT.y) * scale.y,
                        (rawUt.z * 1.0e-6F - biasT.z) * scale.z};
    const float n = norm(field);
    if (!(n >= MIN_MAG_NORM_T && n <= MAX_MAG_NORM_T)) {
        outT = ZERO;
        return Status::MAG_FAIL;
    }
    outT = field;
    return Status::OK;
}

// ----------------------------------------------------------------------
// Control laws
//
// Each returns a dipole command bounded smoothly below 1 in magnitude:
// u / sqrt(|u|^2 + 1).
// ----------------------------------------------------------------------

inline Vec3 softClip(const Vec3& u) {
    const float n = norm(u);
    return scaled(u, 1.0F / std::sqrt(n * n + 1.0F));
}

//! B-dot: oppose the rate of change of the field, estimated from
//! BDOT_SAMPLES samples `dtS` seconds apart (oldest first).
inline Vec3 bdot(const Vec3 samples[BDOT_SAMPLES], float dtS) {
    if (!(dtS > 0.0F)) {
        return ZERO;
    }
    Vec3 rate = ZERO;
    for (uint32_t k = 0; k < BDOT_SAMPLES; k++) {
        rate.x += BDOT_WEIGHTS[k] * samples[k].x;
        rate.y += BDOT_WEIGHTS[k] * samples[k].y;
        rate.z += BDOT_WEIGHTS[k] * samples[k].z;
    }
    return softClip(scaled(rate, -DETUMB_GAIN / dtS));
}

//! B-cross: u = -k (B x omega), from the field (T) and the rate (rad/s).
inline Vec3 bcross(const Vec3& field, const Vec3& omega) {
    return softClip(scaled(cross(field, omega), -DETUMB_GAIN));
}

// ----------------------------------------------------------------------
// Coil allocation
// ----------------------------------------------------------------------

//! Split a dipole command across the six coils.
//!
//! If both coils of exactly one axis are unavailable, the command's
//! component along that axis is removed by projecting it along the field
//! b, which leaves the torque u x b unchanged. The command is then scaled
//! down if the resulting coil effort exceeds the power limit, and each axis
//! is shared equally between its available coils. An unavailable coil gets
//! 0 and must not be commanded.
inline void allocate(const Vec3& u, const Vec3& b, const bool available[COIL_COUNT], float throttle[COIL_COUNT]) {
    // One fully failed axis is compensated; with two or more, none is.
    int32_t failedAxis = -1;
    for (int32_t axis = 0; axis < 3; axis++) {
        if (!available[2 * axis] && !available[2 * axis + 1]) {
            if (failedAxis < 0) {
                failedAxis = axis;
            } else {
                failedAxis = -2;
                break;
            }
        }
    }

    float effective[3] = {u.x, u.y, u.z};
    const float field[3] = {b.x, b.y, b.z};
    if (failedAxis >= 0 && std::fabs(field[failedAxis]) > 1e-9F) {
        const float factor = effective[failedAxis] / field[failedAxis];
        for (int32_t axis = 0; axis < 3; axis++) {
            effective[axis] -= factor * field[axis];
        }
    }

    // Coil effort: an axis split over two coils puts half on each.
    float normSquared = 0.0F;
    for (int32_t axis = 0; axis < 3; axis++) {
        const float value = effective[axis];
        if (available[2 * axis] && available[2 * axis + 1]) {
            normSquared += 0.5F * value * value;
        } else if (available[2 * axis] || available[2 * axis + 1]) {
            normSquared += value * value;
        }
    }
    const float limit = std::sqrt(normSquared) * 0.707F;
    const float scale = (limit > 1.0F) ? limit : 1.0F;

    for (int32_t axis = 0; axis < 3; axis++) {
        const bool plus = available[2 * axis];
        const bool minus = available[2 * axis + 1];
        const float value = effective[axis] / scale;
        throttle[2 * axis] = plus ? ((plus && minus) ? 0.5F * value : value) : 0.0F;
        throttle[2 * axis + 1] = minus ? ((plus && minus) ? 0.5F * value : value) : 0.0F;
    }
}

// ----------------------------------------------------------------------
// Mode rules
// ----------------------------------------------------------------------

//! Next mode from the current one and the angular rate, with hysteresis
//! between each pair of neighbouring modes. A failed gyroscope reading
//! forces STABLE. Very fast tumbling is entered sooner under B-dot, which
//! tolerates less rate than B-cross.
inline Mode nextMode(Mode current, Controller controller, Status gyroStatus, const Vec3& omega) {
    if (gyroStatus != Status::OK) {
        return Mode::STABLE;
    }
    const float rate = norm(omega);
    const float vfTolerance = (controller == Controller::BDOT) ? VF_TUMBLING_TOL_BDOT : VF_TUMBLING_TOL;

    if (current == Mode::VF_TUMBLING) {
        return (rate < vfTolerance) ? Mode::TUMBLING : Mode::VF_TUMBLING;
    }
    if (current == Mode::TUMBLING) {
        if (rate <= TUMBLING_TOL) {
            return Mode::STABLE;
        }
        return (rate >= vfTolerance) ? Mode::VF_TUMBLING : Mode::TUMBLING;
    }
    if (rate >= TUMBLING_TOL) {
        return Mode::TUMBLING;
    }
    if (current == Mode::STABLE) {
        return (rate < DETUMBLED_TOL_LO) ? Mode::ACS_OFF : Mode::STABLE;
    }
    // ACS_OFF, or SUN_POINTING left over from a sun-pointing controller.
    return (rate >= DETUMBLED_TOL_HI) ? Mode::STABLE : Mode::ACS_OFF;
}

}  // namespace AdcsLogic
}  // namespace project

#endif

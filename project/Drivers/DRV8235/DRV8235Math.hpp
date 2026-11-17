#ifndef project_DRV8235_MATH_HPP
#define project_DRV8235_MATH_HPP

#include <stdint.h>

namespace project {
namespace DRV8235Math {

// Register map, from flight/hal/drivers/drv8235.py.
constexpr uint8_t FAULT_STATUS = 0x00;  // Fault flags (read only)
constexpr uint8_t REG_STATUS1 = 0x04;   // VMTR: voltage across the coil (read only)
constexpr uint8_t REG_STATUS2 = 0x05;   // IMTR: current through the coil (read only)
constexpr uint8_t CONFIG0 = 0x09;
constexpr uint8_t CONFIG3 = 0x0C;
constexpr uint8_t CONFIG4 = 0x0D;
constexpr uint8_t REG_CTRL0 = 0x0E;
constexpr uint8_t REG_CTRL1 = 0x0F;  // WSET_VSET: target coil voltage
constexpr uint8_t RC_CTRL2 = 0x13;
constexpr uint8_t RC_CTRL3 = 0x14;

// H-bridge state in CONFIG4 bits 1:0 (IN2 IN1).
constexpr uint8_t BRIDGE_MASK = 0x03;
constexpr uint8_t BRIDGE_COAST = 0x00;
constexpr uint8_t BRIDGE_REVERSE = 0x01;
constexpr uint8_t BRIDGE_FORWARD = 0x02;
constexpr uint8_t BRIDGE_BRAKE = 0x03;

// FAULT_STATUS bits. FAULT is set whenever any of the others is.
constexpr uint8_t FAULT_ANY = 0x80;
constexpr uint8_t FAULT_STALL = 0x20;
constexpr uint8_t FAULT_OCP = 0x10;   // Overcurrent
constexpr uint8_t FAULT_OVP = 0x08;   // Overvoltage
constexpr uint8_t FAULT_TSD = 0x04;   // Overtemperature
constexpr uint8_t FAULT_NPOR = 0x02;  // Undervoltage lockout

//! One bit field to set: the bits in `mask` take the matching bits of `value`.
struct Field {
    uint8_t reg;
    uint8_t mask;
    uint8_t value;
};

//! DRV8235.__init__ in Python, in the same order. Each step is a
//! read-modify-write except where the mask covers the whole register.
constexpr Field INIT_SEQUENCE[] = {
    {CONFIG4, 0x04, 0x04},    // I2C_BC: bridge controlled over I2C
    {CONFIG4, 0x08, 0x08},    // PMODE: PWM programming mode
    {CONFIG4, 0x03, 0x00},    // Bridge to COAST
    {REG_CTRL0, 0x18, 0x18},  // REG_CTRL = 0b11: voltage regulation
    {REG_CTRL1, 0xFF, 0x00},  // WSET_VSET = 0 V
    {CONFIG3, 0x10, 0x10},    // INT_VREF
    {RC_CTRL2, 0xC0, 0x80},   // INV_R_SCALE = 0b10
    {RC_CTRL3, 0xFF, 41},     // INV_R = 41
    {CONFIG0, 0x80, 0x80},    // EN_OUT: enable the outputs
    {CONFIG0, 0x02, 0x02},    // CLR_FLT: clear fault flags
};
constexpr uint32_t INIT_STEPS = sizeof(INIT_SEQUENCE) / sizeof(INIT_SEQUENCE[0]);

inline uint8_t applyField(uint8_t current, const Field& field) {
    return static_cast<uint8_t>((current & static_cast<uint8_t>(~field.mask)) | (field.value & field.mask));
}

inline bool isReverse(uint8_t config4) {
    return (config4 & BRIDGE_MASK) == BRIDGE_REVERSE;
}

//! VMTR in mV: 42.67 V / 255 = 167.33 mV per count. Negative while driving
//! in reverse, as in Python's __read_voltage. Truncates toward zero, matching
//! int(volts * 1000) in eps.py.
inline int32_t voltageMillivolts(uint8_t code, bool reverse) {
    const int32_t mv = static_cast<int32_t>((static_cast<uint32_t>(code) * 16733U) / 100U);
    return reverse ? -mv : mv;
}

//! IMTR in mA: 3.7 A / 255 = 14.51 mA per count. Same sign rule as voltage.
inline int32_t currentMilliamps(uint8_t code, bool reverse) {
    const int32_t ma = static_cast<int32_t>((static_cast<uint32_t>(code) * 1451U) / 100U);
    return reverse ? -ma : ma;
}

//! What to write to drive the coil: WSET_VSET and the bridge bits.
struct Drive {
    uint8_t vset;
    uint8_t bridge;
};

//! Python set_throttle(), which is what ADCS calls. Throttle is in
//! thousandths (-1000..1000) and is scaled so full throttle is the coil's
//! 6.0 V limit, not the chip's 42.67 V: vset = trunc(t * 6.0/42.67 * 255),
//! which is t * 153/4267 in integers. Not used by EPS; here for ADCS.
inline Drive driveFromThrottle(int32_t throttlePermille) {
    if (throttlePermille > 1000) {
        throttlePermille = 1000;
    }
    if (throttlePermille < -1000) {
        throttlePermille = -1000;
    }
    const int32_t vset = (throttlePermille * 153) / 4267;
    if (vset > 0) {
        return {static_cast<uint8_t>(vset), BRIDGE_FORWARD};
    }
    if (vset < 0) {
        return {static_cast<uint8_t>(-vset), BRIDGE_REVERSE};
    }
    return {0, BRIDGE_BRAKE};
}

}  // namespace DRV8235Math
}  // namespace project

#endif

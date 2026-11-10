// ======================================================================
// \title  EpsLogic.hpp
// \brief  EPS decision rules, ported from flight/apps/eps/eps.py
//
// Pure functions with no F Prime dependencies, so every rule can be unit
// tested on its own. EpsManager owns the I/O and calls these to decide.
// ======================================================================

#ifndef project_EpsLogic_HPP
#define project_EpsLogic_HPP

#include <stdint.h>

namespace project {
namespace EpsLogic {

// ----------------------------------------------------------------------
// Constants (apps/eps/eps.py and tasks/eps.py)
// ----------------------------------------------------------------------

//! EPS_POWER_THRESHOLD, in mW
namespace PowerThresholdMw {
constexpr int64_t MAINBOARD = 1000;
constexpr int64_t PERIPHERAL = 1000;
constexpr int64_t RADIO = 3300;
constexpr int64_t JETSON = 16000;
constexpr int64_t TORQUE_COIL = 1500;
}  // namespace PowerThresholdMw

//! EPS_SOC_THRESHOLD, in percent
constexpr int32_t SOC_LOW_POWER_ENTRY = 30;
constexpr int32_t SOC_LOW_POWER_EXIT = 40;

//! EPS_TEMP_THRESHOLD, in centi-degrees C
constexpr int32_t HEATER_ENABLE_CC = 0;
constexpr int32_t HEATER_DISABLE_CC = 500;

//! MAINBOARD_TEMP_OFFSET (tasks/eps.py). CPU temperature minus this stands
//! in for battery temperature when the fuel gauge is unavailable.
constexpr int32_t MAINBOARD_TEMP_OFFSET_CC = 200;

//! Consecutive failed fuel gauge cycles before the gauge is treated as
//! unavailable. Not in the Python; see updateFuelGauge().
constexpr uint32_t FUEL_GAUGE_UNAVAILABLE_AFTER = 3;

// ----------------------------------------------------------------------
// Power and alerts
// ----------------------------------------------------------------------

//! power_mW = mV * mA * 0.001, truncated toward zero.
inline int64_t powerMilliwatts(int32_t millivolts, int32_t milliamps) {
    return (static_cast<int64_t>(millivolts) * milliamps) / 1000;
}

//! GET_POWER_STATUS: alert when power >= threshold. The Python averages over
//! `frequency` samples, which is 1 for EPS at 1 Hz, so this is one compare.
//! Done as mV * mA >= threshold * 1000 in 64-bit integers, which is exact,
//! where the Python's float multiply by 0.001 can round at the boundary.
inline bool powerAlert(int32_t millivolts, int32_t milliamps, int64_t thresholdMw) {
    return static_cast<int64_t>(millivolts) * milliamps >= thresholdMw * 1000;
}

// ----------------------------------------------------------------------
// EPS power flag
// ----------------------------------------------------------------------

//! EPS_POWER_FLAG. Values match the Python constants.
enum class PowerFlag : uint8_t { NONE = 0, LOW_POWER = 1, NOMINAL = 2 };

//! GET_EPS_POWER_FLAG. Returns NONE when the SOC is outside 0..100. The
//! caller then keeps its current flag and reports an error, as
//! update_eps_state() does.
inline PowerFlag nextPowerFlag(PowerFlag current, int32_t socPercent) {
    if (socPercent < 0 || socPercent > 100) {
        return PowerFlag::NONE;
    }
    if (socPercent <= SOC_LOW_POWER_ENTRY) {
        return PowerFlag::LOW_POWER;
    }
    if (socPercent >= SOC_LOW_POWER_EXIT) {
        return PowerFlag::NOMINAL;
    }
    // Between the thresholds: stay LOW_POWER if already there, else NOMINAL
    return (current == PowerFlag::LOW_POWER) ? PowerFlag::LOW_POWER : PowerFlag::NOMINAL;
}

// ----------------------------------------------------------------------
// Battery heaters
// ----------------------------------------------------------------------

//! SHOULD_ENABLE_HEATERS
inline bool shouldEnableHeater(bool enabled, int32_t temperatureCc) {
    return !enabled && (temperatureCc <= HEATER_ENABLE_CC);
}

//! SHOULD_DISABLE_HEATERS
inline bool shouldDisableHeater(bool enabled, int32_t temperatureCc) {
    return enabled && (temperatureCc >= HEATER_DISABLE_CC);
}

//! What to do with one heater this cycle
enum class HeaterAction : uint8_t { NONE, TURN_ON, TURN_OFF };

//! Both checks for one heater, as set_battery_heaters() runs them. The two
//! conditions cannot both hold, so at most one action results.
inline HeaterAction heaterAction(bool enabled, int32_t temperatureCc) {
    if (shouldEnableHeater(enabled, temperatureCc)) {
        return HeaterAction::TURN_ON;
    }
    if (shouldDisableHeater(enabled, temperatureCc)) {
        return HeaterAction::TURN_OFF;
    }
    return HeaterAction::NONE;
}

//! The temperature each heater decision uses
struct HeaterTemperatures {
    int32_t heater0Cc;
    int32_t heater1Cc;
};

//! set_battery_heaters(): AIN1 drives heater 0 and AIN2 drives heater 1.
//! Without the fuel gauge, both use CPU temperature minus the offset.
inline HeaterTemperatures heaterTemperatures(bool useFuelGauge, int32_t ain1Cc, int32_t ain2Cc, int32_t cpuCc) {
    if (useFuelGauge) {
        return {ain1Cc, ain2Cc};
    }
    const int32_t fallback = cpuCc - MAINBOARD_TEMP_OFFSET_CC;
    return {fallback, fallback};
}

// ----------------------------------------------------------------------
// Fuel gauge availability
// ----------------------------------------------------------------------

//! How a fuel gauge cycle should proceed
enum class FuelGaugeMode : uint8_t {
    USE_FUEL_GAUGE,   //!< Read succeeded: use its values for every decision
    SKIP_CYCLE,       //!< Read failed, gauge recently good: keep last values, decide nothing
    USE_CPU_FALLBACK  //!< Gauge unavailable: no power flag update, heaters on CPU temperature
};

//! Fuel gauge history, kept by EpsManager across cycles
struct FuelGaugeHealth {
    bool everRead = false;             //!< At least one fully successful read
    uint32_t consecutiveFailures = 0;  //!< Failed cycles since the last good one
};

//! Record one fuel gauge cycle and decide how it proceeds.
//!
//! Python never acts on a failed reading: the I2C exception aborts the cycle
//! and the old values stay, so a single failure is a skipped cycle. It falls
//! back to CPU temperature only when the device is unavailable
//! (FUEL_GAUGE_AVAILABLE). Here, unavailable means it has never read
//! successfully, or has failed FUEL_GAUGE_UNAVAILABLE_AFTER cycles in a row.
inline FuelGaugeMode updateFuelGauge(FuelGaugeHealth& health, bool readOk) {
    if (readOk) {
        health.everRead = true;
        health.consecutiveFailures = 0;
        return FuelGaugeMode::USE_FUEL_GAUGE;
    }
    if (health.consecutiveFailures < UINT32_MAX) {
        health.consecutiveFailures++;
    }
    const bool available = health.everRead && (health.consecutiveFailures < FUEL_GAUGE_UNAVAILABLE_AFTER);
    return available ? FuelGaugeMode::SKIP_CYCLE : FuelGaugeMode::USE_CPU_FALLBACK;
}

}  // namespace EpsLogic
}  // namespace project

#endif
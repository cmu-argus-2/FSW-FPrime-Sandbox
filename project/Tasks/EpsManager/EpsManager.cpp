// ======================================================================
// \title  EpsManager.cpp
// \brief  EPS task: electrical power subsystem flight logic
// ======================================================================

#include "project/Tasks/EpsManager/EpsManager.hpp"

namespace project {

namespace {

//! Threshold per power monitor port, in the same order as the ports
const I64 MONITOR_THRESHOLD_MW[EpsManager::NUM_POWER_MONITORS] = {
    EpsLogic::PowerThresholdMw::MAINBOARD,
    EpsLogic::PowerThresholdMw::PERIPHERAL,
    EpsLogic::PowerThresholdMw::RADIO,
    EpsLogic::PowerThresholdMw::JETSON,
};

project::EpsPowerFlag toFpp(EpsLogic::PowerFlag flag) {
    return project::EpsPowerFlag(static_cast<project::EpsPowerFlag::T>(static_cast<U8>(flag)));
}

project::EpsFuelGaugeMode toFpp(EpsLogic::FuelGaugeMode mode) {
    return project::EpsFuelGaugeMode(static_cast<project::EpsFuelGaugeMode::T>(static_cast<U8>(mode)));
}

//! Truncate toward zero, as Python's int() does
I32 truncate(F32 value) {
    return static_cast<I32>(value);
}

U8 asFlag(bool value) {
    return value ? 1 : 0;
}

}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

EpsManager ::EpsManager(const char* const compName)
    : EpsManagerComponentBase(compName),
      m_runCount(0),
      m_alert{},
      m_powerFlag(EpsLogic::PowerFlag::NONE),
      m_fuelGaugeHealth(),
      m_heaterAbsent{},
      m_data() {}

EpsManager ::~EpsManager() {}

// ----------------------------------------------------------------------
// Port handlers
// ----------------------------------------------------------------------

void EpsManager ::run_handler(FwIndexType portNum, U32 context) {
    // Every run: power monitors and alerts
    this->readPowerMonitors();
    this->publishAlerts();

    // tasks/eps.py checks log_counter % 5 before incrementing it, so the very
    // first run already includes the fuel gauge work. Same here.
    if ((m_runCount % FUEL_GAUGE_PERIOD) == 0) {
        this->fuelGaugeCycle();
    }

    m_runCount++;
    this->tlmWrite_RunCount(m_runCount);
}

void EpsManager ::pingIn_handler(FwIndexType portNum, U32 key) {
    // Answering on this thread proves the EPS thread is alive. A wedged I2C
    // bus inside a cycle shows up as missed pings in CdhCore.health.
    this->pingOut_out(0, key);
}

// ----------------------------------------------------------------------
// Every run
// ----------------------------------------------------------------------

void EpsManager ::readPowerMonitors() {
    for (FwIndexType i = 0; i < NUM_POWER_MONITORS; i++) {
        // Zeroed, and ADM1176 leaves it untouched on failure, so a monitor
        // that is absent or failing reports 0 (spec: leave its fields at 0).
        project::PowerSample sample;
        if (this->isConnected_powerMonitors_OutputPort(i)) {
            (void)this->powerMonitors_out(i, sample);
        }

        // Saved every run, published every fuel gauge cycle (spec)
        const project::EpsVi vi(sample.get_voltageMv(), sample.get_currentMa());
        switch (i) {
            case 0:
                m_data.set_mainboard(vi);
                break;
            case 1:
                m_data.set_gps(vi);
                break;
            case 2:
                m_data.set_radio(vi);
                break;
            default:
                m_data.set_jetson(vi);
                break;
        }

        // Monitor port i is load i in EpsLoad
        this->updateAlert(i, sample.get_voltageMv(), sample.get_currentMa(), MONITOR_THRESHOLD_MW[i]);
    }
}

void EpsManager ::updateAlert(FwIndexType load, I32 millivolts, I32 milliamps, I64 thresholdMw) {
    const bool alert = EpsLogic::powerAlert(millivolts, milliamps, thresholdMw);
    const I32 powerMw = static_cast<I32>(EpsLogic::powerMilliwatts(millivolts, milliamps));
    const project::EpsLoad which(static_cast<project::EpsLoad::T>(load));

    // Python logs a warning on every cycle the alert holds. At 1 Hz that
    // floods the downlink, so events mark only the transitions; the alert
    // itself is in the Alerts channel every run.
    if (alert && !m_alert[load]) {
        this->log_WARNING_LO_PowerAlertRaised(which, powerMw, static_cast<I32>(thresholdMw));
    } else if (!alert && m_alert[load]) {
        this->log_ACTIVITY_LO_PowerAlertCleared(which, powerMw);
    }
    m_alert[load] = alert;
}

void EpsManager ::publishAlerts() {
    project::EpsAlerts alerts;
    alerts.set_mainboard(asFlag(m_alert[project::EpsLoad::MAINBOARD]));
    alerts.set_peripheral(asFlag(m_alert[project::EpsLoad::PERIPHERAL]));
    alerts.set_radio(asFlag(m_alert[project::EpsLoad::RADIO]));
    alerts.set_jetson(asFlag(m_alert[project::EpsLoad::JETSON]));
    alerts.set_coilXp(asFlag(m_alert[project::EpsLoad::COIL_XP]));
    alerts.set_coilXm(asFlag(m_alert[project::EpsLoad::COIL_XM]));
    alerts.set_coilYp(asFlag(m_alert[project::EpsLoad::COIL_YP]));
    alerts.set_coilYm(asFlag(m_alert[project::EpsLoad::COIL_YM]));
    alerts.set_coilZp(asFlag(m_alert[project::EpsLoad::COIL_ZP]));
    alerts.set_coilZm(asFlag(m_alert[project::EpsLoad::COIL_ZM]));
    this->tlmWrite_Alerts(alerts);
}

// ----------------------------------------------------------------------
// Every FUEL_GAUGE_PERIOD runs
// ----------------------------------------------------------------------

void EpsManager ::fuelGaugeCycle() {
    // Fuel gauge. A good read is one in which every register read succeeded.
    project::BatteryState state;
    U32 failedReads = 0;
    bool readOk = false;
    if (this->isConnected_fuelGauge_OutputPort(0)) {
        failedReads = this->fuelGauge_out(0, state);
        readOk = (failedReads == 0);
    }
    const EpsLogic::FuelGaugeMode mode = EpsLogic::updateFuelGauge(m_fuelGaugeHealth, readOk);

    // CPU temperature: field 2, and the heater fallback
    I32 cpuCc = 0;
    const bool cpuOk = this->isConnected_cpuTemp_OutputPort(0) && this->cpuTemp_out(0, cpuCc);
    m_data.set_mainboardTempCc(cpuOk ? cpuCc : 0);

    switch (mode) {
        case EpsLogic::FuelGaugeMode::USE_FUEL_GAUGE:
            this->storeFuelGauge(state);
            this->updatePowerFlag(m_data.get_socPercent());
            this->controlHeaters(
                EpsLogic::heaterTemperatures(true, m_data.get_ain1TempCc(), m_data.get_ain2TempCc(), 0));
            break;

        case EpsLogic::FuelGaugeMode::SKIP_CYCLE:
            // Python's I2C exception aborts this part of the cycle. Keep the
            // last good values and make no decisions on a failed reading.
            this->log_WARNING_LO_FuelGaugeDegraded(toFpp(mode), failedReads);
            break;

        case EpsLogic::FuelGaugeMode::USE_CPU_FALLBACK:
            this->log_WARNING_LO_FuelGaugeDegraded(toFpp(mode), failedReads);
            // No SOC, so the power flag is left alone. Heaters use CPU
            // temperature minus the offset, but only if there is a CPU
            // temperature: without one there is nothing safe to act on.
            if (cpuOk) {
                this->controlHeaters(EpsLogic::heaterTemperatures(false, 0, 0, cpuCc));
            }
            break;
    }

    m_data.set_powerFlag(toFpp(m_powerFlag));
    m_data.set_heater0(this->heaterState(project::HeaterId::HEATER0));
    m_data.set_heater1(this->heaterState(project::HeaterId::HEATER1));
    this->tlmWrite_Data(m_data);
}

void EpsManager ::storeFuelGauge(const project::BatteryState& state) {
    // Spec: integer fields, truncated toward zero
    m_data.set_socPercent(truncate(state.get_stateOfCharge()));
    m_data.set_capacityMah(truncate(state.get_remainingCapacity()));
    m_data.set_batteryCurrentMa(truncate(state.get_current()));
    m_data.set_batteryVoltageMv(truncate(state.get_voltage()));
    m_data.set_midpointMv(truncate(state.get_midVoltage()));
    m_data.set_tteS(static_cast<U32>(state.get_timeToEmpty()));
    m_data.set_ttfS(static_cast<U32>(state.get_timeToFull()));

    // Already integer centi-degrees C from the driver. Pack temperature is
    // signed there; the spec says unsigned (still open with the spec author).
    m_data.set_packTempCc(state.get_temperature());
    m_data.set_ain1TempCc(state.get_temperatureAin1());
    m_data.set_ain2TempCc(state.get_temperatureAin2());
    m_data.set_dieTempCc(state.get_temperatureDie());
}

void EpsManager ::updatePowerFlag(I32 socPercent) {
    const EpsLogic::PowerFlag next = EpsLogic::nextPowerFlag(m_powerFlag, socPercent);
    if (next == EpsLogic::PowerFlag::NONE) {
        // update_eps_state(): an invalid SOC leaves the flag as it was
        this->log_WARNING_HI_InvalidStateOfCharge(socPercent);
        return;
    }
    if (next != m_powerFlag) {
        this->log_ACTIVITY_HI_PowerFlagChanged(toFpp(m_powerFlag), toFpp(next));
        m_powerFlag = next;
    }
}

void EpsManager ::controlHeaters(const EpsLogic::HeaterTemperatures& temperatures) {
    this->controlHeater(project::HeaterId::HEATER0, temperatures.heater0Cc);
    this->controlHeater(project::HeaterId::HEATER1, temperatures.heater1Cc);
}

void EpsManager ::controlHeater(const project::HeaterId& heater, I32 temperatureCc) {
    const FwIndexType index = (heater == project::HeaterId::HEATER0) ? 0 : 1;
    if (m_heaterAbsent[index] || !this->isConnected_heaterGet_OutputPort(0) ||
        !this->isConnected_heaterSet_OutputPort(0)) {
        return;
    }

    const bool enabled = this->heaterGet_out(0, heater);
    const EpsLogic::HeaterAction action = EpsLogic::heaterAction(enabled, temperatureCc);
    if (action == EpsLogic::HeaterAction::NONE) {
        return;
    }

    const bool enable = (action == EpsLogic::HeaterAction::TURN_ON);
    if (this->heaterSet_out(0, heater, enable)) {
        this->log_ACTIVITY_HI_HeaterSwitched(heater, enable, temperatureCc);
    } else {
        // The driver rejects a heater that is not on this board, and logs
        // that itself. Stop asking, so it is not logged every cycle.
        m_heaterAbsent[index] = true;
    }
}

U8 EpsManager ::heaterState(const project::HeaterId& heater) {
    if (!this->isConnected_heaterGet_OutputPort(0)) {
        return 0;
    }
    return asFlag(this->heaterGet_out(0, heater));
}

}  // namespace project
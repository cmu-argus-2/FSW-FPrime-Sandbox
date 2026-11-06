// ======================================================================
// \title  EpsManagerTester.cpp
// \brief  Unit tests for the EpsManager component
// ======================================================================

#include "EpsManagerTester.hpp"

namespace project {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

EpsManagerTester ::EpsManagerTester()
    : EpsManagerGTestBase("EpsManagerTester", EpsManagerTester::MAX_HISTORY_SIZE),
      component("EpsManager"),
      m_fuelFailures(0),
      m_heaterOn{false, false},
      m_heaterPresent{true, true},
      m_cpuOk(false),
      m_cpuCc(0) {
    for (FwIndexType i = 0; i < EpsManager::NUM_POWER_MONITORS; i++) {
        m_monitor[i] = project::PowerSample(0, 0);
        m_monitorStatus[i] = Drv::I2cStatus::I2C_OK;
    }
    // A healthy pack by default: 80% SOC, thermistors at 20 C, so no
    // heater or power flag action happens unless a test asks for it
    m_battery.set_stateOfCharge(80.0F);
    m_battery.set_temperatureAin1(2000);
    m_battery.set_temperatureAin2(2000);

    this->initComponents();
    this->connectPorts();
}

EpsManagerTester ::~EpsManagerTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests: scheduling and health
// ----------------------------------------------------------------------

void EpsManagerTester ::testFuelGaugeReadOnFirstRunThenEveryFifth() {
    // Run 0 includes the fuel gauge work
    this->runOnce();
    ASSERT_from_fuelGauge_SIZE(1);

    // Runs 1-4 do not
    this->runTimes(4);
    ASSERT_from_fuelGauge_SIZE(1);

    // Run 5 does again
    this->runOnce();
    ASSERT_from_fuelGauge_SIZE(2);
}

void EpsManagerTester ::testRunCountTelemetry() {
    this->runTimes(3);

    ASSERT_TLM_RunCount_SIZE(3);
    ASSERT_TLM_RunCount(0, 1U);
    ASSERT_TLM_RunCount(2, 3U);
}

void EpsManagerTester ::testPingIsAnswered() {
    this->invoke_to_pingIn(0, 0x1234);
    this->component.doDispatch();

    ASSERT_from_pingOut_SIZE(1);
    ASSERT_from_pingOut(0, 0x1234U);
}

// ----------------------------------------------------------------------
// Tests: every run
// ----------------------------------------------------------------------

void EpsManagerTester ::testAlertRaisedOnceAndCleared() {
    // Radio at exactly its 3300 mW threshold
    m_monitor[2] = project::PowerSample(3300, 1000);
    this->runOnce();

    ASSERT_TLM_Alerts_SIZE(1);
    ASSERT_EQ(this->tlmHistory_Alerts->at(0).arg.get_radio(), 1);
    ASSERT_EQ(this->tlmHistory_Alerts->at(0).arg.get_mainboard(), 0);
    ASSERT_EVENTS_PowerAlertRaised_SIZE(1);
    ASSERT_TRUE(this->eventHistory_PowerAlertRaised->at(0).load == project::EpsLoad::RADIO);

    // Still above: the alert stays, but no second event
    this->runOnce();
    ASSERT_EQ(this->tlmHistory_Alerts->at(1).arg.get_radio(), 1);
    ASSERT_EVENTS_PowerAlertRaised_SIZE(1);

    // Just below: cleared, one event
    m_monitor[2] = project::PowerSample(3300, 999);
    this->runOnce();
    ASSERT_EQ(this->tlmHistory_Alerts->at(2).arg.get_radio(), 0);
    ASSERT_EVENTS_PowerAlertCleared_SIZE(1);
}

void EpsManagerTester ::testFailedMonitorReadsAsZero() {
    // 2 W would alert the mainboard, but the read fails
    m_monitor[0] = project::PowerSample(5000, 400);
    m_monitorStatus[0] = Drv::I2cStatus::I2C_READ_ERR;
    this->runOnce();

    ASSERT_EQ(this->tlmHistory_Alerts->at(0).arg.get_mainboard(), 0);
    ASSERT_EVENTS_PowerAlertRaised_SIZE(0);
    ASSERT_EQ(this->lastData().get_mainboard().get_voltageMv(), 0);
    ASSERT_EQ(this->lastData().get_mainboard().get_currentMa(), 0);
}

void EpsManagerTester ::testMonitorValuesPublishedEveryFifthRun() {
    this->runTimes(5);
    ASSERT_TLM_Data_SIZE(1);  // run 0 only

    // Values read on run 5 are in the Data published on run 5
    m_monitor[1] = project::PowerSample(5000, 100);
    this->runOnce();
    ASSERT_TLM_Data_SIZE(2);
    ASSERT_EQ(this->lastData().get_gps().get_voltageMv(), 5000);
    ASSERT_EQ(this->lastData().get_gps().get_currentMa(), 100);
}

// ----------------------------------------------------------------------
// Tests: fuel gauge cycle
// ----------------------------------------------------------------------

void EpsManagerTester ::testFuelGaugeValuesTruncated() {
    m_battery.set_stateOfCharge(55.9F);
    m_battery.set_remainingCapacity(1500.5F);
    m_battery.set_current(-123.9F);
    m_battery.set_voltage(7400.6F);
    m_battery.set_midVoltage(3700.4F);
    m_battery.set_timeToEmpty(562.5F);
    m_battery.set_temperature(2345);
    m_battery.set_temperatureDie(2900);
    this->runOnce();

    const project::EpsData data = this->lastData();
    ASSERT_EQ(data.get_socPercent(), 55);
    ASSERT_EQ(data.get_capacityMah(), 1500);
    ASSERT_EQ(data.get_batteryCurrentMa(), -123);  // toward zero, not -124
    ASSERT_EQ(data.get_batteryVoltageMv(), 7400);
    ASSERT_EQ(data.get_midpointMv(), 3700);
    ASSERT_EQ(data.get_tteS(), 562U);
    ASSERT_EQ(data.get_packTempCc(), 2345);
    ASSERT_EQ(data.get_ain1TempCc(), 2000);
    ASSERT_EQ(data.get_dieTempCc(), 2900);
}

void EpsManagerTester ::testPowerFlagHysteresis() {
    m_battery.set_stateOfCharge(30.0F);
    this->runOnce();  // run 0
    ASSERT_TRUE(this->lastData().get_powerFlag() == project::EpsPowerFlag::LOW_POWER);
    ASSERT_EVENTS_PowerFlagChanged_SIZE(1);

    m_battery.set_stateOfCharge(35.0F);
    this->runTimes(5);  // run 5: inside the band, stays LOW_POWER
    ASSERT_TRUE(this->lastData().get_powerFlag() == project::EpsPowerFlag::LOW_POWER);
    ASSERT_EVENTS_PowerFlagChanged_SIZE(1);

    m_battery.set_stateOfCharge(40.0F);
    this->runTimes(5);  // run 10: at the exit threshold
    ASSERT_TRUE(this->lastData().get_powerFlag() == project::EpsPowerFlag::NOMINAL);
    ASSERT_EVENTS_PowerFlagChanged_SIZE(2);
}

void EpsManagerTester ::testInvalidSocKeepsFlag() {
    m_battery.set_stateOfCharge(101.0F);
    this->runOnce();

    ASSERT_EVENTS_InvalidStateOfCharge_SIZE(1);
    ASSERT_EVENTS_PowerFlagChanged_SIZE(0);
    ASSERT_TRUE(this->lastData().get_powerFlag() == project::EpsPowerFlag::NONE);
}

void EpsManagerTester ::testColdThermistorTurnsHeaterOn() {
    m_battery.set_temperatureAin1(-10);  // heater 0 cold; heater 1 stays at 20 C
    this->runOnce();

    ASSERT_from_heaterSet_SIZE(1);
    ASSERT_TRUE(this->fromPortHistory_heaterSet->at(0).heater == project::HeaterId::HEATER0);
    ASSERT_TRUE(this->fromPortHistory_heaterSet->at(0).enable);
    ASSERT_EVENTS_HeaterSwitched_SIZE(1);
    ASSERT_EQ(this->lastData().get_heater0(), 1);
    ASSERT_EQ(this->lastData().get_heater1(), 0);
}

void EpsManagerTester ::testWarmThermistorTurnsHeaterOff() {
    m_heaterOn[1] = true;
    m_battery.set_temperatureAin2(600);  // above the 500 cC off threshold
    this->runOnce();

    ASSERT_from_heaterSet_SIZE(1);
    ASSERT_TRUE(this->fromPortHistory_heaterSet->at(0).heater == project::HeaterId::HEATER1);
    ASSERT_FALSE(this->fromPortHistory_heaterSet->at(0).enable);
    ASSERT_EQ(this->lastData().get_heater1(), 0);
}

void EpsManagerTester ::testFailedReadAfterGoodSkipsDecisions() {
    this->runOnce();  // run 0: good read, thermistors at 20 C
    this->clearHistory();

    // Run 5: the read fails. Believing its zeroed fields would turn both
    // heaters on (0 cC); a skipped cycle must not.
    m_fuelFailures = 1;
    this->runTimes(5);

    ASSERT_from_heaterSet_SIZE(0);
    ASSERT_EVENTS_FuelGaugeDegraded_SIZE(1);
    ASSERT_TRUE(this->eventHistory_FuelGaugeDegraded->at(0).fgMode == project::EpsFuelGaugeMode::SKIP_CYCLE);
    // Last good values kept
    ASSERT_EQ(this->lastData().get_ain1TempCc(), 2000);
    ASSERT_EQ(this->lastData().get_socPercent(), 80);
}

void EpsManagerTester ::testUnavailableFuelGaugeUsesCpuFallback() {
    m_fuelFailures = 15;  // never reads
    m_cpuOk = true;
    m_cpuCc = 100;  // 1 C, so the fallback is -1 C: both heaters on
    this->runOnce();

    ASSERT_TRUE(this->eventHistory_FuelGaugeDegraded->at(0).fgMode == project::EpsFuelGaugeMode::USE_CPU_FALLBACK);
    ASSERT_from_heaterSet_SIZE(2);
    ASSERT_EQ(this->lastData().get_mainboardTempCc(), 100);
    ASSERT_TRUE(this->lastData().get_powerFlag() == project::EpsPowerFlag::NONE);
}

void EpsManagerTester ::testFallbackWithoutCpuTempDecidesNothing() {
    m_fuelFailures = 15;
    m_cpuOk = false;
    this->runOnce();

    ASSERT_from_heaterSet_SIZE(0);
    ASSERT_EQ(this->lastData().get_mainboardTempCc(), 0);
}

void EpsManagerTester ::testAbsentHeaterNotRetried() {
    m_heaterPresent[1] = false;
    m_battery.set_temperatureAin2(-10);  // heater 1 cold, but not on this board
    this->runOnce();
    ASSERT_from_heaterSet_SIZE(1);  // tried once, rejected

    this->runTimes(5);              // next fuel gauge cycle
    ASSERT_from_heaterSet_SIZE(1);  // not tried again
}

// ----------------------------------------------------------------------
// Driver mocks
// ----------------------------------------------------------------------

Drv::I2cStatus EpsManagerTester ::from_powerMonitors_handler(FwIndexType portNum, project::PowerSample& sample) {
    this->pushFromPortEntry_powerMonitors(sample);
    if (m_monitorStatus[portNum] == Drv::I2cStatus::I2C_OK) {
        sample = m_monitor[portNum];
    }
    return m_monitorStatus[portNum];
}

U32 EpsManagerTester ::from_fuelGauge_handler(FwIndexType portNum, project::BatteryState& sample) {
    this->pushFromPortEntry_fuelGauge(sample);
    if (m_fuelFailures == 0) {
        sample = m_battery;
    }
    return m_fuelFailures;
}

bool EpsManagerTester ::from_heaterSet_handler(FwIndexType portNum, const project::HeaterId& heater, bool enable) {
    this->pushFromPortEntry_heaterSet(heater, enable);
    const FwIndexType i = heaterIndex(heater);
    if (!m_heaterPresent[i]) {
        return false;
    }
    m_heaterOn[i] = enable;
    return true;
}

bool EpsManagerTester ::from_heaterGet_handler(FwIndexType portNum, const project::HeaterId& heater) {
    this->pushFromPortEntry_heaterGet(heater);
    const FwIndexType i = heaterIndex(heater);
    return m_heaterPresent[i] && m_heaterOn[i];
}

bool EpsManagerTester ::from_cpuTemp_handler(FwIndexType portNum, I32& centiC) {
    this->pushFromPortEntry_cpuTemp(centiC);
    if (m_cpuOk) {
        centiC = m_cpuCc;
    }
    return m_cpuOk;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void EpsManagerTester ::runOnce() {
    // run is async: invoking it only queues a message. doDispatch() handles
    // that message here, standing in for the component's own thread.
    this->invoke_to_run(0, 0);
    this->component.doDispatch();
}

void EpsManagerTester ::runTimes(U32 n) {
    for (U32 i = 0; i < n; i++) {
        this->runOnce();
    }
}

project::EpsData EpsManagerTester ::lastData() {
    EXPECT_GT(this->tlmHistory_Data->size(), 0U);
    return this->tlmHistory_Data->at(this->tlmHistory_Data->size() - 1).arg;
}

FwIndexType EpsManagerTester ::heaterIndex(const project::HeaterId& heater) {
    return (heater == project::HeaterId::HEATER0) ? 0 : 1;
}

void EpsManagerTester ::connectPorts() {
    // Tester -> component
    this->connect_to_run(0, this->component.get_run_InputPort(0));
    this->connect_to_pingIn(0, this->component.get_pingIn_InputPort(0));

    // Component -> tester (framework ports; names come from the Fw.Event
    // and Fw.Channel interfaces the component imports)
    this->component.set_logOut_OutputPort(0, this->get_from_logOut(0));
#if FW_ENABLE_TEXT_LOGGING == 1
    this->component.set_logTextOut_OutputPort(0, this->get_from_logTextOut(0));
#endif
    this->component.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));
    this->component.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
    this->component.set_pingOut_OutputPort(0, this->get_from_pingOut(0));

    // Component -> tester (drivers)
    for (FwIndexType i = 0; i < EpsManager::NUM_POWER_MONITORS; i++) {
        this->component.set_powerMonitors_OutputPort(i, this->get_from_powerMonitors(i));
    }
    this->component.set_fuelGauge_OutputPort(0, this->get_from_fuelGauge(0));
    this->component.set_heaterSet_OutputPort(0, this->get_from_heaterSet(0));
    this->component.set_heaterGet_OutputPort(0, this->get_from_heaterGet(0));
    this->component.set_cpuTemp_OutputPort(0, this->get_from_cpuTemp(0));
}

void EpsManagerTester ::initComponents() {
    this->init();
    this->component.init(EpsManagerTester::QUEUE_DEPTH, EpsManagerTester::TEST_INSTANCE_ID);
}

}  // namespace project
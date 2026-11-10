// ======================================================================
// \title  EpsManagerTester.hpp
// \brief  Unit test harness for the EpsManager component
// ======================================================================

#ifndef project_EpsManagerTester_HPP
#define project_EpsManagerTester_HPP

#include "project/Tasks/EpsManager/EpsManager.hpp"
#include "project/Tasks/EpsManager/EpsManagerGTestBase.hpp"

namespace project {

class EpsManagerTester final : public EpsManagerGTestBase {
  public:
    static const FwSizeType MAX_HISTORY_SIZE = 50;
    static const FwSizeType QUEUE_DEPTH = 10;
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    EpsManagerTester();
    ~EpsManagerTester();

    // ----------------------------------------------------------------
    // Tests: scheduling and health
    // ----------------------------------------------------------------

    void testFuelGaugeReadOnFirstRunThenEveryFifth();
    void testRunCountTelemetry();
    void testPingIsAnswered();

    // ----------------------------------------------------------------
    // Tests: every run
    // ----------------------------------------------------------------

    void testAlertRaisedOnceAndCleared();
    void testFailedMonitorReadsAsZero();
    void testMonitorValuesPublishedEveryFifthRun();

    // ----------------------------------------------------------------
    // Tests: fuel gauge cycle
    // ----------------------------------------------------------------

    void testFuelGaugeValuesTruncated();
    void testPowerFlagHysteresis();
    void testInvalidSocKeepsFlag();
    void testColdThermistorTurnsHeaterOn();
    void testWarmThermistorTurnsHeaterOff();
    void testFailedReadAfterGoodSkipsDecisions();
    void testUnavailableFuelGaugeUsesCpuFallback();
    void testFallbackWithoutCpuTempDecidesNothing();
    void testAbsentHeaterNotRetried();

  private:
    // ----------------------------------------------------------------
    // Driver mocks
    // ----------------------------------------------------------------

    Drv::I2cStatus from_powerMonitors_handler(FwIndexType portNum, project::PowerSample& sample) override;
    U32 from_fuelGauge_handler(FwIndexType portNum, project::BatteryState& sample) override;
    bool from_heaterSet_handler(FwIndexType portNum, const project::HeaterId& heater, bool enable) override;
    bool from_heaterGet_handler(FwIndexType portNum, const project::HeaterId& heater) override;
    bool from_cpuTemp_handler(FwIndexType portNum, I32& centiC) override;

    // ----------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------

    //! Send one rate group tick and let the component's thread handle it
    void runOnce();

    //! Send n ticks
    void runTimes(U32 n);

    //! Latest Data channel value
    project::EpsData lastData();

    static FwIndexType heaterIndex(const project::HeaterId& heater);

    void connectPorts();
    void initComponents();

  private:
    EpsManager component;

    // What the mocked drivers report
    project::PowerSample m_monitor[EpsManager::NUM_POWER_MONITORS];
    Drv::I2cStatus m_monitorStatus[EpsManager::NUM_POWER_MONITORS];
    project::BatteryState m_battery;
    U32 m_fuelFailures;
    bool m_heaterOn[2];
    bool m_heaterPresent[2];
    bool m_cpuOk;
    I32 m_cpuCc;
};

}  // namespace project

#endif
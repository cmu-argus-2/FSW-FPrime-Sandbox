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
    static const FwSizeType MAX_HISTORY_SIZE = 20;
    static const FwSizeType QUEUE_DEPTH = 10;
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    EpsManagerTester();
    ~EpsManagerTester();

    // ----------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------

    void testFuelGaugeReadOnFirstRunThenEveryFifth();
    void testFuelGaugeEventCarriesSoc();
    void testRunCountTelemetry();
    void testPingIsAnswered();

  private:
    // ----------------------------------------------------------------
    // Driver mocks
    // ----------------------------------------------------------------

    U32 from_fuelGauge_handler(FwIndexType portNum, project::BatteryState& sample) override;

    // ----------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------

    //! Send one rate group tick and let the component's thread handle it
    void runOnce();

    void connectPorts();
    void initComponents();

  private:
    EpsManager component;
    F32 m_soc;          //!< SOC the mocked fuel gauge reports
    U32 m_failedReads;  //!< Failure count the mocked fuel gauge returns
};

}  // namespace project

#endif
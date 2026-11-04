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
      m_soc(0.0F),
      m_failedReads(0) {
    this->initComponents();
    this->connectPorts();
}

EpsManagerTester ::~EpsManagerTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void EpsManagerTester ::testFuelGaugeReadOnFirstRunThenEveryFifth() {
    // Run 0 includes the fuel gauge work
    this->runOnce();
    ASSERT_from_fuelGauge_SIZE(1);

    // Runs 1-4 do not
    for (U32 i = 0; i < 4; i++) {
        this->runOnce();
    }
    ASSERT_from_fuelGauge_SIZE(1);

    // Run 5 does again
    this->runOnce();
    ASSERT_from_fuelGauge_SIZE(2);
}

void EpsManagerTester ::testFuelGaugeEventCarriesSoc() {
    m_soc = 55.5F;
    m_failedReads = 2;

    this->runOnce();

    ASSERT_EVENTS_FuelGaugeSampled_SIZE(1);
    ASSERT_EVENTS_FuelGaugeSampled(0, 55.5F, 2U);
}

void EpsManagerTester ::testRunCountTelemetry() {
    this->runOnce();
    this->runOnce();
    this->runOnce();

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
// Driver mocks
// ----------------------------------------------------------------------

U32 EpsManagerTester ::from_fuelGauge_handler(FwIndexType portNum, project::BatteryState& sample) {
    this->pushFromPortEntry_fuelGauge(sample);
    sample.set_stateOfCharge(m_soc);
    return m_failedReads;
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

    // Component -> tester (drivers). Only the fuel gauge is used so far;
    // power monitors and heaters stay unconnected until later steps.
    this->component.set_fuelGauge_OutputPort(0, this->get_from_fuelGauge(0));
}

void EpsManagerTester ::initComponents() {
    this->init();
    this->component.init(EpsManagerTester::QUEUE_DEPTH, EpsManagerTester::TEST_INSTANCE_ID);
}

}  // namespace project
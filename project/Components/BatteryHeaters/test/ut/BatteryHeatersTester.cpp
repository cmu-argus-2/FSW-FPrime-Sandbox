// ======================================================================
// \title  BatteryHeatersTester.cpp
// \brief  Unit tests for the BatteryHeaters component
// ======================================================================

#include "BatteryHeatersTester.hpp"

namespace project {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

BatteryHeatersTester ::BatteryHeatersTester(bool heater1Present)
    : BatteryHeatersGTestBase("BatteryHeatersTester", BatteryHeatersTester::MAX_HISTORY_SIZE),
      component("BatteryHeaters"),
      m_heater1Present(heater1Present),
      m_gpioStatus(Drv::GpioStatus::OP_OK),
      m_cmdSeq(0) {
    this->initComponents();
    this->connectPorts();
}

BatteryHeatersTester ::~BatteryHeatersTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests: dual-heater board
// ----------------------------------------------------------------------

void BatteryHeatersTester ::testEnableHeater0DrivesMasterAndHeater0() {
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER0, Fw::CmdResponse::OK);

    ASSERT_from_enableWrite_SIZE(1);
    ASSERT_from_enableWrite(0, Fw::Logic::HIGH);
    ASSERT_from_heater0Write_SIZE(1);
    ASSERT_from_heater0Write(0, Fw::Logic::HIGH);
    ASSERT_from_heater1Write_SIZE(0);

    ASSERT_TLM_Heater0Enabled_SIZE(1);
    ASSERT_TLM_Heater0Enabled(0, true);
    ASSERT_EVENTS_Heater0Enabled_SIZE(1);
}

void BatteryHeatersTester ::testEnableHeater0TwiceWritesMasterOnce() {
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER0, Fw::CmdResponse::OK);
    this->clearHistory();
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER0, Fw::CmdResponse::OK);

    // Master line already high: second command must not touch it again
    ASSERT_from_enableWrite_SIZE(0);
    ASSERT_from_heater0Write_SIZE(1);
    ASSERT_from_heater0Write(0, Fw::Logic::HIGH);
}

void BatteryHeatersTester ::testDisableHeater0DropsMasterWhenHeater1Off() {
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER0, Fw::CmdResponse::OK);
    this->clearHistory();
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_DISABLEHEATER0, Fw::CmdResponse::OK);

    ASSERT_from_heater0Write_SIZE(1);
    ASSERT_from_heater0Write(0, Fw::Logic::LOW);
    ASSERT_from_enableWrite_SIZE(1);
    ASSERT_from_enableWrite(0, Fw::Logic::LOW);

    ASSERT_TLM_Heater0Enabled(0, false);
    ASSERT_EVENTS_Heater0Disabled_SIZE(1);
}

void BatteryHeatersTester ::testEnableHeater1DrivesMasterAndHeater1() {
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER1, Fw::CmdResponse::OK);

    ASSERT_from_enableWrite_SIZE(1);
    ASSERT_from_enableWrite(0, Fw::Logic::HIGH);
    ASSERT_from_heater1Write_SIZE(1);
    ASSERT_from_heater1Write(0, Fw::Logic::HIGH);
    ASSERT_from_heater0Write_SIZE(0);

    ASSERT_TLM_Heater1Enabled_SIZE(1);
    ASSERT_TLM_Heater1Enabled(0, true);
    ASSERT_EVENTS_Heater1Enabled_SIZE(1);
}

void BatteryHeatersTester ::testMasterStaysHighUntilBothHeatersOff() {
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER0, Fw::CmdResponse::OK);
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER1, Fw::CmdResponse::OK);
    this->clearHistory();

    // Heater 1 still on: master must stay high
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_DISABLEHEATER0, Fw::CmdResponse::OK);
    ASSERT_from_heater0Write(0, Fw::Logic::LOW);
    ASSERT_from_enableWrite_SIZE(0);

    this->clearHistory();

    // Last heater off: master drops
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_DISABLEHEATER1, Fw::CmdResponse::OK);
    ASSERT_from_heater1Write(0, Fw::Logic::LOW);
    ASSERT_from_enableWrite_SIZE(1);
    ASSERT_from_enableWrite(0, Fw::Logic::LOW);
}

void BatteryHeatersTester ::testDisableHeater1DropsMasterWhenHeater0Off() {
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER1, Fw::CmdResponse::OK);
    this->clearHistory();
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_DISABLEHEATER1, Fw::CmdResponse::OK);

    ASSERT_from_heater1Write_SIZE(1);
    ASSERT_from_heater1Write(0, Fw::Logic::LOW);
    ASSERT_from_enableWrite_SIZE(1);
    ASSERT_from_enableWrite(0, Fw::Logic::LOW);

    ASSERT_TLM_Heater1Enabled(0, false);
    ASSERT_EVENTS_Heater1Disabled_SIZE(1);
}

// ----------------------------------------------------------------------
// Tests: single-heater board
// ----------------------------------------------------------------------

void BatteryHeatersTester ::testEnableHeater1RejectedWhenNotPresent() {
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER1, Fw::CmdResponse::VALIDATION_ERROR);

    // No GPIO activity at all, including the master line
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_Heater1NotPresent_SIZE(1);
}

void BatteryHeatersTester ::testDisableHeater1RejectedWhenNotPresent() {
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_DISABLEHEATER1, Fw::CmdResponse::VALIDATION_ERROR);

    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_Heater1NotPresent_SIZE(1);
}

void BatteryHeatersTester ::testDisableHeater0DropsMasterWhenHeater1NotPresent() {
    // Regression test for the Python driver bug: DisableHeater0 on a
    // single-heater board read heater1_en.value while heater1_en was None.
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_ENABLEHEATER0, Fw::CmdResponse::OK);
    this->clearHistory();
    this->sendAndCheck(BatteryHeatersComponentBase::OPCODE_DISABLEHEATER0, Fw::CmdResponse::OK);

    ASSERT_from_heater0Write(0, Fw::Logic::LOW);
    ASSERT_from_enableWrite_SIZE(1);
    ASSERT_from_enableWrite(0, Fw::Logic::LOW);
}

// ----------------------------------------------------------------------
// GPIO mocks
// ----------------------------------------------------------------------

Drv::GpioStatus BatteryHeatersTester ::from_enableWrite_handler(FwIndexType portNum, const Fw::Logic& state) {
    this->pushFromPortEntry_enableWrite(state);
    return m_gpioStatus;
}

Drv::GpioStatus BatteryHeatersTester ::from_heater0Write_handler(FwIndexType portNum, const Fw::Logic& state) {
    this->pushFromPortEntry_heater0Write(state);
    return m_gpioStatus;
}

Drv::GpioStatus BatteryHeatersTester ::from_heater1Write_handler(FwIndexType portNum, const Fw::Logic& state) {
    this->pushFromPortEntry_heater1Write(state);
    return m_gpioStatus;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void BatteryHeatersTester ::sendAndCheck(FwOpcodeType opCode, Fw::CmdResponse expected) {
    const U32 seq = ++m_cmdSeq;
    switch (opCode) {
        case BatteryHeatersComponentBase::OPCODE_ENABLEHEATER0:
            this->sendCmd_EnableHeater0(TEST_INSTANCE_ID, seq);
            break;
        case BatteryHeatersComponentBase::OPCODE_ENABLEHEATER1:
            this->sendCmd_EnableHeater1(TEST_INSTANCE_ID, seq);
            break;
        case BatteryHeatersComponentBase::OPCODE_DISABLEHEATER0:
            this->sendCmd_DisableHeater0(TEST_INSTANCE_ID, seq);
            break;
        case BatteryHeatersComponentBase::OPCODE_DISABLEHEATER1:
            this->sendCmd_DisableHeater1(TEST_INSTANCE_ID, seq);
            break;
        default:
            FAIL() << "unknown opcode " << opCode;
    }
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, opCode, seq, expected);
    // Keep only this command's response so later asserts index from 0
    this->cmdResponseHistory->clear();
}

void BatteryHeatersTester ::connectPorts() {
    // Tester -> component
    this->connect_to_cmdIn(0, this->component.get_cmdIn_InputPort(0));

    // Component -> tester (framework ports)
    this->component.set_cmdRegOut_OutputPort(0, this->get_from_cmdRegOut(0));
    this->component.set_cmdResponseOut_OutputPort(0, this->get_from_cmdResponseOut(0));
    this->component.set_eventOut_OutputPort(0, this->get_from_eventOut(0));
#if FW_ENABLE_TEXT_LOGGING == 1
    this->component.set_textEventOut_OutputPort(0, this->get_from_textEventOut(0));
#endif
    this->component.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
    this->component.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));

    // Component -> tester (GPIO, stands in for ZephyrGpioDriver)
    this->component.set_enableWrite_OutputPort(0, this->get_from_enableWrite(0));
    this->component.set_enableRead_OutputPort(0, this->get_from_enableRead(0));
    this->component.set_heater0Write_OutputPort(0, this->get_from_heater0Write(0));
    this->component.set_heater0Read_OutputPort(0, this->get_from_heater0Read(0));

    // Heater 1 is optional: leaving these unconnected models a
    // single-heater board, exactly as the topology would.
    if (m_heater1Present) {
        this->component.set_heater1Write_OutputPort(0, this->get_from_heater1Write(0));
        this->component.set_heater1Read_OutputPort(0, this->get_from_heater1Read(0));
    }
}

void BatteryHeatersTester ::initComponents() {
    this->init();
    this->component.init(BatteryHeatersTester::TEST_INSTANCE_ID);
}

}  // namespace project
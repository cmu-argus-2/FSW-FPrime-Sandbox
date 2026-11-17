// ======================================================================
// \title  DRV8235Tester.cpp
// \brief  Unit tests for the DRV8235 component
// ======================================================================

#include "DRV8235Tester.hpp"

namespace project {

namespace {
using namespace DRV8235Math;

// XP coil: I2C1 0x30 (argus_v4.py)
constexpr U32 kAddress = 0x30;

// VMTR 24 and IMTR 64 decode to 4015 mV and 928 mA (DRV8235MathTest)
constexpr U8 kVoltageCode = 24;
constexpr U8 kCurrentCode = 64;
constexpr I32 kExpectedMv = 4015;
constexpr I32 kExpectedMa = 928;
}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

DRV8235Tester ::DRV8235Tester()
    : DRV8235GTestBase("DRV8235Tester", DRV8235Tester::MAX_HISTORY_SIZE),
      component("DRV8235"),
      m_busStatus(Drv::I2cStatus::I2C_OK),
      m_regs{} {
    // Power-on junk in every register the init sequence touches, so the
    // tests can see that read-modify-write keeps the bits it should.
    m_regs[CONFIG0] = 0x40;
    m_regs[CONFIG3] = 0x01;
    m_regs[CONFIG4] = 0xF3;
    m_regs[REG_CTRL0] = 0x05;
    m_regs[REG_CTRL1] = 0x77;
    m_regs[RC_CTRL2] = 0x3F;
    m_regs[RC_CTRL3] = 0x00;
    m_regs[REG_STATUS1] = kVoltageCode;
    m_regs[REG_STATUS2] = kCurrentCode;
    this->initComponents();
    this->connectPorts();
}

DRV8235Tester ::~DRV8235Tester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void DRV8235Tester ::testConfigureTouchesNoBus() {
    // COIL_EN may still be low here, so configure must not talk to the chip
    ASSERT_TRUE(this->component.configure(kAddress));
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void DRV8235Tester ::testConfigureRejectsBadAddress() {
    ASSERT_FALSE(this->component.configure(0x80));
    ASSERT_EVENTS_ConfigurationError_SIZE(1);
    ASSERT_EVENTS_ConfigurationError(0, 0x80);
}

void DRV8235Tester ::testFirstReadRunsInitSequence() {
    ASSERT_TRUE(this->component.configure(kAddress));

    project::PowerSample sample;
    const Drv::I2cStatus status = this->invoke_to_read(0, sample);
    ASSERT_TRUE(status == Drv::I2cStatus::I2C_OK);

    // Ten writes in Python's order. The two full-register fields (REG_CTRL1,
    // RC_CTRL3) are plain writes; the other eight read first.
    const U8 expectedOrder[] = {CONFIG4, CONFIG4,  CONFIG4,  REG_CTRL0, REG_CTRL1,
                                CONFIG3, RC_CTRL2, RC_CTRL3, CONFIG0,   CONFIG0};
    ASSERT_EQ(m_writes.size(), sizeof(expectedOrder));
    for (FwSizeType i = 0; i < sizeof(expectedOrder); i++) {
        EXPECT_EQ(m_writes[i].reg, expectedOrder[i]) << "write " << i;
    }
    ASSERT_from_busWrite_SIZE(10);
    ASSERT_from_busWriteRead_SIZE(8 + 3);
    ASSERT_EQ(this->fromPortHistory_busWrite->at(0).addr, kAddress);

    // Final register contents: the bits the sequence owns are set, the rest
    // of each register is untouched.
    EXPECT_EQ(m_regs[CONFIG4], 0xFC);    // 0xF3 -> I2C_BC, PMODE set, bridge COAST
    EXPECT_EQ(m_regs[REG_CTRL0], 0x1D);  // 0x05 | voltage regulation (0x18)
    EXPECT_EQ(m_regs[REG_CTRL1], 0x00);  // 0 V target
    EXPECT_EQ(m_regs[CONFIG3], 0x11);    // 0x01 | INT_VREF
    EXPECT_EQ(m_regs[RC_CTRL2], 0xBF);   // bits 7:6 = 0b10
    EXPECT_EQ(m_regs[RC_CTRL3], 41);
    EXPECT_EQ(m_regs[CONFIG0], 0xC2);  // 0x40 | EN_OUT | CLR_FLT

    // Coasting, so the sample is positive
    EXPECT_EQ(sample.get_voltageMv(), kExpectedMv);
    EXPECT_EQ(sample.get_currentMa(), kExpectedMa);

    // read is the EPS path: the caller owns reporting, so no telemetry here
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_Initialized_SIZE(1);
}

void DRV8235Tester ::testLaterReadsSkipInit() {
    this->configureAndInitialize();

    project::PowerSample sample;
    ASSERT_TRUE(this->invoke_to_read(0, sample) == Drv::I2cStatus::I2C_OK);

    // VMTR, IMTR, CONFIG4 and nothing else
    ASSERT_from_busWrite_SIZE(0);
    ASSERT_from_busWriteRead_SIZE(3);
    ASSERT_EQ(sample.get_voltageMv(), kExpectedMv);
    ASSERT_EVENTS_SIZE(0);
}

void DRV8235Tester ::testReverseBridgeNegatesSample() {
    this->configureAndInitialize();
    m_regs[CONFIG4] = static_cast<U8>((m_regs[CONFIG4] & ~BRIDGE_MASK) | BRIDGE_REVERSE);

    project::PowerSample sample;
    ASSERT_TRUE(this->invoke_to_read(0, sample) == Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(sample.get_voltageMv(), -kExpectedMv);
    ASSERT_EQ(sample.get_currentMa(), -kExpectedMa);
}

void DRV8235Tester ::testReadBeforeConfigureTouchesNoBus() {
    project::PowerSample sample;
    const Drv::I2cStatus status = this->invoke_to_read(0, sample);

    ASSERT_TRUE(status == Drv::I2cStatus::I2C_OPEN_ERR);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);

    // Spec: a device that failed to initialize leaves its fields at 0
    ASSERT_EQ(sample.get_voltageMv(), 0);
    ASSERT_EQ(sample.get_currentMa(), 0);
}

void DRV8235Tester ::testInitFailureRetriesOnNextRead() {
    ASSERT_TRUE(this->component.configure(kAddress));
    m_busStatus = Drv::I2cStatus::I2C_ADDRESS_ERR;  // COIL_EN still low: no ACK

    project::PowerSample sample(123, 456);
    const Drv::I2cStatus status = this->invoke_to_read(0, sample);
    ASSERT_TRUE(status == Drv::I2cStatus::I2C_ADDRESS_ERR);
    ASSERT_EQ(sample.get_voltageMv(), 123);
    ASSERT_EQ(sample.get_currentMa(), 456);
    // Stopped at the first failure: one read of CONFIG4, no writes
    ASSERT_from_busWriteRead_SIZE(1);
    ASSERT_from_busWrite_SIZE(0);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_EVENTS_I2cError(0, CONFIG4, Drv::I2cStatus::I2C_ADDRESS_ERR);

    // The chip powers up: the next read runs the whole sequence
    this->clearHistory();
    m_busStatus = Drv::I2cStatus::I2C_OK;
    ASSERT_TRUE(this->invoke_to_read(0, sample) == Drv::I2cStatus::I2C_OK);
    ASSERT_from_busWrite_SIZE(10);
    ASSERT_EQ(sample.get_voltageMv(), kExpectedMv);
    ASSERT_EVENTS_Initialized_SIZE(1);
}

void DRV8235Tester ::testBusFailureAfterInitReinitializes() {
    this->configureAndInitialize();

    m_busStatus = Drv::I2cStatus::I2C_READ_ERR;
    project::PowerSample sample(123, 456);
    ASSERT_TRUE(this->invoke_to_read(0, sample) == Drv::I2cStatus::I2C_READ_ERR);
    ASSERT_EQ(sample.get_voltageMv(), 123);
    ASSERT_TLM_SIZE(0);

    // The coil rail may have been cycled, which resets the chip, so the
    // next good read sets it up again before sampling.
    this->clearHistory();
    m_busStatus = Drv::I2cStatus::I2C_OK;
    ASSERT_TRUE(this->invoke_to_read(0, sample) == Drv::I2cStatus::I2C_OK);
    ASSERT_from_busWrite_SIZE(10);
    ASSERT_EQ(sample.get_voltageMv(), kExpectedMv);
}

void DRV8235Tester ::testPollPublishesTelemetryAndFault() {
    this->configureAndInitialize();
    m_regs[FAULT_STATUS] = FAULT_ANY | FAULT_OCP;

    this->invoke_to_poll(0, 0);

    ASSERT_TLM_Reading_SIZE(1);
    const project::DRV8235Reading reading = this->tlmHistory_Reading->at(0).arg;
    ASSERT_EQ(reading.get_voltageCode(), kVoltageCode);
    ASSERT_EQ(reading.get_currentCode(), kCurrentCode);
    ASSERT_EQ(reading.get_voltageMv(), kExpectedMv);
    ASSERT_EQ(reading.get_currentMa(), kExpectedMa);
    ASSERT_EQ(reading.get_faultStatus(), FAULT_ANY | FAULT_OCP);
    ASSERT_EVENTS_DeviceFault_SIZE(1);
    ASSERT_EVENTS_DeviceFault(0, FAULT_ANY | FAULT_OCP);

    // The driver reports faults but does not clear them
    ASSERT_from_busWrite_SIZE(0);
}

void DRV8235Tester ::testPollWithoutFaultEmitsNoFaultEvent() {
    this->configureAndInitialize();
    m_regs[FAULT_STATUS] = 0;

    this->invoke_to_poll(0, 0);

    ASSERT_TLM_Reading_SIZE(1);
    ASSERT_EQ(this->tlmHistory_Reading->at(0).arg.get_faultStatus(), 0);
    ASSERT_EVENTS_SIZE(0);
}

// ----------------------------------------------------------------------
// I2C mocks
// ----------------------------------------------------------------------

Drv::I2cStatus DRV8235Tester ::from_busWriteRead_handler(FwIndexType portNum,
                                                         U32 addr,
                                                         Fw::Buffer& writeBuffer,
                                                         Fw::Buffer& readBuffer) {
    this->pushFromPortEntry_busWriteRead(addr, writeBuffer, readBuffer);
    // EXPECT, not ASSERT: gtest's ASSERT_* returns void, and this handler
    // must return a status.
    EXPECT_EQ(writeBuffer.getSize(), 1U);
    EXPECT_EQ(readBuffer.getSize(), 1U);
    if (m_busStatus != Drv::I2cStatus::I2C_OK || writeBuffer.getSize() != 1 || readBuffer.getSize() != 1) {
        return m_busStatus;
    }
    const U8 reg = writeBuffer.getData()[0];
    EXPECT_LT(reg, sizeof(m_regs));
    readBuffer.getData()[0] = (reg < sizeof(m_regs)) ? m_regs[reg] : 0;
    return Drv::I2cStatus::I2C_OK;
}

Drv::I2cStatus DRV8235Tester ::from_busWrite_handler(FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) {
    this->pushFromPortEntry_busWrite(addr, serBuffer);
    EXPECT_EQ(serBuffer.getSize(), 2U);
    if (m_busStatus != Drv::I2cStatus::I2C_OK || serBuffer.getSize() != 2) {
        return m_busStatus;
    }
    // The component writes from a stack buffer, so copy the bytes now
    const U8 reg = serBuffer.getData()[0];
    const U8 value = serBuffer.getData()[1];
    EXPECT_LT(reg, sizeof(m_regs));
    if (reg < sizeof(m_regs)) {
        m_regs[reg] = value;
    }
    m_writes.push_back({reg, value});
    return Drv::I2cStatus::I2C_OK;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void DRV8235Tester ::configureAndInitialize() {
    ASSERT_TRUE(this->component.configure(kAddress));
    project::PowerSample sample;
    ASSERT_TRUE(this->invoke_to_read(0, sample) == Drv::I2cStatus::I2C_OK);
    this->clearHistory();
    m_writes.clear();
}

void DRV8235Tester ::connectPorts() {
    // Tester -> component
    this->connect_to_read(0, this->component.get_read_InputPort(0));
    this->connect_to_poll(0, this->component.get_poll_InputPort(0));

    // Component -> tester (framework ports; names come from the Fw.Event
    // and Fw.Channel interfaces the component imports)
    this->component.set_logOut_OutputPort(0, this->get_from_logOut(0));
#if FW_ENABLE_TEXT_LOGGING == 1
    this->component.set_logTextOut_OutputPort(0, this->get_from_logTextOut(0));
#endif
    this->component.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));
    this->component.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));

    // Component -> tester (I2C, stands in for Board.i2c0 or Board.i2c1)
    this->component.set_busWriteRead_OutputPort(0, this->get_from_busWriteRead(0));
    this->component.set_busWrite_OutputPort(0, this->get_from_busWrite(0));
}

void DRV8235Tester ::initComponents() {
    this->init();
    this->component.init(DRV8235Tester::TEST_INSTANCE_ID);
}

}  // namespace project

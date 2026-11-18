// ======================================================================
// \title  ADM1176Tester.cpp
// \brief  Unit tests for the ADM1176 component
// ======================================================================

#include "ADM1176Tester.hpp"

#include <cstring>

namespace project {

namespace {
// The EPS spec's mainboard monitor: I2C1 0x40, 10 mOhm sense resistor,
// 14:1 divider (26.35 V full scale).
constexpr U32 kAddress = 0x40;
constexpr U32 kSenseMilliOhms = 10;

// Raw bytes {0xAB, 0xCD, 0xEF} decode to voltage code 0xABE (2750) and
// current code 0xCDF (3295), as ADM1176MathTest checks.
//   2750 * 26350 / 4096        = 17691 mV
//   3295 * 105840 / (4096*10)  =  8514 mA
constexpr I32 kExpectedMv = 17691;
constexpr I32 kExpectedMa = 8514;
}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ADM1176Tester ::ADM1176Tester()
    : ADM1176GTestBase("ADM1176Tester", ADM1176Tester::MAX_HISTORY_SIZE),
      component("ADM1176"),
      m_busStatus(Drv::I2cStatus::I2C_OK),
      m_readBytes{0xAB, 0xCD, 0xEF},
      m_lastCommand(0) {
    this->initComponents();
    this->connectPorts();
}

ADM1176Tester ::~ADM1176Tester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ADM1176Tester ::testConfigureSendsContinuousHighRangeCommand() {
    ASSERT_TRUE(this->configureForEps());

    // 0x05 = V_CONT | I_CONT, VRANGE clear (14:1). This is the spec's "config 0x05".
    ASSERT_from_i2cWrite_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_i2cWrite->at(0).addr, kAddress);
    ASSERT_EQ(static_cast<U32>(m_lastCommand), 0x05U);
    ASSERT_EVENTS_SIZE(0);
}

void ADM1176Tester ::testReadReturnsDecodedSample() {
    ASSERT_TRUE(this->configureForEps());
    this->clearHistory();

    project::PowerSample sample;
    const Drv::I2cStatus status = this->invoke_to_read(0, sample);

    ASSERT_TRUE(status == Drv::I2cStatus::I2C_OK);
    ASSERT_from_i2cRead_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_i2cRead->at(0).addr, kAddress);
    ASSERT_EQ(sample.get_voltageMv(), kExpectedMv);
    ASSERT_EQ(sample.get_currentMa(), kExpectedMa);

    // read is the EPS path: the caller owns reporting, so no telemetry here
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void ADM1176Tester ::testReadBeforeConfigureTouchesNoBus() {
    project::PowerSample sample;
    const Drv::I2cStatus status = this->invoke_to_read(0, sample);

    ASSERT_TRUE(status == Drv::I2cStatus::I2C_OPEN_ERR);
    ASSERT_from_i2cRead_SIZE(0);
    ASSERT_from_i2cWrite_SIZE(0);

    // Spec: a device that failed to initialize leaves its fields at 0
    ASSERT_EQ(sample.get_voltageMv(), 0);
    ASSERT_EQ(sample.get_currentMa(), 0);
}

void ADM1176Tester ::testReadBusFailureLeavesSampleUnchanged() {
    ASSERT_TRUE(this->configureForEps());
    this->clearHistory();
    m_busStatus = Drv::I2cStatus::I2C_READ_ERR;

    project::PowerSample sample(123, 456);
    const Drv::I2cStatus status = this->invoke_to_read(0, sample);

    ASSERT_TRUE(status == Drv::I2cStatus::I2C_READ_ERR);
    ASSERT_EQ(sample.get_voltageMv(), 123);
    ASSERT_EQ(sample.get_currentMa(), 456);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_TLM_SIZE(0);
}

void ADM1176Tester ::testPollPublishesTelemetry() {
    ASSERT_TRUE(this->configureForEps());
    this->clearHistory();

    this->invoke_to_poll(0, 0);

    ASSERT_TLM_VoltageCode_SIZE(1);
    ASSERT_TLM_VoltageCode(0, 0xABE);
    ASSERT_TLM_CurrentCode(0, 0xCDF);
    ASSERT_TLM_VoltageMv(0, static_cast<U32>(kExpectedMv));
    ASSERT_TLM_CurrentMa(0, static_cast<U32>(kExpectedMa));
}

// ----------------------------------------------------------------------
// I2C mocks
// ----------------------------------------------------------------------

Drv::I2cStatus ADM1176Tester ::from_i2cWrite_handler(FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) {
    this->pushFromPortEntry_i2cWrite(addr, serBuffer);
    // configure() writes from a stack variable, so capture the byte now
    m_lastCommand = (serBuffer.getSize() > 0) ? serBuffer.getData()[0] : 0;
    return m_busStatus;
}

Drv::I2cStatus ADM1176Tester ::from_i2cRead_handler(FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) {
    this->pushFromPortEntry_i2cRead(addr, serBuffer);
    if (m_busStatus == Drv::I2cStatus::I2C_OK) {
        EXPECT_GE(static_cast<FwSizeType>(serBuffer.getSize()), sizeof(m_readBytes));
        std::memcpy(serBuffer.getData(), m_readBytes, sizeof(m_readBytes));
    }
    return m_busStatus;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

bool ADM1176Tester ::configureForEps() {
    return this->component.configure(kAddress, kSenseMilliOhms, false);
}

void ADM1176Tester ::connectPorts() {
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

    // Component -> tester (I2C, stands in for Board.i2c1)
    this->component.set_i2cWrite_OutputPort(0, this->get_from_i2cWrite(0));
    this->component.set_i2cRead_OutputPort(0, this->get_from_i2cRead(0));
}

void ADM1176Tester ::initComponents() {
    this->init();
    this->component.init(ADM1176Tester::TEST_INSTANCE_ID);
}

}  // namespace project
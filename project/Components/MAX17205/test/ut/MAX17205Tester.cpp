#include "MAX17205Tester.hpp"

namespace project {

MAX17205Tester::MAX17205Tester()
    : MAX17205GTestBase("MAX17205Tester", MAX_HISTORY_SIZE), component("MAX17205") {
    this->initComponents();
    this->connectPorts();
}

MAX17205Tester::~MAX17205Tester() {
    this->component.deinit();
}

U16 MAX17205Tester::valueForRegister(U8 address, U8 reg) {
    // These values are raw MAX17205 register values. The component under test
    // must perform the same conversions as the original Python driver.
    if (address == 0x36) {
        switch (reg) {
            case 0xDA:
                return 4000;  // 5000 mV
            case 0x0A:
                return 64;  // 10 mA
            case 0x09:
                return 1280;  // 100 mV
            case 0x06:
                return 2560;  // 10 percent
            case 0x05:
                return 200;  // 100 mAh
            case 0x10:
                return 400;  // 200 mAh
            case 0x17:
                return 123;
            case 0x11:
                return 16;  // 90 seconds
            case 0x20:
                return 32;  // 180 seconds
            case 0xBE:
                return 77;
            case 0x08:
                return 256;  // 100 centi-Celsius
            default:
                return 0;
        }
    }

    switch (reg) {
        case 0x34:
            return 3000;  // 2690 centi-Celsius
        case 0x3B:
            return 3001;
        case 0x35:
            return 3002;
        default:
            return 0;
    }
}

Drv::I2cStatus MAX17205Tester::from_busWriteRead_handler(FwIndexType portNum,
                                                           U32 address,
                                                           Fw::Buffer& writeBuffer,
                                                           Fw::Buffer& readBuffer) {
    // Record the transaction before responding, allowing the test to inspect
    // the requested address and register through the generated history.
    ++m_readCount;
    this->pushFromPortEntry_busWriteRead(address, writeBuffer, readBuffer);

    if (m_nextReadStatus != Drv::I2cStatus::I2C_OK) {
        return m_nextReadStatus;
    }

    EXPECT_EQ(writeBuffer.getSize(), 1U);
    EXPECT_EQ(readBuffer.getSize(), 2U);
    EXPECT_NE(writeBuffer.getData(), nullptr);
    EXPECT_NE(readBuffer.getData(), nullptr);

    // Return the selected register in MAX17205 little-endian order.
    const U8 reg = writeBuffer.getData()[0];
    const U16 value = valueForRegister(static_cast<U8>(address), reg);
    readBuffer.getData()[0] = static_cast<U8>(value & 0xFFU);
    readBuffer.getData()[1] = static_cast<U8>(value >> 8U);
    return Drv::I2cStatus::I2C_OK;
}

Drv::I2cStatus MAX17205Tester::from_busWrite_handler(FwIndexType portNum, U32 address, Fw::Buffer& buffer) {
    // RESET uses the write-only bus port and must send register, low byte,
    // high byte for CONFIG2 = 0x0001.
    ++m_writeCount;
    this->pushFromPortEntry_busWrite(address, buffer);

    EXPECT_EQ(address, 0x36U);
    EXPECT_EQ(buffer.getSize(), 3U);
    EXPECT_NE(buffer.getData(), nullptr);
    EXPECT_EQ(buffer.getData()[0], 0xBB);
    EXPECT_EQ(buffer.getData()[1], 0x01);
    EXPECT_EQ(buffer.getData()[2], 0x00);
    return Drv::I2cStatus::I2C_OK;
}

void MAX17205Tester::testReadAll() {
    // A successful snapshot should read 11 main registers and 3 shadow-RAM
    // temperature registers, then publish the converted telemetry.
    this->clearHistory();
    m_nextReadStatus = Drv::I2cStatus::I2C_OK;
    m_readCount = 0;

    this->sendCmd_READ_ALL(0, 42);
    this->component.doDispatch();

    ASSERT_EQ(m_readCount, 14U);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, MAX17205::OPCODE_READ_ALL, 42, Fw::CmdResponse::OK);
    ASSERT_TLM_Voltage_SIZE(1);
    ASSERT_TLM_Voltage(0, 5000.0F);
    ASSERT_TLM_Current_SIZE(1);
    ASSERT_TLM_Current(0, 10.0F);
    ASSERT_TLM_StateOfCharge_SIZE(1);
    ASSERT_TLM_StateOfCharge(0, 10.0F);
    ASSERT_TLM_TemperatureAin1_SIZE(1);
    ASSERT_TLM_TemperatureAin1(0, 2690);
}

void MAX17205Tester::testReset() {
    // Verify the command reaches the bus with the exact reset payload.
    this->clearHistory();
    m_writeCount = 0;

    this->sendCmd_RESET(0, 43);
    this->component.doDispatch();

    ASSERT_EQ(m_writeCount, 1U);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, MAX17205::OPCODE_RESET, 43, Fw::CmdResponse::OK);
}

void MAX17205Tester::testReadFailure() {
    // Fail the first bus transaction and verify no partial telemetry is sent.
    this->clearHistory();
    m_nextReadStatus = Drv::I2cStatus::I2C_READ_ERR;

    this->sendCmd_READ_ALL(0, 44);
    this->component.doDispatch();

    ASSERT_EQ(m_readCount, 1U);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, MAX17205::OPCODE_READ_ALL, 44, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_TLM_Voltage_SIZE(0);
}

}  // namespace project

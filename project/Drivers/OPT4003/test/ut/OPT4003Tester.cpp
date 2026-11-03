// ======================================================================
// \title  OPT4003Tester.cpp
// \brief  Unit test harness for the OPT4003 component.
// ======================================================================

#include "OPT4003Tester.hpp"

#include "Os/RawTime.hpp"

namespace project {

namespace {

constexpr U32 ADDR = 0x44;

// Register addresses, repeated here rather than reached into the component,
// so that a typo in the component's register map is caught by the tests
// instead of being mirrored by them.
constexpr U8 REG_RESULT_MSB = 0x00;
constexpr U8 REG_RESULT_LSB = 0x01;
constexpr U8 REG_CONFIGURATION = 0x0A;
constexpr U8 REG_FLAGS = 0x0C;
constexpr U8 REG_DEVICE_ID = 0x11;

constexpr U16 FLAG_CONVERSION_READY = 0x0004;

// A healthy part: OPT4003 ID, and a result of exponent 3, mantissa 0x12345,
// with counter 0xA and CRC 0x7 in the low byte of the LSB register.
constexpr U16 RAW_DEVICE_ID = 0x0221;
constexpr U16 RAW_RESULT_MSB = 0x3123;
constexpr U16 RAW_RESULT_LSB = 0x45A7;

// 0x12345 << 3 = 596520 counts, exactly representable as an F32.
const F32 EXPECTED_LUX = static_cast<F32>(596520) * 535e-6F;

}  // namespace

// C++14: static constexpr members bound to a reference by the gtest macros
// are odr-used and need an out-of-line definition.
constexpr FwSizeType OPT4003Tester::MAX_HISTORY_SIZE;
constexpr U32 OPT4003Tester::MAX_TRANSACTIONS;
constexpr U32 OPT4003Tester::MAX_WRITE_PAYLOAD;

OPT4003Tester::OPT4003Tester()
    : OPT4003GTestBase("OPT4003Tester", MAX_HISTORY_SIZE),
      component("OPT4003"),
      m_deviceId(RAW_DEVICE_ID),
      m_resultMsb(RAW_RESULT_MSB),
      m_resultLsb(RAW_RESULT_LSB),
      m_flags(0) {
    this->initComponents();
    this->connectPorts();
}

OPT4003Tester::~OPT4003Tester() {
    // Frees what initComponents() allocated. A no-op for a passive
    // component, but keeps the tester leak-free if the kind ever changes.
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Fake bus
// ----------------------------------------------------------------------

U16 OPT4003Tester::valueForRegister(U8 reg) {
    switch (reg) {
        case REG_DEVICE_ID:
            return this->m_deviceId;
        case REG_RESULT_MSB:
            return this->m_resultMsb;
        case REG_RESULT_LSB:
            return this->m_resultLsb;
        case REG_FLAGS:
            if (this->m_neverReady) {
                return this->m_flags;
            }
            if (this->m_pollsUntilReady > 0) {
                --this->m_pollsUntilReady;
                return this->m_flags;
            }
            return static_cast<U16>(this->m_flags | FLAG_CONVERSION_READY);
        default:
            return 0;
    }
}

Drv::I2cStatus OPT4003Tester::from_busWriteRead_handler(FwIndexType portNum,
                                                        U32 address,
                                                        Fw::Buffer& writeBuffer,
                                                        Fw::Buffer& readBuffer) {
    // Exactly one register byte out and two data bytes back. EXPECT rather
    // than ASSERT because ASSERT_* expands to a bare `return`.
    EXPECT_NE(writeBuffer.getData(), nullptr);
    EXPECT_NE(readBuffer.getData(), nullptr);
    EXPECT_EQ(writeBuffer.getSize(), 1U);
    EXPECT_EQ(readBuffer.getSize(), 2U);
    if (writeBuffer.getData() == nullptr || readBuffer.getData() == nullptr || writeBuffer.getSize() < 1 ||
        readBuffer.getSize() < 2) {
        return Drv::I2cStatus::I2C_OTHER_ERR;
    }

    const U8 reg = writeBuffer.getData()[0];

    // Port history is deliberately not pushed here: a timed-out read makes
    // around a thousand of these calls and would overflow it.
    if (this->m_transactionCount < MAX_TRANSACTIONS) {
        this->m_transactions[this->m_transactionCount] = {address, reg, false};
    }
    ++this->m_transactionCount;
    this->m_flagsReads += (reg == REG_FLAGS) ? 1U : 0U;
    this->m_msbReads += (reg == REG_RESULT_MSB) ? 1U : 0U;
    this->m_lsbReads += (reg == REG_RESULT_LSB) ? 1U : 0U;

    // Injected failure: leave the read buffer alone, as a real driver would.
    if (this->m_failEverything || (this->m_failOne && reg == this->m_failRegister)) {
        return this->m_forcedStatus;
    }

    // Answer big-endian, high byte first, as the part does.
    const U16 value = this->valueForRegister(reg);
    readBuffer.getData()[0] = static_cast<U8>(value >> 8U);
    readBuffer.getData()[1] = static_cast<U8>(value & 0xFFU);
    return Drv::I2cStatus::I2C_OK;
}

Drv::I2cStatus OPT4003Tester::from_busWrite_handler(FwIndexType portNum, U32 address, Fw::Buffer& buffer) {
    ++this->m_writeCount;
    this->pushFromPortEntry_busWrite(address, buffer);

    // Copy the payload out now, while the component's buffer is in scope.
    this->m_lastWriteSize = static_cast<U32>(buffer.getSize());
    EXPECT_LE(this->m_lastWriteSize, MAX_WRITE_PAYLOAD);
    const U8 reg = (buffer.getData() != nullptr && buffer.getSize() > 0) ? buffer.getData()[0] : 0;
    if (buffer.getData() != nullptr && this->m_lastWriteSize <= MAX_WRITE_PAYLOAD) {
        for (U32 i = 0; i < this->m_lastWriteSize; ++i) {
            this->m_lastWrite[i] = buffer.getData()[i];
        }
    }

    if (this->m_transactionCount < MAX_TRANSACTIONS) {
        this->m_transactions[this->m_transactionCount] = {address, reg, true};
    }
    ++this->m_transactionCount;

    if (this->m_failEverything || (this->m_failOne && reg == this->m_failRegister)) {
        return this->m_forcedStatus;
    }
    return Drv::I2cStatus::I2C_OK;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void OPT4003Tester::bootCleanly() {
    this->failNothing();
    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::SUCCESS);
    this->resetRecording();
}

void OPT4003Tester::assertTransaction(U32 index, U32 address, U8 reg, bool write) const {
    ASSERT_LT(index, this->m_transactionCount) << "no transaction recorded at index " << index;
    ASSERT_LT(index, MAX_TRANSACTIONS) << "transaction " << index << " was not kept";
    EXPECT_EQ(this->m_transactions[index].address, address) << "wrong I2C address at transaction " << index;
    EXPECT_EQ(this->m_transactions[index].reg, reg) << "wrong register at transaction " << index;
    EXPECT_EQ(this->m_transactions[index].write, write) << "wrong direction at transaction " << index;
}

void OPT4003Tester::failEverything(Drv::I2cStatus status) {
    this->m_failEverything = true;
    this->m_failOne = false;
    this->m_forcedStatus = status;
}

void OPT4003Tester::failOnly(U8 reg, Drv::I2cStatus status) {
    this->m_failEverything = false;
    this->m_failOne = true;
    this->m_failRegister = reg;
    this->m_forcedStatus = status;
}

void OPT4003Tester::failNothing() {
    this->m_failEverything = false;
    this->m_failOne = false;
    this->m_forcedStatus = Drv::I2cStatus::I2C_OK;
}

void OPT4003Tester::resetRecording() {
    this->clearHistory();
    this->m_transactionCount = 0;
    this->m_flagsReads = 0;
    this->m_msbReads = 0;
    this->m_lsbReads = 0;
    this->m_writeCount = 0;
    this->m_lastWriteSize = 0;
}

// ----------------------------------------------------------------------
// Test cases
// ----------------------------------------------------------------------

void OPT4003Tester::testBoot() {
    this->failNothing();
    this->resetRecording();

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::SUCCESS);

    // The ID is checked before anything is written to the part.
    ASSERT_EQ(this->m_transactionCount, 2U);
    this->assertTransaction(0, ADDR, REG_DEVICE_ID, false);
    this->assertTransaction(1, ADDR, REG_CONFIGURATION, true);

    // CONFIGURATION = 0x3038, high byte first.
    ASSERT_from_busWrite_SIZE(1);
    EXPECT_EQ(this->fromPortHistory_busWrite->at(0).addr, ADDR);
    ASSERT_EQ(this->m_lastWriteSize, 3U);
    EXPECT_EQ(this->m_lastWrite[0], REG_CONFIGURATION);
    EXPECT_EQ(this->m_lastWrite[1], 0x30);
    EXPECT_EQ(this->m_lastWrite[2], 0x38);

    ASSERT_EVENTS_SIZE(0);
}

void OPT4003Tester::testBootWrongDeviceId() {
    // 0x0121 is the OPT4001, the closest part that might be fitted by mistake.
    this->m_deviceId = 0x0121;
    this->failNothing();
    this->resetRecording();

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);

    // Only the ID read: an unidentified part is never written to.
    ASSERT_EQ(this->m_transactionCount, 1U);
    ASSERT_EQ(this->m_writeCount, 0U);
    ASSERT_EVENTS_WrongDeviceId_SIZE(1);
    ASSERT_EVENTS_WrongDeviceId(0, 0x0121);

    // And it is treated as absent afterwards.
    this->resetRecording();
    F32 lux = -1.0F;
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::FAILURE);
    ASSERT_EQ(this->m_transactionCount, 0U);
    EXPECT_EQ(lux, -1.0F);
}

void OPT4003Tester::testBootBusFailure() {
    // The ID read fails.
    this->failOnly(REG_DEVICE_ID, Drv::I2cStatus::I2C_ADDRESS_ERR);
    this->resetRecording();
    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);
    ASSERT_EQ(this->m_writeCount, 0U);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_EVENTS_I2cError(0, REG_DEVICE_ID, Drv::I2cStatus::I2C_ADDRESS_ERR);

    // The ID reads back but the configuration write fails.
    this->failOnly(REG_CONFIGURATION, Drv::I2cStatus::I2C_WRITE_ERR);
    this->resetRecording();
    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_EVENTS_I2cError(0, REG_CONFIGURATION, Drv::I2cStatus::I2C_WRITE_ERR);

    // Either way the sensor is absent.
    this->failNothing();
    this->resetRecording();
    F32 lux = 0.0F;
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::FAILURE);
    ASSERT_EQ(this->m_transactionCount, 0U);
}

void OPT4003Tester::testReadBeforeBoot() {
    this->failNothing();
    this->resetRecording();

    F32 lux = -1.0F;
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::FAILURE);
    project::LightSensorFlags flags;
    ASSERT_EQ(this->invoke_to_readFlags(0, flags), Fw::Success::FAILURE);

    ASSERT_EQ(this->m_transactionCount, 0U);
    EXPECT_EQ(lux, -1.0F);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void OPT4003Tester::testRead() {
    this->bootCleanly();

    F32 lux = 0.0F;
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::SUCCESS);

    ASSERT_EQ(this->m_transactionCount, 3U);
    this->assertTransaction(0, ADDR, REG_FLAGS, false);
    this->assertTransaction(1, ADDR, REG_RESULT_MSB, false);
    this->assertTransaction(2, ADDR, REG_RESULT_LSB, false);

    EXPECT_EQ(lux, EXPECTED_LUX);
    ASSERT_TLM_Lux_SIZE(1);
    ASSERT_TLM_Lux(0, EXPECTED_LUX);
    ASSERT_EVENTS_SIZE(0);
}

void OPT4003Tester::testReadWaitsForConversion() {
    this->bootCleanly();
    this->m_pollsUntilReady = 3;

    F32 lux = 0.0F;
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::SUCCESS);

    // Three "not ready" polls, the fourth sees the flag, then the result.
    EXPECT_EQ(this->m_flagsReads, 4U);
    ASSERT_EQ(this->m_transactionCount, 6U);
    this->assertTransaction(4, ADDR, REG_RESULT_MSB, false);
    this->assertTransaction(5, ADDR, REG_RESULT_LSB, false);
    EXPECT_EQ(lux, EXPECTED_LUX);
}

void OPT4003Tester::testReadTimesOut() {
    this->bootCleanly();
    this->m_neverReady = true;

    Os::RawTime start;
    (void)start.now();
    F32 lux = 0.0F;
    const Fw::Success status = this->invoke_to_read(0, lux);
    Os::RawTime end;
    (void)end.now();
    U32 elapsedUs = 0;
    ASSERT_EQ(end.getDiffUsec(start, elapsedUs), Os::RawTime::Status::OP_OK);

    // The read gives up waiting after the timeout, not before...
    EXPECT_GE(elapsedUs, OPT4003::CONVERSION_TIMEOUT_US);
    EXPECT_GT(this->m_flagsReads, 1U);

    // ...and still returns the last completed conversion as a success.
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    EXPECT_EQ(this->m_msbReads, 1U);
    EXPECT_EQ(this->m_lsbReads, 1U);
    EXPECT_EQ(lux, EXPECTED_LUX);
    ASSERT_TLM_Lux_SIZE(1);
    ASSERT_EVENTS_SIZE(0);

    this->m_neverReady = false;
}

void OPT4003Tester::testReadResultFailure() {
    this->bootCleanly();
    this->failOnly(REG_RESULT_LSB, Drv::I2cStatus::I2C_READ_ERR);

    F32 lux = -1.0F;
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::FAILURE);

    // The caller's value is untouched, and nothing is published, so the
    // ground keeps the last good reading.
    EXPECT_EQ(lux, -1.0F);
    ASSERT_TLM_Lux_SIZE(0);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_EVENTS_I2cError(0, REG_RESULT_LSB, Drv::I2cStatus::I2C_READ_ERR);

    // A failed read does not un-boot the sensor: the next read goes ahead.
    this->failNothing();
    this->resetRecording();
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::SUCCESS);
    EXPECT_EQ(lux, EXPECTED_LUX);
}

void OPT4003Tester::testReadFlags() {
    this->bootCleanly();

    // Overload and high set, conversion ready set (not reported), low clear.
    this->m_flags = 0x000A;
    project::LightSensorFlags flags;
    ASSERT_EQ(this->invoke_to_readFlags(0, flags), Fw::Success::SUCCESS);

    ASSERT_EQ(this->m_transactionCount, 1U);
    this->assertTransaction(0, ADDR, REG_FLAGS, false);
    EXPECT_TRUE(flags.get_overload());
    EXPECT_TRUE(flags.get_thresholdHigh());
    EXPECT_FALSE(flags.get_thresholdLow());

    // The raw register goes to telemetry, conversion-ready bit included.
    ASSERT_TLM_Flags_SIZE(1);
    ASSERT_TLM_Flags(0, 0x000E);

    this->m_flags = 0;
}

void OPT4003Tester::testConfigureAddress() {
    // The +Z sun sensor's four parts sit at 0x54-0x57.
    constexpr U32 ALT_ADDR = 0x57;
    this->component.configure(ALT_ADDR);

    this->failNothing();
    this->resetRecording();
    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::SUCCESS);
    F32 lux = 0.0F;
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::SUCCESS);

    ASSERT_EQ(this->m_transactionCount, 5U);
    this->assertTransaction(0, ALT_ADDR, REG_DEVICE_ID, false);
    this->assertTransaction(1, ALT_ADDR, REG_CONFIGURATION, true);
    this->assertTransaction(2, ALT_ADDR, REG_FLAGS, false);
    this->assertTransaction(3, ALT_ADDR, REG_RESULT_MSB, false);
    this->assertTransaction(4, ALT_ADDR, REG_RESULT_LSB, false);

    this->component.configure(OPT4003::DEFAULT_DEVICE_ADDRESS);
}

void OPT4003Tester::testRunBootsThenSamples() {
    this->failNothing();
    this->resetRecording();

    // First tick: boot, then sample.
    this->invoke_to_run(0, 0);
    ASSERT_EQ(this->m_transactionCount, 5U);
    this->assertTransaction(0, ADDR, REG_DEVICE_ID, false);
    this->assertTransaction(1, ADDR, REG_CONFIGURATION, true);
    this->assertTransaction(2, ADDR, REG_FLAGS, false);
    ASSERT_TLM_Lux_SIZE(1);
    ASSERT_TLM_Lux(0, EXPECTED_LUX);

    // Later ticks only sample.
    this->resetRecording();
    this->invoke_to_run(0, 0);
    ASSERT_EQ(this->m_transactionCount, 3U);
    this->assertTransaction(0, ADDR, REG_FLAGS, false);
    ASSERT_TLM_Lux_SIZE(1);
    ASSERT_EVENTS_SIZE(0);
}

void OPT4003Tester::testErrorThrottle() {
    constexpr U32 THROTTLE_LIMIT = 5;
    this->bootCleanly();

    // Bus down: each read fails on its first FLAGS poll, one event apiece,
    // until the throttle latches.
    this->failEverything(Drv::I2cStatus::I2C_ADDRESS_ERR);
    F32 lux = 0.0F;
    for (U32 i = 0; i < THROTTLE_LIMIT + 2; ++i) {
        ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::FAILURE);
    }
    ASSERT_EVENTS_I2cError_SIZE(THROTTLE_LIMIT);

    // Bus recovers: a clean read re-arms the event.
    this->failNothing();
    this->resetRecording();
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::SUCCESS);
    ASSERT_EVENTS_SIZE(0);

    this->failEverything(Drv::I2cStatus::I2C_ADDRESS_ERR);
    this->resetRecording();
    ASSERT_EQ(this->invoke_to_read(0, lux), Fw::Success::FAILURE);
    ASSERT_EVENTS_I2cError_SIZE(1);
}

}  // namespace project

// ======================================================================
// \title  BMX160Tester.cpp
// \brief  Unit test harness for the BMX160 component.
// ======================================================================

#include "BMX160Tester.hpp"

#include <cstring>

namespace project {

namespace {

constexpr U32 DEVICE_ADDR = 0x68;

// Register addresses and values, repeated here rather than reached into the
// component, so that a typo in the component's private register map is
// caught by the tests instead of being mirrored by them.
constexpr U8 REG_CHIP_ID = 0x00;
constexpr U8 REG_ERR = 0x02;
constexpr U8 REG_PMU_STATUS = 0x03;
constexpr U8 REG_MAG_DATA = 0x04;
constexpr U8 REG_GYR_DATA = 0x0C;
constexpr U8 REG_ACC_DATA = 0x12;
constexpr U8 REG_STATUS = 0x1B;
constexpr U8 REG_ACC_CONF = 0x40;
constexpr U8 REG_ACC_RANGE = 0x41;
constexpr U8 REG_GYR_CONF = 0x42;
constexpr U8 REG_GYR_RANGE = 0x43;
constexpr U8 REG_MAG_CONF = 0x44;
constexpr U8 REG_MAG_IF_CONTROL = 0x4C;  // MAG_IF[1]
constexpr U8 REG_MAG_IF_READ = 0x4D;     // MAG_IF[2]
constexpr U8 REG_MAG_IF_WRITE = 0x4E;    // MAG_IF[3]
constexpr U8 REG_MAG_IF_DATA = 0x4F;     // MAG_IF[4]
constexpr U8 REG_CMD = 0x7E;

constexpr U8 CHIP_ID = 0xD8;

// PMU_STATUS of a part with every sensor running: accelerometer normal,
// gyroscope normal, magnetometer interface low power.
constexpr U8 PMU_ALL_RUNNING = 0x16;
constexpr U8 CMD_SOFT_RESET = 0xB6;
constexpr U8 CMD_ACCEL_NORMAL = 0x11;

// MAG_IF[1] fields.
constexpr U8 MAG_IF_MANUAL = 0x80;
constexpr U8 MAG_IF_BURST_MASK = 0x03;

// STATUS and ERR_REG bits.
constexpr U8 STATUS_MAG_MAN_OP = 0x04;
constexpr U8 ERR_DROP_CMD = 0x40;
constexpr U8 ERR_FATAL = 0x01;

// Transactions in each stage of boot; see testBootSequence.
constexpr U32 RESET_AND_ID_TRANSACTIONS = 3;
constexpr U32 MAG_INIT_TRANSACTIONS = 39;
constexpr U32 ACCEL_START = RESET_AND_ID_TRANSACTIONS + MAG_INIT_TRANSACTIONS;
constexpr U32 GYRO_START = ACCEL_START + 7;
constexpr U32 PMU_CHECK = GYRO_START + 7;
constexpr U32 BOOT_TRANSACTIONS = PMU_CHECK + 1;

// Raw axis counts used by the read tests: positive, negative and small, so
// a byte-order or sign mistake on any axis changes the result.
constexpr I16 RAW_X = 1000;
constexpr I16 RAW_Y = -2000;
constexpr I16 RAW_Z = 7;

// The vector the component should produce from the raw axes above at a
// given scale. Built with the same float operations as a straightforward
// decode, so results compare exactly.
project::ImuVector expectedVector(F32 scale) {
    return project::ImuVector(static_cast<F32>(RAW_X) * scale, static_cast<F32>(RAW_Y) * scale,
                              static_cast<F32>(RAW_Z) * scale);
}

// Raw magnetometer registers for the read test: X, Y, Z with status bits in
// their low bits, and RHALL, all little-endian.
constexpr U8 RAW_MAG[BMX160::MAG_DATA_BYTES] = {0x85, 0x0C, 0x33, 0xF8, 0xA1, 0xF6, 0x43, 0x6A};

}  // namespace

// This project builds as C++14, where a `static constexpr` member that is
// bound to a reference -- which is exactly what the gtest EXPECT_/ASSERT_
// macros do with their arguments -- is odr-used and therefore needs an
// out-of-line definition. Without these the tests fail to link.
constexpr U8 BMX160Tester::TRIM_REGS[BMX160::BMM_TRIM_LENGTH];
constexpr FwSizeType BMX160Tester::MAX_HISTORY_SIZE;
constexpr U32 BMX160Tester::REGISTER_COUNT;
constexpr U32 BMX160Tester::BMM_REGISTER_COUNT;
constexpr U32 BMX160Tester::MAX_TRANSACTIONS;

BMX160Tester::BMX160Tester() : BMX160GTestBase("BMX160Tester", MAX_HISTORY_SIZE), component("BMX160") {
    this->initComponents();
    this->connectPorts();
    this->m_registers[REG_CHIP_ID] = CHIP_ID;
    this->m_registers[REG_PMU_STATUS] = PMU_ALL_RUNNING;
    std::memcpy(&this->m_bmm[BMX160::BMM_TRIM_START], TRIM_REGS, sizeof TRIM_REGS);
}

BMX160Tester::~BMX160Tester() {
    // Free what initComponents() allocated. A passive component has no queue,
    // but every TEST builds a fresh tester, and this call stays correct if the
    // component's kind ever changes. macOS does not detect leaks here; the
    // Linux CI runner does.
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Test cases: boot
// ----------------------------------------------------------------------

void BMX160Tester::testBootSequence() {
    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::SUCCESS);

    // The full sequence, one line per I2C transaction. CMD and both range
    // registers are written read-modify-write; each range is then read back
    // to set the scale factor; each accelerometer and gyroscope data-rate
    // and power-mode write is followed by an ERR_REG check. Each BMM150
    // transfer polls STATUS once, since the fake completes them at once.
    ASSERT_EQ(this->m_transactionCount, BOOT_TRANSACTIONS);
    U32 i = 0;
    this->assertRead(i++, REG_CMD);  // soft reset
    this->assertWrite(i++, REG_CMD, CMD_SOFT_RESET);
    this->assertRead(i++, REG_CHIP_ID);  // identify

    this->assertWrite(i++, REG_CMD, 0x19);             // magnetometer interface on
    this->assertWrite(i++, REG_MAG_IF_CONTROL, 0x80);  // setup mode
    this->assertBmmWrite(i, 0x4B, 0x01);               // BMM150 out of suspend
    i += 3;
    this->assertBmmRead(i, 0x5D, 2);  // trim
    i += 4;
    this->assertBmmRead(i, 0x62, 2);
    i += 4;
    this->assertBmmRead(i, 0x64, 2);
    i += 4;
    this->assertBmmRead(i, 0x68, 8);
    i += 4;
    this->assertBmmRead(i, 0x70, 2);
    i += 4;
    this->assertWrite(i++, REG_MAG_IF_CONTROL, 0x80);  // back to 1-byte bursts
    this->assertBmmWrite(i, 0x51, 0x17);               // XY repetitions
    i += 3;
    this->assertBmmWrite(i, 0x52, 0x52);  // Z repetitions
    i += 3;
    this->assertBmmWrite(i, 0x4C, 0x02);  // forced mode
    i += 3;
    this->assertWrite(i++, REG_MAG_IF_READ, 0x42);     // data mode reads X onward
    this->assertWrite(i++, REG_MAG_CONF, 0x05);        // 12.5 Hz
    this->assertWrite(i++, REG_MAG_IF_CONTROL, 0x03);  // data mode, 8-byte bursts
    this->assertWrite(i++, REG_CMD, 0x1A);             // interface low power
    ASSERT_EQ(i, ACCEL_START);

    this->assertRead(i++, REG_ACC_RANGE);  // accelerometer: +/-8 g
    this->assertWrite(i++, REG_ACC_RANGE, 0x08);
    this->assertRead(i++, REG_ACC_RANGE);
    this->assertWrite(i++, REG_ACC_CONF, 0x06);  // 25 Hz, OSR4
    this->assertRead(i++, REG_ERR);
    this->assertWrite(i++, REG_CMD, CMD_ACCEL_NORMAL);  // normal power mode
    this->assertRead(i++, REG_ERR);
    ASSERT_EQ(i, GYRO_START);

    this->assertRead(i++, REG_GYR_RANGE);  // gyroscope: +/-500 deg/s
    this->assertWrite(i++, REG_GYR_RANGE, 0x02);
    this->assertRead(i++, REG_GYR_RANGE);
    this->assertWrite(i++, REG_GYR_CONF, 0x07);  // 50 Hz, OSR4
    this->assertRead(i++, REG_ERR);
    this->assertWrite(i++, REG_CMD, 0x15);  // normal power mode
    this->assertRead(i++, REG_ERR);
    ASSERT_EQ(i, PMU_CHECK);

    this->assertRead(i++, REG_PMU_STATUS);  // power modes reached, reported

    // The setup-mode writes reached the BMM150.
    EXPECT_EQ(this->m_bmm[0x4B], 0x01);
    EXPECT_EQ(this->m_bmm[0x51], 0x17);
    EXPECT_EQ(this->m_bmm[0x52], 0x52);
    EXPECT_EQ(this->m_bmm[0x4C], 0x02);

    ASSERT_EVENTS_SIZE(0);
    ASSERT_TLM_PmuStatus_SIZE(1);
    ASSERT_TLM_PmuStatus(0, PMU_ALL_RUNNING);
}

void BMX160Tester::testBootWrongChipId() {
    this->m_registers[REG_CHIP_ID] = 0xD1;  // a BMI160, not a BMX160

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);

    // Reset and identify only: nothing is configured on an unknown part.
    ASSERT_EQ(this->m_transactionCount, RESET_AND_ID_TRANSACTIONS);
    this->assertRead(2, REG_CHIP_ID);
    ASSERT_EVENTS_WrongChipId_SIZE(1);
    ASSERT_EVENTS_WrongChipId(0, 0xD1);

    // The part stays absent.
    this->resetRecording();
    project::ImuVector sample;
    EXPECT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(this->m_transactionCount, 0U);
}

void BMX160Tester::testBootBusFailure() {
    this->failOnly(REG_CHIP_ID, Drv::I2cStatus::I2C_ADDRESS_ERR);

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);

    // Nothing after the failed identify.
    ASSERT_EQ(this->m_transactionCount, RESET_AND_ID_TRANSACTIONS);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_EVENTS_I2cError(0, REG_CHIP_ID, Drv::I2cStatus::I2C_ADDRESS_ERR);
    ASSERT_EVENTS_WrongChipId_SIZE(0);

    this->failNothing();
    this->resetRecording();
    project::ImuVector sample;
    EXPECT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(this->m_transactionCount, 0U);
}

void BMX160Tester::testBootSettingRejected() {
    // error_code 2: the data rate and bandwidth are not compatible.
    this->m_registers[REG_ERR] = 0x04;

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);

    // Boot stops at the ERR_REG check after the accelerometer data rate.
    // The magnetometer bring-up before it does not check ERR_REG.
    ASSERT_EQ(this->m_transactionCount, ACCEL_START + 5);
    this->assertWrite(ACCEL_START + 3, REG_ACC_CONF, 0x06);
    this->assertRead(ACCEL_START + 4, REG_ERR);
    ASSERT_EVENTS_SettingRejected_SIZE(1);
    ASSERT_EVENTS_SettingRejected(0, REG_ACC_CONF, 0x04);

    this->resetRecording();
    project::ImuVector sample;
    EXPECT_EQ(this->invoke_to_readAccel(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(this->m_transactionCount, 0U);
}

void BMX160Tester::testBootCommandDropped() {
    // The data-rate check only looks at error_code, so it passes, and the
    // read clears the drop flag left by the magnetometer commands; the
    // power-mode check rejects any flag, so the dropped accelerometer
    // command stops boot.
    this->m_dropCommands = true;

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);

    ASSERT_EQ(this->m_transactionCount, ACCEL_START + 7);
    this->assertWrite(ACCEL_START + 5, REG_CMD, CMD_ACCEL_NORMAL);
    this->assertRead(ACCEL_START + 6, REG_ERR);
    ASSERT_EVENTS_SettingRejected_SIZE(1);
    ASSERT_EVENTS_SettingRejected(0, REG_CMD, ERR_DROP_CMD);
}

void BMX160Tester::testBootInvalidRangeReadback() {
    // Accelerometer: code 0x10 maps past the end of the range table.
    this->m_holdOne = true;
    this->m_heldRegister = REG_ACC_RANGE;
    this->m_registers[REG_ACC_RANGE] = 0x10;

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);
    ASSERT_EQ(this->m_transactionCount, ACCEL_START + 3);
    this->assertRead(ACCEL_START + 2, REG_ACC_RANGE);
    ASSERT_EVENTS_InvalidRangeReadback_SIZE(1);
    ASSERT_EVENTS_InvalidRangeReadback(0, REG_ACC_RANGE, 0x10);

    // Gyroscope: code 0x07 is not one of the five ranges.
    this->resetRecording();
    this->m_heldRegister = REG_GYR_RANGE;
    this->m_registers[REG_ACC_RANGE] = 0x00;
    this->m_registers[REG_GYR_RANGE] = 0x07;

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);
    ASSERT_EQ(this->m_transactionCount, GYRO_START + 3);
    this->assertRead(GYRO_START + 2, REG_GYR_RANGE);
    ASSERT_EVENTS_InvalidRangeReadback_SIZE(1);
    ASSERT_EVENTS_InvalidRangeReadback(0, REG_GYR_RANGE, 0x07);

    // The part stays absent.
    this->resetRecording();
    project::ImuVector sample;
    EXPECT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(this->m_transactionCount, 0U);
}

void BMX160Tester::testBootMagTransferTimeout() {
    // mag_man_op stuck high: the first transfer, waking the BMM150, never
    // completes.
    this->m_registers[REG_STATUS] = STATUS_MAG_MAN_OP;

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::FAILURE);
    ASSERT_EVENTS_MagTransferTimeout_SIZE(1);
    ASSERT_EVENTS_MagTransferTimeout(0, 0x4B);

    // Everything after the transfer was started is STATUS polling: nothing
    // further was configured.
    const U32 firstPoll = RESET_AND_ID_TRANSACTIONS + 4;
    ASSERT_GT(this->m_transactionCount, firstPoll);
    const U32 recorded = (this->m_transactionCount < MAX_TRANSACTIONS) ? this->m_transactionCount : MAX_TRANSACTIONS;
    for (U32 i = firstPoll; i < recorded; i++) {
        this->assertRead(i, REG_STATUS);
    }

    this->resetRecording();
    project::ImuVector sample;
    EXPECT_EQ(this->invoke_to_readMag(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(this->m_transactionCount, 0U);
}

// ----------------------------------------------------------------------
// Test cases: reads
// ----------------------------------------------------------------------

void BMX160Tester::testReadBeforeBoot() {
    project::ImuVector sample;
    project::GyroRange range;
    project::ImuErrors errors;

    EXPECT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(this->invoke_to_readAccel(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(this->invoke_to_readMag(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(this->invoke_to_setGyroRange(0, project::GyroRange::DPS_2000), Fw::Success::FAILURE);
    EXPECT_EQ(this->invoke_to_getGyroRange(0, range), Fw::Success::FAILURE);
    EXPECT_EQ(this->invoke_to_readErrors(0, errors), Fw::Success::FAILURE);

    EXPECT_EQ(this->m_transactionCount, 0U);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void BMX160Tester::testReadGyro() {
    this->bootCleanly();
    this->loadAxes(REG_GYR_DATA, RAW_X, RAW_Y, RAW_Z);

    project::ImuVector sample;
    ASSERT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::SUCCESS);

    // One burst read of all six bytes, so the axes come from one sample.
    ASSERT_EQ(this->m_transactionCount, 1U);
    this->assertRead(0, REG_GYR_DATA, 6);

    const project::ImuVector expected = expectedVector(BMX160::gyroScale(project::GyroRange::DPS_500));
    EXPECT_EQ(sample, expected);
    ASSERT_TLM_Gyro_SIZE(1);
    ASSERT_TLM_Gyro(0, expected);
    ASSERT_TLM_Accel_SIZE(0);
    ASSERT_TLM_Mag_SIZE(0);
}

void BMX160Tester::testReadAccel() {
    this->bootCleanly();
    this->loadAxes(REG_ACC_DATA, RAW_X, RAW_Y, RAW_Z);

    project::ImuVector sample;
    ASSERT_EQ(this->invoke_to_readAccel(0, sample), Fw::Success::SUCCESS);

    ASSERT_EQ(this->m_transactionCount, 1U);
    this->assertRead(0, REG_ACC_DATA, 6);

    const project::ImuVector expected = expectedVector(BMX160::accelScale(0x08));
    EXPECT_EQ(sample, expected);
    ASSERT_TLM_Accel_SIZE(1);
    ASSERT_TLM_Accel(0, expected);
    ASSERT_TLM_Gyro_SIZE(0);
    ASSERT_TLM_Mag_SIZE(0);
}

void BMX160Tester::testReadMag() {
    this->bootCleanly();
    std::memcpy(&this->m_registers[REG_MAG_DATA], RAW_MAG, sizeof RAW_MAG);

    project::ImuVector sample;
    ASSERT_EQ(this->invoke_to_readMag(0, sample), Fw::Success::SUCCESS);

    // One burst read of X, Y, Z and RHALL; no interface traffic, since data
    // mode keeps DATA current by itself.
    ASSERT_EQ(this->m_transactionCount, 1U);
    this->assertRead(0, REG_MAG_DATA, 8);

    // Compensated with the trim this part reported at boot.
    const project::ImuVector expected = BMX160::decodeMag(RAW_MAG, BMX160::decodeTrim(TRIM_REGS));
    EXPECT_EQ(sample, expected);
    ASSERT_TLM_Mag_SIZE(1);
    ASSERT_TLM_Mag(0, expected);
    ASSERT_TLM_Gyro_SIZE(0);
    ASSERT_TLM_Accel_SIZE(0);
}

void BMX160Tester::testReadFailure() {
    this->bootCleanly();
    this->failOnly(REG_GYR_DATA, Drv::I2cStatus::I2C_READ_ERR);

    // The caller's struct is left exactly as it was passed in.
    project::ImuVector sample(1.0F, 2.0F, 3.0F);
    ASSERT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::FAILURE);
    EXPECT_EQ(sample, project::ImuVector(1.0F, 2.0F, 3.0F));

    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_I2cError_SIZE(1);
    ASSERT_EVENTS_I2cError(0, REG_GYR_DATA, Drv::I2cStatus::I2C_READ_ERR);
}

void BMX160Tester::testSetGyroRange() {
    this->bootCleanly();

    ASSERT_EQ(this->invoke_to_setGyroRange(0, project::GyroRange::DPS_2000), Fw::Success::SUCCESS);

    // Read-modify-write, then read back.
    ASSERT_EQ(this->m_transactionCount, 3U);
    this->assertRead(0, REG_GYR_RANGE);
    this->assertWrite(1, REG_GYR_RANGE, 0x00);
    this->assertRead(2, REG_GYR_RANGE);

    // Later reads use the new scale.
    this->loadAxes(REG_GYR_DATA, RAW_X, RAW_Y, RAW_Z);
    project::ImuVector sample;
    ASSERT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::SUCCESS);
    EXPECT_EQ(sample, expectedVector(BMX160::gyroScale(project::GyroRange::DPS_2000)));

    // And the part reports the new range.
    this->resetRecording();
    project::GyroRange range = project::GyroRange::DPS_125;
    ASSERT_EQ(this->invoke_to_getGyroRange(0, range), Fw::Success::SUCCESS);
    EXPECT_EQ(range, project::GyroRange::DPS_2000);
    ASSERT_EQ(this->m_transactionCount, 1U);
    this->assertRead(0, REG_GYR_RANGE);
}

void BMX160Tester::testReadErrors() {
    this->bootCleanly();
    this->m_registers[REG_ERR] = ERR_DROP_CMD | ERR_FATAL;

    project::ImuErrors errors;
    ASSERT_EQ(this->invoke_to_readErrors(0, errors), Fw::Success::SUCCESS);

    // Two reads. The first clears drop_cmd_err, so it must be taken from
    // the first read; fatal_err survives and is taken from the second.
    ASSERT_EQ(this->m_transactionCount, 2U);
    this->assertRead(0, REG_ERR);
    this->assertRead(1, REG_ERR);
    EXPECT_TRUE(errors.get_dropCommandError());
    EXPECT_TRUE(errors.get_fatalError());

    // A healthy part reports nothing.
    this->m_registers[REG_ERR] = 0x00;
    ASSERT_EQ(this->invoke_to_readErrors(0, errors), Fw::Success::SUCCESS);
    EXPECT_FALSE(errors.get_dropCommandError());
    EXPECT_FALSE(errors.get_fatalError());
}

// ----------------------------------------------------------------------
// Test cases: wiring and reporting
// ----------------------------------------------------------------------

void BMX160Tester::testConfigureAddress() {
    this->component.configure(0x69);

    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::SUCCESS);
    ASSERT_EQ(this->m_transactionCount, BOOT_TRANSACTIONS);
    for (U32 i = 0; i < BOOT_TRANSACTIONS; i++) {
        EXPECT_EQ(this->m_transactions[i].address, 0x69U) << "transaction " << i;
    }

    this->resetRecording();
    project::ImuVector sample;
    ASSERT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::SUCCESS);
    this->assertRead(0, REG_GYR_DATA, 6, 0x69);
}

void BMX160Tester::testRunBootsThenSamples() {
    this->loadAxes(REG_GYR_DATA, RAW_X, RAW_Y, RAW_Z);
    this->loadAxes(REG_ACC_DATA, RAW_X, RAW_Y, RAW_Z);

    // First tick: boot, then one read of each sensor. The magnetometer data
    // is loaded after boot, since boot's trim reads pass through DATA.
    this->invoke_to_run(0, 0);
    ASSERT_EQ(this->m_transactionCount, BOOT_TRANSACTIONS + 3);
    this->assertRead(BOOT_TRANSACTIONS, REG_GYR_DATA, 6);
    this->assertRead(BOOT_TRANSACTIONS + 1, REG_ACC_DATA, 6);
    this->assertRead(BOOT_TRANSACTIONS + 2, REG_MAG_DATA, 8);
    ASSERT_TLM_Gyro_SIZE(1);
    ASSERT_TLM_Accel_SIZE(1);
    ASSERT_TLM_Mag_SIZE(1);

    // Second tick: already booted, so only the three reads.
    this->resetRecording();
    std::memcpy(&this->m_registers[REG_MAG_DATA], RAW_MAG, sizeof RAW_MAG);
    this->invoke_to_run(0, 0);
    ASSERT_EQ(this->m_transactionCount, 3U);
    this->assertRead(0, REG_GYR_DATA, 6);
    this->assertRead(1, REG_ACC_DATA, 6);
    this->assertRead(2, REG_MAG_DATA, 8);
    ASSERT_TLM_Mag_SIZE(1);
    ASSERT_TLM_Mag(0, BMX160::decodeMag(RAW_MAG, BMX160::decodeTrim(TRIM_REGS)));
}

void BMX160Tester::testErrorThrottle() {
    this->bootCleanly();
    this->failEverything(Drv::I2cStatus::I2C_OTHER_ERR);

    // Seven failures, five events.
    project::ImuVector sample;
    for (U32 i = 0; i < 7; i++) {
        EXPECT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::FAILURE);
    }
    ASSERT_EVENTS_I2cError_SIZE(5);

    // A clean read re-arms the event.
    this->failNothing();
    ASSERT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::SUCCESS);
    this->clearHistory();
    this->failEverything(Drv::I2cStatus::I2C_OTHER_ERR);
    EXPECT_EQ(this->invoke_to_readGyro(0, sample), Fw::Success::FAILURE);
    ASSERT_EVENTS_I2cError_SIZE(1);
}

// ----------------------------------------------------------------------
// Fake bus
// ----------------------------------------------------------------------

Drv::I2cStatus BMX160Tester::from_busWriteRead_handler(FwIndexType portNum,
                                                       U32 address,
                                                       Fw::Buffer& writeBuffer,
                                                       Fw::Buffer& readBuffer) {
    EXPECT_EQ(writeBuffer.getSize(), 1U);
    const U8 reg = writeBuffer.getData()[0];
    const U32 count = static_cast<U32>(readBuffer.getSize());

    if (this->m_transactionCount < MAX_TRANSACTIONS) {
        this->m_transactions[this->m_transactionCount] = {address, reg, false, 0, count};
    }
    this->m_transactionCount++;

    if (this->m_failEverything || (this->m_failOne && reg == this->m_failRegister)) {
        return this->m_forcedStatus;
    }

    // Burst read: consecutive registers, as the part auto-increments.
    EXPECT_LE(reg + count, REGISTER_COUNT);
    for (U32 offset = 0; offset < count && reg + offset < REGISTER_COUNT; offset++) {
        readBuffer.getData()[offset] = this->m_registers[reg + offset];
    }

    // Reading ERR_REG clears every flag but fatal_err.
    if (reg <= REG_ERR && REG_ERR < reg + count) {
        this->m_registers[REG_ERR] &= ERR_FATAL;
    }
    return Drv::I2cStatus::I2C_OK;
}

Drv::I2cStatus BMX160Tester::from_busWrite_handler(FwIndexType portNum, U32 address, Fw::Buffer& buffer) {
    EXPECT_EQ(buffer.getSize(), 2U);
    const U8 reg = buffer.getData()[0];
    const U8 value = buffer.getData()[1];

    if (this->m_transactionCount < MAX_TRANSACTIONS) {
        this->m_transactions[this->m_transactionCount] = {address, reg, true, value, 1};
    }
    this->m_transactionCount++;

    if (this->m_failEverything || (this->m_failOne && reg == this->m_failRegister)) {
        return this->m_forcedStatus;
    }

    if (reg == REG_CMD) {
        // CMD is write-only: it triggers an action and always reads back 0.
        // Soft reset is never dropped.
        if (this->m_dropCommands && value != CMD_SOFT_RESET) {
            this->m_registers[REG_ERR] |= ERR_DROP_CMD;
        }
        return Drv::I2cStatus::I2C_OK;
    }
    if (reg < REGISTER_COUNT && !(this->m_holdOne && reg == this->m_heldRegister)) {
        this->m_registers[reg] = value;
    }

    // Setup-mode transfers to and from the fake BMM150.
    const U8 control = this->m_registers[REG_MAG_IF_CONTROL];
    if ((control & MAG_IF_MANUAL) != 0U) {
        if (reg == REG_MAG_IF_WRITE) {
            this->m_bmm[value] = this->m_registers[REG_MAG_IF_DATA];
        } else if (reg == REG_MAG_IF_READ) {
            static const U32 burstLengths[4] = {1, 2, 6, 8};
            const U32 count = burstLengths[control & MAG_IF_BURST_MASK];
            for (U32 offset = 0; offset < count && value + offset < BMM_REGISTER_COUNT; offset++) {
                this->m_registers[REG_MAG_DATA + offset] = this->m_bmm[value + offset];
            }
        }
    }
    return Drv::I2cStatus::I2C_OK;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void BMX160Tester::bootCleanly() {
    ASSERT_EQ(this->invoke_to_boot(0), Fw::Success::SUCCESS);
    this->resetRecording();
}

void BMX160Tester::loadAxes(U8 start, I16 x, I16 y, I16 z) {
    const I16 axes[3] = {x, y, z};
    for (U32 axis = 0; axis < 3; axis++) {
        const U16 raw = static_cast<U16>(axes[axis]);
        this->m_registers[start + 2 * axis] = static_cast<U8>(raw & 0xFFU);
        this->m_registers[start + 2 * axis + 1] = static_cast<U8>(raw >> 8);
    }
}

void BMX160Tester::assertBmmWrite(U32 index, U8 bmmReg, U8 value) const {
    this->assertWrite(index, REG_MAG_IF_DATA, value);
    this->assertWrite(index + 1, REG_MAG_IF_WRITE, bmmReg);
    this->assertRead(index + 2, REG_STATUS);
}

void BMX160Tester::assertBmmRead(U32 index, U8 bmmReg, U32 count) const {
    const U8 burst = (count == 8U) ? 0x03 : 0x01;
    this->assertWrite(index, REG_MAG_IF_CONTROL, static_cast<U8>(MAG_IF_MANUAL | burst));
    this->assertWrite(index + 1, REG_MAG_IF_READ, bmmReg);
    this->assertRead(index + 2, REG_STATUS);
    this->assertRead(index + 3, REG_MAG_DATA, count);
}

void BMX160Tester::assertRead(U32 index, U8 reg, U32 length, U32 address) const {
    ASSERT_LT(index, this->m_transactionCount);
    ASSERT_LT(index, MAX_TRANSACTIONS);
    const Transaction& t = this->m_transactions[index];
    EXPECT_EQ(t.address, address) << "transaction " << index;
    EXPECT_EQ(t.reg, reg) << "transaction " << index;
    EXPECT_FALSE(t.write) << "transaction " << index;
    EXPECT_EQ(t.length, length) << "transaction " << index;
}

void BMX160Tester::assertWrite(U32 index, U8 reg, U8 value, U32 address) const {
    ASSERT_LT(index, this->m_transactionCount);
    ASSERT_LT(index, MAX_TRANSACTIONS);
    const Transaction& t = this->m_transactions[index];
    EXPECT_EQ(t.address, address) << "transaction " << index;
    EXPECT_EQ(t.reg, reg) << "transaction " << index;
    EXPECT_TRUE(t.write) << "transaction " << index;
    EXPECT_EQ(t.value, value) << "transaction " << index;
}

void BMX160Tester::failEverything(Drv::I2cStatus status) {
    this->m_failEverything = true;
    this->m_failOne = false;
    this->m_forcedStatus = status;
}

void BMX160Tester::failOnly(U8 reg, Drv::I2cStatus status) {
    this->m_failEverything = false;
    this->m_failOne = true;
    this->m_failRegister = reg;
    this->m_forcedStatus = status;
}

void BMX160Tester::failNothing() {
    this->m_failEverything = false;
    this->m_failOne = false;
}

void BMX160Tester::resetRecording() {
    this->m_transactionCount = 0;
    std::memset(this->m_transactions, 0, sizeof this->m_transactions);
    this->clearHistory();
}

}  // namespace project

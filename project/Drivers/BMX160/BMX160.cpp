// ======================================================================
// \title  BMX160.cpp
// \brief  BMX160 accelerometer and gyroscope device manager implementation.
// ======================================================================

#include "project/Drivers/BMX160/BMX160.hpp"

#include "Fw/Types/Assert.hpp"
#include "Os/RawTime.hpp"
#include "Os/Task.hpp"

namespace project {

namespace {

// BMM150 sentinels for an axis whose measurement overflowed, after the
// fields are shifted down to their 13-bit (X, Y) and 15-bit (Z) width.
constexpr I16 MAG_OVERFLOW_XY = -4096;
constexpr I16 MAG_OVERFLOW_Z = -16384;

// Arithmetic right shift (rounding toward negative infinity). Shifting a
// negative value right is implementation-defined before C++20, and this
// project builds as C++14, so the negative case is spelled out.
I16 shiftRightSigned(I16 value, U32 bits) {
    if (value >= 0) {
        return static_cast<I16>(value >> bits);
    }
    return static_cast<I16>(-((-(value + 1)) >> bits) - 1);
}

// Temperature-compensate one X or Y reading, per the BMM150 data sheet's
// floating-point formula. `axisTrim` and `axisOffset` are dig_x2/dig_x1 or
// dig_y2/dig_y1.
F32 compensateXy(I16 adc, U16 rhall, I8 axisTrim, I8 axisOffset, const BMX160::MagTrim& trim) {
    if (adc == MAG_OVERFLOW_XY || rhall == 0U || trim.xyz1 == 0U) {
        return 0.0F;
    }
    const F32 p0 = static_cast<F32>(trim.xyz1) * 16384.0F / static_cast<F32>(rhall) - 16384.0F;
    const F32 p1 = static_cast<F32>(trim.xy2) * (p0 * p0 / 268435456.0F);
    const F32 p2 = p1 + p0 * static_cast<F32>(trim.xy1) / 16384.0F;
    const F32 p4 = static_cast<F32>(adc) * ((p2 + 256.0F) * (static_cast<F32>(axisTrim) + 160.0F));
    return (p4 / 8192.0F + static_cast<F32>(axisOffset) * 8.0F) / 16.0F;
}

// Temperature-compensate one Z reading, per the BMM150 data sheet's
// floating-point formula.
F32 compensateZ(I16 adc, U16 rhall, const BMX160::MagTrim& trim) {
    if (adc == MAG_OVERFLOW_Z) {
        return 0.0F;
    }
    if (trim.z2 == 0 || trim.z1 == 0U || rhall == 0U || trim.xyz1 == 0U) {
        return 0.0F;
    }
    const I32 p0 = static_cast<I32>(adc) - static_cast<I32>(trim.z4);
    const I32 p1 = static_cast<I32>(rhall) - static_cast<I32>(trim.xyz1);
    const I32 p2 = static_cast<I32>(trim.z3) * p1;
    const F32 p3 = static_cast<F32>(trim.z2) + static_cast<F32>(trim.z1) * static_cast<F32>(rhall) / 32768.0F;
    if (p3 == 0.0F) {
        return 0.0F;
    }
    const F32 p5 = static_cast<F32>(p0) * 131072.0F - static_cast<F32>(p2);
    return p5 / (p3 * 64.0F);
}

}  // namespace

// This project builds as C++14, where a `static constexpr` member that is
// bound to a reference or indexed anywhere (the gtest macros bind them)
// needs an out-of-line definition, or the link fails.
constexpr U32 BMX160::DEFAULT_DEVICE_ADDRESS;
constexpr U8 BMX160::CHIP_ID;
constexpr U8 BMX160::CMD_SOFT_RESET;
constexpr U8 BMX160::CMD_ACCEL_NORMAL;
constexpr U8 BMX160::CMD_GYRO_NORMAL;
constexpr U8 BMX160::CMD_MAG_IF_NORMAL;
constexpr U8 BMX160::CMD_MAG_IF_LOW_POWER;
constexpr U8 BMX160::ACC_RANGE_8G;
constexpr U8 BMX160::ACC_CONF_BOOT;
constexpr U8 BMX160::GYR_CONF_BOOT;
constexpr GyroRange::T BMX160::GYRO_RANGE_BOOT;
constexpr U8 BMX160::MAG_IF_SETUP_MODE;
constexpr U8 BMX160::MAG_IF_BURST_2;
constexpr U8 BMX160::MAG_IF_BURST_8;
constexpr U8 BMX160::MAG_IF_DATA_MODE;
constexpr U8 BMX160::MAG_CONF_12_5HZ;
constexpr U8 BMX160::BMM_POWER_CONTROL;
constexpr U8 BMX160::BMM_POWER_ON;
constexpr U8 BMX160::BMM_OP_MODE;
constexpr U8 BMX160::BMM_FORCED_MODE;
constexpr U8 BMX160::BMM_REP_XY;
constexpr U8 BMX160::BMM_REP_XY_HIGH_ACCURACY;
constexpr U8 BMX160::BMM_REP_Z;
constexpr U8 BMX160::BMM_REP_Z_HIGH_ACCURACY;
constexpr U8 BMX160::BMM_DATA_X_LSB;
constexpr U8 BMX160::BMM_TRIM_START;
constexpr U32 BMX160::BMM_TRIM_LENGTH;
constexpr F32 BMX160::COUNTS_PER_FULL_SCALE;
constexpr F32 BMX160::GYRO_FULL_SCALE_DPS[GyroRange::NUM_CONSTANTS];
constexpr U32 BMX160::ACCEL_RANGE_COUNT;
constexpr F32 BMX160::ACCEL_FULL_SCALE_G[ACCEL_RANGE_COUNT];
constexpr F32 BMX160::DEG_TO_RAD;
constexpr F32 BMX160::STANDARD_GRAVITY;
constexpr F32 BMX160::MAG_UNTRIMMED_SCALE;
constexpr U32 BMX160::AXIS_DATA_BYTES;
constexpr U32 BMX160::MAG_DATA_BYTES;
constexpr U32 BMX160::SOFT_RESET_DELAY_US;
constexpr U32 BMX160::POST_WRITE_IDLE_US;
constexpr U32 BMX160::MAG_IF_STARTUP_DELAY_US;
constexpr U32 BMX160::MAG_WARMUP_DELAY_US;
constexpr U32 BMX160::MAG_TRANSFER_TIMEOUT_US;
constexpr U32 BMX160::ACCEL_STARTUP_DELAY_US;
constexpr U32 BMX160::GYRO_STARTUP_DELAY_US;

// ----------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------

BMX160::BMX160(const char* const compName)
    : BMX160ComponentBase(compName),
      m_deviceAddress(DEFAULT_DEVICE_ADDRESS),
      m_booted(false),
      m_gyroScale(0.0F),
      m_accelScale(0.0F),
      m_magTrim() {}

BMX160::~BMX160() {}

void BMX160::configure(U32 deviceAddress) {
    this->m_deviceAddress = deviceAddress;
}

// ----------------------------------------------------------------------
// Register decoding
// ----------------------------------------------------------------------

I16 BMX160::toSigned(U8 lsb, U8 msb) {
    const U16 raw = static_cast<U16>((static_cast<U16>(msb) << 8) | lsb);
    // Written as a branch rather than a plain static_cast<I16>: converting an
    // out-of-range unsigned value to a signed type is implementation-defined
    // before C++20, and this project builds as C++14. This form is well
    // defined under every standard and compiles to the same instruction.
    return (raw < 0x8000U) ? static_cast<I16>(raw) : static_cast<I16>(static_cast<I32>(raw) - 0x10000);
}

project::ImuVector BMX160::decodeAxes(const U8 bytes[AXIS_DATA_BYTES], F32 scale) {
    return project::ImuVector(static_cast<F32>(toSigned(bytes[0], bytes[1])) * scale,
                              static_cast<F32>(toSigned(bytes[2], bytes[3])) * scale,
                              static_cast<F32>(toSigned(bytes[4], bytes[5])) * scale);
}

F32 BMX160::gyroScale(GyroRange range) {
    // A GyroRange can only hold a valid value, so the index is in bounds.
    return (GYRO_FULL_SCALE_DPS[range.e] / COUNTS_PER_FULL_SCALE) * DEG_TO_RAD;
}

F32 BMX160::accelScale(U8 rangeCode) {
    const U32 index = static_cast<U32>(rangeCode >> 2);
    FW_ASSERT(index < ACCEL_RANGE_COUNT, static_cast<FwAssertArgType>(rangeCode));
    return (ACCEL_FULL_SCALE_G[index] / COUNTS_PER_FULL_SCALE) * STANDARD_GRAVITY;
}

project::ImuErrors BMX160::decodeErrors(U8 errReg) {
    return project::ImuErrors((errReg & ERR_DROP_CMD) != 0U, (errReg & ERR_FATAL) != 0U);
}

BMX160::MagTrim BMX160::decodeTrim(const U8 regs[BMM_TRIM_LENGTH]) {
    // Offsets are BMM150 register addresses minus BMM_TRIM_START. The 8-bit
    // terms are signed except xy1; the 16-bit terms are little-endian.
    MagTrim trim;
    trim.x1 = static_cast<I8>(regs[0x5D - BMM_TRIM_START]);
    trim.y1 = static_cast<I8>(regs[0x5E - BMM_TRIM_START]);
    trim.z4 = toSigned(regs[0x62 - BMM_TRIM_START], regs[0x63 - BMM_TRIM_START]);
    trim.x2 = static_cast<I8>(regs[0x64 - BMM_TRIM_START]);
    trim.y2 = static_cast<I8>(regs[0x65 - BMM_TRIM_START]);
    trim.z2 = toSigned(regs[0x68 - BMM_TRIM_START], regs[0x69 - BMM_TRIM_START]);
    trim.z1 = static_cast<U16>(regs[0x6A - BMM_TRIM_START] | (regs[0x6B - BMM_TRIM_START] << 8));
    // xyz1 is 15 bits wide; bit 15 of the pair is not part of it.
    trim.xyz1 = static_cast<U16>((regs[0x6C - BMM_TRIM_START] | (regs[0x6D - BMM_TRIM_START] << 8)) & 0x7FFF);
    trim.z3 = toSigned(regs[0x6E - BMM_TRIM_START], regs[0x6F - BMM_TRIM_START]);
    trim.xy2 = static_cast<I8>(regs[0x70 - BMM_TRIM_START]);
    trim.xy1 = regs[0x71 - BMM_TRIM_START];
    return trim;
}

project::ImuVector BMX160::decodeMag(const U8 bytes[MAG_DATA_BYTES], const MagTrim& trim) {
    // X and Y are 13-bit and Z 15-bit signed values, left-aligned in their
    // register pairs; RHALL is a 14-bit unsigned value. The low bits are
    // status flags, not part of the measurement.
    const I16 x = shiftRightSigned(toSigned(bytes[0], bytes[1]), 3);
    const I16 y = shiftRightSigned(toSigned(bytes[2], bytes[3]), 3);
    const I16 z = shiftRightSigned(toSigned(bytes[4], bytes[5]), 1);
    const U16 rhall = static_cast<U16>((bytes[6] | (bytes[7] << 8)) >> 2);

    if (trim.xyz1 == 0U) {
        return project::ImuVector(static_cast<F32>(x) * MAG_UNTRIMMED_SCALE, static_cast<F32>(y) * MAG_UNTRIMMED_SCALE,
                                  static_cast<F32>(z) * MAG_UNTRIMMED_SCALE);
    }
    return project::ImuVector(compensateXy(x, rhall, trim.x2, trim.x1, trim),
                              compensateXy(y, rhall, trim.y2, trim.y1, trim), compensateZ(z, rhall, trim));
}

// ----------------------------------------------------------------------
// Bus helpers
// ----------------------------------------------------------------------

Drv::I2cStatus BMX160::readRegisters(Register start, U8* data, U32 count) {
    // One write-read transaction: the start register out, `count` bytes
    // back. The part auto-increments the register address during the read.
    // The buffers are stack locals or the caller's, which is safe because
    // busWriteRead_out is synchronous.
    U8 registerAddress = static_cast<U8>(start);
    Fw::Buffer writeBuffer(&registerAddress, static_cast<Fw::Buffer::SizeType>(sizeof registerAddress));
    Fw::Buffer readBuffer(data, static_cast<Fw::Buffer::SizeType>(count));

    const Drv::I2cStatus status = this->busWriteRead_out(0, this->m_deviceAddress, writeBuffer, readBuffer);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->log_WARNING_HI_I2cError(static_cast<U8>(start), status);
    }
    return status;
}

Drv::I2cStatus BMX160::readRegister(Register reg, U8& value) {
    return this->readRegisters(reg, &value, 1);
}

Drv::I2cStatus BMX160::writeRegister(Register reg, U8 value) {
    U8 request[2] = {static_cast<U8>(reg), value};
    Fw::Buffer writeBuffer(request, static_cast<Fw::Buffer::SizeType>(sizeof request));

    const Drv::I2cStatus status = this->busWrite_out(0, this->m_deviceAddress, writeBuffer);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->log_WARNING_HI_I2cError(static_cast<U8>(reg), status);
    }
    // Waited after a failed write too: the part may have taken it anyway.
    this->delayUs(POST_WRITE_IDLE_US);
    return status;
}

// ----------------------------------------------------------------------
// Transaction patterns
// ----------------------------------------------------------------------

Drv::I2cStatus BMX160::readThenWrite(Register reg, U8 value) {
    U8 previous = 0;
    const Drv::I2cStatus status = this->readRegister(reg, previous);
    if (status != Drv::I2cStatus::I2C_OK) {
        return status;
    }
    return this->writeRegister(reg, value);
}

Fw::Success BMX160::writeAndCheck(Register reg, U8 value, U8 rejectMask) {
    if (this->writeRegister(reg, value) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    U8 errReg = 0;
    if (this->readRegister(ERR_REG_ADDR, errReg) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if ((errReg & rejectMask) != 0U) {
        this->log_WARNING_HI_SettingRejected(static_cast<U8>(reg), errReg);
        return Fw::Success::FAILURE;
    }
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::readAxes(Register start, F32 scale, project::ImuVector& sample) {
    U8 bytes[AXIS_DATA_BYTES] = {};
    if (this->readRegisters(start, bytes, AXIS_DATA_BYTES) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    sample = decodeAxes(bytes, scale);
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::waitForMagTransfer(U8 bmmReg) {
    Os::RawTime start;
    (void)start.now();
    while (true) {
        U8 status = 0;
        if (this->readRegister(STATUS_ADDR, status) != Drv::I2cStatus::I2C_OK) {
            return Fw::Success::FAILURE;
        }
        if ((status & STATUS_MAG_MAN_OP) == 0U) {
            return Fw::Success::SUCCESS;
        }
        Os::RawTime now;
        (void)now.now();
        U32 elapsedUs = 0;
        if (now.getDiffUsec(start, elapsedUs) != Os::RawTime::Status::OP_OK || elapsedUs > MAG_TRANSFER_TIMEOUT_US) {
            this->log_WARNING_HI_MagTransferTimeout(bmmReg);
            return Fw::Success::FAILURE;
        }
    }
}

Fw::Success BMX160::bmmWrite(U8 bmmReg, U8 value) {
    // The data byte first: writing the address is what starts the transfer.
    if (this->writeRegister(MAG_IF_WRITE_DATA_ADDR, value) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (this->writeRegister(MAG_IF_WRITE_ADDR, bmmReg) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    return this->waitForMagTransfer(bmmReg);
}

Fw::Success BMX160::bmmRead(U8 bmmReg, U8* data, U32 count) {
    FW_ASSERT(count == 2U || count == 8U, static_cast<FwAssertArgType>(count));
    // Setup mode stays on while the burst length changes; writing the
    // address starts the transfer, and the bytes land in DATA.
    const U8 burst = (count == 8U) ? MAG_IF_BURST_8 : MAG_IF_BURST_2;
    if (this->writeRegister(MAG_IF_CONTROL_ADDR, static_cast<U8>(MAG_IF_SETUP_MODE | burst)) !=
        Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (this->writeRegister(MAG_IF_READ_ADDR, bmmReg) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (this->waitForMagTransfer(bmmReg) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->readRegisters(MAG_DATA_ADDR, data, count) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    return Fw::Success::SUCCESS;
}

void BMX160::delayUs(U32 microseconds) {
    // The Zephyr Os::Task::delay sleeps in whole milliseconds and truncates
    // anything shorter, which would turn a 650 us wait into none at all.
    // Every delay here is a minimum the part requires, so round up.
    const U32 milliseconds = (microseconds + 999U) / 1000U;
    (void)Os::Task::delay(Fw::TimeInterval(milliseconds / 1000U, (milliseconds % 1000U) * 1000U));
}

// ----------------------------------------------------------------------
// Operations
// ----------------------------------------------------------------------

Fw::Success BMX160::bootDevice() {
    this->m_booted = false;

    // A soft reset returns every register to its reset value, so the
    // configuration below always starts from a known state. Soft reset is
    // the one command CMD accepts even while busy.
    if (this->readThenWrite(CMD_ADDR, CMD_SOFT_RESET) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    this->delayUs(SOFT_RESET_DELAY_US);

    // Identify the part before configuring it, so a misaddressed or
    // unexpected device is never written to.
    U8 chipId = 0;
    if (this->readRegister(CHIP_ID_ADDR, chipId) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (chipId != CHIP_ID) {
        this->log_WARNING_HI_WrongChipId(chipId);
        return Fw::Success::FAILURE;
    }

    // The magnetometer first: all three sensors' power modes are set
    // through CMD, and the magnetometer interface must be in normal mode
    // for the setup-mode transfers below.
    if (this->initMag() != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->initAccel() != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->initGyro() != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }

    // Report the power mode each sensor actually reached. ERR_REG only shows
    // a command that was refused; a sensor that never left suspend still
    // reads as zero, and this is what tells the two apart.
    U8 pmuStatus = 0;
    if (this->readRegister(PMU_STATUS_ADDR, pmuStatus) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    this->tlmWrite_PmuStatus(pmuStatus);

    this->m_booted = true;
    this->log_WARNING_HI_WrongChipId_ThrottleClear();
    this->log_WARNING_HI_SettingRejected_ThrottleClear();
    this->log_WARNING_HI_InvalidRangeReadback_ThrottleClear();
    this->log_WARNING_HI_MagTransferTimeout_ThrottleClear();
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::initMag() {
    // Power the magnetometer interface up and enter setup mode. This
    // command and the data mode settings below are not followed by an
    // ERR_REG check.
    if (this->writeRegister(CMD_ADDR, CMD_MAG_IF_NORMAL) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    this->delayUs(MAG_IF_STARTUP_DELAY_US);
    if (this->writeRegister(MAG_IF_CONTROL_ADDR, MAG_IF_SETUP_MODE) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }

    // Wake the BMM150 from suspend, where its trim registers cannot be read.
    if (this->bmmWrite(BMM_POWER_CONTROL, BMM_POWER_ON) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->readMagTrim() != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    // The trim reads changed the burst length; return to 1-byte bursts.
    if (this->writeRegister(MAG_IF_CONTROL_ADDR, MAG_IF_SETUP_MODE) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }

    // High-accuracy repetitions, then forced mode. The forced-mode write is
    // deliberately last: in data mode the interface repeats the last
    // setup-mode write to trigger each measurement.
    if (this->bmmWrite(BMM_REP_XY, BMM_REP_XY_HIGH_ACCURACY) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->bmmWrite(BMM_REP_Z, BMM_REP_Z_HIGH_ACCURACY) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->bmmWrite(BMM_OP_MODE, BMM_FORCED_MODE) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }

    // Data mode: read X, Y, Z and RHALL starting at BMM_DATA_X_LSB, 8 bytes
    // at a time, at 12.5 Hz. The read address is written while still in
    // setup mode, so it also triggers one 1-byte read that nothing uses.
    if (this->writeRegister(MAG_IF_READ_ADDR, BMM_DATA_X_LSB) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (this->writeRegister(MAG_CONF_ADDR, MAG_CONF_12_5HZ) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (this->writeRegister(MAG_IF_CONTROL_ADDR, MAG_IF_DATA_MODE) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }

    // Low power duty-cycles the interface between polls.
    if (this->writeRegister(CMD_ADDR, CMD_MAG_IF_LOW_POWER) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    this->delayUs(MAG_WARMUP_DELAY_US);
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::readMagTrim() {
    // Five reads covering the registers that hold trim terms: 0x5D-0x5E,
    // 0x62-0x63, 0x64-0x65, 0x68-0x6F and 0x70-0x71. Registers in the gaps
    // are not read and stay zero.
    U8 regs[BMM_TRIM_LENGTH] = {};
    struct Block {
        U8 reg;
        U32 count;
    };
    const Block blocks[] = {{0x5D, 2}, {0x62, 2}, {0x64, 2}, {0x68, 8}, {0x70, 2}};
    for (const Block& block : blocks) {
        if (this->bmmRead(block.reg, &regs[block.reg - BMM_TRIM_START], block.count) != Fw::Success::SUCCESS) {
            return Fw::Success::FAILURE;
        }
    }
    this->m_magTrim = decodeTrim(regs);
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::initAccel() {
    // Range. The scale factor is taken from the value read back, so it
    // always matches what the part actually holds.
    if (this->readThenWrite(ACC_RANGE_ADDR, ACC_RANGE_8G) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    U8 rangeCode = 0;
    if (this->readRegister(ACC_RANGE_ADDR, rangeCode) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (static_cast<U32>(rangeCode >> 2) >= ACCEL_RANGE_COUNT) {
        this->log_WARNING_HI_InvalidRangeReadback(static_cast<U8>(ACC_RANGE_ADDR), rangeCode);
        return Fw::Success::FAILURE;
    }
    this->m_accelScale = accelScale(rangeCode);

    if (this->writeAndCheck(ACC_CONF_ADDR, ACC_CONF_BOOT, ERR_CODE_MASK) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->writeAndCheck(CMD_ADDR, CMD_ACCEL_NORMAL, ERR_ANY) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    this->delayUs(ACCEL_STARTUP_DELAY_US);
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::initGyro() {
    if (this->applyGyroRange(GYRO_RANGE_BOOT) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->writeAndCheck(GYR_CONF_ADDR, GYR_CONF_BOOT, ERR_CODE_MASK) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    if (this->writeAndCheck(CMD_ADDR, CMD_GYRO_NORMAL, ERR_ANY) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    this->delayUs(GYRO_STARTUP_DELAY_US);
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::applyGyroRange(GyroRange range) {
    // The range codes are the GyroRange values, so the enum is written as is.
    if (this->readThenWrite(GYR_RANGE_ADDR, static_cast<U8>(range.e)) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    GyroRange held;
    if (this->fetchGyroRange(held) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    this->m_gyroScale = gyroScale(held);
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::fetchGyroRange(GyroRange& range) {
    U8 raw = 0;
    if (this->readRegister(GYR_RANGE_ADDR, raw) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    // Checked before constructing a GyroRange, whose constructor asserts on
    // an invalid value. Bits 7:3 are reserved and codes 5-7 are not ranges.
    if (!GyroRange::isValid(raw)) {
        this->log_WARNING_HI_InvalidRangeReadback(static_cast<U8>(GYR_RANGE_ADDR), raw);
        return Fw::Success::FAILURE;
    }
    range = GyroRange(raw);
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::sampleGyro(project::ImuVector& rate) {
    if (!this->m_booted) {
        return Fw::Success::FAILURE;
    }
    if (this->readAxes(GYR_DATA_ADDR, this->m_gyroScale, rate) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    this->tlmWrite_Gyro(rate);
    // The read succeeded, so the bus is healthy: re-arm the error event so a
    // later fault is reported again.
    this->log_WARNING_HI_I2cError_ThrottleClear();
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::sampleAccel(project::ImuVector& accel) {
    if (!this->m_booted) {
        return Fw::Success::FAILURE;
    }
    if (this->readAxes(ACC_DATA_ADDR, this->m_accelScale, accel) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    this->tlmWrite_Accel(accel);
    this->log_WARNING_HI_I2cError_ThrottleClear();
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::sampleMag(project::ImuVector& field) {
    if (!this->m_booted) {
        return Fw::Success::FAILURE;
    }
    U8 bytes[MAG_DATA_BYTES] = {};
    if (this->readRegisters(MAG_DATA_ADDR, bytes, MAG_DATA_BYTES) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    field = decodeMag(bytes, this->m_magTrim);
    this->tlmWrite_Mag(field);
    this->log_WARNING_HI_I2cError_ThrottleClear();
    return Fw::Success::SUCCESS;
}

Fw::Success BMX160::sampleErrors(project::ImuErrors& errors) {
    if (!this->m_booted) {
        return Fw::Success::FAILURE;
    }
    // Two reads, one per flag. Reading ERR_REG clears drop_cmd_err, so it is
    // taken from the first read; fatal_err survives reads and is taken from
    // the second. The second read is part of the flown transaction sequence.
    U8 first = 0;
    U8 second = 0;
    if (this->readRegister(ERR_REG_ADDR, first) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    if (this->readRegister(ERR_REG_ADDR, second) != Drv::I2cStatus::I2C_OK) {
        return Fw::Success::FAILURE;
    }
    errors = project::ImuErrors(decodeErrors(first).get_dropCommandError(), decodeErrors(second).get_fatalError());
    return Fw::Success::SUCCESS;
}

// ----------------------------------------------------------------------
// Handlers
// ----------------------------------------------------------------------

Fw::Success BMX160::boot_handler(FwIndexType portNum) {
    return this->bootDevice();
}

Fw::Success BMX160::readGyro_handler(FwIndexType portNum, project::ImuVector& sample) {
    return this->sampleGyro(sample);
}

Fw::Success BMX160::readAccel_handler(FwIndexType portNum, project::ImuVector& sample) {
    return this->sampleAccel(sample);
}

Fw::Success BMX160::readMag_handler(FwIndexType portNum, project::ImuVector& sample) {
    return this->sampleMag(sample);
}

Fw::Success BMX160::setGyroRange_handler(FwIndexType portNum, const project::GyroRange& range) {
    if (!this->m_booted) {
        return Fw::Success::FAILURE;
    }
    return this->applyGyroRange(range);
}

Fw::Success BMX160::getGyroRange_handler(FwIndexType portNum, project::GyroRange& range) {
    if (!this->m_booted) {
        return Fw::Success::FAILURE;
    }
    return this->fetchGyroRange(range);
}

Fw::Success BMX160::readErrors_handler(FwIndexType portNum, project::ImuErrors& errors) {
    return this->sampleErrors(errors);
}

void BMX160::run_handler(FwIndexType portNum, U32 context) {
    // Bring-up path: with no task to call boot, retry it every tick until the
    // part answers. Failures are already reported through events.
    if (!this->m_booted && this->bootDevice() != Fw::Success::SUCCESS) {
        return;
    }
    project::ImuVector sample;
    (void)this->sampleGyro(sample);
    (void)this->sampleAccel(sample);
    (void)this->sampleMag(sample);
}

}  // namespace project

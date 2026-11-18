// ======================================================================
// \title  BMX160.hpp
// \brief  BMX160 accelerometer, gyroscope and magnetometer device manager.
//
// Register semantics are per the BMI160 data sheet (BST-BMI160-DS000),
// whose accelerometer/gyroscope core and register map the BMX160 shares,
// and the BMM150 data sheet for the magnetometer behind it.
//
// Layout of this class:
//
//   * Static, pure decode functions for the data registers, scale factors,
//     error flags, magnetometer trim and magnetometer compensation, so the
//     unit tests can check every conversion without an I2C mock.
//
//   * A boot step that resets, identifies and configures the part, and one
//     read step per sensor. A part that has not booted is treated as absent
//     and never touches the bus.
// ======================================================================

#ifndef project_BMX160_HPP
#define project_BMX160_HPP

#include "project/Drivers/BMX160/BMX160ComponentAc.hpp"

namespace project {

class BMX160 final : public BMX160ComponentBase {
  public:
    // SDO strapped low selects 0x68; strapped high, 0x69.
    static constexpr U32 DEFAULT_DEVICE_ADDRESS = 0x68;

    //! CHIP_ID (0x00) of a BMX160.
    static constexpr U8 CHIP_ID = 0xD8;

    // ----------------------------------------------------------------------
    // CMD (0x7E) opcodes
    // ----------------------------------------------------------------------
    static constexpr U8 CMD_SOFT_RESET = 0xB6;
    static constexpr U8 CMD_ACCEL_NORMAL = 0x11;      // acc_set_pmu_mode: normal
    static constexpr U8 CMD_GYRO_NORMAL = 0x15;       // gyr_set_pmu_mode: normal
    static constexpr U8 CMD_MAG_IF_NORMAL = 0x19;     // mag_set_pmu_mode: normal
    static constexpr U8 CMD_MAG_IF_LOW_POWER = 0x1A;  // mag_set_pmu_mode: low power

    // ----------------------------------------------------------------------
    // Accelerometer and gyroscope boot configuration
    //
    // ACC_CONF (0x40):  7 acc_us | 6:4 acc_bwp | 3:0 acc_odr
    // GYR_CONF (0x42):  7:6 reserved | 5:4 gyr_bwp | 3:0 gyr_odr
    //
    // Known limitation: both CONF bytes leave the bandwidth field at 0,
    // which selects OSR4 filtering rather than normal mode. For the
    // gyroscope at 50 Hz that is a 3 dB cutoff of 5.06 Hz instead of
    // 20.8 Hz, which adds lag to the rate measurement. This configuration
    // is kept deliberately: it is the one the spacecraft flies, and
    // timing and sensor behavior must stay identical for comparison
    // against it. Normal mode would be gyr_bwp = 0b10 (GYR_CONF 0x27).
    // ----------------------------------------------------------------------
    static constexpr U8 ACC_RANGE_8G = 0x08;   // ACC_RANGE (0x41) code for +/-8 g
    static constexpr U8 ACC_CONF_BOOT = 0x06;  // acc_us 0, acc_bwp OSR4, 25 Hz
    static constexpr U8 GYR_CONF_BOOT = 0x07;  // gyr_bwp OSR4, 50 Hz
    static constexpr GyroRange::T GYRO_RANGE_BOOT = GyroRange::DPS_500;

    // ----------------------------------------------------------------------
    // Magnetometer interface
    //
    // The BMM150 is reached through the magnetometer interface registers.
    // In setup mode (MAG_IF[1] bit 7 set), writing a BMM150 register address
    // to MAG_IF[3] writes MAG_IF[4] to it, and writing one to MAG_IF[2]
    // reads mag_rd_burst bytes from it into DATA (0x04). In data mode the
    // interface polls the BMM150 by itself at the MAG_CONF rate, re-sending
    // the last setup-mode write to trigger each measurement, and DATA
    // always holds the latest X, Y, Z and RHALL.
    // ----------------------------------------------------------------------
    static constexpr U8 MAG_IF_SETUP_MODE = 0x80;  // mag_manual_en, 1-byte bursts
    static constexpr U8 MAG_IF_BURST_2 = 0x01;     // mag_rd_burst: 2 bytes
    static constexpr U8 MAG_IF_BURST_8 = 0x03;     // mag_rd_burst: 8 bytes
    static constexpr U8 MAG_IF_DATA_MODE = 0x03;   // manual off, 8-byte bursts (X, Y, Z, RHALL)
    static constexpr U8 MAG_CONF_12_5HZ = 0x05;    // MAG_CONF (0x44) mag_odr

    // BMM150 registers and the values written to them at boot. The
    // repetition counts are the BMM150 high-accuracy preset: 47 XY and 83 Z
    // samples per measurement, about 50 ms, which the 80 ms poll period of
    // 12.5 Hz accommodates.
    static constexpr U8 BMM_POWER_CONTROL = 0x4B;
    static constexpr U8 BMM_POWER_ON = 0x01;  // suspend -> sleep
    static constexpr U8 BMM_OP_MODE = 0x4C;
    static constexpr U8 BMM_FORCED_MODE = 0x02;  // one measurement per write
    static constexpr U8 BMM_REP_XY = 0x51;
    static constexpr U8 BMM_REP_XY_HIGH_ACCURACY = 0x17;  // nXY = 1 + 2 * 23
    static constexpr U8 BMM_REP_Z = 0x52;
    static constexpr U8 BMM_REP_Z_HIGH_ACCURACY = 0x52;  // nZ = 1 + 82
    static constexpr U8 BMM_DATA_X_LSB = 0x42;           // start of X, Y, Z, RHALL

    //! BMM150 factory trim registers, 0x5D through 0x71.
    static constexpr U8 BMM_TRIM_START = 0x5D;
    static constexpr U32 BMM_TRIM_LENGTH = 21;

    // ----------------------------------------------------------------------
    // Scaling
    //
    // Each accelerometer and gyroscope axis is a signed 16-bit count, so
    // +/-full scale spans +/-32768 counts: one count is (full scale / 32768)
    // of the sensor's unit.
    // ----------------------------------------------------------------------
    static constexpr F32 COUNTS_PER_FULL_SCALE = 32768.0F;

    //! Gyroscope full scale in deg/s, indexed by GyroRange (the GYR_RANGE code).
    static constexpr F32 GYRO_FULL_SCALE_DPS[GyroRange::NUM_CONSTANTS] = {2000.0F, 1000.0F, 500.0F, 250.0F, 125.0F};

    //! Accelerometer full scale in g, indexed by (ACC_RANGE code >> 2). The
    //! valid codes 0x03, 0x05, 0x08 and 0x0C map onto indices 0..3.
    static constexpr U32 ACCEL_RANGE_COUNT = 4;
    static constexpr F32 ACCEL_FULL_SCALE_G[ACCEL_RANGE_COUNT] = {2.0F, 4.0F, 8.0F, 16.0F};

    static constexpr F32 DEG_TO_RAD = 3.14159265358979F / 180.0F;
    static constexpr F32 STANDARD_GRAVITY = 9.80665F;  // m/s^2 per g

    //! uT per count of an uncompensated magnetometer axis, used only when
    //! the trim registers read back blank.
    static constexpr F32 MAG_UNTRIMMED_SCALE = 1.0F / 16.0F;

    //! Bytes in the accelerometer or gyroscope data block: X, Y, Z, two
    //! bytes each, LSB first.
    static constexpr U32 AXIS_DATA_BYTES = 6;

    //! Bytes in the magnetometer data block: X, Y, Z and RHALL, LSB first.
    static constexpr U32 MAG_DATA_BYTES = 8;

    // ----------------------------------------------------------------------
    // Delays and timeouts, in microseconds
    //
    // CMD drops any write that arrives while a previous command is still
    // executing, so each power-mode change is followed by a wait.
    // ----------------------------------------------------------------------
    static constexpr U32 SOFT_RESET_DELAY_US = 3000;
    //! Idle time required after every register write before the next access
    //! (BMI160 data sheet 3.2.4). 2 us suffices in normal mode, but in
    //! suspend or low-power mode an access sooner than this discards the
    //! write without any error, and every sensor starts out suspended.
    static constexpr U32 POST_WRITE_IDLE_US = 450;
    static constexpr U32 MAG_IF_STARTUP_DELAY_US = 650;
    //! After entering data mode, before the first magnetometer sample is valid.
    static constexpr U32 MAG_WARMUP_DELAY_US = 100000;
    //! Longest a setup-mode BMM150 transfer may take before it is abandoned.
    static constexpr U32 MAG_TRANSFER_TIMEOUT_US = 1000;
    static constexpr U32 ACCEL_STARTUP_DELAY_US = 5000;
    // The data sheet gives 55 ms typical from suspend to normal mode, so the
    // first gyroscope samples after boot may still be settling.
    static constexpr U32 GYRO_STARTUP_DELAY_US = 8100;

    // ----------------------------------------------------------------------
    // Magnetometer trim
    // ----------------------------------------------------------------------

    //! BMM150 factory trim coefficients, used by the compensation formulas.
    struct MagTrim {
        I8 x1;     // 0x5D
        I8 y1;     // 0x5E
        I16 z4;    // 0x62-0x63
        I8 x2;     // 0x64
        I8 y2;     // 0x65
        I16 z2;    // 0x68-0x69
        U16 z1;    // 0x6A-0x6B
        U16 xyz1;  // 0x6C-0x6D, 15 bits
        I16 z3;    // 0x6E-0x6F
        I8 xy2;    // 0x70
        U8 xy1;    // 0x71
    };

    explicit BMX160(const char* const compName);
    ~BMX160() override;

    //! Set this instance's I2C address. Call from the topology before the
    //! part is booted.
    void configure(U32 deviceAddress);

    // ----------------------------------------------------------------------
    // Register decoding
    // ----------------------------------------------------------------------

    //! Combine one little-endian register pair into a signed count.
    static I16 toSigned(U8 lsb, U8 msb);

    //! Decode six data bytes (X LSB, X MSB, Y LSB, ...) into a vector,
    //! multiplying each axis by `scale`.
    static project::ImuVector decodeAxes(const U8 bytes[AXIS_DATA_BYTES], F32 scale);

    //! rad/s per count for a gyroscope range.
    static F32 gyroScale(GyroRange range);

    //! m/s^2 per count for an ACC_RANGE code. The code must satisfy
    //! (rangeCode >> 2) < ACCEL_RANGE_COUNT.
    static F32 accelScale(U8 rangeCode);

    //! Split ERR_REG (0x02): bit 6 drop_cmd_err, bit 0 fatal_err.
    static project::ImuErrors decodeErrors(U8 errReg);

    //! Decode the trim registers, BMM_TRIM_START onward.
    static MagTrim decodeTrim(const U8 regs[BMM_TRIM_LENGTH]);

    //! Decode the magnetometer data block into a field in uT, compensated
    //! with `trim` per the BMM150 data sheet. An axis that overflowed, or
    //! that cannot be compensated because RHALL or a trim term is zero,
    //! reads 0. If the trim is blank (xyz1 == 0), each axis is scaled by
    //! MAG_UNTRIMMED_SCALE instead.
    static project::ImuVector decodeMag(const U8 bytes[MAG_DATA_BYTES], const MagTrim& trim);

  private:
    // ----------------------------------------------------------------------
    // Register map
    // ----------------------------------------------------------------------
    enum Register : U8 {
        CHIP_ID_ADDR = 0x00,            // Chip identification
        ERR_REG_ADDR = 0x02,            // Error flags; all but fatal_err clear on read
        PMU_STATUS_ADDR = 0x03,         // Power mode each sensor is actually in
        MAG_DATA_ADDR = 0x04,           // Magnetometer X/Y/Z/RHALL, 8 bytes little-endian
        GYR_DATA_ADDR = 0x0C,           // Gyroscope X/Y/Z, 6 bytes little-endian
        ACC_DATA_ADDR = 0x12,           // Accelerometer X/Y/Z, 6 bytes little-endian
        STATUS_ADDR = 0x1B,             // Data-ready and operation-in-progress flags
        ACC_CONF_ADDR = 0x40,           // Accelerometer bandwidth and data rate
        ACC_RANGE_ADDR = 0x41,          // Accelerometer full-scale range
        GYR_CONF_ADDR = 0x42,           // Gyroscope bandwidth and data rate
        GYR_RANGE_ADDR = 0x43,          // Gyroscope full-scale range
        MAG_CONF_ADDR = 0x44,           // Magnetometer poll rate in data mode
        MAG_IF_CONTROL_ADDR = 0x4C,     // MAG_IF[1]: setup/data mode, read burst length
        MAG_IF_READ_ADDR = 0x4D,        // MAG_IF[2]: BMM150 register to read
        MAG_IF_WRITE_ADDR = 0x4E,       // MAG_IF[3]: BMM150 register to write
        MAG_IF_WRITE_DATA_ADDR = 0x4F,  // MAG_IF[4]: byte to write
        CMD_ADDR = 0x7E                 // Command register
    };

    // ERR_REG fields.
    static constexpr U8 ERR_DROP_CMD = 1U << 6;
    static constexpr U8 ERR_CODE_MASK = 0x1EU;  // error_code, bits 4:1
    static constexpr U8 ERR_FATAL = 1U << 0;
    static constexpr U8 ERR_ANY = 0xFFU;

    // STATUS fields.
    static constexpr U8 STATUS_MAG_MAN_OP = 1U << 2;  // setup-mode transfer in progress

    // ----------------------------------------------------------------------
    // Bus helpers
    //
    // Each is exactly one I2C transaction. On failure the outputs are left
    // untouched and the I2cError event is emitted naming the register, so
    // no caller can forget to report it.
    // ----------------------------------------------------------------------

    //! Write `start`, then read `count` consecutive registers into `data`.
    Drv::I2cStatus readRegisters(Register start, U8* data, U32 count);

    //! Read a single register.
    Drv::I2cStatus readRegister(Register reg, U8& value);

    //! Write a single register, then wait POST_WRITE_IDLE_US so the write
    //! takes effect before the next access.
    Drv::I2cStatus writeRegister(Register reg, U8 value);

    // ----------------------------------------------------------------------
    // Transaction patterns
    //
    // Multi-transaction sequences that recur during configuration. Each
    // stops at the first failed transaction.
    // ----------------------------------------------------------------------

    //! Read `reg`, then write `value` to it: two transactions. CMD and the
    //! range registers are written this way. The read result is not used;
    //! it is part of the transaction sequence the spacecraft flies, which is
    //! kept so bus timing stays comparable with it.
    Drv::I2cStatus readThenWrite(Register reg, U8 value);

    //! Write `value` to `reg`, then read ERR_REG. If any bit of
    //! `rejectMask` is set there, emit SettingRejected and fail. Data-rate
    //! writes check ERR_CODE_MASK; power-mode commands check ERR_ANY, since
    //! a command dropped by a busy CMD register shows up only as
    //! drop_cmd_err.
    Fw::Success writeAndCheck(Register reg, U8 value, U8 rejectMask);

    //! Burst-read one sensor's six data bytes starting at `start` and decode
    //! them with `scale`. One transaction, so all three axes come from the
    //! same sample. `sample` is only written on SUCCESS.
    Fw::Success readAxes(Register start, F32 scale, project::ImuVector& sample);

    //! Poll STATUS until the setup-mode transfer to or from `bmmReg`
    //! completes, failing with MagTransferTimeout after
    //! MAG_TRANSFER_TIMEOUT_US. Polls back to back: the timeout is shorter
    //! than any delay the OS could schedule.
    Fw::Success waitForMagTransfer(U8 bmmReg);

    //! Write one BMM150 register through the magnetometer interface.
    Fw::Success bmmWrite(U8 bmmReg, U8 value);

    //! Read `count` (2 or 8) consecutive BMM150 registers through the
    //! magnetometer interface.
    Fw::Success bmmRead(U8 bmmReg, U8* data, U32 count);

    // ----------------------------------------------------------------------
    // Operations
    // ----------------------------------------------------------------------

    //! Soft-reset, check CHIP_ID, then bring up the magnetometer, the
    //! accelerometer and the gyroscope.
    Fw::Success bootDevice();

    //! Wake the BMM150, read its trim, configure it, and start data mode.
    Fw::Success initMag();

    //! Read the BMM150 trim registers into m_magTrim.
    Fw::Success readMagTrim();

    //! Range, data rate and power mode for the accelerometer.
    Fw::Success initAccel();

    //! Range, data rate and power mode for the gyroscope.
    Fw::Success initGyro();

    //! Write GYR_RANGE, then read it back and take the scale factor from
    //! the value the part actually holds.
    Fw::Success applyGyroRange(GyroRange range);

    //! Read GYR_RANGE. `range` is only written on SUCCESS.
    Fw::Success fetchGyroRange(GyroRange& range);

    //! Read the gyroscope, then publish it.
    Fw::Success sampleGyro(project::ImuVector& rate);

    //! Read the accelerometer, then publish it.
    Fw::Success sampleAccel(project::ImuVector& accel);

    //! Read the magnetometer, then publish it.
    Fw::Success sampleMag(project::ImuVector& field);

    //! Read and decode ERR_REG.
    Fw::Success sampleErrors(project::ImuErrors& errors);

    //! Wait, without busy-looping, for at least `microseconds`.
    void delayUs(U32 microseconds);

    // ----------------------------------------------------------------------
    // Handlers
    // ----------------------------------------------------------------------

    Fw::Success boot_handler(FwIndexType portNum) override;
    Fw::Success readGyro_handler(FwIndexType portNum, project::ImuVector& sample) override;
    Fw::Success readAccel_handler(FwIndexType portNum, project::ImuVector& sample) override;
    Fw::Success readMag_handler(FwIndexType portNum, project::ImuVector& sample) override;
    Fw::Success setGyroRange_handler(FwIndexType portNum, const project::GyroRange& range) override;
    Fw::Success getGyroRange_handler(FwIndexType portNum, project::GyroRange& range) override;
    Fw::Success readErrors_handler(FwIndexType portNum, project::ImuErrors& errors) override;
    void run_handler(FwIndexType portNum, U32 context) override;

    // ----------------------------------------------------------------------
    // State
    // ----------------------------------------------------------------------

    U32 m_deviceAddress;

    //! Set by a successful boot. Until then the part is treated as absent.
    bool m_booted;

    //! Per-count scale factors for the ranges the part currently holds.
    F32 m_gyroScale;
    F32 m_accelScale;

    //! Trim read from this part's BMM150 at boot.
    MagTrim m_magTrim;
};

}  // namespace project

#endif

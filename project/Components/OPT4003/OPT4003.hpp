// ======================================================================
// \title  OPT4003.hpp
// \brief  OPT4003 ambient light sensor device manager.
//
// The OPT4003 reports illuminance as a 20-bit mantissa and a 4-bit
// exponent split across two 16-bit result registers. All registers are
// big-endian over I2C.
//
// Layout of this class:
//
//   * Static, pure decode functions for the device ID, the result
//     registers and the configuration word, so the unit tests can check
//     every bit field without an I2C mock.
//
//   * A boot step that identifies the part and programs it, and a read
//     step that waits for a conversion and fetches the result. A sensor
//     that has not booted is treated as absent and never touches the bus.
// ======================================================================

#ifndef project_OPT4003_HPP
#define project_OPT4003_HPP

#include "project/Components/OPT4003/OPT4003ComponentAc.hpp"

namespace project {

class OPT4003 final : public OPT4003ComponentBase {
  public:
    // The part's ADDR pin selects one of four 7-bit addresses starting at
    // 0x44. The +Z sun sensor uses a separate range (0x54-0x57).
    static constexpr U32 DEFAULT_DEVICE_ADDRESS = 0x44;

    // ----------------------------------------------------------------------
    // Configuration register (0x0A) fields
    //
    //   15     QWAKE            quick wake-up from standby
    //   14     reserved         must be written 0
    //   13:10  RANGE            12 = auto-range
    //   9:6    CONVERSION_TIME  0 = 600 us
    //   5:4    OPERATING_MODE   3 = continuous
    //   3      LATCH            1 = latched window comparator
    //   2      INT_POL          0 = INT pin active low
    //   1:0    FAULT_COUNT      0 = one fault triggers a flag
    //
    // The conversion time is the shortest the part offers, so a fresh result
    // is almost always ready by the time the ADCS task asks for one.
    // ----------------------------------------------------------------------
    static constexpr U16 CONFIG_QUICK_WAKEUP = 0;
    static constexpr U16 CONFIG_RANGE_AUTO = 12;
    static constexpr U16 CONFIG_CONVERSION_TIME_600US = 0;
    static constexpr U16 CONFIG_MODE_CONTINUOUS = 3;
    static constexpr U16 CONFIG_LATCH = 1;
    static constexpr U16 CONFIG_INT_POL_ACTIVE_LOW = 0;
    static constexpr U16 CONFIG_FAULT_COUNT_ONE = 0;

    //! The full configuration word written at boot.
    static constexpr U16 CONFIGURATION =
        static_cast<U16>((CONFIG_QUICK_WAKEUP << 15) | (CONFIG_RANGE_AUTO << 10) | (CONFIG_CONVERSION_TIME_600US << 6) |
                         (CONFIG_MODE_CONTINUOUS << 4) | (CONFIG_LATCH << 3) | (CONFIG_INT_POL_ACTIVE_LOW << 2) |
                         CONFIG_FAULT_COUNT_ONE);

    //! How long a read waits for the conversion-ready flag before reading
    //! the result registers anyway, and how often it polls in between.
    static constexpr U32 CONVERSION_TIMEOUT_US = 1100000;
    static constexpr U32 CONVERSION_POLL_US = 1000;

    explicit OPT4003(const char* const compName);
    ~OPT4003() override;

    //! Set this instance's I2C address. Call from the topology before the
    //! sensor is booted.
    void configure(U32 deviceAddress);

    // ----------------------------------------------------------------------
    // Register decoding
    // ----------------------------------------------------------------------

    //! True if DEVICE_ID (0x11) identifies an OPT4003: DIDL (bits 13:12)
    //! is 0 and DIDH (bits 11:0) is 0x221. Bits 15:14 are reserved.
    static bool isExpectedDeviceId(U16 raw);

    //! Combine RESULT_MSB_CH0 and RESULT_LSB_CH0 into lux.
    //!
    //!   msb: 15:12 EXPONENT, 11:0 RESULT_MSB
    //!   lsb: 15:8 RESULT_LSB, 7:4 COUNTER, 3:0 CRC
    //!
    //!   mantissa = (RESULT_MSB << 8) + RESULT_LSB
    //!   lux      = (mantissa << EXPONENT) * 535e-6
    //!
    //! COUNTER and CRC are not used.
    static F32 decodeLux(U16 msb, U16 lsb);

    //! Split FLAGS (0x0C) into the threshold and overload flags.
    static project::LightSensorFlags decodeFlags(U16 raw);

  private:
    // ----------------------------------------------------------------------
    // Register map
    // ----------------------------------------------------------------------
    enum Register : U8 {
        RESULT_MSB_CH0_ADDR = 0x00,  // Exponent and upper 12 mantissa bits
        RESULT_LSB_CH0_ADDR = 0x01,  // Lower 8 mantissa bits, counter, CRC
        CONFIGURATION_ADDR = 0x0A,   // Range, conversion time, mode, interrupt
        FLAGS_ADDR = 0x0C,           // Overload, conversion ready, thresholds
        DEVICE_ID_ADDR = 0x11        // DIDL and DIDH
    };

    // FLAGS register bits.
    static constexpr U16 FLAG_OVERLOAD = 1U << 3;
    static constexpr U16 FLAG_CONVERSION_READY = 1U << 2;
    static constexpr U16 FLAG_HIGH = 1U << 1;
    static constexpr U16 FLAG_LOW = 1U << 0;

    // ----------------------------------------------------------------------
    // Bus helpers
    // ----------------------------------------------------------------------

    //! Write a register address then read its 16-bit big-endian value.
    //! On failure `value` is left untouched and the I2cError event is
    //! emitted naming this register.
    Drv::I2cStatus readRegister(Register reg, U16& value);

    //! Write a register address followed by a 16-bit big-endian value.
    //! Reports a failure the same way readRegister does.
    Drv::I2cStatus writeRegister(Register reg, U16 value);

    // ----------------------------------------------------------------------
    // Operations
    // ----------------------------------------------------------------------

    //! Identify the part, then program the configuration register.
    Fw::Success bootDevice();

    //! Poll FLAGS until a conversion is ready or the timeout passes, then
    //! read both result registers. A timeout is not an error: the result
    //! registers still hold the most recent completed conversion.
    Fw::Success sampleLux(F32& lux);

    //! Read FLAGS once and publish it.
    Fw::Success sampleFlags(project::LightSensorFlags& flags);

    // ----------------------------------------------------------------------
    // Handlers
    // ----------------------------------------------------------------------

    Fw::Success boot_handler(FwIndexType portNum) override;
    Fw::Success read_handler(FwIndexType portNum, F32& lux) override;
    Fw::Success readFlags_handler(FwIndexType portNum, project::LightSensorFlags& flags) override;
    void run_handler(FwIndexType portNum, U32 context) override;

    // ----------------------------------------------------------------------
    // State
    // ----------------------------------------------------------------------

    U32 m_deviceAddress;

    //! Set by a successful boot. Until then the sensor is treated as absent.
    bool m_booted;
};

}  // namespace project

#endif

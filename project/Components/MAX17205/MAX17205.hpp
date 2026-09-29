// ======================================================================
// \title  MAX17205.hpp
// \brief  MAX17205 fuel-gauge device manager.
//
// The MAX17205 is a multi-cell battery fuel gauge. Every measurement it
// exposes is a 16-bit little-endian register read over I2C, scaled by a
// per-register constant from the datasheet.
//
// Layout of this class:
//
//   * One read* method per measurement. Each does its own transaction,
//     decodes it, caches the result and publishes its telemetry channel.
//     Keeping them separate means a caller can sample one measurement
//     without paying for all fourteen transactions, and a fault in one
//     register is isolated to that one channel.
//
//   * The register scaling lives in static decode* functions. They are pure,
//     so the unit tests can check every conversion constant without standing
//     up an I2C mock.
//
//   * The most recent successful value of each measurement is cached, so
//     other components can read the current battery state without forcing a
//     bus transaction.
// ======================================================================

#ifndef project_MAX17205_HPP
#define project_MAX17205_HPP

#include "project/Components/MAX17205/MAX17205ComponentAc.hpp"

namespace project {

class MAX17205 final : public MAX17205ComponentBase {
  public:
    // The part answers on two 7-bit I2C addresses. 0x36 is the fuel-gauge
    // register bank; 0x0B is shadow RAM, which holds the thermistor and die
    // temperatures.
    static constexpr U32 DEFAULT_DEVICE_ADDRESS = 0x36;
    static constexpr U32 DEFAULT_SHADOW_ADDRESS = 0x0B;

    explicit MAX17205(const char* const compName);
    ~MAX17205() override;

    //! Override the default I2C addresses. Call from the topology before the
    //! component is started if a board variant strapped the part differently.
    void configure(U32 deviceAddress, U32 shadowAddress);

    // ----------------------------------------------------------------------
    // Register decoding
    //
    // Static and pure so they can be unit tested directly. Scaling constants
    // are per the datasheet, with the board's 10 mOhm sense resistor where
    // the conversion depends on it.
    // ----------------------------------------------------------------------

    //! Reinterpret an assembled register value as two's complement.
    //! readRegister has already done the little-endian byte assembly.
    static I16 toSigned(U16 raw);

    //! RepSOC -> percent. 1/256 % per count.
    static F32 decodeStateOfCharge(U16 raw);

    //! RepCap / FullCapRep -> mAh. 0.5 mAh per count.
    static F32 decodeCapacity(U16 raw);

    //! Current -> mA. Signed, negative while discharging.
    static F32 decodeCurrent(I16 raw);

    //! VBat -> mV. 1.25 mV per count.
    static F32 decodeVoltage(U16 raw);

    //! VCell -> mV. 0.078125 mV per count.
    static F32 decodeMidVoltage(U16 raw);

    //! TTE / TTF -> seconds. 5.625 s per count.
    static F32 decodeTime(U16 raw);

    //! Temp -> centi-Celsius. 0.390625 centi-Celsius per count.
    //! Signed: the pack can and does go below freezing.
    static F32 decodeTemperature(I16 raw);

    //! Shadow-RAM thermistor register -> centi-Celsius.
    static I32 decodeThermistorTemperature(U16 raw);

  private:
    // ----------------------------------------------------------------------
    // Register map
    // ----------------------------------------------------------------------
    enum Register : U8 {
        STATUS_ADDR = 0x00,    // Alert and chip status flags (bitfield)

        // Fuel-gauge register bank, I2C address 0x36
        VCELL_ADDR = 0x09,     // Contains alert status and chip status
        REPSOC_ADDR = 0x06,    // Reported state of charge
        REPCAP_ADDR = 0x05,    // Reported remaining capacity
        CURRENT_ADDR = 0x0A,   // Battery current
        TTE_ADDR = 0x11,       // Time to empty
        TTF_ADDR = 0x20,       // Time to full
        CAPACITY_ADDR = 0x10,  // Full capacity estimate (FullCapRep)
        VBAT_ADDR = 0xDA,      // Battery pack voltage
        CYCLES_ADDR = 0x17,    // Charge/discharge cycle count
        TIMERH_ADDR = 0xBE,    // Time since power up
        TEMP_ADDR = 0x08,      // Pack temperature

        CONFIG2_ADDR = 0xBB,  // Config2; bit 0 requests a reset

        // Shadow RAM, I2C address 0x0B
        TEMP1_ADDR = 0x34,   // AIN1 thermistor temperature
        TEMP2_ADDR = 0x3B,   // AIN2 thermistor temperature
        INTTEMP_ADDR = 0x35  // Internal die temperature
    };

    // ----------------------------------------------------------------------
    // Bus helpers
    // ----------------------------------------------------------------------

    //! Write a register address then read its 16-bit little-endian value.
    //! On failure `value` is left untouched, the I2cError event is emitted
    //! naming this register, and the bus status is returned. Reporting lives
    //! here so that no caller can forget it.
    Drv::I2cStatus readRegister(U32 address, Register reg, U16& value);

    //! Write a register address followed by a 16-bit little-endian value.
    //! Reports a failure the same way readRegister does.
    Drv::I2cStatus writeRegister(U32 address, Register reg, U8 valueLow, U8 valueHigh);

    // ----------------------------------------------------------------------
    // Per-measurement reads
    //
    // Each performs its transaction, decodes it, and publishes the matching
    // telemetry channel. Returns the bus status so the caller can decide what
    // a failure means.
    //
    // There is deliberately no cached copy of any measurement here. The
    // telemetry database already retains the last value written to every
    // channel, so a member per measurement would be a second copy of the same
    // state with no reader. If a future component needs battery state on
    // board -- a power manager shedding load at low state of charge, say --
    // the way to hand it over is a port, not a public getter: another
    // component has no pointer to this one and cannot call its methods. That
    // port handler would run on the caller's thread and must not touch the
    // bus, and *that* is the point at which a guarded cache starts earning
    // its keep.
    // ----------------------------------------------------------------------
    
    Drv::I2cStatus readStatus();
    Drv::I2cStatus readStateOfCharge();
    Drv::I2cStatus readRemainingCapacity();
    Drv::I2cStatus readFullCapacity();
    Drv::I2cStatus readCurrent();
    Drv::I2cStatus readVoltage();
    Drv::I2cStatus readMidVoltage();
    Drv::I2cStatus readCycles();
    Drv::I2cStatus readTimeToEmpty();
    Drv::I2cStatus readTimeToFull();
    Drv::I2cStatus readTimeSincePowerUp();
    Drv::I2cStatus readTemperature();
    Drv::I2cStatus readTemperatureAin1();
    Drv::I2cStatus readTemperatureAin2();
    Drv::I2cStatus readTemperatureDie();

    //! Request a power-on reset of the gauge via CONFIG2.
    Drv::I2cStatus resetDevice();

    //! Run every read above, publishing whatever succeeds.
    //! \return the number of reads that failed (0 means a clean sample)
    U32 sampleAll();

    // ----------------------------------------------------------------------
    // Handlers
    // ----------------------------------------------------------------------

    void run_handler(FwIndexType portNum, U32 context) override;
    void READ_ALL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    // ----------------------------------------------------------------------
    // State
    // ----------------------------------------------------------------------

    U32 m_deviceAddress;
    U32 m_shadowAddress;
};

}  // namespace project

#endif

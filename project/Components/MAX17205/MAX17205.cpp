// ======================================================================
// \title  MAX17205.cpp
// \brief  MAX17205 fuel-gauge device manager implementation.
// ======================================================================

#include "project/Components/MAX17205/MAX17205.hpp"

namespace project {

// ----------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------

MAX17205::MAX17205(const char* const compName)
    : MAX17205ComponentBase(compName),
      m_deviceAddress(DEFAULT_DEVICE_ADDRESS),
      m_shadowAddress(DEFAULT_SHADOW_ADDRESS) {}

MAX17205::~MAX17205() {}

void MAX17205::configure(U32 deviceAddress, U32 shadowAddress) {
    this->m_deviceAddress = deviceAddress;
    this->m_shadowAddress = shadowAddress;
}

// ----------------------------------------------------------------------
// Register decoding
// ----------------------------------------------------------------------

I16 MAX17205::toSigned(U16 raw) {
    // Reinterpret an assembled register value as two's complement. The
    // little-endian byte assembly itself happens once, in readRegister.
    //
    // Written out as a branch rather than a plain static_cast<I16> on purpose:
    // converting an out-of-range unsigned value to a signed type is only
    // implementation-defined before C++20, and this project builds as C++14
    // (CONFIG_STD_CPP14 in prj.conf). The explicit form is well defined under
    // every standard and folds to the same single instruction, so the
    // portability costs nothing.
    return (raw < 0x8000U) ? static_cast<I16>(raw) : static_cast<I16>(static_cast<I32>(raw) - 0x10000);
}

F32 MAX17205::decodeStateOfCharge(U16 raw) {
    // RepSOC is 1/256 of a percent per count.
    return static_cast<F32>(raw) / 256.0F;
}

F32 MAX17205::decodeCapacity(U16 raw) {
    // Capacity LSB is 5.0 uVh / Rsense, which is 0.5 mAh with the board's
    // 10 mOhm sense resistor. Applies to both RepCap and FullCapRep.
    return static_cast<F32>(raw) * 0.5F;
}

F32 MAX17205::decodeCurrent(I16 raw) {
    // Current LSB is 1.5625 uV / Rsense, which is 0.15625 mA with a 10 mOhm
    // sense resistor. Note 0.15625 is 5/32, exactly representable in binary
    // floating point, so this conversion is exact.
    return static_cast<F32>(raw) * 0.15625F;
}

F32 MAX17205::decodeVoltage(U16 raw) {
    // VBat is 1.25 mV per count.
    return static_cast<F32>(raw) * 1.25F;
}

F32 MAX17205::decodeMidVoltage(U16 raw) {
    // VCell is 0.078125 mV per count.
    return static_cast<F32>(raw) * 0.078125F;
}

F32 MAX17205::decodeTime(U16 raw) {
    // TTE and TTF are both 5.625 seconds per count.
    return static_cast<F32>(raw) * 5.625F;
}

I32 MAX17205::decodeTemperature(I16 raw) {
    // Temp is 1/256 degree C per count; 0.390625 is 100/256, converting
    // straight to centi-Celsius.
    //
    // The register is signed two's complement, so the parameter is I16 and
    // the caller must put the register value through toSigned() first. Taking
    // a signed parameter is what keeps that obligation visible at the call
    // site instead of letting an unsigned value decode quietly into a wrong
    // answer. Current is handled the same way, for the same reason.
    //
    // Decoded as unsigned this wraps below freezing: -1 C arrives as 0xFF00,
    // which read unsigned is 25500 centi-Celsius rather than -100. That is a
    // sign inversion on a channel an operator uses to judge whether the pack
    // is too cold, so it has to be right even though nothing on board
    // currently actuates on it -- the thermistor channels are the ones wired
    // to heater decisions, and they use a separate unsigned-Kelvin encoding.
    // Scale in floating point, then truncate once. Doing the multiply in
    // integers instead (raw * 100 / 256) would be exact too, but this keeps
    // the datasheet's constant visible in the code.
    return static_cast<I32>(static_cast<F32>(raw) * 0.390625F);
}

I32 MAX17205::decodeThermistorTemperature(U16 raw) {
    // Shadow-RAM thermistor registers report tenths of a Kelvin. Subtracting
    // 2731 (273.1 K) gives tenths of a degree C; times 10 gives centi-Celsius.
    return (static_cast<I32>(raw) - 2731) * 10;
}

// ----------------------------------------------------------------------
// Bus helpers
// ----------------------------------------------------------------------

Drv::I2cStatus MAX17205::readRegister(U32 address, Register reg, U16& value) {
    // A register read is a single write-read transaction: write the one-byte
    // register index, then read two bytes back with a repeated start. The
    // buffers are stack locals, which is safe because busWriteRead_out is a
    // synchronous call -- the driver is finished with them before it returns.
    U8 registerAddress = static_cast<U8>(reg);
    U8 response[2] = {0, 0};
    Fw::Buffer writeBuffer(&registerAddress, static_cast<Fw::Buffer::SizeType>(sizeof registerAddress));
    Fw::Buffer readBuffer(response, static_cast<Fw::Buffer::SizeType>(sizeof response));

    const Drv::I2cStatus status = this->busWriteRead_out(0, address, writeBuffer, readBuffer);
    if (status == Drv::I2cStatus::I2C_OK) {
        value = static_cast<U16>(static_cast<U16>(response[0]) | static_cast<U16>(response[1] << 8));
    } else {
        this->log_WARNING_HI_I2cError(static_cast<U8>(reg), status);
    }
    return status;
}

Drv::I2cStatus MAX17205::writeRegister(U32 address, Register reg, U8 valueLow, U8 valueHigh) {
    // Write-only transaction: register index followed by the 16-bit value,
    // low byte first.
    U8 request[3] = {static_cast<U8>(reg), valueLow, valueHigh};
    Fw::Buffer writeBuffer(request, static_cast<Fw::Buffer::SizeType>(sizeof request));

    const Drv::I2cStatus status = this->busWrite_out(0, address, writeBuffer);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->log_WARNING_HI_I2cError(static_cast<U8>(reg), status);
    }
    return status;
}

// ----------------------------------------------------------------------
// Per-measurement reads
//
// Each one does its transaction, decodes it, and publishes it. A failed read
// publishes nothing, so the telemetry database keeps the last good value for
// that channel rather than showing a fault value; readRegister has already
// emitted the I2cError event naming the register.
// ----------------------------------------------------------------------

Drv::I2cStatus MAX17205::readStatus(project::BatteryState&) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, STATUS_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        this->tlmWrite_Status(raw);
    }
    return status;
}

Drv::I2cStatus MAX17205::readStateOfCharge(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, REPSOC_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_stateOfCharge(decodeStateOfCharge(raw));
        this->tlmWrite_StateOfCharge(state.get_stateOfCharge());
    }
    return status;
}

Drv::I2cStatus MAX17205::readRemainingCapacity(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, REPCAP_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_remainingCapacity(decodeCapacity(raw));
        this->tlmWrite_RemainingCapacity(state.get_remainingCapacity());
    }
    return status;
}

Drv::I2cStatus MAX17205::readFullCapacity(project::BatteryState& state) {
    // FullCapRep uses the same 0.5 mAh per count scaling as RepCap.
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, CAPACITY_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_fullCapacity(decodeCapacity(raw));
        this->tlmWrite_FullCapacity(state.get_fullCapacity());
    }
    return status;
}

Drv::I2cStatus MAX17205::readCurrent(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, CURRENT_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        // Signed: negative while the pack is discharging.
        state.set_current(decodeCurrent(toSigned(raw)));
        this->tlmWrite_Current(state.get_current());
    }
    return status;
}

Drv::I2cStatus MAX17205::readVoltage(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, VBAT_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_voltage(decodeVoltage(raw));
        this->tlmWrite_Voltage(state.get_voltage());
    }
    return status;
}

Drv::I2cStatus MAX17205::readMidVoltage(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, VCELL_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_midVoltage(decodeMidVoltage(raw));
        this->tlmWrite_MidVoltage(state.get_midVoltage());
    }
    return status;
}

Drv::I2cStatus MAX17205::readCycles(project::BatteryState& state) {
    // Reported raw; the ground applies whatever cycle scaling it wants.
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, CYCLES_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_cycles(raw);
        this->tlmWrite_Cycles(state.get_cycles());
    }
    return status;
}

Drv::I2cStatus MAX17205::readTimeToEmpty(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, TTE_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_timeToEmpty(decodeTime(raw));
        this->tlmWrite_TimeToEmpty(state.get_timeToEmpty());
    }
    return status;
}

Drv::I2cStatus MAX17205::readTimeToFull(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, TTF_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_timeToFull(decodeTime(raw));
        this->tlmWrite_TimeToFull(state.get_timeToFull());
    }
    return status;
}

Drv::I2cStatus MAX17205::readTimeSincePowerUp(project::BatteryState& state) {
    // Reported raw; see the channel comment in the .fpp about the units.
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, TIMERH_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_timeSincePowerUp(raw);
        this->tlmWrite_TimeSincePowerUp(state.get_timeSincePowerUp());
    }
    return status;
}

Drv::I2cStatus MAX17205::readTemperature(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_deviceAddress, TEMP_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        // Signed, like Current: a pack below freezing must read negative.
        state.set_temperature(decodeTemperature(toSigned(raw)));
        this->tlmWrite_Temperature(state.get_temperature());
    }
    return status;
}

Drv::I2cStatus MAX17205::readTemperatureAin1(project::BatteryState& state) {
    // Note the shadow address: the thermistor registers live behind 0x0B,
    // not the fuel-gauge address.
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_shadowAddress, TEMP1_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_temperatureAin1(decodeThermistorTemperature(raw));
        this->tlmWrite_TemperatureAin1(state.get_temperatureAin1());
    }
    return status;
}

Drv::I2cStatus MAX17205::readTemperatureAin2(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_shadowAddress, TEMP2_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_temperatureAin2(decodeThermistorTemperature(raw));
        this->tlmWrite_TemperatureAin2(state.get_temperatureAin2());
    }
    return status;
}

Drv::I2cStatus MAX17205::readTemperatureDie(project::BatteryState& state) {
    U16 raw = 0;
    const Drv::I2cStatus status = this->readRegister(this->m_shadowAddress, INTTEMP_ADDR, raw);
    if (status == Drv::I2cStatus::I2C_OK) {
        state.set_temperatureDie(decodeThermistorTemperature(raw));
        this->tlmWrite_TemperatureDie(state.get_temperatureDie());
    }
    return status;
}

Drv::I2cStatus MAX17205::resetDevice() {
    // CONFIG2 bit 0 (POR_CMD) requests a full power-on reset of the gauge.
    return this->writeRegister(this->m_deviceAddress, CONFIG2_ADDR, 0x01, 0x00);
}

// ----------------------------------------------------------------------
// Sampling
// ----------------------------------------------------------------------

U32 MAX17205::sampleAll(project::BatteryState& state) {
    // The reads are deliberately independent. Each is its own transaction and
    // a failure in one says nothing about the others, so one flaky register
    // must not cost an entire sample of battery telemetry. Count the failures
    // and let the caller decide what to do about them.
    U32 failures = 0;

    failures += (this->readStatus(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;

    // Fuel-gauge register bank (0x36).
    failures += (this->readVoltage(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readCurrent(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readMidVoltage(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readStateOfCharge(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readRemainingCapacity(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readFullCapacity(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readCycles(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readTimeToEmpty(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readTimeToFull(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readTimeSincePowerUp(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readTemperature(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;

    // Shadow RAM (0x0B).
    failures += (this->readTemperatureAin1(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readTemperatureAin2(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;
    failures += (this->readTemperatureDie(state) == Drv::I2cStatus::I2C_OK) ? 0U : 1U;

    if (failures == 0) {
        // A clean sweep means the bus is healthy again, so re-arm the error
        // event. Without this, five failures would silence the channel for
        // the rest of the mission.
        this->log_WARNING_HI_I2cError_ThrottleClear();
    }

    return failures;
}

// ----------------------------------------------------------------------
// Handlers
// ----------------------------------------------------------------------

U32 MAX17205::read_handler(FwIndexType portNum, project::BatteryState& state) {
    // Runs on the calling task's thread: the ~15 blocking I2C transactions
    // below are spent out of that task's time budget, which is the point of
    // this component being passive. The port is guarded, so a second caller
    // waits rather than interleaving register reads.
    return this->sampleAll(state);
}

Drv::I2cStatus MAX17205::reset_handler(FwIndexType portNum) {
    return this->resetDevice();
}

void MAX17205::run_handler(FwIndexType portNum, U32 context) {
    // Rate-group entry point for running without a task driving the gauge.
    // Does the same work as read_handler and keeps only the telemetry;
    // failures are already reported through the throttled I2cError event.
    project::BatteryState state;
    (void)this->sampleAll(state);
}

}  // namespace project

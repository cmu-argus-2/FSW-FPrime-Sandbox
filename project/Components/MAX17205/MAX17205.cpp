#include "project/Components/MAX17205/MAX17205.hpp"

namespace project {

MAX17205::MAX17205(const char* const compName)
    : MAX17205ComponentBase(compName),
      m_deviceAddress(DEFAULT_DEVICE_ADDRESS),
      m_shadowAddress(DEFAULT_SHADOW_ADDRESS) {}

MAX17205::~MAX17205() {}

void MAX17205::configure(U32 deviceAddress, U32 shadowAddress) {
    // The MAX17205 exposes normal measurements and shadow-RAM measurements at
    // separate I2C addresses. Keep both configurable for board variants.
    m_deviceAddress = deviceAddress;
    m_shadowAddress = shadowAddress;
}

I16 MAX17205::unpackSigned(U8 lowByte, U8 highByte) {
    // MAX17205 registers arrive little-endian. Casting the assembled U16 to
    // I16 preserves the two's-complement current value from the Python driver.
    const U16 value = static_cast<U16>(lowByte) | (static_cast<U16>(highByte) << 8);
    return static_cast<I16>(value);
}

F32 MAX17205::decodeStateOfCharge(U16 raw) {
    // Reported state of charge uses 1/256 percent per register count.
    return static_cast<F32>(raw) / 256.0F;
}

F32 MAX17205::decodeCapacity(U16 raw) {
    // Reported and full capacity use 0.5 mAh per register count.
    return static_cast<F32>(raw) * 0.5F;
}

F32 MAX17205::decodeCurrent(I16 raw) {
    // The Python expression 0.0015625 / 0.01 simplifies to 0.15625 mA/count.
    return static_cast<F32>(raw) * 0.15625F;
}

F32 MAX17205::decodeVoltage(U16 raw) {
    // VBAT is reported in 1.25 mV register increments.
    return static_cast<F32>(raw) * 1.25F;
}

F32 MAX17205::decodeMidVoltage(U16 raw) {
    // VCELL is reported in 0.078125 mV register increments.
    return static_cast<F32>(raw) * 0.078125F;
}

F32 MAX17205::decodeTime(U16 raw) {
    // TTE and TTF use 5.625 seconds per register count.
    return static_cast<F32>(raw) * 5.625F;
}

F32 MAX17205::decodeTemperature(U16 raw) {
    // The pack temperature register is scaled as 100/256 centi-degrees C.
    return static_cast<F32>(raw) * 0.390625F;
}

I32 MAX17205::decodeThermistorTemperature(U16 raw) {
    // Shadow-RAM thermistor values are 0.1 K. Convert to centi-degrees C.
    return (static_cast<I32>(raw) - 2731) * 10;
}

Drv::I2cStatus MAX17205::readRegister(U32 address, U8 reg, U16& value) {
    // A register read is one write-read transaction: send the register index,
    // then let the bus driver fill the two-byte response buffer.
    U8 registerAddress = reg;
    U8 response[2] = {0, 0};
    Fw::Buffer writeBuffer(&registerAddress, sizeof(registerAddress));
    Fw::Buffer readBuffer(response, sizeof(response));

    const Drv::I2cStatus status = this->busWriteRead_out(0, address, writeBuffer, readBuffer);
    if (status == Drv::I2cStatus::I2C_OK) {
        value = static_cast<U16>(response[0]) | (static_cast<U16>(response[1]) << 8);
    }
    return status;
}

Drv::I2cStatus MAX17205::writeRegister(U32 address, U8 reg, U8 valueLow, U8 valueHigh) {
    // Reset is a write-only transaction. The busWrite port avoids requiring a
    // dummy read buffer and matches the MAX17205 register protocol.
    U8 request[3] = {reg, valueLow, valueHigh};
    Fw::Buffer writeBuffer(request, sizeof(request));
    return this->busWrite_out(0, address, writeBuffer);
}

void MAX17205::reportError(FwOpcodeType opCode, U32 cmdSeq, Drv::I2cStatus status) {
    // Commands fail visibly through both an event and the normal F´ response.
    this->log_WARNING_HI_I2cError(status);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
}

void MAX17205::READ_ALL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Keep the command synchronous from the device's point of view: publish a
    // complete snapshot only as each register transaction succeeds.
    U16 raw = 0;
    Drv::I2cStatus status = Drv::I2cStatus::I2C_OK;

    // Main device address: pack and fuel-gauge measurements.
    status = this->readRegister(m_deviceAddress, VBAT, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_Voltage(decodeVoltage(raw));

    status = this->readRegister(m_deviceAddress, CURRENT, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_Current(decodeCurrent(unpackSigned(static_cast<U8>(raw), static_cast<U8>(raw >> 8))));

    status = this->readRegister(m_deviceAddress, VCELL, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_MidVoltage(decodeMidVoltage(raw));

    status = this->readRegister(m_deviceAddress, REPSOC, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_StateOfCharge(decodeStateOfCharge(raw));

    status = this->readRegister(m_deviceAddress, REPCAP, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_RemainingCapacity(decodeCapacity(raw));

    status = this->readRegister(m_deviceAddress, CAPACITY, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_FullCapacity(decodeCapacity(raw));

    status = this->readRegister(m_deviceAddress, AVGCELL, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_Cycles(raw);

    status = this->readRegister(m_deviceAddress, TTE, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_TimeToEmpty(decodeTime(raw));

    status = this->readRegister(m_deviceAddress, TTF, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_TimeToFull(decodeTime(raw));

    status = this->readRegister(m_deviceAddress, TIMERH, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_TimeSincePowerUp(raw);

    status = this->readRegister(m_deviceAddress, TEMP, raw);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->reportError(opCode, cmdSeq, status);
        return;
    }
    this->tlmWrite_Temperature(decodeTemperature(raw));

    // Shadow-RAM address: the two external thermistors and internal die sensor.
    const U8 shadowRegisters[] = {TEMP1, TEMP2, INTTEMP};
    for (FwSizeType index = 0; index < 3; ++index) {
        status = this->readRegister(m_shadowAddress, shadowRegisters[index], raw);
        if (status != Drv::I2cStatus::I2C_OK) {
            this->reportError(opCode, cmdSeq, status);
            return;
        }
        if (index == 0) {
            this->tlmWrite_TemperatureAin1(decodeThermistorTemperature(raw));
        } else if (index == 1) {
            this->tlmWrite_TemperatureAin2(decodeThermistorTemperature(raw));
        } else {
            this->tlmWrite_TemperatureDie(decodeThermistorTemperature(raw));
        }
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MAX17205::RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // CONFIG2 bit 0 is the reset request used by the original Python driver.
    const Drv::I2cStatus status = this->writeRegister(m_deviceAddress, CONFIG2, 0x01, 0x00);
    if (status == Drv::I2cStatus::I2C_OK) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->reportError(opCode, cmdSeq, status);
    }
}

}  // namespace project

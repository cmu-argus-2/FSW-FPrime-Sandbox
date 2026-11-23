#include "project/Drivers/DRV8235/DRV8235.hpp"

namespace project {

DRV8235::DRV8235(const char* const compName) : DRV8235ComponentBase(compName) {}

bool DRV8235::configure(U32 address) {
    if (address > 0x7F || !this->isConnected_busWriteRead_OutputPort(0) || !this->isConnected_busWrite_OutputPort(0)) {
        this->log_WARNING_HI_ConfigurationError(address);
        return false;
    }
    this->m_address = address;
    this->m_configured = true;
    this->m_initialized = false;
    return true;
}

Drv::I2cStatus DRV8235::readRegister(U8 reg, U8& value) {
    // Stack buffers are safe: busWriteRead_out is synchronous, so the bus
    // driver is done with them before it returns.
    U8 registerAddress = reg;
    U8 response = 0;
    Fw::Buffer writeBuffer(&registerAddress, static_cast<Fw::Buffer::SizeType>(sizeof registerAddress));
    Fw::Buffer readBuffer(&response, static_cast<Fw::Buffer::SizeType>(sizeof response));

    const Drv::I2cStatus status = this->busWriteRead_out(0, this->m_address, writeBuffer, readBuffer);
    if (status == Drv::I2cStatus::I2C_OK) {
        value = response;
    } else {
        this->log_WARNING_HI_I2cError(reg, status);
    }
    return status;
}

Drv::I2cStatus DRV8235::writeRegister(U8 reg, U8 value) {
    U8 request[2] = {reg, value};
    Fw::Buffer writeBuffer(request, static_cast<Fw::Buffer::SizeType>(sizeof request));

    const Drv::I2cStatus status = this->busWrite_out(0, this->m_address, writeBuffer);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->log_WARNING_HI_I2cError(reg, status);
    }
    return status;
}

Drv::I2cStatus DRV8235::writeField(const DRV8235Math::Field& field) {
    U8 current = 0;
    if (field.mask != 0xFF) {
        const Drv::I2cStatus status = this->readRegister(field.reg, current);
        if (status != Drv::I2cStatus::I2C_OK) {
            return status;
        }
    }
    return this->writeRegister(field.reg, DRV8235Math::applyField(current, field));
}

Drv::I2cStatus DRV8235::initialize() {
    for (U32 step = 0; step < DRV8235Math::INIT_STEPS; step++) {
        const Drv::I2cStatus status = this->writeField(DRV8235Math::INIT_SEQUENCE[step]);
        if (status != Drv::I2cStatus::I2C_OK) {
            return status;
        }
    }
    this->m_initialized = true;
    this->log_ACTIVITY_LO_Initialized(this->m_address);
    return Drv::I2cStatus::I2C_OK;
}

Drv::I2cStatus DRV8235::takeSample(Codes& codes, project::PowerSample& sample) {
    if (!this->m_configured) {
        return Drv::I2cStatus::I2C_OPEN_ERR;
    }

    Drv::I2cStatus status = Drv::I2cStatus::I2C_OK;
    if (!this->m_initialized) {
        status = this->initialize();
    }

    // CONFIG4 carries the bridge direction, which sets the sign. It is read
    // from the chip rather than cached so a direction set by ADCS is honored.
    Codes raw = {0, 0, 0};
    if (status == Drv::I2cStatus::I2C_OK) {
        status = this->readRegister(DRV8235Math::REG_STATUS1, raw.voltage);
    }
    if (status == Drv::I2cStatus::I2C_OK) {
        status = this->readRegister(DRV8235Math::REG_STATUS2, raw.current);
    }
    if (status == Drv::I2cStatus::I2C_OK) {
        status = this->readRegister(DRV8235Math::CONFIG4, raw.config4);
    }

    if (status != Drv::I2cStatus::I2C_OK) {
        // The chip may have lost power (COIL_EN low) and with it its
        // registers, so set it up again once it answers.
        this->m_initialized = false;
        return status;
    }

    // A good read means the bus is healthy again, so re-arm the error event.
    this->log_WARNING_HI_I2cError_ThrottleClear();

    const bool reverse = DRV8235Math::isReverse(raw.config4);
    codes = raw;
    sample.set_voltageMv(DRV8235Math::voltageMillivolts(raw.voltage, reverse));
    sample.set_currentMa(DRV8235Math::currentMilliamps(raw.current, reverse));
    return Drv::I2cStatus::I2C_OK;
}

Drv::I2cStatus DRV8235::read_handler(FwIndexType portNum, project::PowerSample& sample) {
    Codes codes = {0, 0, 0};
    return this->takeSample(codes, sample);
}

void DRV8235::poll_handler(FwIndexType portNum, U32 context) {
    Codes codes = {0, 0, 0};
    project::PowerSample sample;
    if (this->takeSample(codes, sample) != Drv::I2cStatus::I2C_OK) {
        return;
    }

    U8 faults = 0;
    if (this->readRegister(DRV8235Math::FAULT_STATUS, faults) != Drv::I2cStatus::I2C_OK) {
        this->m_initialized = false;
        return;
    }

    this->tlmWrite_Reading(
        project::DRV8235Reading(codes.voltage, codes.current, sample.get_voltageMv(), sample.get_currentMa(), faults));
    if ((faults & DRV8235Math::FAULT_ANY) != 0) {
        this->log_WARNING_HI_DeviceFault(faults);
    }
}

}  // namespace project

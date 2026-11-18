#include "project/Drivers/ADM1176/ADM1176.hpp"

namespace project {

ADM1176::ADM1176(const char* const compName) : ADM1176ComponentBase(compName) {}

bool ADM1176::configure(U32 address, U32 senseResistanceMilliOhms, bool lowVoltageRange) {
    if (address > 0x7F || senseResistanceMilliOhms == 0 || !this->isConnected_i2cWrite_OutputPort(0) ||
        !this->isConnected_i2cRead_OutputPort(0)) {
        this->log_WARNING_HI_ConfigurationError(address, senseResistanceMilliOhms);
        return false;
    }
    this->m_address = address;
    this->m_senseResistanceMilliOhms = senseResistanceMilliOhms;
    this->m_lowVoltageRange = lowVoltageRange;
    this->m_configured = true;

    // C0=V_CONT, C2=I_CONT, and C4 selects the 7:2 (low) voltage range.
    U8 command = static_cast<U8>(0x05 | (lowVoltageRange ? 0x10 : 0x00));
    Fw::Buffer commandBuffer(&command, sizeof(command));
    const Drv::I2cStatus status = this->i2cWrite_out(0, this->m_address, commandBuffer);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->m_configured = false;
        this->log_WARNING_HI_I2cError(0, status);
        return false;
    }
    return true;
}

Drv::I2cStatus ADM1176::takeSample(ADM1176Math::Sample& codes, project::PowerSample& sample) {
    if (!this->m_configured) {
        return Drv::I2cStatus::I2C_OPEN_ERR;
    }

    U8 bytes[3] = {};
    Fw::Buffer readBuffer(bytes, sizeof(bytes));
    const Drv::I2cStatus status = this->i2cRead_out(0, this->m_address, readBuffer);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->log_WARNING_HI_I2cError(1, status);
        return status;
    }

    // A good read means the bus is healthy again, so re-arm the error event.
    this->log_WARNING_HI_I2cError_ThrottleClear();

    codes = ADM1176Math::decode(bytes);
    sample.set_voltageMv(static_cast<I32>(ADM1176Math::voltageMillivolts(codes.voltageCode, this->m_lowVoltageRange)));
    sample.set_currentMa(
        static_cast<I32>(ADM1176Math::currentMilliamps(codes.currentCode, this->m_senseResistanceMilliOhms)));
    return Drv::I2cStatus::I2C_OK;
}

Drv::I2cStatus ADM1176::read_handler(FwIndexType portNum, project::PowerSample& sample) {
    ADM1176Math::Sample codes = {0, 0};
    return this->takeSample(codes, sample);
}

void ADM1176::poll_handler(FwIndexType portNum, U32 context) {
    ADM1176Math::Sample codes = {0, 0};
    project::PowerSample sample;
    if (this->takeSample(codes, sample) != Drv::I2cStatus::I2C_OK) {
        return;
    }

    this->tlmWrite_VoltageCode(codes.voltageCode);
    this->tlmWrite_CurrentCode(codes.currentCode);
    this->tlmWrite_VoltageMv(static_cast<U32>(sample.get_voltageMv()));
    this->tlmWrite_CurrentMa(static_cast<U32>(sample.get_currentMa()));
}

}  // namespace project
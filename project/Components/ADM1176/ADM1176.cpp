#include "project/Components/ADM1176/ADM1176.hpp"
#include "project/Components/ADM1176/ADM1176Math.hpp"

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

void ADM1176::poll_handler(FwIndexType portNum, U32 context) {
    if (!this->m_configured) {
        return;
    }

    U8 bytes[3] = {};
    Fw::Buffer readBuffer(bytes, sizeof(bytes));
    const Drv::I2cStatus status = this->i2cRead_out(0, this->m_address, readBuffer);
    if (status != Drv::I2cStatus::I2C_OK) {
        this->log_WARNING_HI_I2cError(1, status);
        return;
    }

    const ADM1176Math::Sample sample = ADM1176Math::decode(bytes);
    const U32 voltageMv = ADM1176Math::voltageMillivolts(sample.voltageCode, this->m_lowVoltageRange);
    const U32 currentMa = ADM1176Math::currentMilliamps(sample.currentCode, this->m_senseResistanceMilliOhms);

    this->tlmWrite_VoltageCode(sample.voltageCode);
    this->tlmWrite_CurrentCode(sample.currentCode);
    this->tlmWrite_VoltageMv(voltageMv);
    this->tlmWrite_CurrentMa(currentMa);
}

}  // namespace project

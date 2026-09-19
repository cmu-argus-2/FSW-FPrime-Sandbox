#ifndef project_MAX17205_HPP
#define project_MAX17205_HPP

#include "project/Components/MAX17205/MAX17205ComponentAc.hpp"

namespace project {

class MAX17205 final : public MAX17205ComponentBase {
  public:
    static constexpr U32 DEFAULT_DEVICE_ADDRESS = 0x36;
    static constexpr U32 DEFAULT_SHADOW_ADDRESS = 0x0B;

    explicit MAX17205(const char* const compName);
    ~MAX17205() override;

    void configure(U32 deviceAddress, U32 shadowAddress);

    static I16 unpackSigned(U8 lowByte, U8 highByte);
    static F32 decodeStateOfCharge(U16 raw);
    static F32 decodeCapacity(U16 raw);
    static F32 decodeCurrent(I16 raw);
    static F32 decodeVoltage(U16 raw);
    static F32 decodeMidVoltage(U16 raw);
    static F32 decodeTime(U16 raw);
    static F32 decodeTemperature(U16 raw);
    static I32 decodeThermistorTemperature(U16 raw);

  private:
    enum Register : U8 {
        VCELL = 0x09,
        REPSOC = 0x06,
        REPCAP = 0x05,
        CURRENT = 0x0A,
        TTE = 0x11,
        TTF = 0x20,
        CAPACITY = 0x10,
        VBAT = 0xDA,
        AVGCELL = 0x17,
        TIMERH = 0xBE,
        TEMP = 0x08,
        CONFIG2 = 0xBB,
        TEMP1 = 0x34,
        TEMP2 = 0x3B,
        INTTEMP = 0x35
    };

    Drv::I2cStatus readRegister(U32 address, U8 reg, U16& value);
    Drv::I2cStatus writeRegister(U32 address, U8 reg, U8 valueLow, U8 valueHigh);
    void reportError(FwOpcodeType opCode, U32 cmdSeq, Drv::I2cStatus status);

    void READ_ALL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    U32 m_deviceAddress;
    U32 m_shadowAddress;
};

}  // namespace project

#endif

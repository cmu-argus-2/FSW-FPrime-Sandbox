#ifndef project_DRV8235_HPP
#define project_DRV8235_HPP

#include "project/Drivers/DRV8235/DRV8235ComponentAc.hpp"
#include "project/Drivers/DRV8235/DRV8235Math.hpp"

namespace project {

class DRV8235 final : public DRV8235ComponentBase {
  public:
    explicit DRV8235(const char* const compName);
    ~DRV8235() override = default;

    //! Store the 7-bit I2C address. Does not touch the bus: the chip may
    //! still be unpowered (COIL_EN low) when the topology calls this.
    bool configure(U32 address);

  private:
    //! Raw register values from one sample
    struct Codes {
        U8 voltage;
        U8 current;
        U8 config4;
    };

    Drv::I2cStatus read_handler(FwIndexType portNum, project::PowerSample& sample) override;
    void poll_handler(FwIndexType portNum, U32 context) override;

    //! Set up the chip if needed, then read and decode one sample. On I2C_OK
    //! fills both `codes` and `sample`; on failure leaves both untouched.
    Drv::I2cStatus takeSample(Codes& codes, project::PowerSample& sample);

    //! Run DRV8235Math::INIT_SEQUENCE. Stops at the first bus failure.
    Drv::I2cStatus initialize();

    Drv::I2cStatus readRegister(U8 reg, U8& value);
    Drv::I2cStatus writeRegister(U8 reg, U8 value);

    //! Read-modify-write one bit field; a full-register field is a plain write
    Drv::I2cStatus writeField(const DRV8235Math::Field& field);

    U32 m_address = 0;
    bool m_configured = false;   //!< configure() accepted an address
    bool m_initialized = false;  //!< INIT_SEQUENCE ran since the last bus failure
};

}  // namespace project

#endif

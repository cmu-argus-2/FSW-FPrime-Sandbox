#ifndef project_ADM1176_HPP
#define project_ADM1176_HPP

#include "project/Components/ADM1176/ADM1176ComponentAc.hpp"

namespace project {

class ADM1176 final : public ADM1176ComponentBase {
  public:
    explicit ADM1176(const char* const compName);
    ~ADM1176() override = default;

    //! Configure the 7-bit I2C address, sense resistor (mOhm), and voltage range.
    bool configure(U32 address, U32 senseResistanceMilliOhms, bool lowVoltageRange);

  private:
    void poll_handler(FwIndexType portNum, U32 context) override;

    U32 m_address = 0;
    U32 m_senseResistanceMilliOhms = 0;
    bool m_lowVoltageRange = false;
    bool m_configured = false;
};

}  // namespace project

#endif

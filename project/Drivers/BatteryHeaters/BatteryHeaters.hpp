// ======================================================================
// \title  BatteryHeaters.hpp
// \brief  Header for Argus battery heater enable/disable component
// ======================================================================

#ifndef project_BatteryHeaters_HPP
#define project_BatteryHeaters_HPP

#include "project/Drivers/BatteryHeaters/BatteryHeatersComponentAc.hpp"

namespace project {

class BatteryHeaters : public BatteryHeatersComponentBase {
  public:
    // ----------------------------------------------------------------
    // Construction, initialization, and destruction
    // ----------------------------------------------------------------

    explicit BatteryHeaters(const char* const compName);
    ~BatteryHeaters();

  private:
    // ----------------------------------------------------------------
    // Command handlers
    // ----------------------------------------------------------------

    void EnableHeater0_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void EnableHeater1_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void DisableHeater0_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void DisableHeater1_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    // ----------------------------------------------------------------
    // Port handlers (EPS task)
    // ----------------------------------------------------------------

    bool setHeater_handler(FwIndexType portNum, const project::HeaterId& heater, bool enable) override;
    bool getHeater_handler(FwIndexType portNum, const project::HeaterId& heater) override;

    // ----------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------

    //! Heater 0 always exists; heater 1 only if its port is connected
    bool heaterPresent(const project::HeaterId& heater);

    //! Turn a heater on, raising the master line first if needed.
    //! Returns false (and emits Heater1NotPresent) if the heater is absent.
    bool enableHeater(const project::HeaterId& heater);

    //! Turn a heater off, dropping the master line once no heater is on.
    //! Returns false (and emits Heater1NotPresent) if the heater is absent.
    bool disableHeater(const project::HeaterId& heater);

    //! Shadow state, mirrors the Python driver's __en_val / __heaterN_en_val
    bool m_enableState;
    bool m_heater0State;
    bool m_heater1State;
};

}  // namespace project

#endif
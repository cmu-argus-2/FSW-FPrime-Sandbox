// ======================================================================
// \title  BatteryHeaters.hpp
// \brief  Header for Argus battery heater enable/disable component
// ======================================================================

#ifndef project_BatteryHeaters_HPP
#define project_BatteryHeaters_HPP

#include "project/Components/BatteryHeaters/BatteryHeatersComponentAc.hpp"

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
      // Helpers
      // ----------------------------------------------------------------

      
      //! Shadow state, mirrors the Python driver's __en_val / __heaterN_en_val
      bool m_enableState;
      bool m_heater0State;
      bool m_heater1State;
  };

}

#endif

// ======================================================================
// \title  BatteryHeaters.cpp
// \brief  Implementation for Argus battery heater enable/disable component
// ======================================================================

#include "project/Components/BatteryHeaters/BatteryHeaters.hpp"

namespace project {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

BatteryHeaters ::BatteryHeaters(const char* const compName)
    : BatteryHeatersComponentBase(compName), m_enableState(false), m_heater0State(false), m_heater1State(false) {}

BatteryHeaters ::~BatteryHeaters() {}

// ----------------------------------------------------------------------
// Command handlers
// ----------------------------------------------------------------------

void BatteryHeaters ::EnableHeater0_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!m_enableState) {
        this->enableWrite_out(0, Fw::Logic::HIGH);
        m_enableState = true;
    }
    this->heater0Write_out(0, Fw::Logic::HIGH);
    m_heater0State = true;

    this->tlmWrite_Heater0Enabled(true);
    this->log_ACTIVITY_HI_Heater0Enabled();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void BatteryHeaters ::EnableHeater1_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Mirrors the Python driver's "if HEAT1_EN is not None" guard: heater 1
    // is only usable if its port was connected in the topology.
    if (!this->isConnected_heater1Write_OutputPort(0)) {
        this->log_WARNING_LO_Heater1NotPresent();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    if (!m_enableState) {
        this->enableWrite_out(0, Fw::Logic::HIGH);
        m_enableState = true;
    }
    this->heater1Write_out(0, Fw::Logic::HIGH);
    m_heater1State = true;

    this->tlmWrite_Heater1Enabled(true);
    this->log_ACTIVITY_HI_Heater1Enabled();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void BatteryHeaters ::DisableHeater0_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->heater0Write_out(0, Fw::Logic::LOW);
    m_heater0State = false;

    // Drop the master enable line only if heater 1 doesn't exist on this
    // board, or exists and is also off. (The Python driver unconditionally
    // dereferenced heater1_en.value here, which throws on single-heater
    // boards where heater1_en is None -- that's the bug this guard fixes.)
    if (!this->isConnected_heater1Write_OutputPort(0) || !m_heater1State) {
        this->enableWrite_out(0, Fw::Logic::LOW);
        m_enableState = false;
    }

    this->tlmWrite_Heater0Enabled(false);
    this->log_ACTIVITY_HI_Heater0Disabled();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void BatteryHeaters ::DisableHeater1_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!this->isConnected_heater1Write_OutputPort(0)) {
        this->log_WARNING_LO_Heater1NotPresent();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    this->heater1Write_out(0, Fw::Logic::LOW);
    m_heater1State = false;

    if (!m_heater0State) {
        this->enableWrite_out(0, Fw::Logic::LOW);
        m_enableState = false;
    }

    this->tlmWrite_Heater1Enabled(false);
    this->log_ACTIVITY_HI_Heater1Disabled();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace project

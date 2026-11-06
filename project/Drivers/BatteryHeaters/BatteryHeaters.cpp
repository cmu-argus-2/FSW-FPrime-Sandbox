// ======================================================================
// \title  BatteryHeaters.cpp
// \brief  Implementation for Argus battery heater enable/disable component
// ======================================================================

#include "project/Drivers/BatteryHeaters/BatteryHeaters.hpp"

namespace project {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

BatteryHeaters ::BatteryHeaters(const char* const compName)
    : BatteryHeatersComponentBase(compName), m_enableState(false), m_heater0State(false), m_heater1State(false) {}

BatteryHeaters ::~BatteryHeaters() {}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

bool BatteryHeaters ::heaterPresent(const project::HeaterId& heater) {
    // Mirrors the Python driver's "if HEAT1_EN is not None" guard: heater 1
    // is only usable if its port was connected in the topology.
    return (heater == project::HeaterId::HEATER0) || this->isConnected_heater1Write_OutputPort(0);
}

bool BatteryHeaters ::enableHeater(const project::HeaterId& heater) {
    if (!this->heaterPresent(heater)) {
        this->log_WARNING_LO_Heater1NotPresent();
        return false;
    }

    if (!m_enableState) {
        this->enableWrite_out(0, Fw::Logic::HIGH);
        m_enableState = true;
    }

    if (heater == project::HeaterId::HEATER0) {
        this->heater0Write_out(0, Fw::Logic::HIGH);
        m_heater0State = true;
        this->tlmWrite_Heater0Enabled(true);
        this->log_ACTIVITY_HI_Heater0Enabled();
    } else {
        this->heater1Write_out(0, Fw::Logic::HIGH);
        m_heater1State = true;
        this->tlmWrite_Heater1Enabled(true);
        this->log_ACTIVITY_HI_Heater1Enabled();
    }
    return true;
}

bool BatteryHeaters ::disableHeater(const project::HeaterId& heater) {
    if (!this->heaterPresent(heater)) {
        this->log_WARNING_LO_Heater1NotPresent();
        return false;
    }

    if (heater == project::HeaterId::HEATER0) {
        this->heater0Write_out(0, Fw::Logic::LOW);
        m_heater0State = false;
    } else {
        this->heater1Write_out(0, Fw::Logic::LOW);
        m_heater1State = false;
    }

    // Drop the master enable line only once no heater is left on. Heater 1
    // counts as off on a single-heater board. (The Python driver
    // unconditionally dereferenced heater1_en.value here, which throws on
    // single-heater boards where heater1_en is None. This guard fixes that.)
    const bool heater1On = this->isConnected_heater1Write_OutputPort(0) && m_heater1State;
    if (!m_heater0State && !heater1On) {
        this->enableWrite_out(0, Fw::Logic::LOW);
        m_enableState = false;
    }

    if (heater == project::HeaterId::HEATER0) {
        this->tlmWrite_Heater0Enabled(false);
        this->log_ACTIVITY_HI_Heater0Disabled();
    } else {
        this->tlmWrite_Heater1Enabled(false);
        this->log_ACTIVITY_HI_Heater1Disabled();
    }
    return true;
}

// ----------------------------------------------------------------------
// Command handlers
// ----------------------------------------------------------------------

void BatteryHeaters ::EnableHeater0_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->enableHeater(project::HeaterId::HEATER0);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void BatteryHeaters ::EnableHeater1_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const bool ok = this->enableHeater(project::HeaterId::HEATER1);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::VALIDATION_ERROR);
}

void BatteryHeaters ::DisableHeater0_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->disableHeater(project::HeaterId::HEATER0);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void BatteryHeaters ::DisableHeater1_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const bool ok = this->disableHeater(project::HeaterId::HEATER1);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::VALIDATION_ERROR);
}

// ----------------------------------------------------------------------
// Port handlers (EPS task)
// ----------------------------------------------------------------------

bool BatteryHeaters ::setHeater_handler(FwIndexType portNum, const project::HeaterId& heater, bool enable) {
    return enable ? this->enableHeater(heater) : this->disableHeater(heater);
}

bool BatteryHeaters ::getHeater_handler(FwIndexType portNum, const project::HeaterId& heater) {
    if (heater == project::HeaterId::HEATER0) {
        return m_heater0State;
    }
    return this->heaterPresent(heater) && m_heater1State;
}

}  // namespace project
// ======================================================================
// \title  EpsManager.cpp
// \brief  EPS task: electrical power subsystem flight logic
// ======================================================================

#include "project/Tasks/EpsManager/EpsManager.hpp"

namespace project {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

EpsManager ::EpsManager(const char* const compName) : EpsManagerComponentBase(compName), m_runCount(0) {}

EpsManager ::~EpsManager() {}

// ----------------------------------------------------------------------
// Port handlers
// ----------------------------------------------------------------------

void EpsManager ::run_handler(FwIndexType portNum, U32 context) {
    // tasks/eps.py checks log_counter % 5 before incrementing it, so the very
    // first run already includes the fuel gauge work. Same here.
    if ((m_runCount % FUEL_GAUGE_PERIOD) == 0) {
        this->fuelGaugeCycle();
    }
    m_runCount++;
    this->tlmWrite_RunCount(m_runCount);
}

void EpsManager ::pingIn_handler(FwIndexType portNum, U32 key) {
    // Answering on this thread proves the EPS thread is alive. A wedged I2C
    // bus inside a cycle shows up as missed pings in CdhCore.health.
    this->pingOut_out(0, key);
}

// ----------------------------------------------------------------------
// Cycle work
// ----------------------------------------------------------------------

void EpsManager ::fuelGaugeCycle() {
    if (!this->isConnected_fuelGauge_OutputPort(0)) {
        return;
    }
    // Default-constructed, so every field starts at 0. MAX17205 leaves a
    // field untouched when its register read fails, which gives the spec's
    // "leave its fields at 0" for a failing device.
    project::BatteryState state;
    const U32 failedReads = this->fuelGauge_out(0, state);
    this->log_ACTIVITY_LO_FuelGaugeSampled(state.get_stateOfCharge(), failedReads);
}

}  // namespace project
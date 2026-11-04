// ======================================================================
// \title  EpsManager.hpp
// \brief  EPS task: electrical power subsystem flight logic
// ======================================================================

#ifndef project_EpsManager_HPP
#define project_EpsManager_HPP

#include "project/Tasks/EpsManager/EpsManagerComponentAc.hpp"

namespace project {

class EpsManager final : public EpsManagerComponentBase {
  public:
    //! Every Nth run also does the fuel gauge work (spec: every 5th run at 1 Hz)
    static constexpr U32 FUEL_GAUGE_PERIOD = 5;

    explicit EpsManager(const char* const compName);
    ~EpsManager() override;

  private:
    // ----------------------------------------------------------------
    // Port handlers
    // ----------------------------------------------------------------

    void run_handler(FwIndexType portNum, U32 context) override;
    void pingIn_handler(FwIndexType portNum, U32 key) override;

    // ----------------------------------------------------------------
    // Cycle work
    // ----------------------------------------------------------------

    //! Read the fuel gauge and report it (every FUEL_GAUGE_PERIOD runs)
    void fuelGaugeCycle();

    U32 m_runCount;  //!< Runs handled since boot
};

}  // namespace project

#endif
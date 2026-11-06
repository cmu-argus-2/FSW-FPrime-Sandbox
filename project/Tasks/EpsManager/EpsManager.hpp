// ======================================================================
// \title  EpsManager.hpp
// \brief  EPS task: electrical power subsystem flight logic
// ======================================================================

#ifndef project_EpsManager_HPP
#define project_EpsManager_HPP

#include "project/Tasks/EpsManager/EpsLogic.hpp"
#include "project/Tasks/EpsManager/EpsManagerComponentAc.hpp"

namespace project {

class EpsManager final : public EpsManagerComponentBase {
  public:
    //! Every Nth run also does the fuel gauge work (spec: every 5th run at 1 Hz)
    static constexpr U32 FUEL_GAUGE_PERIOD = 5;

    //! Power monitor ports: mainboard, GPS (peripheral), radio, Jetson
    static constexpr FwIndexType NUM_POWER_MONITORS = 4;

    //! Loads with power alerts (EpsLoad): 4 monitors and 6 torque coils
    static constexpr FwIndexType NUM_LOADS = 10;

    explicit EpsManager(const char* const compName);
    ~EpsManager() override;

  private:
    // ----------------------------------------------------------------
    // Port handlers
    // ----------------------------------------------------------------

    void run_handler(FwIndexType portNum, U32 context) override;
    void pingIn_handler(FwIndexType portNum, U32 key) override;

    // ----------------------------------------------------------------
    // Every run
    // ----------------------------------------------------------------

    //! Read the power monitors, store their values, update their alerts
    void readPowerMonitors();

    //! Recompute one load's alert and emit an event when it changes
    void updateAlert(FwIndexType load, I32 millivolts, I32 milliamps, I64 thresholdMw);

    //! Publish the Alerts channel
    void publishAlerts();

    // ----------------------------------------------------------------
    // Every FUEL_GAUGE_PERIOD runs
    // ----------------------------------------------------------------

    //! Fuel gauge, CPU temperature, power flag, heaters, then the Data channel
    void fuelGaugeCycle();

    //! Copy a good fuel gauge sample into m_data, truncated toward zero
    void storeFuelGauge(const project::BatteryState& state);

    //! Advance the power flag from a new SOC
    void updatePowerFlag(I32 socPercent);

    //! Run the heater rules for both heaters
    void controlHeaters(const EpsLogic::HeaterTemperatures& temperatures);

    //! Run the heater rules for one heater
    void controlHeater(const project::HeaterId& heater, I32 temperatureCc);

    //! 1 if the heater is on, 0 if off or not connected
    U8 heaterState(const project::HeaterId& heater);

    // ----------------------------------------------------------------
    // State
    // ----------------------------------------------------------------

    U32 m_runCount;                               //!< Runs handled since boot
    bool m_alert[NUM_LOADS];                      //!< Current alert per load
    EpsLogic::PowerFlag m_powerFlag;              //!< Current EPS power flag
    EpsLogic::FuelGaugeHealth m_fuelGaugeHealth;  //!< Fuel gauge history
    bool m_heaterAbsent[2];                       //!< Heater rejected by the driver
    project::EpsData m_data;                      //!< Latest values for the Data channel
};

}  // namespace project

#endif
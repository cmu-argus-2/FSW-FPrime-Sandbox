// ======================================================================
// \title  AdcsManager.hpp
// \brief  ADCS task: detumbling with the B-dot or B-cross control law.
//
// The decisions (sensor checks, control laws, coil allocation, mode rules)
// are the pure functions in AdcsLogic.hpp. This class owns the timing, the
// IMU and the telemetry.
// ======================================================================

#ifndef project_AdcsManager_HPP
#define project_AdcsManager_HPP

#include "Os/RawTime.hpp"
#include "project/Tasks/AdcsManager/AdcsLogic.hpp"
#include "project/Tasks/AdcsManager/AdcsManagerComponentAc.hpp"

namespace project {

class AdcsManager final : public AdcsManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Control cycle timing, in microseconds
    // ----------------------------------------------------------------------

    //! B-dot: spacing of the six magnetometer samples. The IMU refreshes its
    //! magnetometer data every 80 ms (12.5 Hz), so each sample is new.
    static constexpr U32 MAG_SAMPLE_INTERVAL_US = 80000;

    //! B-dot: how long the coils stay on after the samples.
    static constexpr U32 BDOT_COILS_ON_US = 500000;

    //! B-cross: length of the control cycle, and its gyroscope period.
    static constexpr U32 BCROSS_CYCLE_US = 1000000;
    static constexpr U32 BCROSS_STEP_US = 50000;

    explicit AdcsManager(const char* const compName);
    ~AdcsManager() override;

  private:
    // ----------------------------------------------------------------------
    // Handlers
    // ----------------------------------------------------------------------

    //! One run: sense, update the mode, then one blocking control cycle.
    void run_handler(FwIndexType portNum, U32 context) override;

    void SET_CONTROLLER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const project::AdcsController& controller) override;
    void SET_SATELLITE_STATE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const project::SatelliteState& state) override;

    // ----------------------------------------------------------------------
    // Steps of a run
    // ----------------------------------------------------------------------

    //! Read and check the magnetometer, in T. A failed read is MAG_FAIL.
    AdcsLogic::Status readMag(AdcsLogic::Vec3& field);

    //! Read and check the gyroscope, in rad/s. A failed read is GYRO_FAIL.
    AdcsLogic::Status readGyro(AdcsLogic::Vec3& rate);

    //! Set the gyroscope range the satellite state calls for: 2000 deg/s
    //! while detumbling, 125 deg/s in nominal (finest resolution). The range
    //! is read first and written only if it differs.
    void applyGyroRangeForState();

    //! Advance the mode, reporting a change.
    void updateMode();

    //! Six magnetometer samples, the B-dot command for the coils, then the
    //! coils on for BDOT_COILS_ON_US and off.
    void bdotCycle();

    //! The gyroscope re-read and the B-cross command updated every
    //! BCROSS_STEP_US for BCROSS_CYCLE_US, then the coils off.
    void bcrossCycle();

    //! Allocate m_dipole to the coils and command them.
    void allocateAndCommand();

    //! Zero the dipole and the coils.
    void stopControl();

    //! Send each coil its throttle. The coil drivers are not connected yet,
    //! so this is where their ports will be called; until then it has no
    //! effect, and every coil is reported unavailable to the allocator.
    void commandCoils(const F32 throttle[AdcsLogic::COIL_COUNT]);

    //! Publish this run's telemetry. `sensed` is false when the run skipped
    //! the sensors (LOW_POWER), so their channels are left unchanged.
    void publish(bool sensed, const Os::RawTime& runStart);

    //! Sleep for at least `microseconds`. The Zephyr task delay sleeps whole
    //! milliseconds and truncates, so this rounds up.
    static void sleepAtLeastUs(U32 microseconds);

    //! Microseconds since `start`. A clock failure reads as the largest
    //! value, so a wait that depends on it ends instead of running forever.
    static U32 elapsedUs(const Os::RawTime& start);

    // ----------------------------------------------------------------------
    // State
    // ----------------------------------------------------------------------

    project::SatelliteState m_satState;
    AdcsLogic::Controller m_controller;
    AdcsLogic::Mode m_mode;

    //! Set once the IMU has booted; until then each run retries the boot.
    bool m_imuBooted;

    AdcsLogic::Vec3 m_gyro;
    AdcsLogic::Status m_gyroStatus;
    AdcsLogic::Vec3 m_mag;
    AdcsLogic::Status m_magStatus;

    AdcsLogic::Vec3 m_dipole;
    F32 m_throttle[AdcsLogic::COIL_COUNT];

    //! Which coils can be commanded. None until the coil drivers exist.
    bool m_coilAvailable[AdcsLogic::COIL_COUNT];
};

}  // namespace project

#endif

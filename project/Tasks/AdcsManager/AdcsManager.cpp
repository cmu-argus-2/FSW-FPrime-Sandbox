// ======================================================================
// \title  AdcsManager.cpp
// \brief  ADCS task: detumbling with the B-dot or B-cross control law.
// ======================================================================

#include "project/Tasks/AdcsManager/AdcsManager.hpp"

#include <limits>

#include "Os/Task.hpp"

namespace project {

// This project builds as C++14, where a `static constexpr` member that is
// bound to a reference anywhere (the gtest macros do this) needs an
// out-of-line definition, or the link fails.
constexpr U32 AdcsManager::MAG_SAMPLE_INTERVAL_US;
constexpr U32 AdcsManager::BDOT_COILS_ON_US;
constexpr U32 AdcsManager::BCROSS_CYCLE_US;
constexpr U32 AdcsManager::BCROSS_STEP_US;

namespace {

// Magnetometer calibration: hard-iron bias in T and per-axis scale. No
// calibration yet, so no correction.
constexpr AdcsLogic::Vec3 MAG_BIAS_T = {0.0F, 0.0F, 0.0F};
constexpr AdcsLogic::Vec3 MAG_SCALE = {1.0F, 1.0F, 1.0F};

// B-dot sample spacing in seconds, as the control law takes it.
constexpr F32 MAG_SAMPLE_INTERVAL_S = static_cast<F32>(AdcsManager::MAG_SAMPLE_INTERVAL_US) / 1.0e6F;

constexpr F32 NO_THROTTLE[AdcsLogic::COIL_COUNT] = {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};

AdcsLogic::Vec3 toVec3(const project::ImuVector& v) {
    return {v.get_x(), v.get_y(), v.get_z()};
}

project::ImuVector toImuVector(const AdcsLogic::Vec3& v) {
    return project::ImuVector(v.x, v.y, v.z);
}

project::AdcsSensorStatus toTlm(AdcsLogic::Status status) {
    return static_cast<project::AdcsSensorStatus::T>(status);
}

project::AdcsMode toTlm(AdcsLogic::Mode mode) {
    return static_cast<project::AdcsMode::T>(mode);
}

}  // namespace

// ----------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------

AdcsManager::AdcsManager(const char* const compName)
    : AdcsManagerComponentBase(compName),
      // Without a state manager there is no STARTUP to leave, so the task
      // starts out detumbling with the default control law.
      m_satState(project::SatelliteState::DETUMBLING),
      m_controller(AdcsLogic::Controller::BDOT),
      m_mode(AdcsLogic::Mode::TUMBLING),
      m_imuBooted(false),
      m_gyro(AdcsLogic::ZERO),
      m_gyroStatus(AdcsLogic::Status::OK),
      m_mag(AdcsLogic::ZERO),
      m_magStatus(AdcsLogic::Status::OK),
      m_dipole(AdcsLogic::ZERO),
      m_throttle{},
      m_coilAvailable{} {}

AdcsManager::~AdcsManager() {}

// ----------------------------------------------------------------------
// Handlers
// ----------------------------------------------------------------------

void AdcsManager::run_handler(FwIndexType portNum, U32 context) {
    Os::RawTime runStart;
    (void)runStart.now();

    // The IMU is booted here rather than at startup so that a part that did
    // not answer is tried again on the next run.
    if (!this->m_imuBooted) {
        this->m_imuBooted = (this->imuBoot_out(0) == Fw::Success::SUCCESS);
    }

    if (this->m_satState == project::SatelliteState::STARTUP) {
        return;
    }

    if (this->m_satState == project::SatelliteState::LOW_POWER) {
        this->stopControl();
        this->publish(false, runStart);
        return;
    }

    // Sensor snapshot: magnetometer first, then gyroscope.
    this->m_magStatus = this->readMag(this->m_mag);
    this->m_gyroStatus = this->readGyro(this->m_gyro);

    this->applyGyroRangeForState();
    this->updateMode();

    if (this->m_mode == AdcsLogic::Mode::ACS_OFF || this->m_mode == AdcsLogic::Mode::VF_TUMBLING) {
        this->stopControl();
    } else if (this->m_controller == AdcsLogic::Controller::BDOT) {
        this->bdotCycle();
    } else {
        this->bcrossCycle();
    }

    this->publish(true, runStart);
}

void AdcsManager::SET_CONTROLLER_cmdHandler(FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            const project::AdcsController& controller) {
    this->m_controller = static_cast<AdcsLogic::Controller>(controller.e);
    this->log_ACTIVITY_HI_ControllerSet(controller);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AdcsManager::SET_SATELLITE_STATE_cmdHandler(FwOpcodeType opCode,
                                                 U32 cmdSeq,
                                                 const project::SatelliteState& state) {
    this->m_satState = state;
    this->log_ACTIVITY_HI_SatelliteStateSet(state);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Steps of a run
// ----------------------------------------------------------------------

AdcsLogic::Status AdcsManager::readMag(AdcsLogic::Vec3& field) {
    project::ImuVector raw;
    if (this->imuReadMag_out(0, raw) != Fw::Success::SUCCESS) {
        field = AdcsLogic::ZERO;
        return AdcsLogic::Status::MAG_FAIL;
    }
    return AdcsLogic::correctMag(toVec3(raw), MAG_BIAS_T, MAG_SCALE, field);
}

AdcsLogic::Status AdcsManager::readGyro(AdcsLogic::Vec3& rate) {
    project::ImuVector raw;
    if (this->imuReadGyro_out(0, raw) != Fw::Success::SUCCESS) {
        rate = AdcsLogic::ZERO;
        return AdcsLogic::Status::GYRO_FAIL;
    }
    return AdcsLogic::checkGyro(toVec3(raw), rate);
}

void AdcsManager::applyGyroRangeForState() {
    project::GyroRange wanted;
    if (this->m_satState == project::SatelliteState::DETUMBLING) {
        wanted = project::GyroRange::DPS_2000;
    } else if (this->m_satState == project::SatelliteState::NOMINAL) {
        wanted = project::GyroRange::DPS_125;
    } else {
        return;
    }
    // An IMU that cannot report its range is not written to either; its
    // driver has already reported the failure.
    project::GyroRange current;
    if (this->imuGetGyroRange_out(0, current) == Fw::Success::SUCCESS && current != wanted) {
        (void)this->imuSetGyroRange_out(0, wanted);
    }
}

void AdcsManager::updateMode() {
    const AdcsLogic::Mode next =
        AdcsLogic::nextMode(this->m_mode, this->m_controller, this->m_gyroStatus, this->m_gyro);
    if (next != this->m_mode) {
        this->log_ACTIVITY_HI_ModeChanged(toTlm(this->m_mode), toTlm(next));
        this->m_mode = next;
    }
}

void AdcsManager::bdotCycle() {
    // Only samples that pass the plausibility check are kept; the control
    // law needs all six.
    AdcsLogic::Vec3 samples[AdcsLogic::BDOT_SAMPLES];
    U32 good = 0;
    for (U32 k = 0; k < AdcsLogic::BDOT_SAMPLES; k++) {
        AdcsLogic::Vec3 field;
        if (this->readMag(field) == AdcsLogic::Status::OK) {
            samples[good] = field;
            good++;
        }
        if (k + 1 < AdcsLogic::BDOT_SAMPLES) {
            sleepAtLeastUs(MAG_SAMPLE_INTERVAL_US);
        }
    }

    // The latest good sample becomes the run's field; with none, the field
    // keeps its last value and is reported failed.
    if (good > 0) {
        this->m_mag = samples[good - 1];
        this->m_magStatus = AdcsLogic::Status::OK;
    } else {
        this->m_magStatus = AdcsLogic::Status::MAG_FAIL;
    }

    if (good == AdcsLogic::BDOT_SAMPLES) {
        this->m_dipole = AdcsLogic::bdot(samples, MAG_SAMPLE_INTERVAL_S);
        this->allocateAndCommand();
    } else {
        this->stopControl();
    }

    sleepAtLeastUs(BDOT_COILS_ON_US);
    this->commandCoils(NO_THROTTLE);
}

void AdcsManager::bcrossCycle() {
    Os::RawTime cycleStart;
    (void)cycleStart.now();
    while (elapsedUs(cycleStart) < BCROSS_CYCLE_US) {
        Os::RawTime stepStart;
        (void)stepStart.now();

        this->m_gyroStatus = this->readGyro(this->m_gyro);
        // The field is the run's snapshot; only the rate is refreshed.
        const bool sensorsOk =
            (this->m_gyroStatus == AdcsLogic::Status::OK) && (this->m_magStatus == AdcsLogic::Status::OK);
        this->m_dipole = sensorsOk ? AdcsLogic::bcross(this->m_mag, this->m_gyro) : AdcsLogic::ZERO;
        this->allocateAndCommand();

        const U32 stepUs = elapsedUs(stepStart);
        if (stepUs < BCROSS_STEP_US) {
            sleepAtLeastUs(BCROSS_STEP_US - stepUs);
        }
    }
    this->commandCoils(NO_THROTTLE);
}

void AdcsManager::allocateAndCommand() {
    AdcsLogic::allocate(this->m_dipole, this->m_mag, this->m_coilAvailable, this->m_throttle);
    this->commandCoils(this->m_throttle);
}

void AdcsManager::stopControl() {
    this->m_dipole = AdcsLogic::ZERO;
    for (U32 coil = 0; coil < AdcsLogic::COIL_COUNT; coil++) {
        this->m_throttle[coil] = 0.0F;
    }
    this->commandCoils(NO_THROTTLE);
}

void AdcsManager::commandCoils(const F32 throttle[AdcsLogic::COIL_COUNT]) {
    (void)throttle;
}

void AdcsManager::publish(bool sensed, const Os::RawTime& runStart) {
    this->tlmWrite_SatState(this->m_satState);
    this->tlmWrite_Controller(static_cast<project::AdcsController::T>(this->m_controller));
    this->tlmWrite_Mode(toTlm(this->m_mode));
    if (sensed) {
        this->tlmWrite_Gyro(toImuVector(this->m_gyro));
        this->tlmWrite_GyroStatus(toTlm(this->m_gyroStatus));
        this->tlmWrite_Mag(toImuVector(this->m_mag));
        this->tlmWrite_MagStatus(toTlm(this->m_magStatus));
    }
    this->tlmWrite_Dipole(project::AdcsDipole({this->m_dipole.x, this->m_dipole.y, this->m_dipole.z}));
    project::CoilThrottles throttles;
    for (U32 coil = 0; coil < AdcsLogic::COIL_COUNT; coil++) {
        throttles[coil] = this->m_throttle[coil];
    }
    this->tlmWrite_Throttles(throttles);
    this->tlmWrite_CycleTimeUs(elapsedUs(runStart));
}

void AdcsManager::sleepAtLeastUs(U32 microseconds) {
    const U32 milliseconds = (microseconds + 999U) / 1000U;
    (void)Os::Task::delay(Fw::TimeInterval(milliseconds / 1000U, (milliseconds % 1000U) * 1000U));
}

U32 AdcsManager::elapsedUs(const Os::RawTime& start) {
    Os::RawTime now;
    (void)now.now();
    U32 elapsed = 0;
    if (now.getDiffUsec(start, elapsed) != Os::RawTime::Status::OP_OK) {
        return std::numeric_limits<U32>::max();
    }
    return elapsed;
}

}  // namespace project

// ======================================================================
// \title  AdcsManagerTester.cpp
// \brief  Unit test harness for the AdcsManager component.
// ======================================================================

#include "AdcsManagerTester.hpp"

#include "Os/RawTime.hpp"

namespace project {

// This project builds as C++14, where a `static constexpr` member that is
// bound to a reference -- which is exactly what the gtest EXPECT_/ASSERT_
// macros do with their arguments -- is odr-used and therefore needs an
// out-of-line definition. Without these the tests fail to link.
constexpr FwSizeType AdcsManagerTester::MAX_HISTORY_SIZE;
constexpr U32 AdcsManagerTester::MAG_SCRIPT_LENGTH;

namespace {

// An angular rate inside the stable bound (0.54 rad/s) but above the
// detumbled one (0.087 rad/s), so a run starting in TUMBLING moves to STABLE
// and control stays on.
const project::ImuVector STABLE_RATE(0.1F, -0.2F, 0.2F);  // 0.3 rad/s

// Faster than the B-dot very-fast-tumbling bound (2.62 rad/s).
const project::ImuVector VERY_FAST_RATE(2.0F, 2.0F, 2.0F);  // 3.46 rad/s

const F32 NO_THROTTLE[AdcsLogic::COIL_COUNT] = {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};

}  // namespace

AdcsManagerTester::AdcsManagerTester()
    : AdcsManagerGTestBase("AdcsManagerTester", MAX_HISTORY_SIZE),
      component("AdcsManager"),
      m_gyroValue(STABLE_RATE),
      m_range(project::GyroRange::DPS_500),
      m_lastRangeSet(project::GyroRange::DPS_500) {
    this->initComponents();
    this->connectPorts();
    // A field near 45 uT that drifts a little from read to read, so the
    // B-dot command is non-zero and depends on every sample.
    for (U32 k = 0; k < MAG_SCRIPT_LENGTH; k++) {
        const F32 d = static_cast<F32>(k);
        this->m_magScript[k] = project::ImuVector(20.0F + 0.5F * d, -30.0F - 0.3F * d, 25.0F + 0.2F * d);
    }
}

AdcsManagerTester::~AdcsManagerTester() {
    // Free the message queue initComponents() allocated. macOS does not
    // detect this leak; the Linux CI runner does.
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Test cases
// ----------------------------------------------------------------------

void AdcsManagerTester::testStartupDoesNothing() {
    this->setState(project::SatelliteState::STARTUP);

    this->runOnce();

    EXPECT_EQ(this->m_bootCalls, 1U);
    EXPECT_EQ(this->m_magReads, 0U);
    EXPECT_EQ(this->m_gyroReads, 0U);
    EXPECT_EQ(this->m_rangeGets, 0U);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void AdcsManagerTester::testBdotCycle() {
    const U32 elapsed = this->runOnce();

    // One snapshot read, then six samples; one gyroscope read.
    EXPECT_EQ(this->m_bootCalls, 1U);
    EXPECT_EQ(this->m_magReads, 7U);
    EXPECT_EQ(this->m_gyroReads, 1U);

    // DETUMBLING wants 2000 deg/s; the part held 500, so it was changed.
    EXPECT_EQ(this->m_rangeGets, 1U);
    ASSERT_EQ(this->m_rangeSets, 1U);
    EXPECT_EQ(this->m_lastRangeSet, project::GyroRange::DPS_2000);

    // Five 80 ms gaps and 500 ms with the coils on.
    EXPECT_GE(elapsed, 5U * AdcsManager::MAG_SAMPLE_INTERVAL_US + AdcsManager::BDOT_COILS_ON_US);

    ASSERT_EVENTS_ModeChanged_SIZE(1);
    ASSERT_EVENTS_ModeChanged(0, project::AdcsMode::TUMBLING, project::AdcsMode::STABLE);

    // The command is the B-dot law over the six samples (reads 1-6).
    AdcsLogic::Vec3 samples[AdcsLogic::BDOT_SAMPLES];
    for (U32 k = 0; k < AdcsLogic::BDOT_SAMPLES; k++) {
        samples[k] = this->scriptedFieldT(k + 1);
    }
    const AdcsLogic::Vec3 u = AdcsLogic::bdot(samples, 0.08F);
    ASSERT_TLM_Dipole_SIZE(1);
    ASSERT_TLM_Dipole(0, project::AdcsDipole({u.x, u.y, u.z}));
    EXPECT_GT(AdcsLogic::norm(u), 0.0F);

    // No coil is available yet, so every allocated throttle is 0.
    ASSERT_TLM_Throttles_SIZE(1);
    ASSERT_TLM_Throttles(0, project::CoilThrottles(NO_THROTTLE));

    // The run's field is the last sample.
    const AdcsLogic::Vec3 last = this->scriptedFieldT(6);
    ASSERT_TLM_Mag(0, project::ImuVector(last.x, last.y, last.z));
    ASSERT_TLM_MagStatus(0, project::AdcsSensorStatus::OK);
    ASSERT_TLM_Gyro(0, STABLE_RATE);
    ASSERT_TLM_GyroStatus(0, project::AdcsSensorStatus::OK);
    ASSERT_TLM_SatState(0, project::SatelliteState::DETUMBLING);
    ASSERT_TLM_Controller(0, project::AdcsController::BDOT);
    ASSERT_TLM_Mode(0, project::AdcsMode::STABLE);
    ASSERT_TLM_CycleTimeUs_SIZE(1);
    EXPECT_GE(this->tlmHistory_CycleTimeUs->at(0).arg, 5U * AdcsManager::MAG_SAMPLE_INTERVAL_US);
}

void AdcsManagerTester::testNominalKeepsMatchingRange() {
    this->setState(project::SatelliteState::NOMINAL);
    this->m_range = project::GyroRange::DPS_125;

    this->runOnce();

    EXPECT_EQ(this->m_rangeGets, 1U);
    EXPECT_EQ(this->m_rangeSets, 0U);
}

void AdcsManagerTester::testBcrossCycle() {
    this->setController(project::AdcsController::BCROSS);
    ASSERT_EVENTS_ControllerSet_SIZE(1);
    ASSERT_EVENTS_ControllerSet(0, project::AdcsController::BCROSS);
    this->clearHistory();

    const U32 elapsed = this->runOnce();

    // The cycle fills a full second, re-reading the gyroscope every 50 ms:
    // the snapshot read plus at most 20 more.
    EXPECT_GE(elapsed, AdcsManager::BCROSS_CYCLE_US);
    EXPECT_GE(this->m_gyroReads, 10U);
    EXPECT_LE(this->m_gyroReads, 21U);
    EXPECT_EQ(this->m_magReads, 1U);

    // The command uses the snapshot field and the latest rate.
    const AdcsLogic::Vec3 field = this->scriptedFieldT(0);
    const AdcsLogic::Vec3 u = AdcsLogic::bcross(field, {STABLE_RATE.get_x(), STABLE_RATE.get_y(), STABLE_RATE.get_z()});
    ASSERT_TLM_Dipole_SIZE(1);
    ASSERT_TLM_Dipole(0, project::AdcsDipole({u.x, u.y, u.z}));
    ASSERT_TLM_Controller(0, project::AdcsController::BCROSS);
}

void AdcsManagerTester::testLowPowerSkipsSensing() {
    this->setState(project::SatelliteState::LOW_POWER);

    const U32 elapsed = this->runOnce();

    EXPECT_EQ(this->m_magReads, 0U);
    EXPECT_EQ(this->m_gyroReads, 0U);
    EXPECT_EQ(this->m_rangeGets, 0U);
    EXPECT_LT(elapsed, AdcsManager::BDOT_COILS_ON_US);

    ASSERT_TLM_Dipole(0, project::AdcsDipole({0.0F, 0.0F, 0.0F}));
    ASSERT_TLM_Throttles(0, project::CoilThrottles(NO_THROTTLE));
    ASSERT_TLM_SatState(0, project::SatelliteState::LOW_POWER);
    ASSERT_TLM_Gyro_SIZE(0);
    ASSERT_TLM_Mag_SIZE(0);
}

void AdcsManagerTester::testVeryFastTumblingStopsControl() {
    this->m_gyroValue = VERY_FAST_RATE;

    const U32 elapsed = this->runOnce();

    // The snapshot only: no control cycle.
    EXPECT_EQ(this->m_magReads, 1U);
    EXPECT_LT(elapsed, AdcsManager::BDOT_COILS_ON_US);
    ASSERT_EVENTS_ModeChanged(0, project::AdcsMode::TUMBLING, project::AdcsMode::VF_TUMBLING);
    ASSERT_TLM_Dipole(0, project::AdcsDipole({0.0F, 0.0F, 0.0F}));
}

void AdcsManagerTester::testFailedMagnetometerStopsBdot() {
    this->m_magFails = true;

    const U32 elapsed = this->runOnce();

    // Every sample was still attempted, and the cycle kept its timing.
    EXPECT_EQ(this->m_magReads, 7U);
    EXPECT_GE(elapsed, 5U * AdcsManager::MAG_SAMPLE_INTERVAL_US + AdcsManager::BDOT_COILS_ON_US);
    ASSERT_TLM_MagStatus(0, project::AdcsSensorStatus::MAG_FAIL);
    ASSERT_TLM_Dipole(0, project::AdcsDipole({0.0F, 0.0F, 0.0F}));
}

void AdcsManagerTester::testFailedGyroscopeForcesStable() {
    this->m_gyroFails = true;

    this->runOnce();

    ASSERT_TLM_GyroStatus(0, project::AdcsSensorStatus::GYRO_FAIL);
    ASSERT_TLM_Mode(0, project::AdcsMode::STABLE);
    // B-dot uses only the magnetometer, so it still ran.
    EXPECT_EQ(this->m_magReads, 7U);
}

void AdcsManagerTester::testImuBootRetried() {
    // LOW_POWER keeps each run short; the boot happens before the state is
    // looked at.
    this->setState(project::SatelliteState::LOW_POWER);
    this->m_bootResult = Fw::Success::FAILURE;

    this->runOnce();
    this->runOnce();
    EXPECT_EQ(this->m_bootCalls, 2U);

    this->m_bootResult = Fw::Success::SUCCESS;
    this->runOnce();
    this->runOnce();
    EXPECT_EQ(this->m_bootCalls, 3U);
}

// ----------------------------------------------------------------------
// Fake IMU
// ----------------------------------------------------------------------

Fw::Success AdcsManagerTester::from_imuBoot_handler(FwIndexType portNum) {
    this->m_bootCalls++;
    return this->m_bootResult;
}

Fw::Success AdcsManagerTester::from_imuReadGyro_handler(FwIndexType portNum, project::ImuVector& sample) {
    this->m_gyroReads++;
    if (this->m_gyroFails) {
        return Fw::Success::FAILURE;
    }
    sample = this->m_gyroValue;
    return Fw::Success::SUCCESS;
}

Fw::Success AdcsManagerTester::from_imuReadMag_handler(FwIndexType portNum, project::ImuVector& sample) {
    const U32 index = (this->m_magReads < MAG_SCRIPT_LENGTH) ? this->m_magReads : MAG_SCRIPT_LENGTH - 1;
    this->m_magReads++;
    if (this->m_magFails) {
        return Fw::Success::FAILURE;
    }
    sample = this->m_magScript[index];
    return Fw::Success::SUCCESS;
}

Fw::Success AdcsManagerTester::from_imuGetGyroRange_handler(FwIndexType portNum, project::GyroRange& range) {
    this->m_rangeGets++;
    range = this->m_range;
    return Fw::Success::SUCCESS;
}

Fw::Success AdcsManagerTester::from_imuSetGyroRange_handler(FwIndexType portNum, const project::GyroRange& range) {
    this->m_rangeSets++;
    this->m_lastRangeSet = range;
    this->m_range = range;
    return Fw::Success::SUCCESS;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

U32 AdcsManagerTester::runOnce() {
    Os::RawTime start;
    (void)start.now();
    this->invoke_to_run(0, 0);
    EXPECT_EQ(this->component.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    Os::RawTime end;
    (void)end.now();
    U32 elapsed = 0;
    (void)end.getDiffUsec(start, elapsed);
    return elapsed;
}

void AdcsManagerTester::setState(project::SatelliteState::T state) {
    this->sendCmd_SET_SATELLITE_STATE(0, 0, state);
    EXPECT_EQ(this->component.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, AdcsManager::OPCODE_SET_SATELLITE_STATE, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SatelliteStateSet(0, state);
    this->clearHistory();
}

void AdcsManagerTester::setController(project::AdcsController::T controller) {
    this->sendCmd_SET_CONTROLLER(0, 0, controller);
    EXPECT_EQ(this->component.doDispatch(), Fw::QueuedComponentBase::MSG_DISPATCH_OK);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, AdcsManager::OPCODE_SET_CONTROLLER, 0, Fw::CmdResponse::OK);
}

AdcsLogic::Vec3 AdcsManagerTester::scriptedFieldT(U32 index) const {
    const project::ImuVector& ut = this->m_magScript[index];
    return {ut.get_x() * 1.0e-6F, ut.get_y() * 1.0e-6F, ut.get_z() * 1.0e-6F};
}

}  // namespace project

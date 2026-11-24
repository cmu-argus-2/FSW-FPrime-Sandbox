// ======================================================================
// \title  AdcsManagerTestMain.cpp
// \brief  Unit tests for the AdcsManager component, run against a fake IMU.
//
// The control cycles keep their real timing, so a B-dot run takes about
// 0.9 s and a B-cross run about 1 s.
// ======================================================================

#include <gtest/gtest.h>

#include "AdcsManagerTester.hpp"
#include "Fw/Test/UnitTest.hpp"

TEST(AdcsManager, StartupDoesNothing) {
    COMMENT("STARTUP boots the IMU but neither senses nor controls.");
    project::AdcsManagerTester tester;
    tester.testStartupDoesNothing();
}

TEST(AdcsManager, RunsBdotCycle) {
    COMMENT("Snapshot, six magnetometer samples 80 ms apart, the B-dot command, 500 ms with the coils on.");
    project::AdcsManagerTester tester;
    tester.testBdotCycle();
}

TEST(AdcsManager, NominalKeepsMatchingRange) {
    COMMENT("NOMINAL wants 125 deg/s and leaves a range that already matches.");
    project::AdcsManagerTester tester;
    tester.testNominalKeepsMatchingRange();
}

TEST(AdcsManager, RunsBcrossCycle) {
    COMMENT("The gyroscope re-read and the B-cross command updated every 50 ms for 1 s.");
    project::AdcsManagerTester tester;
    tester.testBcrossCycle();
}

TEST(AdcsManager, LowPowerSkipsSensing) {
    COMMENT("LOW_POWER skips sensing and control.");
    project::AdcsManagerTester tester;
    tester.testLowPowerSkipsSensing();
}

TEST(AdcsManager, VeryFastTumblingStopsControl) {
    COMMENT("Very fast tumbling turns control off without a cycle.");
    project::AdcsManagerTester tester;
    tester.testVeryFastTumblingStopsControl();
}

TEST(AdcsManager, FailedMagnetometerStopsBdot) {
    COMMENT("Without six good magnetometer samples, B-dot commands nothing.");
    project::AdcsManagerTester tester;
    tester.testFailedMagnetometerStopsBdot();
}

TEST(AdcsManager, FailedGyroscopeForcesStable) {
    COMMENT("A failed gyroscope forces STABLE; B-dot still runs.");
    project::AdcsManagerTester tester;
    tester.testFailedGyroscopeForcesStable();
}

TEST(AdcsManager, RetriesImuBoot) {
    COMMENT("A failed IMU boot is retried on the next run, and only until it works.");
    project::AdcsManagerTester tester;
    tester.testImuBootRetried();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

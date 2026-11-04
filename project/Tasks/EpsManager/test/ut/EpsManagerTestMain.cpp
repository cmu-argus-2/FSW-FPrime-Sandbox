// ======================================================================
// \title  EpsManagerTestMain.cpp
// \brief  GoogleTest entry point for EpsManager unit tests
//
// EpsLogicTest.cpp links into the same executable.
// ======================================================================

#include "EpsManagerTester.hpp"

// Scheduling and health

TEST(EpsManager, FuelGaugeReadOnFirstRunThenEveryFifth) {
    project::EpsManagerTester tester;
    tester.testFuelGaugeReadOnFirstRunThenEveryFifth();
}

TEST(EpsManager, RunCountTelemetry) {
    project::EpsManagerTester tester;
    tester.testRunCountTelemetry();
}

TEST(EpsManager, PingIsAnswered) {
    project::EpsManagerTester tester;
    tester.testPingIsAnswered();
}

// Every run

TEST(EpsManager, AlertRaisedOnceAndCleared) {
    project::EpsManagerTester tester;
    tester.testAlertRaisedOnceAndCleared();
}

TEST(EpsManager, FailedMonitorReadsAsZero) {
    project::EpsManagerTester tester;
    tester.testFailedMonitorReadsAsZero();
}

TEST(EpsManager, MonitorValuesPublishedEveryFifthRun) {
    project::EpsManagerTester tester;
    tester.testMonitorValuesPublishedEveryFifthRun();
}

// Fuel gauge cycle

TEST(EpsManager, FuelGaugeValuesTruncated) {
    project::EpsManagerTester tester;
    tester.testFuelGaugeValuesTruncated();
}

TEST(EpsManager, PowerFlagHysteresis) {
    project::EpsManagerTester tester;
    tester.testPowerFlagHysteresis();
}

TEST(EpsManager, InvalidSocKeepsFlag) {
    project::EpsManagerTester tester;
    tester.testInvalidSocKeepsFlag();
}

TEST(EpsManager, ColdThermistorTurnsHeaterOn) {
    project::EpsManagerTester tester;
    tester.testColdThermistorTurnsHeaterOn();
}

TEST(EpsManager, WarmThermistorTurnsHeaterOff) {
    project::EpsManagerTester tester;
    tester.testWarmThermistorTurnsHeaterOff();
}

TEST(EpsManager, FailedReadAfterGoodSkipsDecisions) {
    project::EpsManagerTester tester;
    tester.testFailedReadAfterGoodSkipsDecisions();
}

TEST(EpsManager, UnavailableFuelGaugeUsesCpuFallback) {
    project::EpsManagerTester tester;
    tester.testUnavailableFuelGaugeUsesCpuFallback();
}

TEST(EpsManager, FallbackWithoutCpuTempDecidesNothing) {
    project::EpsManagerTester tester;
    tester.testFallbackWithoutCpuTempDecidesNothing();
}

TEST(EpsManager, AbsentHeaterNotRetried) {
    project::EpsManagerTester tester;
    tester.testAbsentHeaterNotRetried();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
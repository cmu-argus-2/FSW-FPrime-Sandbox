// ======================================================================
// \title  EpsManagerTestMain.cpp
// \brief  GoogleTest entry point for EpsManager unit tests
// ======================================================================

#include "EpsManagerTester.hpp"

TEST(EpsManager, FuelGaugeReadOnFirstRunThenEveryFifth) {
    project::EpsManagerTester tester;
    tester.testFuelGaugeReadOnFirstRunThenEveryFifth();
}

TEST(EpsManager, FuelGaugeEventCarriesSoc) {
    project::EpsManagerTester tester;
    tester.testFuelGaugeEventCarriesSoc();
}

TEST(EpsManager, RunCountTelemetry) {
    project::EpsManagerTester tester;
    tester.testRunCountTelemetry();
}

TEST(EpsManager, PingIsAnswered) {
    project::EpsManagerTester tester;
    tester.testPingIsAnswered();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
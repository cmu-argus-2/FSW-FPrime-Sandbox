// ======================================================================
// \title  DRV8235TestMain.cpp
// \brief  GoogleTest entry point for DRV8235 unit tests
// ======================================================================

#include "DRV8235Tester.hpp"

// The pure math tests live in DRV8235MathTest.cpp and link into this
// same executable.

TEST(DRV8235Component, ConfigureTouchesNoBus) {
    project::DRV8235Tester tester;
    tester.testConfigureTouchesNoBus();
}

TEST(DRV8235Component, ConfigureRejectsBadAddress) {
    project::DRV8235Tester tester;
    tester.testConfigureRejectsBadAddress();
}

TEST(DRV8235Component, FirstReadRunsInitSequence) {
    project::DRV8235Tester tester;
    tester.testFirstReadRunsInitSequence();
}

TEST(DRV8235Component, LaterReadsSkipInit) {
    project::DRV8235Tester tester;
    tester.testLaterReadsSkipInit();
}

TEST(DRV8235Component, ReverseBridgeNegatesSample) {
    project::DRV8235Tester tester;
    tester.testReverseBridgeNegatesSample();
}

TEST(DRV8235Component, ReadBeforeConfigureTouchesNoBus) {
    project::DRV8235Tester tester;
    tester.testReadBeforeConfigureTouchesNoBus();
}

TEST(DRV8235Component, InitFailureRetriesOnNextRead) {
    project::DRV8235Tester tester;
    tester.testInitFailureRetriesOnNextRead();
}

TEST(DRV8235Component, BusFailureAfterInitReinitializes) {
    project::DRV8235Tester tester;
    tester.testBusFailureAfterInitReinitializes();
}

TEST(DRV8235Component, PollPublishesTelemetryAndFault) {
    project::DRV8235Tester tester;
    tester.testPollPublishesTelemetryAndFault();
}

TEST(DRV8235Component, PollWithoutFaultEmitsNoFaultEvent) {
    project::DRV8235Tester tester;
    tester.testPollWithoutFaultEmitsNoFaultEvent();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

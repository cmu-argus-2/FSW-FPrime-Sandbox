// ======================================================================
// \title  ADM1176TestMain.cpp
// \brief  GoogleTest entry point for ADM1176 unit tests
// ======================================================================

#include "ADM1176Tester.hpp"

// The pure math tests live in ADM1176MathTest.cpp and link into this
// same executable.

TEST(ADM1176Component, ConfigureSendsContinuousHighRangeCommand) {
    project::ADM1176Tester tester;
    tester.testConfigureSendsContinuousHighRangeCommand();
}

TEST(ADM1176Component, ReadReturnsDecodedSample) {
    project::ADM1176Tester tester;
    tester.testReadReturnsDecodedSample();
}

TEST(ADM1176Component, ReadBeforeConfigureTouchesNoBus) {
    project::ADM1176Tester tester;
    tester.testReadBeforeConfigureTouchesNoBus();
}

TEST(ADM1176Component, ReadBusFailureLeavesSampleUnchanged) {
    project::ADM1176Tester tester;
    tester.testReadBusFailureLeavesSampleUnchanged();
}

TEST(ADM1176Component, PollPublishesTelemetry) {
    project::ADM1176Tester tester;
    tester.testPollPublishesTelemetry();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
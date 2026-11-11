// ======================================================================
// \title  I2cScannerTestMain.cpp
// \brief  Unit tests for the I2cScanner component, run against fake buses.
// ======================================================================

#include <gtest/gtest.h>

#include "Fw/Test/UnitTest.hpp"
#include "I2cScannerTester.hpp"

TEST(I2cScanner, ReportsPresentDevices) {
    COMMENT("Each bus reports exactly the addresses that acknowledged.");
    project::I2cScannerTester tester;
    tester.testReportsPresentDevices();
}

TEST(I2cScanner, ProbesEveryAddressOnce) {
    COMMENT("Every non-reserved address is probed once, in order, with a 1-byte read.");
    project::I2cScannerTester tester;
    tester.testProbesEveryAddressOnce();
}

TEST(I2cScanner, ReportsEmptyBus) {
    COMMENT("A bus with nothing on it reports none.");
    project::I2cScannerTester tester;
    tester.testReportsEmptyBus();
}

TEST(I2cScanner, ReportsStuckBus) {
    COMMENT("Every address acknowledging keeps an exact count and truncates the list.");
    project::I2cScannerTester tester;
    tester.testReportsStuckBus();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

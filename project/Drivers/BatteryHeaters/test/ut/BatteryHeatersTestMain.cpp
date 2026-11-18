// ======================================================================
// \title  BatteryHeatersTestMain.cpp
// \brief  GoogleTest entry point for BatteryHeaters unit tests
// ======================================================================

#include "BatteryHeatersTester.hpp"

// ---------------------------------------------------------------------
// Dual-heater board
// ---------------------------------------------------------------------

TEST(DualHeater, EnableHeater0DrivesMasterAndHeater0) {
    project::BatteryHeatersTester tester(true);
    tester.testEnableHeater0DrivesMasterAndHeater0();
}

TEST(DualHeater, EnableHeater0TwiceWritesMasterOnce) {
    project::BatteryHeatersTester tester(true);
    tester.testEnableHeater0TwiceWritesMasterOnce();
}

TEST(DualHeater, DisableHeater0DropsMasterWhenHeater1Off) {
    project::BatteryHeatersTester tester(true);
    tester.testDisableHeater0DropsMasterWhenHeater1Off();
}

TEST(DualHeater, EnableHeater1DrivesMasterAndHeater1) {
    project::BatteryHeatersTester tester(true);
    tester.testEnableHeater1DrivesMasterAndHeater1();
}

TEST(DualHeater, MasterStaysHighUntilBothHeatersOff) {
    project::BatteryHeatersTester tester(true);
    tester.testMasterStaysHighUntilBothHeatersOff();
}

TEST(DualHeater, DisableHeater1DropsMasterWhenHeater0Off) {
    project::BatteryHeatersTester tester(true);
    tester.testDisableHeater1DropsMasterWhenHeater0Off();
}

// ---------------------------------------------------------------------
// Dual-heater board, EPS task ports
// ---------------------------------------------------------------------

TEST(DualHeater, SetHeater0OnDrivesMasterAndHeater0) {
    project::BatteryHeatersTester tester(true);
    tester.testSetHeater0OnDrivesMasterAndHeater0();
}

TEST(DualHeater, SetHeater0OffKeepsMasterWhileHeater1On) {
    project::BatteryHeatersTester tester(true);
    tester.testSetHeater0OffKeepsMasterWhileHeater1On();
}

// ---------------------------------------------------------------------
// Single-heater board (heater 1 ports left unconnected)
// ---------------------------------------------------------------------

TEST(SingleHeater, EnableHeater1Rejected) {
    project::BatteryHeatersTester tester(false);
    tester.testEnableHeater1RejectedWhenNotPresent();
}

TEST(SingleHeater, DisableHeater1Rejected) {
    project::BatteryHeatersTester tester(false);
    tester.testDisableHeater1RejectedWhenNotPresent();
}

TEST(SingleHeater, DisableHeater0DropsMaster) {
    project::BatteryHeatersTester tester(false);
    tester.testDisableHeater0DropsMasterWhenHeater1NotPresent();
}

TEST(SingleHeater, SetHeater1Rejected) {
    project::BatteryHeatersTester tester(false);
    tester.testSetHeater1RejectedWhenNotPresent();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
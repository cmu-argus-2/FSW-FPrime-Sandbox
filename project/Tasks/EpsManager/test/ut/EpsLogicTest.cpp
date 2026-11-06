// ======================================================================
// \title  EpsLogicTest.cpp
// \brief  Unit tests for the EPS decision rules (EpsLogic.hpp)
//
// Links into the EpsManager UT executable; main() is in EpsManagerTestMain.cpp.
// ======================================================================

#include <gtest/gtest.h>

#include "project/Tasks/EpsManager/EpsLogic.hpp"

using namespace project::EpsLogic;

// ----------------------------------------------------------------------
// Power and alerts
// ----------------------------------------------------------------------

TEST(EpsLogicPower, MilliwattsTruncateTowardZero) {
    EXPECT_EQ(powerMilliwatts(5000, 199), 995);
    EXPECT_EQ(powerMilliwatts(-5000, -199), 995);
    EXPECT_EQ(powerMilliwatts(5000, -199), -995);
    EXPECT_EQ(powerMilliwatts(0, 10581), 0);
}

TEST(EpsLogicPower, AlertAtExactThreshold) {
    // 5000 mV * 200 mA = 1000 mW: at the mainboard threshold, so alert
    EXPECT_TRUE(powerAlert(5000, 200, PowerThresholdMw::MAINBOARD));
    // 5000 mV * 199 mA = 995 mW: just under
    EXPECT_FALSE(powerAlert(5000, 199, PowerThresholdMw::MAINBOARD));
}

TEST(EpsLogicPower, AlertForEachThreshold) {
    EXPECT_TRUE(powerAlert(5000, 200, PowerThresholdMw::PERIPHERAL));
    EXPECT_FALSE(powerAlert(5000, 199, PowerThresholdMw::PERIPHERAL));

    EXPECT_TRUE(powerAlert(3300, 1000, PowerThresholdMw::RADIO));
    EXPECT_FALSE(powerAlert(3300, 999, PowerThresholdMw::RADIO));

    EXPECT_TRUE(powerAlert(16000, 1000, PowerThresholdMw::JETSON));
    EXPECT_FALSE(powerAlert(16000, 999, PowerThresholdMw::JETSON));

    EXPECT_TRUE(powerAlert(1500, 1000, PowerThresholdMw::TORQUE_COIL));
    EXPECT_FALSE(powerAlert(1500, 999, PowerThresholdMw::TORQUE_COIL));
}

TEST(EpsLogicPower, AlertWithCoilInReverse) {
    // DRV8235 negates both voltage and current in reverse, so power stays positive
    EXPECT_TRUE(powerAlert(-6000, -300, PowerThresholdMw::TORQUE_COIL));
    EXPECT_FALSE(powerAlert(-1000, -100, PowerThresholdMw::TORQUE_COIL));
}

TEST(EpsLogicPower, FullScaleDoesNotOverflow) {
    // ADM1176 full scale at 10 mOhm: 26343 mV * 10581 mA, about 278 W
    EXPECT_EQ(powerMilliwatts(26343, 10581), 278735);
    EXPECT_TRUE(powerAlert(26343, 10581, PowerThresholdMw::JETSON));
}

// ----------------------------------------------------------------------
// EPS power flag
// ----------------------------------------------------------------------

TEST(EpsLogicPowerFlag, LowPowerAtOrBelow30) {
    EXPECT_EQ(nextPowerFlag(PowerFlag::NONE, 30), PowerFlag::LOW_POWER);
    EXPECT_EQ(nextPowerFlag(PowerFlag::NONE, 0), PowerFlag::LOW_POWER);
    EXPECT_EQ(nextPowerFlag(PowerFlag::NOMINAL, 30), PowerFlag::LOW_POWER);
}

TEST(EpsLogicPowerFlag, NominalAtOrAbove40) {
    EXPECT_EQ(nextPowerFlag(PowerFlag::NONE, 40), PowerFlag::NOMINAL);
    EXPECT_EQ(nextPowerFlag(PowerFlag::LOW_POWER, 40), PowerFlag::NOMINAL);
    EXPECT_EQ(nextPowerFlag(PowerFlag::LOW_POWER, 100), PowerFlag::NOMINAL);
}

TEST(EpsLogicPowerFlag, BandKeepsLowPower) {
    EXPECT_EQ(nextPowerFlag(PowerFlag::LOW_POWER, 31), PowerFlag::LOW_POWER);
    EXPECT_EQ(nextPowerFlag(PowerFlag::LOW_POWER, 39), PowerFlag::LOW_POWER);
}

TEST(EpsLogicPowerFlag, BandOtherwiseNominal) {
    // Spec: "stays LOW_POWER if it was already LOW_POWER, else NOMINAL"
    EXPECT_EQ(nextPowerFlag(PowerFlag::NOMINAL, 31), PowerFlag::NOMINAL);
    EXPECT_EQ(nextPowerFlag(PowerFlag::NOMINAL, 39), PowerFlag::NOMINAL);
    EXPECT_EQ(nextPowerFlag(PowerFlag::NONE, 35), PowerFlag::NOMINAL);
}

TEST(EpsLogicPowerFlag, InvalidSocReturnsNone) {
    EXPECT_EQ(nextPowerFlag(PowerFlag::NOMINAL, -1), PowerFlag::NONE);
    EXPECT_EQ(nextPowerFlag(PowerFlag::LOW_POWER, 101), PowerFlag::NONE);
}

// ----------------------------------------------------------------------
// Battery heaters
// ----------------------------------------------------------------------

TEST(EpsLogicHeaters, EnableWhenOffAtOrBelowZero) {
    EXPECT_TRUE(shouldEnableHeater(false, 0));
    EXPECT_TRUE(shouldEnableHeater(false, -1));
    EXPECT_FALSE(shouldEnableHeater(false, 1));
    EXPECT_FALSE(shouldEnableHeater(true, -100));
}

TEST(EpsLogicHeaters, DisableWhenOnAtOrAbove500) {
    EXPECT_TRUE(shouldDisableHeater(true, 500));
    EXPECT_TRUE(shouldDisableHeater(true, 501));
    EXPECT_FALSE(shouldDisableHeater(true, 499));
    EXPECT_FALSE(shouldDisableHeater(false, 600));
}

TEST(EpsLogicHeaters, ActionCoversDeadband) {
    EXPECT_EQ(heaterAction(false, 0), HeaterAction::TURN_ON);
    EXPECT_EQ(heaterAction(true, 500), HeaterAction::TURN_OFF);
    // Between 1 and 499 cC nothing changes, whichever state the heater is in
    EXPECT_EQ(heaterAction(false, 250), HeaterAction::NONE);
    EXPECT_EQ(heaterAction(true, 250), HeaterAction::NONE);
    // Already in the right state
    EXPECT_EQ(heaterAction(true, -100), HeaterAction::NONE);
    EXPECT_EQ(heaterAction(false, 600), HeaterAction::NONE);
}

TEST(EpsLogicHeaters, ThermistorsDriveEachHeater) {
    const HeaterTemperatures temps = heaterTemperatures(true, -50, 700, 2500);
    EXPECT_EQ(temps.heater0Cc, -50);
    EXPECT_EQ(temps.heater1Cc, 700);
}

TEST(EpsLogicHeaters, CpuFallbackSubtractsOffset) {
    const HeaterTemperatures temps = heaterTemperatures(false, -50, 700, 2500);
    EXPECT_EQ(temps.heater0Cc, 2300);
    EXPECT_EQ(temps.heater1Cc, 2300);
}

// ----------------------------------------------------------------------
// Fuel gauge availability
// ----------------------------------------------------------------------

TEST(EpsLogicFuelGauge, NeverReadFallsBackImmediately) {
    // Like Python with FUEL_GAUGE_AVAILABLE false from boot
    FuelGaugeHealth health;
    EXPECT_EQ(updateFuelGauge(health, false), FuelGaugeMode::USE_CPU_FALLBACK);
}

TEST(EpsLogicFuelGauge, FailuresAfterGoodReadSkipThenFallBack) {
    FuelGaugeHealth health;
    EXPECT_EQ(updateFuelGauge(health, true), FuelGaugeMode::USE_FUEL_GAUGE);

    // A recently good gauge that fails is skipped, as Python's exception does
    EXPECT_EQ(updateFuelGauge(health, false), FuelGaugeMode::SKIP_CYCLE);
    EXPECT_EQ(updateFuelGauge(health, false), FuelGaugeMode::SKIP_CYCLE);

    // Third failure in a row: treat it as unavailable
    EXPECT_EQ(updateFuelGauge(health, false), FuelGaugeMode::USE_CPU_FALLBACK);
    EXPECT_EQ(updateFuelGauge(health, false), FuelGaugeMode::USE_CPU_FALLBACK);
}

TEST(EpsLogicFuelGauge, GoodReadRecovers) {
    FuelGaugeHealth health;
    updateFuelGauge(health, true);
    updateFuelGauge(health, false);
    updateFuelGauge(health, false);
    updateFuelGauge(health, false);

    EXPECT_EQ(updateFuelGauge(health, true), FuelGaugeMode::USE_FUEL_GAUGE);
    EXPECT_EQ(health.consecutiveFailures, 0U);
    EXPECT_EQ(updateFuelGauge(health, false), FuelGaugeMode::SKIP_CYCLE);
}
#include <gtest/gtest.h>

#include "project/Drivers/DRV8235/DRV8235Math.hpp"

using namespace project::DRV8235Math;

TEST(DRV8235Math, VoltageScalesAndTruncates) {
    // 167.33 mV per count. These match int(code * 0.16733 * 1000) in Python
    // for every code 0..255.
    EXPECT_EQ(voltageMillivolts(0, false), 0);
    EXPECT_EQ(voltageMillivolts(1, false), 167);
    EXPECT_EQ(voltageMillivolts(24, false), 4015);
    EXPECT_EQ(voltageMillivolts(255, false), 42669);
}

TEST(DRV8235Math, CurrentScalesAndTruncates) {
    // 14.51 mA per count; matches int(code * 0.01451 * 1000) for 0..255.
    EXPECT_EQ(currentMilliamps(0, false), 0);
    EXPECT_EQ(currentMilliamps(1, false), 14);
    EXPECT_EQ(currentMilliamps(64, false), 928);
    EXPECT_EQ(currentMilliamps(255, false), 3700);
}

TEST(DRV8235Math, ReverseNegatesBoth) {
    EXPECT_EQ(voltageMillivolts(24, true), -4015);
    EXPECT_EQ(currentMilliamps(64, true), -928);
    EXPECT_EQ(voltageMillivolts(0, true), 0);
}

TEST(DRV8235Math, OnlyBridgeReverseIsReverse) {
    EXPECT_TRUE(isReverse(BRIDGE_REVERSE));
    EXPECT_TRUE(isReverse(0xFD));  // Other CONFIG4 bits do not matter
    EXPECT_FALSE(isReverse(BRIDGE_COAST));
    EXPECT_FALSE(isReverse(BRIDGE_FORWARD));
    EXPECT_FALSE(isReverse(BRIDGE_BRAKE));
}

TEST(DRV8235Math, ApplyFieldKeepsOtherBits) {
    EXPECT_EQ(applyField(0xF3, {CONFIG4, 0x03, 0x00}), 0xF0);
    EXPECT_EQ(applyField(0x3F, {RC_CTRL2, 0xC0, 0x80}), 0xBF);
    EXPECT_EQ(applyField(0x5A, {RC_CTRL3, 0xFF, 41}), 41);
    // Value bits outside the mask are ignored
    EXPECT_EQ(applyField(0x00, {CONFIG0, 0x80, 0xFF}), 0x80);
}

TEST(DRV8235Math, InitSequenceMatchesPython) {
    ASSERT_EQ(INIT_STEPS, 10U);
    // Bridge setup comes first and output enable comes last but one, so the
    // outputs only turn on once the chip is coasting at 0 V.
    EXPECT_EQ(INIT_SEQUENCE[0].reg, CONFIG4);
    EXPECT_EQ(INIT_SEQUENCE[4].reg, REG_CTRL1);
    EXPECT_EQ(INIT_SEQUENCE[4].value, 0);
    EXPECT_EQ(INIT_SEQUENCE[8].reg, CONFIG0);
    EXPECT_EQ(INIT_SEQUENCE[8].mask, 0x80);
    EXPECT_EQ(INIT_SEQUENCE[9].mask, 0x02);
}

TEST(DRV8235Math, ThrottleScalesToSixVoltCoilLimit) {
    // Full throttle is 6.0 V of the chip's 42.67 V: trunc(0.1406 * 255) = 35
    const Drive full = driveFromThrottle(1000);
    EXPECT_EQ(full.vset, 35);
    EXPECT_EQ(full.bridge, BRIDGE_FORWARD);

    const Drive half = driveFromThrottle(500);
    EXPECT_EQ(half.vset, 17);
    EXPECT_EQ(half.bridge, BRIDGE_FORWARD);
}

TEST(DRV8235Math, NegativeThrottleDrivesReverseWithMagnitude) {
    // Python set_throttle takes abs() here; set_throttle_raw forgot to, but
    // nothing in flight calls set_throttle_raw.
    const Drive drive = driveFromThrottle(-1000);
    EXPECT_EQ(drive.vset, 35);
    EXPECT_EQ(drive.bridge, BRIDGE_REVERSE);
}

TEST(DRV8235Math, SmallOrZeroThrottleBrakes) {
    EXPECT_EQ(driveFromThrottle(0).bridge, BRIDGE_BRAKE);
    // 0.027 * 35.86 = 0.97, which truncates to 0, so Python brakes too
    EXPECT_EQ(driveFromThrottle(27).bridge, BRIDGE_BRAKE);
    EXPECT_EQ(driveFromThrottle(27).vset, 0);
}

TEST(DRV8235Math, ThrottleClamps) {
    EXPECT_EQ(driveFromThrottle(5000).vset, 35);
    EXPECT_EQ(driveFromThrottle(-5000).vset, 35);
    EXPECT_EQ(driveFromThrottle(-5000).bridge, BRIDGE_REVERSE);
}

#include <gtest/gtest.h>

#include "project/Components/ADM1176/ADM1176Math.hpp"

using namespace project::ADM1176Math;

TEST(ADM1176Math, DecodePacksHighBytesAndNibbles) {
    // Each high byte carries ADC bits 11:4; the third byte holds both low nibbles.
    const uint8_t packed[3] = {0xAB, 0xCD, 0xEF};
    const Sample sample = decode(packed);
    EXPECT_EQ(sample.voltageCode, 0xABE);
    EXPECT_EQ(sample.currentCode, 0xCDF);
}

TEST(ADM1176Math, VoltageScalesForBothRanges) {
    EXPECT_EQ(voltageMillivolts(0, true), 0U);
    EXPECT_EQ(voltageMillivolts(4095, true), 6648U);
    EXPECT_EQ(voltageMillivolts(4095, false), 26343U);
}

TEST(ADM1176Math, CurrentScalesBySenseResistor) {
    EXPECT_EQ(currentMilliamps(0, 10), 0U);
    EXPECT_EQ(currentMilliamps(4095, 10), 10581U);
    EXPECT_EQ(currentMilliamps(4095, 20), 5290U);
    EXPECT_EQ(currentMilliamps(4095, 0), 0U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
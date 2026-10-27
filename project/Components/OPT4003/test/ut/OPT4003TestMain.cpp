// ======================================================================
// \title  OPT4003TestMain.cpp
// \brief  Unit tests for the OPT4003 light sensor component.
//
// Two layers of test here:
//
//   * The decode functions, checked directly. They are pure, so a wrong
//     bit position is caught without any I2C mocking, and the values below
//     double as worked examples of the datasheet's register layout.
//
//   * The component behavior, exercised through OPT4003Tester with a fake
//     I2C bus: boot order, the conversion-ready wait, which registers are
//     read, what telemetry comes out, and how bus failures are handled.
// ======================================================================

#include <gtest/gtest.h>

#include "Fw/Test/UnitTest.hpp"
#include "OPT4003Tester.hpp"

// ----------------------------------------------------------------------
// Decoding
// ----------------------------------------------------------------------

TEST(OPT4003Decode, IdentifiesDevice) {
    COMMENT("DEVICE_ID must read DIDL = 0 and DIDH = 0x221.");
    EXPECT_TRUE(project::OPT4003::isExpectedDeviceId(0x0221));
    // Bits 15:14 are reserved and must not affect the check.
    EXPECT_TRUE(project::OPT4003::isExpectedDeviceId(0xC221));
    // DIDL nonzero.
    EXPECT_FALSE(project::OPT4003::isExpectedDeviceId(0x1221));
    // The OPT4001's DIDH.
    EXPECT_FALSE(project::OPT4003::isExpectedDeviceId(0x0121));
    // A bus that floats low.
    EXPECT_FALSE(project::OPT4003::isExpectedDeviceId(0x0000));
}

TEST(OPT4003Decode, ComputesLux) {
    COMMENT("lux = (((RESULT_MSB << 8) + RESULT_LSB) << EXPONENT) * 535e-6");
    EXPECT_FLOAT_EQ(project::OPT4003::decodeLux(0x0000, 0x0000), 0.0F);
    // Mantissa 1, exponent 0: one count.
    EXPECT_FLOAT_EQ(project::OPT4003::decodeLux(0x0000, 0x0100), 535e-6F);
    // Mantissa 1, exponent 1: two counts.
    EXPECT_FLOAT_EQ(project::OPT4003::decodeLux(0x1000, 0x0100), 1070e-6F);
    // Exponent 3, mantissa 0x12345: 596520 counts.
    EXPECT_FLOAT_EQ(project::OPT4003::decodeLux(0x3123, 0x45A7), 319.1382F);
}

TEST(OPT4003Decode, IgnoresCounterAndCrc) {
    COMMENT("The low byte of RESULT_LSB_CH0 is COUNTER and CRC, not data.");
    EXPECT_EQ(project::OPT4003::decodeLux(0x3123, 0x4500), project::OPT4003::decodeLux(0x3123, 0x45FF));
}

TEST(OPT4003Decode, CoversFullScale) {
    COMMENT("Exponent 8 with a full mantissa is the top of auto-range, about 143.6 klux.");
    EXPECT_NEAR(project::OPT4003::decodeLux(0x8FFF, 0xFF00), 143612.83F, 0.05F);
}

TEST(OPT4003Decode, DoesNotWrapOnOutOfRangeExponent) {
    COMMENT("A corrupted exponent of 15 must not wrap into a small value.");
    // (0xFFFFF << 15) * 535e-6 = 18382442.5; a 32-bit shift would wrap to
    // about 2.3 million.
    EXPECT_NEAR(project::OPT4003::decodeLux(0xFFFF, 0xFF00), 18382442.5F, 4.0F);
}

TEST(OPT4003Decode, SplitsFlags) {
    COMMENT("FLAGS: bit 3 overload, bit 2 conversion ready, bit 1 high, bit 0 low.");
    const project::LightSensorFlags none = project::OPT4003::decodeFlags(0x0004);
    EXPECT_FALSE(none.get_overload());
    EXPECT_FALSE(none.get_thresholdHigh());
    EXPECT_FALSE(none.get_thresholdLow());

    EXPECT_TRUE(project::OPT4003::decodeFlags(0x0008).get_overload());
    EXPECT_TRUE(project::OPT4003::decodeFlags(0x0002).get_thresholdHigh());
    EXPECT_TRUE(project::OPT4003::decodeFlags(0x0001).get_thresholdLow());
}

TEST(OPT4003Decode, ConfigurationWord) {
    COMMENT("Auto-range, 600 us, continuous, latched, active-low INT, one fault.");
    EXPECT_EQ(project::OPT4003::CONFIGURATION, 0x3038);
}

// ----------------------------------------------------------------------
// Component behavior
// ----------------------------------------------------------------------

TEST(OPT4003, Boots) {
    COMMENT("Boot reads DEVICE_ID, then writes the configuration word.");
    project::OPT4003Tester tester;
    tester.testBoot();
}

TEST(OPT4003, RejectsWrongDeviceId) {
    COMMENT("A part with the wrong ID is never configured and stays absent.");
    project::OPT4003Tester tester;
    tester.testBootWrongDeviceId();
}

TEST(OPT4003, ReportsBootBusFailure) {
    COMMENT("A bus failure during boot leaves the sensor absent.");
    project::OPT4003Tester tester;
    tester.testBootBusFailure();
}

TEST(OPT4003, StaysOffBusBeforeBoot) {
    COMMENT("A sensor that has not booted never touches the bus.");
    project::OPT4003Tester tester;
    tester.testReadBeforeBoot();
}

TEST(OPT4003, ReadsLux) {
    COMMENT("A read polls FLAGS once, then reads both result registers.");
    project::OPT4003Tester tester;
    tester.testRead();
}

TEST(OPT4003, WaitsForConversion) {
    COMMENT("A read keeps polling until the conversion is ready.");
    project::OPT4003Tester tester;
    tester.testReadWaitsForConversion();
}

TEST(OPT4003, ReadsAfterConversionTimeout) {
    COMMENT("A read that never sees conversion-ready still returns the result.");
    project::OPT4003Tester tester;
    tester.testReadTimesOut();
}

TEST(OPT4003, ReportsResultReadFailure) {
    COMMENT("A failed result read is reported and publishes nothing.");
    project::OPT4003Tester tester;
    tester.testReadResultFailure();
}

TEST(OPT4003, ReadsFlags) {
    COMMENT("readFlags decodes the threshold and overload bits.");
    project::OPT4003Tester tester;
    tester.testReadFlags();
}

TEST(OPT4003, HonorsConfiguredAddress) {
    COMMENT("configure() moves every transaction to the new address.");
    project::OPT4003Tester tester;
    tester.testConfigureAddress();
}

TEST(OPT4003, BootsAndSamplesOnRateGroup) {
    COMMENT("The rate-group port boots once, then samples on every tick.");
    project::OPT4003Tester tester;
    tester.testRunBootsThenSamples();
}

TEST(OPT4003, ThrottlesAndRearmsErrorEvent) {
    COMMENT("The I2C error event throttles, and a clean read re-arms it.");
    project::OPT4003Tester tester;
    tester.testErrorThrottle();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

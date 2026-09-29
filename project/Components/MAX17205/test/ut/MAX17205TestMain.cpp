// ======================================================================
// \title  MAX17205TestMain.cpp
// \brief  Unit tests for the MAX17205 fuel-gauge component.
//
// Two layers of test here:
//
//   * The decode* conversions, checked directly. They are pure functions, so
//     a wrong scaling constant is caught here without any I2C mocking, and
//     the expected values below double as documentation of the datasheet
//     scaling.
//
//   * The component behavior, exercised through MAX17205Tester with a fake
//     I2C bus: which registers get read on which address, what telemetry
//     comes out, and how bus failures are handled.
// ======================================================================

#include <gtest/gtest.h>

#include "Fw/Test/UnitTest.hpp"
#include "MAX17205Tester.hpp"
#include "STest/Random/Random.hpp"

// ----------------------------------------------------------------------
// Decoding
// ----------------------------------------------------------------------

TEST(MAX17205Decode, ReinterpretsRegistersAsTwosComplement) {
    COMMENT("Verify two's-complement reinterpretation of register values.");
    // Below 0x8000 the value passes through unchanged.
    EXPECT_EQ(project::MAX17205::toSigned(0x0000), static_cast<I16>(0));
    EXPECT_EQ(project::MAX17205::toSigned(0x1234), static_cast<I16>(0x1234));
    // 0x7FFF is the largest positive value; 0x8000 is the first negative one.
    // This boundary is where a wrong comparison would show up.
    EXPECT_EQ(project::MAX17205::toSigned(0x7FFF), static_cast<I16>(32767));
    EXPECT_EQ(project::MAX17205::toSigned(0x8000), static_cast<I16>(-32768));
    EXPECT_EQ(project::MAX17205::toSigned(0xFFFF), static_cast<I16>(-1));
}

TEST(MAX17205Decode, ScalesStateOfCharge) {
    COMMENT("RepSOC is 1/256 percent per count.");
    EXPECT_FLOAT_EQ(project::MAX17205::decodeStateOfCharge(0), 0.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeStateOfCharge(256), 1.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeStateOfCharge(19968), 78.0F);
    // A fully charged pack reads 100 %, well inside the register range.
    EXPECT_FLOAT_EQ(project::MAX17205::decodeStateOfCharge(25600), 100.0F);
}

TEST(MAX17205Decode, ScalesCapacity) {
    COMMENT("RepCap and FullCapRep are 0.5 mAh per count.");
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCapacity(0), 0.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCapacity(3000), 1500.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCapacity(4000), 2000.0F);
}

TEST(MAX17205Decode, ScalesSignedCurrent) {
    COMMENT("Current is 0.15625 mA per count with a 10 mOhm sense resistor.");
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCurrent(0), 0.0F);
    // Charging is positive, discharging negative. Getting this sign wrong
    // would make a draining battery look like it was charging.
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCurrent(640), 100.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCurrent(-640), -100.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCurrent(-320), -50.0F);
}

TEST(MAX17205Decode, ScalesVoltages) {
    COMMENT("VBat is 1.25 mV per count; VCell is 0.078125 mV per count.");
    EXPECT_FLOAT_EQ(project::MAX17205::decodeVoltage(6400), 8000.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeVoltage(0), 0.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeMidVoltage(51200), 4000.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeMidVoltage(0), 0.0F);
}

TEST(MAX17205Decode, ScalesTimes) {
    COMMENT("TTE and TTF are 5.625 seconds per count.");
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTime(0), 0.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTime(1600), 9000.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTime(800), 4500.0F);
}

TEST(MAX17205Decode, ScalesPackTemperature) {
    COMMENT("Temp is signed, 0.390625 centi-Celsius per count.");
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTemperature(0), 0.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTemperature(256), 100.0F);    // 1 C
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTemperature(5632), 2200.0F);  // 22 C

    // Sub-zero readings must come back negative. A pack in eclipse is exactly
    // the case a battery heater has to react to, so a decode that reported
    // -1 C as a large positive number would hide the condition that matters
    // most.
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTemperature(-256), -100.0F);    // -1 C
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTemperature(-5120), -2000.0F);  // -20 C

    // The full register path, not just the arithmetic: 0xFF00 off the wire is
    // -1 C, not 25500 centi-Celsius.
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTemperature(project::MAX17205::toSigned(0xFF00)), -100.0F);
}

TEST(MAX17205Decode, ScalesThermistorTemperature) {
    COMMENT("Shadow-RAM thermistors report tenths of a Kelvin.");
    // 273.1 K is 0 C.
    EXPECT_EQ(project::MAX17205::decodeThermistorTemperature(2731), 0);
    EXPECT_EQ(project::MAX17205::decodeThermistorTemperature(3000), 2690);
    EXPECT_EQ(project::MAX17205::decodeThermistorTemperature(3050), 3190);
    // Unlike the pack temperature, these do go negative correctly, which
    // matters for a spacecraft in eclipse.
    EXPECT_EQ(project::MAX17205::decodeThermistorTemperature(2631), -1000);  // -10 C
}

// ----------------------------------------------------------------------
// Component behavior
// ----------------------------------------------------------------------

TEST(MAX17205, ReadsAllMeasurements) {
    COMMENT("READ_ALL reads every register on the right address and publishes telemetry.");
    project::MAX17205Tester tester;
    tester.testReadAll();
}

TEST(MAX17205, SurvivesOneBadRegister) {
    COMMENT("A single failing register does not abort the rest of the snapshot.");
    project::MAX17205Tester tester;
    tester.testReadAllPartialFailure();
}

TEST(MAX17205, SamplesOnRateGroup) {
    COMMENT("The scheduled port samples without producing a command response.");
    project::MAX17205Tester tester;
    tester.testScheduledSample();
}

TEST(MAX17205, ResetsDevice) {
    COMMENT("RESET writes the expected CONFIG2 transaction.");
    project::MAX17205Tester tester;
    tester.testReset();
}

TEST(MAX17205, ReportsResetFailure) {
    COMMENT("A failed RESET is reported as an event and a failed command.");
    project::MAX17205Tester tester;
    tester.testResetFailure();
}

TEST(MAX17205, HonorsConfiguredAddresses) {
    COMMENT("configure() redirects both the main and shadow register banks.");
    project::MAX17205Tester tester;
    tester.testConfigureAddresses();
}

TEST(MAX17205, ReportsSubZeroTemperature) {
    COMMENT("A pack below freezing reports a negative temperature end to end.");
    project::MAX17205Tester tester;
    tester.testSubZeroTemperature();
}

TEST(MAX17205, ThrottlesAndRearmsErrorEvent) {
    COMMENT("The I2C error event throttles, and a clean sample re-arms it.");
    project::MAX17205Tester tester;
    tester.testErrorThrottle();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    // ! Seed testing is used so you can rerun the same randomized test! 
    // since this randomization is not used in our tests, seed generation is commented out
    
    // STest::Random::seed();
    return RUN_ALL_TESTS();
}

// ======================================================================
// \title  BMX160TestMain.cpp
// \brief  Unit tests for the BMX160 component.
//
// Two layers of test here:
//
//   * The decode functions, checked directly. They are pure, so a wrong
//     scaling constant or byte order is caught without any I2C mocking, and
//     the expected values below double as documentation of the data sheet
//     scaling.
//
//   * The component behavior, exercised through BMX160Tester with a fake
//     I2C bus: which registers get read and written, in what order, what
//     telemetry comes out, and how bus failures are handled.
// ======================================================================

#include <gtest/gtest.h>

#include "BMX160Tester.hpp"
#include "Fw/Test/UnitTest.hpp"

namespace {

constexpr F32 PI = 3.14159265358979F;
constexpr F32 STANDARD_GRAVITY = 9.80665F;

// Scale factors are computed in a different order here than any sensible
// implementation would use, so compare with a relative tolerance.
void expectClose(F32 actual, F32 expected, F32 relative = 1e-6F) {
    EXPECT_NEAR(actual, expected, relative * (expected < 0 ? -expected : expected));
}

// Pack raw magnetometer registers: X, Y, Z (status bits included) and
// RHALL, little-endian, as the part presents them in DATA.
void packMag(I16 x, I16 y, I16 z, U16 rhall, U8 out[project::BMX160::MAG_DATA_BYTES]) {
    const U16 words[4] = {static_cast<U16>(x), static_cast<U16>(y), static_cast<U16>(z), rhall};
    for (U32 i = 0; i < 4; i++) {
        out[2 * i] = static_cast<U8>(words[i] & 0xFFU);
        out[2 * i + 1] = static_cast<U8>(words[i] >> 8);
    }
}

// Raw counts for the magnetometer cases below. After dropping the status
// bits they are X 400, Y -250, Z -1200 and RHALL 6800; the negative ones
// check that the shift rounds toward negative infinity.
constexpr I16 MAG_RAW_X = 3205;
constexpr I16 MAG_RAW_Y = -1997;
constexpr I16 MAG_RAW_Z = -2399;
constexpr U16 MAG_RAW_RHALL = 27203;

// The compensation is single precision here and was double precision when
// the expected values were computed, so compare to within 1e-5 relative.
constexpr F32 MAG_TOLERANCE = 1e-5F;

}  // namespace

// ----------------------------------------------------------------------
// Decoding
// ----------------------------------------------------------------------

TEST(BMX160Decode, CombinesLittleEndianTwosComplement) {
    COMMENT("Each axis is LSB first, signed.");
    EXPECT_EQ(project::BMX160::toSigned(0x00, 0x00), static_cast<I16>(0));
    EXPECT_EQ(project::BMX160::toSigned(0x34, 0x12), static_cast<I16>(0x1234));
    EXPECT_EQ(project::BMX160::toSigned(0xFF, 0x7F), static_cast<I16>(32767));
    EXPECT_EQ(project::BMX160::toSigned(0x00, 0x80), static_cast<I16>(-32768));
    EXPECT_EQ(project::BMX160::toSigned(0xFF, 0xFF), static_cast<I16>(-1));
}

TEST(BMX160Decode, DecodesAxesInOrder) {
    COMMENT("Bytes are X, Y, Z, each scaled by the same factor.");
    const U8 bytes[6] = {0x00, 0x10, 0x00, 0xF0, 0x01, 0x00};  // 4096, -4096, 1
    const project::ImuVector v = project::BMX160::decodeAxes(bytes, 0.5F);
    EXPECT_EQ(v.get_x(), 2048.0F);
    EXPECT_EQ(v.get_y(), -2048.0F);
    EXPECT_EQ(v.get_z(), 0.5F);
}

TEST(BMX160Decode, ScalesGyroscopeRanges) {
    COMMENT("rad/s per count is (full scale in deg/s / 32768) * pi / 180.");
    expectClose(project::BMX160::gyroScale(project::GyroRange::DPS_2000), 2000.0F / 32768.0F * PI / 180.0F);
    expectClose(project::BMX160::gyroScale(project::GyroRange::DPS_1000), 1000.0F / 32768.0F * PI / 180.0F);
    expectClose(project::BMX160::gyroScale(project::GyroRange::DPS_500), 500.0F / 32768.0F * PI / 180.0F);
    expectClose(project::BMX160::gyroScale(project::GyroRange::DPS_250), 250.0F / 32768.0F * PI / 180.0F);
    expectClose(project::BMX160::gyroScale(project::GyroRange::DPS_125), 125.0F / 32768.0F * PI / 180.0F);
}

TEST(BMX160Decode, ScalesAccelerometerRanges) {
    COMMENT("m/s^2 per count is (full scale in g / 32768) * 9.80665.");
    expectClose(project::BMX160::accelScale(0x03), 2.0F / 32768.0F * STANDARD_GRAVITY);
    expectClose(project::BMX160::accelScale(0x05), 4.0F / 32768.0F * STANDARD_GRAVITY);
    expectClose(project::BMX160::accelScale(0x08), 8.0F / 32768.0F * STANDARD_GRAVITY);
    expectClose(project::BMX160::accelScale(0x0C), 16.0F / 32768.0F * STANDARD_GRAVITY);
}

TEST(BMX160Decode, SplitsErrorFlags) {
    COMMENT("drop_cmd_err is bit 6, fatal_err is bit 0; error_code is ignored.");
    project::ImuErrors e = project::BMX160::decodeErrors(0x41);
    EXPECT_TRUE(e.get_dropCommandError());
    EXPECT_TRUE(e.get_fatalError());
    e = project::BMX160::decodeErrors(0x1E);
    EXPECT_FALSE(e.get_dropCommandError());
    EXPECT_FALSE(e.get_fatalError());
}

TEST(BMX160Decode, DecodesMagnetometerTrim) {
    COMMENT("Trim terms are taken from their registers with the right width and sign.");
    const project::BMX160::MagTrim t = project::BMX160::decodeTrim(project::BMX160Tester::TRIM_REGS);
    EXPECT_EQ(t.x1, -3);
    EXPECT_EQ(t.y1, 2);
    EXPECT_EQ(t.z4, -120);
    EXPECT_EQ(t.x2, 26);
    EXPECT_EQ(t.y2, -5);
    EXPECT_EQ(t.z2, 763);
    EXPECT_EQ(t.z1, 24747U);
    EXPECT_EQ(t.xyz1, 6639U);  // bit 15 of the register pair is set and must be dropped
    EXPECT_EQ(t.z3, -180);
    EXPECT_EQ(t.xy2, -3);
    EXPECT_EQ(t.xy1, 29U);
}

// The expected fields in the magnetometer tests below were produced by
// running the BMM150 data sheet's floating-point compensation, in double
// precision, on the trim above and the raw counts.

TEST(BMX160Decode, CompensatesMagnetometer) {
    COMMENT("A normal reading is compensated with the trim, in uT.");
    U8 raw[project::BMX160::MAG_DATA_BYTES];
    packMag(MAG_RAW_X, MAG_RAW_Y, MAG_RAW_Z, MAG_RAW_RHALL, raw);
    const project::ImuVector field =
        project::BMX160::decodeMag(raw, project::BMX160::decodeTrim(project::BMX160Tester::TRIM_REGS));
    expectClose(field.get_x(), 143.421803F, MAG_TOLERANCE);
    expectClose(field.get_y(), -74.4801056F, MAG_TOLERANCE);
    expectClose(field.get_z(), -374.907603F, MAG_TOLERANCE);
}

TEST(BMX160Decode, ZeroesOverflowedMagnetometerAxes) {
    COMMENT("An overflowed axis reads 0; the others are unaffected.");
    const project::BMX160::MagTrim trim = project::BMX160::decodeTrim(project::BMX160Tester::TRIM_REGS);
    U8 raw[project::BMX160::MAG_DATA_BYTES];

    packMag(-32768, MAG_RAW_Y, MAG_RAW_Z, MAG_RAW_RHALL, raw);  // X at -4096 after the shift
    project::ImuVector field = project::BMX160::decodeMag(raw, trim);
    EXPECT_EQ(field.get_x(), 0.0F);
    expectClose(field.get_y(), -74.4801056F, MAG_TOLERANCE);
    expectClose(field.get_z(), -374.907603F, MAG_TOLERANCE);

    packMag(MAG_RAW_X, MAG_RAW_Y, -32768, MAG_RAW_RHALL, raw);  // Z at -16384 after the shift
    field = project::BMX160::decodeMag(raw, trim);
    expectClose(field.get_x(), 143.421803F, MAG_TOLERANCE);
    EXPECT_EQ(field.get_z(), 0.0F);
}

TEST(BMX160Decode, ZeroesMagnetometerWithoutRhall) {
    COMMENT("With RHALL 0 no axis can be compensated, so all read 0.");
    U8 raw[project::BMX160::MAG_DATA_BYTES];
    packMag(MAG_RAW_X, MAG_RAW_Y, MAG_RAW_Z, 3, raw);  // RHALL 0 after the shift
    const project::ImuVector field =
        project::BMX160::decodeMag(raw, project::BMX160::decodeTrim(project::BMX160Tester::TRIM_REGS));
    EXPECT_EQ(field, project::ImuVector(0.0F, 0.0F, 0.0F));
}

TEST(BMX160Decode, ScalesMagnetometerWithBlankTrim) {
    COMMENT("A part whose trim reads back blank is scaled at 1/16 uT per count.");
    U8 raw[project::BMX160::MAG_DATA_BYTES];
    packMag(MAG_RAW_X, MAG_RAW_Y, MAG_RAW_Z, MAG_RAW_RHALL, raw);
    const U8 blank[project::BMX160::BMM_TRIM_LENGTH] = {};
    const project::ImuVector field = project::BMX160::decodeMag(raw, project::BMX160::decodeTrim(blank));
    EXPECT_EQ(field, project::ImuVector(25.0F, -15.625F, -75.0F));
}

// ----------------------------------------------------------------------
// Component behavior
// ----------------------------------------------------------------------

TEST(BMX160, BootsInOrder) {
    COMMENT("Boot resets, identifies and configures the part in a fixed sequence.");
    project::BMX160Tester tester;
    tester.testBootSequence();
}

TEST(BMX160, RejectsWrongChipId) {
    COMMENT("A part with the wrong chip ID is never configured.");
    project::BMX160Tester tester;
    tester.testBootWrongChipId();
}

TEST(BMX160, StaysAbsentAfterBootBusFailure) {
    COMMENT("A bus failure during boot leaves the part absent.");
    project::BMX160Tester tester;
    tester.testBootBusFailure();
}

TEST(BMX160, AbandonsBootOnRejectedSetting) {
    COMMENT("An error_code after a data-rate write fails boot.");
    project::BMX160Tester tester;
    tester.testBootSettingRejected();
}

TEST(BMX160, AbandonsBootOnDroppedCommand) {
    COMMENT("A dropped power-mode command fails boot.");
    project::BMX160Tester tester;
    tester.testBootCommandDropped();
}

TEST(BMX160, AbandonsBootOnInvalidRangeReadback) {
    COMMENT("A range register reading back an invalid value fails boot.");
    project::BMX160Tester tester;
    tester.testBootInvalidRangeReadback();
}

TEST(BMX160, AbandonsBootOnMagTransferTimeout) {
    COMMENT("A magnetometer interface transfer that never completes fails boot.");
    project::BMX160Tester tester;
    tester.testBootMagTransferTimeout();
}

TEST(BMX160, StaysOffBusBeforeBoot) {
    COMMENT("A part that has not booted never touches the bus.");
    project::BMX160Tester tester;
    tester.testReadBeforeBoot();
}

TEST(BMX160, ReadsGyroscope) {
    COMMENT("readGyro burst-reads and scales the gyroscope data.");
    project::BMX160Tester tester;
    tester.testReadGyro();
}

TEST(BMX160, ReadsAccelerometer) {
    COMMENT("readAccel burst-reads and scales the accelerometer data.");
    project::BMX160Tester tester;
    tester.testReadAccel();
}

TEST(BMX160, ReadsMagnetometer) {
    COMMENT("readMag burst-reads the magnetometer and compensates it with the part's trim.");
    project::BMX160Tester tester;
    tester.testReadMag();
}

TEST(BMX160, ReportsReadFailure) {
    COMMENT("A failed read is reported and publishes nothing.");
    project::BMX160Tester tester;
    tester.testReadFailure();
}

TEST(BMX160, ChangesGyroscopeRange) {
    COMMENT("setGyroRange rewrites GYR_RANGE and rescales later reads.");
    project::BMX160Tester tester;
    tester.testSetGyroRange();
}

TEST(BMX160, ReadsErrorFlags) {
    COMMENT("readErrors reads ERR_REG once per flag.");
    project::BMX160Tester tester;
    tester.testReadErrors();
}

TEST(BMX160, HonorsConfiguredAddress) {
    COMMENT("configure() moves every transaction to the new address.");
    project::BMX160Tester tester;
    tester.testConfigureAddress();
}

TEST(BMX160, RunBootsThenSamples) {
    COMMENT("The rate-group port boots once, then samples on every tick.");
    project::BMX160Tester tester;
    tester.testRunBootsThenSamples();
}

TEST(BMX160, ThrottlesAndRearmsErrorEvent) {
    COMMENT("The I2C error event throttles, and a clean read re-arms it.");
    project::BMX160Tester tester;
    tester.testErrorThrottle();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <gtest/gtest.h>

#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"
#include "MAX17205Tester.hpp"

TEST(MAX17205, DecodesLittleEndianSignedCurrent) {
    COMMENT("Verify little-endian signed current decoding.");
    EXPECT_EQ(project::MAX17205::unpackSigned(0x00, 0x80), static_cast<I16>(-32768));
    EXPECT_EQ(project::MAX17205::unpackSigned(0x34, 0x12), static_cast<I16>(0x1234));
}

TEST(MAX17205, ConvertsRegisterValues) {
    COMMENT("Verify MAX17205 register scaling conversions.");
    EXPECT_FLOAT_EQ(project::MAX17205::decodeStateOfCharge(2560), 10.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCapacity(200), 100.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeCurrent(64), 10.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeVoltage(4000), 5000.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeMidVoltage(1280), 100.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTime(16), 90.0F);
    EXPECT_FLOAT_EQ(project::MAX17205::decodeTemperature(256), 100.0F);
    EXPECT_EQ(project::MAX17205::decodeThermistorTemperature(3000), 2690);
}

TEST(MAX17205, ReadsAllMeasurements) {
    COMMENT("Verify a successful READ_ALL command and mocked I2C snapshot.");
    project::MAX17205Tester tester;
    tester.testReadAll();
}

TEST(MAX17205, ResetsDevice) {
    COMMENT("Verify RESET writes the expected CONFIG2 transaction.");
    project::MAX17205Tester tester;
    tester.testReset();
}

TEST(MAX17205, ReportsReadFailure) {
    COMMENT("Verify I2C failures produce an error command response.");
    project::MAX17205Tester tester;
    tester.testReadFailure();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

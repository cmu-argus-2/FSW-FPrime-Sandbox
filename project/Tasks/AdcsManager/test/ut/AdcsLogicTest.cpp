// ======================================================================
// \title  AdcsLogicTest.cpp
// \brief  AdcsLogic checked against the flight software's own ADCS functions.
//
// AdcsVectors.hpp holds inputs and the outputs the flight software's
// functions produced for them, in double precision (gen_adcs_vectors.py).
// AdcsLogic works in single precision, so values compare to a relative
// tolerance; statuses and modes compare exactly.
// ======================================================================

#include <gtest/gtest.h>

#include <cmath>

#include "project/Tasks/AdcsManager/AdcsLogic.hpp"

namespace {

using project::AdcsLogic::COIL_COUNT;
using project::AdcsLogic::Vec3;
namespace AdcsLogic = project::AdcsLogic;

struct GyroCase {
    Vec3 raw;
    int status;
    Vec3 out;
};

struct MagCase {
    Vec3 rawUt;
    Vec3 biasT;
    Vec3 scale;
    int status;
    Vec3 outT;
};

struct BdotCase {
    Vec3 samples[AdcsLogic::BDOT_SAMPLES];
    float dtS;
    Vec3 u;
};

struct BcrossCase {
    Vec3 field;
    Vec3 omega;
    Vec3 u;
};

struct AllocCase {
    Vec3 u;
    Vec3 b;
    bool available[COIL_COUNT];
    float throttle[COIL_COUNT];
    bool status[COIL_COUNT];
};

struct ModeCase {
    int current;
    int controller;
    int gyroStatus;
    Vec3 omega;
    int expected;
};

#include "AdcsVectors.hpp"

// Single precision against a double precision reference: allow 1e-5 of the
// value, plus a small floor so that values near zero compare sensibly.
void expectClose(float actual, float expected, float floor = 1e-9F) {
    EXPECT_NEAR(actual, expected, std::fabs(expected) * 1e-5F + floor);
}

void expectClose(const Vec3& actual, const Vec3& expected, float floor = 1e-9F) {
    expectClose(actual.x, expected.x, floor);
    expectClose(actual.y, expected.y, floor);
    expectClose(actual.z, expected.z, floor);
}

}  // namespace

TEST(AdcsLogic, ChecksGyroscopeReadings) {
    for (const GyroCase& c : GYRO_CASES) {
        Vec3 out = {1.0F, 1.0F, 1.0F};
        EXPECT_EQ(static_cast<int>(AdcsLogic::checkGyro(c.raw, out)), c.status);
        expectClose(out, c.out);
    }
}

TEST(AdcsLogic, RejectsNanGyroscopeReading) {
    Vec3 out = {1.0F, 1.0F, 1.0F};
    EXPECT_EQ(AdcsLogic::checkGyro({NAN, 0.0F, 0.0F}, out), AdcsLogic::Status::GYRO_FAIL);
    EXPECT_EQ(out.x, 0.0F);
    EXPECT_EQ(out.y, 0.0F);
    EXPECT_EQ(out.z, 0.0F);
}

TEST(AdcsLogic, CorrectsMagnetometerReadings) {
    for (const MagCase& c : MAG_CASES) {
        Vec3 out = {1.0F, 1.0F, 1.0F};
        EXPECT_EQ(static_cast<int>(AdcsLogic::correctMag(c.rawUt, c.biasT, c.scale, out)), c.status);
        expectClose(out, c.outT, 1e-12F);
    }
}

TEST(AdcsLogic, ComputesBdotCommand) {
    for (const BdotCase& c : BDOT_CASES) {
        expectClose(AdcsLogic::bdot(c.samples, c.dtS), c.u, 1e-6F);
    }
}

TEST(AdcsLogic, BdotNeedsPositiveSpacing) {
    const Vec3 u = AdcsLogic::bdot(BDOT_CASES[1].samples, 0.0F);
    EXPECT_EQ(u.x, 0.0F);
    EXPECT_EQ(u.y, 0.0F);
    EXPECT_EQ(u.z, 0.0F);
}

TEST(AdcsLogic, ComputesBcrossCommand) {
    for (const BcrossCase& c : BCROSS_CASES) {
        expectClose(AdcsLogic::bcross(c.field, c.omega), c.u, 1e-6F);
    }
}

TEST(AdcsLogic, AllocatesCoils) {
    for (const AllocCase& c : ALLOC_CASES) {
        float throttle[COIL_COUNT];
        for (float& t : throttle) {
            t = 9.0F;  // overwritten for every coil
        }
        AdcsLogic::allocate(c.u, c.b, c.available, throttle);
        for (uint32_t coil = 0; coil < COIL_COUNT; coil++) {
            expectClose(throttle[coil], c.throttle[coil], 1e-6F);
            // An unavailable coil always gets exactly 0. The flight
            // allocator's per-coil status is just the availability.
            if (!c.available[coil]) {
                EXPECT_EQ(throttle[coil], 0.0F) << "coil " << coil;
            }
            EXPECT_EQ(c.status[coil], c.available[coil]) << "coil " << coil;
        }
    }
}

TEST(AdcsLogic, FollowsModeRules) {
    for (const ModeCase& c : MODE_CASES) {
        const AdcsLogic::Mode next = AdcsLogic::nextMode(static_cast<AdcsLogic::Mode>(c.current),
                                                         static_cast<AdcsLogic::Controller>(c.controller),
                                                         static_cast<AdcsLogic::Status>(c.gyroStatus), c.omega);
        EXPECT_EQ(static_cast<int>(next), c.expected)
            << "from mode " << c.current << ", controller " << c.controller << ", gyro status " << c.gyroStatus
            << ", rate " << AdcsLogic::norm(c.omega);
    }
}

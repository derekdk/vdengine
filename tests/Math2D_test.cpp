/**
 * @file Math2D_test.cpp
 * @brief Unit tests for Math2D.h gameplay math helpers
 */

#include <vde/api/Math2D.h>

#include <gtest/gtest.h>

using namespace vde::math2d;

// ============================================================================
// Scalar helpers
// ============================================================================

TEST(Math2DScalar, ClampWithinRange) {
    EXPECT_FLOAT_EQ(clamp(0.5f, 0.0f, 1.0f), 0.5f);
}

TEST(Math2DScalar, ClampBelowMin) {
    EXPECT_FLOAT_EQ(clamp(-1.0f, 0.0f, 1.0f), 0.0f);
}

TEST(Math2DScalar, ClampAboveMax) {
    EXPECT_FLOAT_EQ(clamp(5.0f, 0.0f, 1.0f), 1.0f);
}

TEST(Math2DScalar, SaturateClamps01) {
    EXPECT_FLOAT_EQ(saturate(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(saturate(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(saturate(1.5f), 1.0f);
}

TEST(Math2DScalar, LerpInterpolates) {
    EXPECT_FLOAT_EQ(lerp(0.0f, 10.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(lerp(0.0f, 10.0f, 0.5f), 5.0f);
    EXPECT_FLOAT_EQ(lerp(0.0f, 10.0f, 1.0f), 10.0f);
}

TEST(Math2DScalar, InverseLerpFindsT) {
    EXPECT_FLOAT_EQ(inverseLerp(0.0f, 10.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(inverseLerp(0.0f, 10.0f, 5.0f), 0.5f);
    EXPECT_FLOAT_EQ(inverseLerp(0.0f, 10.0f, 10.0f), 1.0f);
}

TEST(Math2DScalar, InverseLerpDegenerateRange) {
    EXPECT_FLOAT_EQ(inverseLerp(5.0f, 5.0f, 5.0f), 0.0f);
}

TEST(Math2DScalar, NearlyZeroDetectsSmallValues) {
    EXPECT_TRUE(nearlyZero(0.0f));
    EXPECT_TRUE(nearlyZero(0.00001f));
    EXPECT_FALSE(nearlyZero(1.0f));
}

TEST(Math2DScalar, NearlyEqualCompares) {
    EXPECT_TRUE(nearlyEqual(1.0f, 1.0f));
    EXPECT_TRUE(nearlyEqual(1.0f, 1.00001f));
    EXPECT_FALSE(nearlyEqual(1.0f, 2.0f));
}

// ============================================================================
// Vector helpers
// ============================================================================

TEST(Math2DVec, LengthSquared) {
    EXPECT_FLOAT_EQ(lengthSquared(glm::vec2(3.0f, 4.0f)), 25.0f);
}

TEST(Math2DVec, DistanceSquared) {
    EXPECT_FLOAT_EQ(distanceSquared(glm::vec2(0.0f), glm::vec2(3.0f, 4.0f)), 25.0f);
}

TEST(Math2DVec, NormalizeOrZeroNonZero) {
    auto n = normalizeOrZero(glm::vec2(3.0f, 0.0f));
    EXPECT_NEAR(n.x, 1.0f, 0.001f);
    EXPECT_NEAR(n.y, 0.0f, 0.001f);
}

TEST(Math2DVec, NormalizeOrZeroZeroVector) {
    auto n = normalizeOrZero(glm::vec2(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
}

TEST(Math2DVec, LerpVec2) {
    auto r = lerp(glm::vec2(0.0f, 0.0f), glm::vec2(10.0f, 20.0f), 0.5f);
    EXPECT_NEAR(r.x, 5.0f, 0.001f);
    EXPECT_NEAR(r.y, 10.0f, 0.001f);
}

TEST(Math2DVec, MoveTowardReachesTarget) {
    auto r = moveToward(glm::vec2(0.0f), glm::vec2(1.0f, 0.0f), 10.0f);
    EXPECT_NEAR(r.x, 1.0f, 0.001f);
    EXPECT_NEAR(r.y, 0.0f, 0.001f);
}

TEST(Math2DVec, MoveTowardPartialMove) {
    auto r = moveToward(glm::vec2(0.0f), glm::vec2(10.0f, 0.0f), 3.0f);
    EXPECT_NEAR(r.x, 3.0f, 0.001f);
    EXPECT_NEAR(r.y, 0.0f, 0.001f);
}

TEST(Math2DVec, PerpendicularLeftRotates90CCW) {
    auto p = perpendicularLeft(glm::vec2(1.0f, 0.0f));
    EXPECT_NEAR(p.x, 0.0f, 0.001f);
    EXPECT_NEAR(p.y, 1.0f, 0.001f);
}

TEST(Math2DVec, PerpendicularRightRotates90CW) {
    auto p = perpendicularRight(glm::vec2(1.0f, 0.0f));
    EXPECT_NEAR(p.x, 0.0f, 0.001f);
    EXPECT_NEAR(p.y, -1.0f, 0.001f);
}

TEST(Math2DVec, DirectionFromAngleDegreesUp) {
    auto d = directionFromAngleDegrees(0.0f);
    EXPECT_NEAR(d.x, 0.0f, 0.001f);
    EXPECT_NEAR(d.y, 1.0f, 0.001f);
}

TEST(Math2DVec, DirectionFromAngleDegreesRight) {
    auto d = directionFromAngleDegrees(90.0f);
    EXPECT_NEAR(d.x, 1.0f, 0.001f);
    EXPECT_NEAR(d.y, 0.0f, 0.001f);
}

TEST(Math2DVec, AngleDegreesFromUpRoundTrip) {
    for (float a : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
        auto d = directionFromAngleDegrees(a);
        float result = angleDegreesFromUp(d);
        EXPECT_NEAR(result, a, 0.1f) << "angle=" << a;
    }
}

// ============================================================================
// Type conversion
// ============================================================================

TEST(Math2DConvert, ToPositionFromVec2) {
    auto p = toPosition(glm::vec2(1.0f, 2.0f), 3.0f);
    EXPECT_FLOAT_EQ(p.x, 1.0f);
    EXPECT_FLOAT_EQ(p.y, 2.0f);
    EXPECT_FLOAT_EQ(p.z, 3.0f);
}

TEST(Math2DConvert, ToVec2FromPosition) {
    auto v = toVec2(vde::Position(1.0f, 2.0f, 3.0f));
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
}

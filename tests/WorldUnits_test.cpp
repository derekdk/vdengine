/**
 * @file WorldUnits_test.cpp
 * @brief Unit tests for WorldUnits.h (Phase 2.5)
 *
 * Tests Meters, WorldPoint, WorldExtent, and CoordinateSystem types.
 */

#include <vde/api/WorldUnits.h>

#include <cmath>

#include <gtest/gtest.h>

using namespace vde;

// ============================================================================
// Meters Tests
// ============================================================================

class MetersTest : public ::testing::Test {};

TEST_F(MetersTest, DefaultConstructor) {
    Meters m;
    EXPECT_FLOAT_EQ(m.value, 0.0f);
}

TEST_F(MetersTest, ValueConstructor) {
    Meters m(100.0f);
    EXPECT_FLOAT_EQ(m.value, 100.0f);
}

TEST_F(MetersTest, UserDefinedLiteralInteger) {
    Meters m = 100_m;
    EXPECT_FLOAT_EQ(m.value, 100.0f);
}

TEST_F(MetersTest, UserDefinedLiteralFloat) {
    Meters m = 50.5_m;
    EXPECT_FLOAT_EQ(m.value, 50.5f);
}

TEST_F(MetersTest, ImplicitConversionToFloat) {
    Meters m(75.0f);
    float f = m;
    EXPECT_FLOAT_EQ(f, 75.0f);
}

TEST_F(MetersTest, Negation) {
    Meters m = 100_m;
    Meters neg = -m;
    EXPECT_FLOAT_EQ(neg.value, -100.0f);
}

TEST_F(MetersTest, Addition) {
    Meters a = 100_m;
    Meters b = 50_m;
    Meters result = a + b;
    EXPECT_FLOAT_EQ(result.value, 150.0f);
}

TEST_F(MetersTest, Subtraction) {
    Meters a = 100_m;
    Meters b = 30_m;
    Meters result = a - b;
    EXPECT_FLOAT_EQ(result.value, 70.0f);
}

TEST_F(MetersTest, MultiplicationByScalar) {
    Meters m = 100_m;
    Meters result = m * 2.5f;
    EXPECT_FLOAT_EQ(result.value, 250.0f);
}

TEST_F(MetersTest, DivisionByScalar) {
    Meters m = 100_m;
    Meters result = m / 4.0f;
    EXPECT_FLOAT_EQ(result.value, 25.0f);
}

TEST_F(MetersTest, CompoundAddition) {
    Meters m = 100_m;
    m += 50_m;
    EXPECT_FLOAT_EQ(m.value, 150.0f);
}

TEST_F(MetersTest, CompoundSubtraction) {
    Meters m = 100_m;
    m -= 25_m;
    EXPECT_FLOAT_EQ(m.value, 75.0f);
}

TEST_F(MetersTest, CompoundMultiplication) {
    Meters m = 100_m;
    m *= 3.0f;
    EXPECT_FLOAT_EQ(m.value, 300.0f);
}

TEST_F(MetersTest, CompoundDivision) {
    Meters m = 100_m;
    m /= 2.0f;
    EXPECT_FLOAT_EQ(m.value, 50.0f);
}

TEST_F(MetersTest, EqualityComparison) {
    EXPECT_TRUE(100_m == 100_m);
    EXPECT_FALSE(100_m == 50_m);
}

TEST_F(MetersTest, InequalityComparison) {
    EXPECT_TRUE(100_m != 50_m);
    EXPECT_FALSE(100_m != 100_m);
}

TEST_F(MetersTest, LessThan) {
    EXPECT_TRUE(50_m < 100_m);
    EXPECT_FALSE(100_m < 50_m);
    EXPECT_FALSE(100_m < 100_m);
}

TEST_F(MetersTest, LessThanOrEqual) {
    EXPECT_TRUE(50_m <= 100_m);
    EXPECT_TRUE(100_m <= 100_m);
    EXPECT_FALSE(100_m <= 50_m);
}

TEST_F(MetersTest, GreaterThan) {
    EXPECT_TRUE(100_m > 50_m);
    EXPECT_FALSE(50_m > 100_m);
    EXPECT_FALSE(100_m > 100_m);
}

TEST_F(MetersTest, GreaterThanOrEqual) {
    EXPECT_TRUE(100_m >= 50_m);
    EXPECT_TRUE(100_m >= 100_m);
    EXPECT_FALSE(50_m >= 100_m);
}

TEST_F(MetersTest, AbsoluteValue) {
    EXPECT_FLOAT_EQ(Meters(-100.0f).abs().value, 100.0f);
    EXPECT_FLOAT_EQ(Meters(100.0f).abs().value, 100.0f);
    EXPECT_FLOAT_EQ(Meters(0.0f).abs().value, 0.0f);
}

// ============================================================================
// CoordinateSystem Tests
// ============================================================================

class CoordinateSystemTest : public ::testing::Test {};

TEST_F(CoordinateSystemTest, DefaultYUpNorth) {
    CoordinateSystem cs;
    EXPECT_FLOAT_EQ(cs.north.z, 1.0f);
    EXPECT_FLOAT_EQ(cs.north.x, 0.0f);
    EXPECT_FLOAT_EQ(cs.north.y, 0.0f);
}

TEST_F(CoordinateSystemTest, DefaultYUpEast) {
    CoordinateSystem cs;
    EXPECT_FLOAT_EQ(cs.east.x, 1.0f);
    EXPECT_FLOAT_EQ(cs.east.y, 0.0f);
    EXPECT_FLOAT_EQ(cs.east.z, 0.0f);
}

TEST_F(CoordinateSystemTest, DefaultYUpUp) {
    CoordinateSystem cs;
    EXPECT_FLOAT_EQ(cs.up.y, 1.0f);
    EXPECT_FLOAT_EQ(cs.up.x, 0.0f);
    EXPECT_FLOAT_EQ(cs.up.z, 0.0f);
}

TEST_F(CoordinateSystemTest, OppositeDirections) {
    CoordinateSystem cs;
    glm::vec3 south = cs.south();
    glm::vec3 west = cs.west();
    glm::vec3 down = cs.down();

    EXPECT_FLOAT_EQ(south.z, -1.0f);
    EXPECT_FLOAT_EQ(west.x, -1.0f);
    EXPECT_FLOAT_EQ(down.y, -1.0f);
}

TEST_F(CoordinateSystemTest, YUpPreset) {
    CoordinateSystem cs = CoordinateSystem::yUp();
    EXPECT_FLOAT_EQ(cs.north.z, 1.0f);
    EXPECT_FLOAT_EQ(cs.east.x, 1.0f);
    EXPECT_FLOAT_EQ(cs.up.y, 1.0f);
}

TEST_F(CoordinateSystemTest, ZUpPreset) {
    CoordinateSystem cs = CoordinateSystem::zUp();
    EXPECT_FLOAT_EQ(cs.north.y, 1.0f);  // North = +Y in Z-up
    EXPECT_FLOAT_EQ(cs.east.x, 1.0f);   // East = +X
    EXPECT_FLOAT_EQ(cs.up.z, 1.0f);     // Up = +Z
}

// ============================================================================
// WorldPoint Tests
// ============================================================================

class WorldPointTest : public ::testing::Test {};

TEST_F(WorldPointTest, DefaultConstructor) {
    WorldPoint pt;
    EXPECT_FLOAT_EQ(pt.x.value, 0.0f);
    EXPECT_FLOAT_EQ(pt.y.value, 0.0f);
    EXPECT_FLOAT_EQ(pt.z.value, 0.0f);
}

TEST_F(WorldPointTest, ValueConstructor) {
    WorldPoint pt(10_m, 20_m, 30_m);
    EXPECT_FLOAT_EQ(pt.x.value, 10.0f);
    EXPECT_FLOAT_EQ(pt.y.value, 20.0f);
    EXPECT_FLOAT_EQ(pt.z.value, 30.0f);
}

TEST_F(WorldPointTest, FromGlmVec3) {
    WorldPoint pt(glm::vec3(1.5f, 2.5f, 3.5f));
    EXPECT_FLOAT_EQ(pt.x.value, 1.5f);
    EXPECT_FLOAT_EQ(pt.y.value, 2.5f);
    EXPECT_FLOAT_EQ(pt.z.value, 3.5f);
}

TEST_F(WorldPointTest, ToVec3) {
    WorldPoint pt(10_m, 20_m, 30_m);
    glm::vec3 v = pt.toVec3();
    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 20.0f);
    EXPECT_FLOAT_EQ(v.z, 30.0f);
}

TEST_F(WorldPointTest, FromDirectionsDefaultCoords) {
    // Y-up: north=+Z, east=+X, up=+Y
    WorldPoint pt = WorldPoint::fromDirections(100_m, 50_m, 20_m);
    // northSouth maps to Z, eastWest maps to X, upDown maps to Y
    EXPECT_FLOAT_EQ(pt.x.value, 50.0f);   // east
    EXPECT_FLOAT_EQ(pt.y.value, 20.0f);   // up
    EXPECT_FLOAT_EQ(pt.z.value, 100.0f);  // north
}

TEST_F(WorldPointTest, FromDirectionsNegative) {
    // South, west, down should be negative
    WorldPoint pt = WorldPoint::fromDirections(-100_m, -50_m, -20_m);
    EXPECT_FLOAT_EQ(pt.x.value, -50.0f);   // west
    EXPECT_FLOAT_EQ(pt.y.value, -20.0f);   // down
    EXPECT_FLOAT_EQ(pt.z.value, -100.0f);  // south
}

TEST_F(WorldPointTest, FromDirectionsWithCustomCoords) {
    CoordinateSystem zUp = CoordinateSystem::zUp();
    WorldPoint pt = WorldPoint::fromDirections(100_m, 50_m, 20_m, zUp);
    // Z-up: north=+Y, east=+X, up=+Z
    EXPECT_FLOAT_EQ(pt.x.value, 50.0f);   // east
    EXPECT_FLOAT_EQ(pt.y.value, 100.0f);  // north
    EXPECT_FLOAT_EQ(pt.z.value, 20.0f);   // up
}

TEST_F(WorldPointTest, Addition) {
    WorldPoint a(10_m, 20_m, 30_m);
    WorldPoint b(5_m, 10_m, 15_m);
    WorldPoint result = a + b;
    EXPECT_FLOAT_EQ(result.x.value, 15.0f);
    EXPECT_FLOAT_EQ(result.y.value, 30.0f);
    EXPECT_FLOAT_EQ(result.z.value, 45.0f);
}

TEST_F(WorldPointTest, Subtraction) {
    WorldPoint a(10_m, 20_m, 30_m);
    WorldPoint b(5_m, 10_m, 15_m);
    WorldPoint result = a - b;
    EXPECT_FLOAT_EQ(result.x.value, 5.0f);
    EXPECT_FLOAT_EQ(result.y.value, 10.0f);
    EXPECT_FLOAT_EQ(result.z.value, 15.0f);
}

TEST_F(WorldPointTest, ScalarMultiplication) {
    WorldPoint pt(10_m, 20_m, 30_m);
    WorldPoint result = pt * 2.0f;
    EXPECT_FLOAT_EQ(result.x.value, 20.0f);
    EXPECT_FLOAT_EQ(result.y.value, 40.0f);
    EXPECT_FLOAT_EQ(result.z.value, 60.0f);
}

// ============================================================================
// WorldExtent Tests
// ============================================================================

class WorldExtentTest : public ::testing::Test {};

TEST_F(WorldExtentTest, DefaultConstructor) {
    WorldExtent ext;
    EXPECT_FLOAT_EQ(ext.width.value, 0.0f);
    EXPECT_FLOAT_EQ(ext.height.value, 0.0f);
    EXPECT_FLOAT_EQ(ext.depth.value, 0.0f);
}

TEST_F(WorldExtentTest, ValueConstructor) {
    WorldExtent ext(100_m, 50_m, 200_m);
    EXPECT_FLOAT_EQ(ext.width.value, 100.0f);
    EXPECT_FLOAT_EQ(ext.height.value, 50.0f);
    EXPECT_FLOAT_EQ(ext.depth.value, 200.0f);
}

TEST_F(WorldExtentTest, FlatFactory) {
    WorldExtent ext = WorldExtent::flat(100_m, 200_m);
    EXPECT_FLOAT_EQ(ext.width.value, 100.0f);
    EXPECT_FLOAT_EQ(ext.height.value, 0.0f);
    EXPECT_FLOAT_EQ(ext.depth.value, 200.0f);
}

TEST_F(WorldExtentTest, ToVec3) {
    WorldExtent ext(10_m, 20_m, 30_m);
    glm::vec3 v = ext.toVec3();
    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 20.0f);
    EXPECT_FLOAT_EQ(v.z, 30.0f);
}

TEST_F(WorldExtentTest, Is2D) {
    WorldExtent flat = WorldExtent::flat(100_m, 200_m);
    WorldExtent vol = WorldExtent(100_m, 50_m, 200_m);

    EXPECT_TRUE(flat.is2D());
    EXPECT_FALSE(vol.is2D());
}

TEST_F(WorldExtentTest, Volume) {
    WorldExtent ext(10_m, 20_m, 30_m);
    EXPECT_FLOAT_EQ(ext.volume(), 6000.0f);  // 10 * 20 * 30
}

TEST_F(WorldExtentTest, VolumeIs2D) {
    WorldExtent ext = WorldExtent::flat(100_m, 200_m);
    EXPECT_FLOAT_EQ(ext.volume(), 0.0f);  // No height
}

TEST_F(WorldExtentTest, BaseArea) {
    WorldExtent ext(10_m, 20_m, 30_m);
    EXPECT_FLOAT_EQ(ext.baseArea(), 300.0f);  // 10 * 30
}

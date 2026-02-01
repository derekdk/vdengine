/**
 * @file WorldBounds_test.cpp
 * @brief Unit tests for WorldBounds.h (Phase 2.5)
 * 
 * Tests WorldBounds and WorldBounds2D classes.
 */

#include <gtest/gtest.h>
#include <vde/api/WorldBounds.h>

using namespace vde;

// ============================================================================
// WorldBounds Tests
// ============================================================================

class WorldBoundsTest : public ::testing::Test {
protected:
    // Standard test bounds: -100 to +100 in all dimensions
    WorldBounds standardBounds;
    
    void SetUp() override {
        standardBounds = WorldBounds(
            WorldPoint(-100_m, -50_m, -100_m),
            WorldPoint(100_m, 50_m, 100_m)
        );
    }
};

TEST_F(WorldBoundsTest, DefaultConstructor) {
    WorldBounds bounds;
    // Default WorldPoints should be zero
    EXPECT_FLOAT_EQ(bounds.min.x.value, 0.0f);
    EXPECT_FLOAT_EQ(bounds.max.x.value, 0.0f);
}

TEST_F(WorldBoundsTest, PointConstructor) {
    WorldBounds bounds(
        WorldPoint(10_m, 20_m, 30_m),
        WorldPoint(40_m, 50_m, 60_m)
    );
    EXPECT_FLOAT_EQ(bounds.min.x.value, 10.0f);
    EXPECT_FLOAT_EQ(bounds.max.z.value, 60.0f);
}

TEST_F(WorldBoundsTest, CardinalDirectionAccessors) {
    // North = max Z, South = min Z
    EXPECT_FLOAT_EQ(standardBounds.northLimit().value, 100.0f);
    EXPECT_FLOAT_EQ(standardBounds.southLimit().value, -100.0f);
    
    // East = max X, West = min X
    EXPECT_FLOAT_EQ(standardBounds.eastLimit().value, 100.0f);
    EXPECT_FLOAT_EQ(standardBounds.westLimit().value, -100.0f);
    
    // Up = max Y, Down = min Y
    EXPECT_FLOAT_EQ(standardBounds.upLimit().value, 50.0f);
    EXPECT_FLOAT_EQ(standardBounds.downLimit().value, -50.0f);
}

TEST_F(WorldBoundsTest, Width) {
    EXPECT_FLOAT_EQ(standardBounds.width().value, 200.0f);
}

TEST_F(WorldBoundsTest, Height) {
    EXPECT_FLOAT_EQ(standardBounds.height().value, 100.0f);
}

TEST_F(WorldBoundsTest, Depth) {
    EXPECT_FLOAT_EQ(standardBounds.depth().value, 200.0f);
}

TEST_F(WorldBoundsTest, Extent) {
    WorldExtent ext = standardBounds.extent();
    EXPECT_FLOAT_EQ(ext.width.value, 200.0f);
    EXPECT_FLOAT_EQ(ext.height.value, 100.0f);
    EXPECT_FLOAT_EQ(ext.depth.value, 200.0f);
}

TEST_F(WorldBoundsTest, Center) {
    WorldPoint center = standardBounds.center();
    EXPECT_FLOAT_EQ(center.x.value, 0.0f);
    EXPECT_FLOAT_EQ(center.y.value, 0.0f);
    EXPECT_FLOAT_EQ(center.z.value, 0.0f);
}

TEST_F(WorldBoundsTest, CenterOffset) {
    WorldBounds offset(
        WorldPoint(0_m, 0_m, 0_m),
        WorldPoint(100_m, 100_m, 100_m)
    );
    WorldPoint center = offset.center();
    EXPECT_FLOAT_EQ(center.x.value, 50.0f);
    EXPECT_FLOAT_EQ(center.y.value, 50.0f);
    EXPECT_FLOAT_EQ(center.z.value, 50.0f);
}

TEST_F(WorldBoundsTest, ContainsPointInside) {
    WorldPoint inside(0_m, 0_m, 0_m);
    EXPECT_TRUE(standardBounds.contains(inside));
}

TEST_F(WorldBoundsTest, ContainsPointOnEdge) {
    WorldPoint edge(100_m, 50_m, 100_m);
    EXPECT_TRUE(standardBounds.contains(edge));
}

TEST_F(WorldBoundsTest, ContainsPointOutside) {
    WorldPoint outside(150_m, 0_m, 0_m);
    EXPECT_FALSE(standardBounds.contains(outside));
}

TEST_F(WorldBoundsTest, IntersectsOverlapping) {
    WorldBounds other(
        WorldPoint(50_m, 25_m, 50_m),
        WorldPoint(150_m, 75_m, 150_m)
    );
    EXPECT_TRUE(standardBounds.intersects(other));
}

TEST_F(WorldBoundsTest, IntersectsContained) {
    WorldBounds smaller(
        WorldPoint(-50_m, -25_m, -50_m),
        WorldPoint(50_m, 25_m, 50_m)
    );
    EXPECT_TRUE(standardBounds.intersects(smaller));
}

TEST_F(WorldBoundsTest, IntersectsDisjoint) {
    WorldBounds far(
        WorldPoint(200_m, 200_m, 200_m),
        WorldPoint(300_m, 300_m, 300_m)
    );
    EXPECT_FALSE(standardBounds.intersects(far));
}

TEST_F(WorldBoundsTest, Is2DFalse) {
    EXPECT_FALSE(standardBounds.is2D());
}

TEST_F(WorldBoundsTest, Is2DTrue) {
    WorldBounds flat(
        WorldPoint(-100_m, 0_m, -100_m),
        WorldPoint(100_m, 0_m, 100_m)
    );
    EXPECT_TRUE(flat.is2D());
}

TEST_F(WorldBoundsTest, FromDirectionalLimits) {
    WorldBounds bounds = WorldBounds::fromDirectionalLimits(
        100_m, -100_m,  // north, south
        -100_m, 100_m,  // west, east
        50_m, -50_m     // up, down
    );
    
    EXPECT_FLOAT_EQ(bounds.northLimit().value, 100.0f);
    EXPECT_FLOAT_EQ(bounds.southLimit().value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.eastLimit().value, 100.0f);
    EXPECT_FLOAT_EQ(bounds.westLimit().value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.upLimit().value, 50.0f);
    EXPECT_FLOAT_EQ(bounds.downLimit().value, -50.0f);
}

TEST_F(WorldBoundsTest, FromDirectionalLimitsWithHelpers) {
    WorldBounds bounds = WorldBounds::fromDirectionalLimits(
        100_m, WorldBounds::south(100_m),
        WorldBounds::west(100_m), 100_m,
        20_m, WorldBounds::down(10_m)
    );
    
    EXPECT_FLOAT_EQ(bounds.northLimit().value, 100.0f);
    EXPECT_FLOAT_EQ(bounds.southLimit().value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.width().value, 200.0f);
    EXPECT_FLOAT_EQ(bounds.height().value, 30.0f);
}

TEST_F(WorldBoundsTest, FromCenterAndExtent) {
    WorldBounds bounds = WorldBounds::fromCenterAndExtent(
        WorldPoint(0_m, 0_m, 0_m),
        WorldExtent(200_m, 100_m, 200_m)
    );
    
    EXPECT_FLOAT_EQ(bounds.min.x.value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.max.x.value, 100.0f);
    EXPECT_FLOAT_EQ(bounds.min.y.value, -50.0f);
    EXPECT_FLOAT_EQ(bounds.max.y.value, 50.0f);
}

TEST_F(WorldBoundsTest, FromCenterAndExtentOffset) {
    WorldBounds bounds = WorldBounds::fromCenterAndExtent(
        WorldPoint(50_m, 25_m, 50_m),
        WorldExtent(100_m, 50_m, 100_m)
    );
    
    EXPECT_FLOAT_EQ(bounds.min.x.value, 0.0f);
    EXPECT_FLOAT_EQ(bounds.max.x.value, 100.0f);
}

TEST_F(WorldBoundsTest, FlatFactory) {
    WorldBounds bounds = WorldBounds::flat(100_m, -100_m, -100_m, 100_m);
    
    EXPECT_TRUE(bounds.is2D());
    EXPECT_FLOAT_EQ(bounds.height().value, 0.0f);
    EXPECT_FLOAT_EQ(bounds.width().value, 200.0f);
    EXPECT_FLOAT_EQ(bounds.depth().value, 200.0f);
}

TEST_F(WorldBoundsTest, HelperSouth) {
    Meters s = WorldBounds::south(100_m);
    EXPECT_FLOAT_EQ(s.value, -100.0f);
}

TEST_F(WorldBoundsTest, HelperWest) {
    Meters w = WorldBounds::west(50_m);
    EXPECT_FLOAT_EQ(w.value, -50.0f);
}

TEST_F(WorldBoundsTest, HelperDown) {
    Meters d = WorldBounds::down(25_m);
    EXPECT_FLOAT_EQ(d.value, -25.0f);
}

// ============================================================================
// WorldBounds2D Tests
// ============================================================================

class WorldBounds2DTest : public ::testing::Test {
protected:
    WorldBounds2D standardBounds;
    
    void SetUp() override {
        standardBounds = WorldBounds2D(-100_m, -100_m, 100_m, 100_m);
    }
};

TEST_F(WorldBounds2DTest, DefaultConstructor) {
    WorldBounds2D bounds;
    EXPECT_FLOAT_EQ(bounds.minX.value, 0.0f);
    EXPECT_FLOAT_EQ(bounds.minY.value, 0.0f);
}

TEST_F(WorldBounds2DTest, ValueConstructor) {
    EXPECT_FLOAT_EQ(standardBounds.minX.value, -100.0f);
    EXPECT_FLOAT_EQ(standardBounds.minY.value, -100.0f);
    EXPECT_FLOAT_EQ(standardBounds.maxX.value, 100.0f);
    EXPECT_FLOAT_EQ(standardBounds.maxY.value, 100.0f);
}

TEST_F(WorldBounds2DTest, Width) {
    EXPECT_FLOAT_EQ(standardBounds.width().value, 200.0f);
}

TEST_F(WorldBounds2DTest, Height) {
    EXPECT_FLOAT_EQ(standardBounds.height().value, 200.0f);
}

TEST_F(WorldBounds2DTest, Extent) {
    WorldExtent ext = standardBounds.extent();
    EXPECT_FLOAT_EQ(ext.width.value, 200.0f);
    EXPECT_FLOAT_EQ(ext.height.value, 0.0f);  // 2D extent has no height
    EXPECT_FLOAT_EQ(ext.depth.value, 200.0f);
}

TEST_F(WorldBounds2DTest, Center) {
    glm::vec2 center = standardBounds.center();
    EXPECT_FLOAT_EQ(center.x, 0.0f);
    EXPECT_FLOAT_EQ(center.y, 0.0f);
}

TEST_F(WorldBounds2DTest, ContainsPointInside) {
    EXPECT_TRUE(standardBounds.contains(0_m, 0_m));
}

TEST_F(WorldBounds2DTest, ContainsPointOnEdge) {
    EXPECT_TRUE(standardBounds.contains(100_m, 100_m));
}

TEST_F(WorldBounds2DTest, ContainsPointOutside) {
    EXPECT_FALSE(standardBounds.contains(150_m, 0_m));
}

TEST_F(WorldBounds2DTest, ContainsGlmVec2) {
    EXPECT_TRUE(standardBounds.contains(glm::vec2(50.0f, 50.0f)));
    EXPECT_FALSE(standardBounds.contains(glm::vec2(150.0f, 0.0f)));
}

TEST_F(WorldBounds2DTest, FromCardinal) {
    WorldBounds2D bounds = WorldBounds2D::fromCardinal(
        100_m, -100_m,  // north, south (Y)
        -100_m, 100_m   // west, east (X)
    );
    
    EXPECT_FLOAT_EQ(bounds.minX.value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.maxX.value, 100.0f);
    EXPECT_FLOAT_EQ(bounds.minY.value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.maxY.value, 100.0f);
}

TEST_F(WorldBounds2DTest, FromLRTB) {
    WorldBounds2D bounds = WorldBounds2D::fromLRTB(
        -50_m, 50_m,    // left, right
        100_m, -100_m   // top, bottom
    );
    
    EXPECT_FLOAT_EQ(bounds.minX.value, -50.0f);
    EXPECT_FLOAT_EQ(bounds.maxX.value, 50.0f);
    EXPECT_FLOAT_EQ(bounds.minY.value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.maxY.value, 100.0f);
}

TEST_F(WorldBounds2DTest, FromCenter) {
    WorldBounds2D bounds = WorldBounds2D::fromCenter(0_m, 0_m, 200_m, 200_m);
    
    EXPECT_FLOAT_EQ(bounds.minX.value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.maxX.value, 100.0f);
    EXPECT_FLOAT_EQ(bounds.minY.value, -100.0f);
    EXPECT_FLOAT_EQ(bounds.maxY.value, 100.0f);
}

TEST_F(WorldBounds2DTest, FromCenterOffset) {
    WorldBounds2D bounds = WorldBounds2D::fromCenter(50_m, 50_m, 100_m, 100_m);
    
    EXPECT_FLOAT_EQ(bounds.minX.value, 0.0f);
    EXPECT_FLOAT_EQ(bounds.maxX.value, 100.0f);
    EXPECT_FLOAT_EQ(bounds.minY.value, 0.0f);
    EXPECT_FLOAT_EQ(bounds.maxY.value, 100.0f);
}

TEST_F(WorldBounds2DTest, ToWorldBounds) {
    WorldBounds worldBounds = standardBounds.toWorldBounds(20_m, -10_m);
    
    EXPECT_FLOAT_EQ(worldBounds.upLimit().value, 20.0f);
    EXPECT_FLOAT_EQ(worldBounds.downLimit().value, -10.0f);
    EXPECT_FLOAT_EQ(worldBounds.westLimit().value, -100.0f);
    EXPECT_FLOAT_EQ(worldBounds.eastLimit().value, 100.0f);
}

TEST_F(WorldBounds2DTest, ToWorldBoundsFlat) {
    WorldBounds worldBounds = standardBounds.toWorldBounds();
    EXPECT_TRUE(worldBounds.is2D());
}

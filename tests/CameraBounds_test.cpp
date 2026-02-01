/**
 * @file CameraBounds_test.cpp
 * @brief Unit tests for CameraBounds.h (Phase 2.5)
 * 
 * Tests Pixels, ScreenSize, PixelToWorldMapping, and CameraBounds2D classes.
 */

#include <gtest/gtest.h>
#include <vde/api/CameraBounds.h>
#include <cmath>

using namespace vde;

// ============================================================================
// Pixels Tests
// ============================================================================

class PixelsTest : public ::testing::Test {};

TEST_F(PixelsTest, DefaultConstructor) {
    Pixels px;
    EXPECT_FLOAT_EQ(px.value, 0.0f);
}

TEST_F(PixelsTest, FloatConstructor) {
    Pixels px(1920.0f);
    EXPECT_FLOAT_EQ(px.value, 1920.0f);
}

TEST_F(PixelsTest, IntConstructor) {
    Pixels px(1080);
    EXPECT_FLOAT_EQ(px.value, 1080.0f);
}

TEST_F(PixelsTest, UserDefinedLiteralInteger) {
    Pixels px = 1920_px;
    EXPECT_FLOAT_EQ(px.value, 1920.0f);
}

TEST_F(PixelsTest, UserDefinedLiteralFloat) {
    Pixels px = 1920.5_px;
    EXPECT_FLOAT_EQ(px.value, 1920.5f);
}

TEST_F(PixelsTest, ImplicitConversionToFloat) {
    Pixels px(100.0f);
    float f = px;
    EXPECT_FLOAT_EQ(f, 100.0f);
}

TEST_F(PixelsTest, Negation) {
    Pixels px = 100_px;
    Pixels neg = -px;
    EXPECT_FLOAT_EQ(neg.value, -100.0f);
}

TEST_F(PixelsTest, Addition) {
    Pixels a = 100_px;
    Pixels b = 50_px;
    Pixels result = a + b;
    EXPECT_FLOAT_EQ(result.value, 150.0f);
}

TEST_F(PixelsTest, Subtraction) {
    Pixels a = 100_px;
    Pixels b = 30_px;
    Pixels result = a - b;
    EXPECT_FLOAT_EQ(result.value, 70.0f);
}

TEST_F(PixelsTest, MultiplicationByScalar) {
    Pixels px = 100_px;
    Pixels result = px * 2.5f;
    EXPECT_FLOAT_EQ(result.value, 250.0f);
}

TEST_F(PixelsTest, DivisionByScalar) {
    Pixels px = 100_px;
    Pixels result = px / 4.0f;
    EXPECT_FLOAT_EQ(result.value, 25.0f);
}

// ============================================================================
// ScreenSize Tests
// ============================================================================

class ScreenSizeTest : public ::testing::Test {};

TEST_F(ScreenSizeTest, DefaultConstructor) {
    ScreenSize size;
    EXPECT_FLOAT_EQ(size.width.value, 1920.0f);
    EXPECT_FLOAT_EQ(size.height.value, 1080.0f);
}

TEST_F(ScreenSizeTest, PixelsConstructor) {
    ScreenSize size(1280_px, 720_px);
    EXPECT_FLOAT_EQ(size.width.value, 1280.0f);
    EXPECT_FLOAT_EQ(size.height.value, 720.0f);
}

TEST_F(ScreenSizeTest, UintConstructor) {
    ScreenSize size(1920u, 1080u);
    EXPECT_FLOAT_EQ(size.width.value, 1920.0f);
    EXPECT_FLOAT_EQ(size.height.value, 1080.0f);
}

TEST_F(ScreenSizeTest, AspectRatio16by9) {
    ScreenSize size(1920_px, 1080_px);
    EXPECT_NEAR(size.aspectRatio(), 16.0f / 9.0f, 0.001f);
}

TEST_F(ScreenSizeTest, AspectRatio4by3) {
    ScreenSize size(1024_px, 768_px);
    EXPECT_NEAR(size.aspectRatio(), 4.0f / 3.0f, 0.001f);
}

// ============================================================================
// PixelToWorldMapping Tests
// ============================================================================

class PixelToWorldMappingTest : public ::testing::Test {};

TEST_F(PixelToWorldMappingTest, DefaultConstructor) {
    PixelToWorldMapping mapping;
    // Default: 100 pixels = 1 meter
    EXPECT_NEAR(mapping.getPixelsPerMeter(), 100.0f, 0.001f);
}

TEST_F(PixelToWorldMappingTest, FromPixelsPerMeter) {
    PixelToWorldMapping mapping = PixelToWorldMapping::fromPixelsPerMeter(50.0f);
    EXPECT_NEAR(mapping.getPixelsPerMeter(), 50.0f, 0.001f);
}

TEST_F(PixelToWorldMappingTest, FitWidth) {
    // Fit 20 meters across 1920 pixels
    PixelToWorldMapping mapping = PixelToWorldMapping::fitWidth(20_m, 1920_px);
    EXPECT_NEAR(mapping.getPixelsPerMeter(), 96.0f, 0.001f);  // 1920/20
}

TEST_F(PixelToWorldMappingTest, FitHeight) {
    // Fit 10 meters across 1080 pixels
    PixelToWorldMapping mapping = PixelToWorldMapping::fitHeight(10_m, 1080_px);
    EXPECT_NEAR(mapping.getPixelsPerMeter(), 108.0f, 0.001f);  // 1080/10
}

TEST_F(PixelToWorldMappingTest, ToWorldMeters) {
    PixelToWorldMapping mapping = PixelToWorldMapping::fromPixelsPerMeter(100.0f);
    Meters result = mapping.toWorld(500_px);
    EXPECT_FLOAT_EQ(result.value, 5.0f);
}

TEST_F(PixelToWorldMappingTest, ToPixels) {
    PixelToWorldMapping mapping = PixelToWorldMapping::fromPixelsPerMeter(100.0f);
    Pixels result = mapping.toPixels(10_m);
    EXPECT_FLOAT_EQ(result.value, 1000.0f);
}

TEST_F(PixelToWorldMappingTest, ToWorldVec2) {
    PixelToWorldMapping mapping = PixelToWorldMapping::fromPixelsPerMeter(100.0f);
    glm::vec2 result = mapping.toWorld(glm::vec2(500.0f, 300.0f));
    EXPECT_FLOAT_EQ(result.x, 5.0f);
    EXPECT_FLOAT_EQ(result.y, 3.0f);
}

TEST_F(PixelToWorldMappingTest, ToPixelsVec2) {
    PixelToWorldMapping mapping = PixelToWorldMapping::fromPixelsPerMeter(100.0f);
    glm::vec2 result = mapping.toPixels(glm::vec2(5.0f, 3.0f));
    EXPECT_FLOAT_EQ(result.x, 500.0f);
    EXPECT_FLOAT_EQ(result.y, 300.0f);
}

// ============================================================================
// CameraBounds2D Tests
// ============================================================================

class CameraBounds2DTest : public ::testing::Test {
protected:
    CameraBounds2D camera;
    
    void SetUp() override {
        camera.setScreenSize(1920_px, 1080_px);
        camera.setWorldWidth(16_m);
        camera.centerOn(0_m, 0_m);
    }
};

TEST_F(CameraBounds2DTest, DefaultConstructor) {
    CameraBounds2D cam;
    EXPECT_FLOAT_EQ(cam.getZoom(), 1.0f);
}

TEST_F(CameraBounds2DTest, SetScreenSize) {
    camera.setScreenSize(1280_px, 720_px);
    ScreenSize size = camera.getScreenSize();
    EXPECT_FLOAT_EQ(size.width.value, 1280.0f);
    EXPECT_FLOAT_EQ(size.height.value, 720.0f);
}

TEST_F(CameraBounds2DTest, SetWorldWidth) {
    camera.setWorldWidth(32_m);
    EXPECT_FLOAT_EQ(camera.getVisibleWidth().value, 32.0f);
}

TEST_F(CameraBounds2DTest, SetWorldHeight) {
    // 1920x1080 = 16:9 aspect ratio
    camera.setWorldHeight(9_m);
    // Width should be 9 * (16/9) = 16
    EXPECT_NEAR(camera.getVisibleWidth().value, 16.0f, 0.001f);
}

TEST_F(CameraBounds2DTest, GetVisibleWidth) {
    EXPECT_FLOAT_EQ(camera.getVisibleWidth().value, 16.0f);
}

TEST_F(CameraBounds2DTest, GetVisibleHeight) {
    // 16m width / (1920/1080) aspect = 9m height
    EXPECT_NEAR(camera.getVisibleHeight().value, 9.0f, 0.001f);
}

TEST_F(CameraBounds2DTest, SetZoom) {
    camera.setZoom(2.0f);
    EXPECT_FLOAT_EQ(camera.getZoom(), 2.0f);
    EXPECT_FLOAT_EQ(camera.getVisibleWidth().value, 8.0f);  // 16 / 2
}

TEST_F(CameraBounds2DTest, ZoomClampsToPositive) {
    camera.setZoom(-1.0f);
    EXPECT_GT(camera.getZoom(), 0.0f);
}

TEST_F(CameraBounds2DTest, CenterOn) {
    camera.centerOn(10_m, 5_m);
    glm::vec2 center = camera.getCenter();
    EXPECT_FLOAT_EQ(center.x, 10.0f);
    EXPECT_FLOAT_EQ(center.y, 5.0f);
}

TEST_F(CameraBounds2DTest, CenterOnVec2) {
    camera.centerOn(glm::vec2(10.0f, 5.0f));
    glm::vec2 center = camera.getCenter();
    EXPECT_FLOAT_EQ(center.x, 10.0f);
    EXPECT_FLOAT_EQ(center.y, 5.0f);
}

TEST_F(CameraBounds2DTest, Move) {
    camera.centerOn(0_m, 0_m);
    camera.move(5_m, 3_m);
    glm::vec2 center = camera.getCenter();
    EXPECT_FLOAT_EQ(center.x, 5.0f);
    EXPECT_FLOAT_EQ(center.y, 3.0f);
}

TEST_F(CameraBounds2DTest, GetVisibleBounds) {
    camera.centerOn(0_m, 0_m);
    WorldBounds2D visible = camera.getVisibleBounds();
    
    // Camera centered at origin with 16m width, 9m height
    EXPECT_FLOAT_EQ(visible.minX.value, -8.0f);
    EXPECT_FLOAT_EQ(visible.maxX.value, 8.0f);
    EXPECT_NEAR(visible.minY.value, -4.5f, 0.001f);
    EXPECT_NEAR(visible.maxY.value, 4.5f, 0.001f);
}

TEST_F(CameraBounds2DTest, GetVisibleBoundsOffset) {
    camera.centerOn(10_m, 5_m);
    WorldBounds2D visible = camera.getVisibleBounds();
    
    EXPECT_FLOAT_EQ(visible.minX.value, 2.0f);
    EXPECT_FLOAT_EQ(visible.maxX.value, 18.0f);
}

TEST_F(CameraBounds2DTest, ScreenToWorldCenter) {
    // Screen center (960, 540) should map to world center (0, 0)
    glm::vec2 world = camera.screenToWorld(960_px, 540_px);
    EXPECT_NEAR(world.x, 0.0f, 0.01f);
    EXPECT_NEAR(world.y, 0.0f, 0.01f);
}

TEST_F(CameraBounds2DTest, ScreenToWorldTopLeft) {
    // Screen top-left (0, 0) should map to world top-left (-8, 4.5)
    glm::vec2 world = camera.screenToWorld(0_px, 0_px);
    EXPECT_NEAR(world.x, -8.0f, 0.01f);
    EXPECT_NEAR(world.y, 4.5f, 0.01f);  // Top of screen = max Y
}

TEST_F(CameraBounds2DTest, ScreenToWorldBottomRight) {
    // Screen bottom-right (1920, 1080) should map to world bottom-right (8, -4.5)
    glm::vec2 world = camera.screenToWorld(1920_px, 1080_px);
    EXPECT_NEAR(world.x, 8.0f, 0.01f);
    EXPECT_NEAR(world.y, -4.5f, 0.01f);  // Bottom of screen = min Y
}

TEST_F(CameraBounds2DTest, WorldToScreenCenter) {
    // World center (0, 0) should map to screen center (960, 540)
    glm::vec2 screen = camera.worldToScreen(0_m, 0_m);
    EXPECT_NEAR(screen.x, 960.0f, 0.1f);
    EXPECT_NEAR(screen.y, 540.0f, 0.1f);
}

TEST_F(CameraBounds2DTest, WorldToScreenTopLeft) {
    // World top-left (-8, 4.5) should map to screen top-left (0, 0)
    glm::vec2 screen = camera.worldToScreen(-8_m, Meters(4.5f));
    EXPECT_NEAR(screen.x, 0.0f, 0.1f);
    EXPECT_NEAR(screen.y, 0.0f, 0.1f);
}

TEST_F(CameraBounds2DTest, IsVisiblePointInside) {
    EXPECT_TRUE(camera.isVisible(0_m, 0_m));
    EXPECT_TRUE(camera.isVisible(5_m, 2_m));
}

TEST_F(CameraBounds2DTest, IsVisiblePointOutside) {
    EXPECT_FALSE(camera.isVisible(100_m, 0_m));
    EXPECT_FALSE(camera.isVisible(0_m, 100_m));
}

TEST_F(CameraBounds2DTest, IsVisibleBoundsInside) {
    WorldBounds2D inside = WorldBounds2D::fromCenter(0_m, 0_m, 4_m, 4_m);
    EXPECT_TRUE(camera.isVisible(inside));
}

TEST_F(CameraBounds2DTest, IsVisibleBoundsPartialOverlap) {
    WorldBounds2D partial = WorldBounds2D::fromCenter(10_m, 0_m, 10_m, 4_m);
    EXPECT_TRUE(camera.isVisible(partial));  // Overlaps right edge
}

TEST_F(CameraBounds2DTest, IsVisibleBoundsOutside) {
    WorldBounds2D outside = WorldBounds2D::fromCenter(100_m, 100_m, 4_m, 4_m);
    EXPECT_FALSE(camera.isVisible(outside));
}

TEST_F(CameraBounds2DTest, SetConstraintBounds) {
    WorldBounds2D constraints = WorldBounds2D::fromCenter(0_m, 0_m, 100_m, 100_m);
    camera.setConstraintBounds(constraints);
    EXPECT_TRUE(camera.hasConstraintBounds());
}

TEST_F(CameraBounds2DTest, ClearConstraintBounds) {
    WorldBounds2D constraints = WorldBounds2D::fromCenter(0_m, 0_m, 100_m, 100_m);
    camera.setConstraintBounds(constraints);
    camera.clearConstraintBounds();
    EXPECT_FALSE(camera.hasConstraintBounds());
}

TEST_F(CameraBounds2DTest, ConstraintsPreventOutOfBounds) {
    // Set tight constraints: 20x20 meter area
    WorldBounds2D constraints = WorldBounds2D::fromCenter(0_m, 0_m, 20_m, 20_m);
    camera.setConstraintBounds(constraints);
    
    // Try to move camera way out
    camera.centerOn(100_m, 100_m);
    
    // Camera should be clamped within constraints
    glm::vec2 center = camera.getCenter();
    WorldBounds2D visible = camera.getVisibleBounds();
    
    // Visible area should stay within constraints
    EXPECT_LE(visible.maxX.value, 10.0f + 0.1f);
    EXPECT_LE(visible.maxY.value, 10.0f + 0.1f);
}

TEST_F(CameraBounds2DTest, GetMapping) {
    PixelToWorldMapping mapping = camera.getMapping();
    EXPECT_GT(mapping.getPixelsPerMeter(), 0.0f);
}

/**
 * @file ViewportRect_test.cpp
 * @brief Unit tests for ViewportRect
 */

#include <vde/api/ViewportRect.h>

#include <gtest/gtest.h>

namespace vde::test {

// ============================================================================
// Default Construction
// ============================================================================

TEST(ViewportRectTest, DefaultIsFullWindow) {
    ViewportRect rect;
    EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_FLOAT_EQ(rect.width, 1.0f);
    EXPECT_FLOAT_EQ(rect.height, 1.0f);
}

TEST(ViewportRectTest, DefaultEqualsFullWindow) {
    ViewportRect def;
    ViewportRect full = ViewportRect::fullWindow();
    EXPECT_EQ(def, full);
}

// ============================================================================
// Static Factory Methods
// ============================================================================

TEST(ViewportRectTest, FullWindowFactory) {
    auto rect = ViewportRect::fullWindow();
    EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_FLOAT_EQ(rect.width, 1.0f);
    EXPECT_FLOAT_EQ(rect.height, 1.0f);
}

TEST(ViewportRectTest, TopLeftFactory) {
    auto rect = ViewportRect::topLeft();
    EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_FLOAT_EQ(rect.width, 0.5f);
    EXPECT_FLOAT_EQ(rect.height, 0.5f);
}

TEST(ViewportRectTest, TopRightFactory) {
    auto rect = ViewportRect::topRight();
    EXPECT_FLOAT_EQ(rect.x, 0.5f);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_FLOAT_EQ(rect.width, 0.5f);
    EXPECT_FLOAT_EQ(rect.height, 0.5f);
}

TEST(ViewportRectTest, BottomLeftFactory) {
    auto rect = ViewportRect::bottomLeft();
    EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_FLOAT_EQ(rect.y, 0.5f);
    EXPECT_FLOAT_EQ(rect.width, 0.5f);
    EXPECT_FLOAT_EQ(rect.height, 0.5f);
}

TEST(ViewportRectTest, BottomRightFactory) {
    auto rect = ViewportRect::bottomRight();
    EXPECT_FLOAT_EQ(rect.x, 0.5f);
    EXPECT_FLOAT_EQ(rect.y, 0.5f);
    EXPECT_FLOAT_EQ(rect.width, 0.5f);
    EXPECT_FLOAT_EQ(rect.height, 0.5f);
}

TEST(ViewportRectTest, LeftHalfFactory) {
    auto rect = ViewportRect::leftHalf();
    EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_FLOAT_EQ(rect.width, 0.5f);
    EXPECT_FLOAT_EQ(rect.height, 1.0f);
}

TEST(ViewportRectTest, RightHalfFactory) {
    auto rect = ViewportRect::rightHalf();
    EXPECT_FLOAT_EQ(rect.x, 0.5f);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_FLOAT_EQ(rect.width, 0.5f);
    EXPECT_FLOAT_EQ(rect.height, 1.0f);
}

TEST(ViewportRectTest, TopHalfFactory) {
    auto rect = ViewportRect::topHalf();
    EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_FLOAT_EQ(rect.width, 1.0f);
    EXPECT_FLOAT_EQ(rect.height, 0.5f);
}

TEST(ViewportRectTest, BottomHalfFactory) {
    auto rect = ViewportRect::bottomHalf();
    EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_FLOAT_EQ(rect.y, 0.5f);
    EXPECT_FLOAT_EQ(rect.width, 1.0f);
    EXPECT_FLOAT_EQ(rect.height, 0.5f);
}

// ============================================================================
// Quad Layout — four quadrants tile the full window with no overlap
// ============================================================================

TEST(ViewportRectTest, QuadrantsAreNonOverlapping) {
    auto tl = ViewportRect::topLeft();
    auto tr = ViewportRect::topRight();
    auto bl = ViewportRect::bottomLeft();
    auto br = ViewportRect::bottomRight();

    // Top-left and top-right share the top edge, split at x=0.5
    EXPECT_FLOAT_EQ(tl.x + tl.width, tr.x);
    EXPECT_FLOAT_EQ(tl.y, tr.y);

    // Top-left and bottom-left share the left edge, split at y=0.5
    EXPECT_FLOAT_EQ(tl.y + tl.height, bl.y);
    EXPECT_FLOAT_EQ(tl.x, bl.x);

    // Bottom-right abuts both bottom-left and top-right
    EXPECT_FLOAT_EQ(bl.x + bl.width, br.x);
    EXPECT_FLOAT_EQ(tr.y + tr.height, br.y);
}

TEST(ViewportRectTest, QuadrantsCoverFullWindow) {
    auto tl = ViewportRect::topLeft();
    auto br = ViewportRect::bottomRight();

    // Top-left starts at (0,0)
    EXPECT_FLOAT_EQ(tl.x, 0.0f);
    EXPECT_FLOAT_EQ(tl.y, 0.0f);

    // Bottom-right ends at (1,1)
    EXPECT_FLOAT_EQ(br.x + br.width, 1.0f);
    EXPECT_FLOAT_EQ(br.y + br.height, 1.0f);
}

// ============================================================================
// Contains (Hit Test)
// ============================================================================

TEST(ViewportRectTest, ContainsInterior) {
    auto rect = ViewportRect::topLeft();  // {0, 0, 0.5, 0.5}
    EXPECT_TRUE(rect.contains(0.25f, 0.25f));
}

TEST(ViewportRectTest, ContainsTopLeftCorner) {
    auto rect = ViewportRect::topLeft();
    EXPECT_TRUE(rect.contains(0.0f, 0.0f));
}

TEST(ViewportRectTest, ContainsBottomRightEdge) {
    auto rect = ViewportRect::topLeft();  // {0, 0, 0.5, 0.5}
    EXPECT_TRUE(rect.contains(0.5f, 0.5f));
}

TEST(ViewportRectTest, ContainsExterior) {
    auto rect = ViewportRect::topLeft();  // {0, 0, 0.5, 0.5}
    EXPECT_FALSE(rect.contains(0.6f, 0.6f));
    EXPECT_FALSE(rect.contains(0.51f, 0.25f));
    EXPECT_FALSE(rect.contains(0.25f, 0.51f));
}

TEST(ViewportRectTest, ContainsFullWindowAlwaysTrue) {
    auto full = ViewportRect::fullWindow();
    EXPECT_TRUE(full.contains(0.0f, 0.0f));
    EXPECT_TRUE(full.contains(0.5f, 0.5f));
    EXPECT_TRUE(full.contains(1.0f, 1.0f));
    EXPECT_TRUE(full.contains(0.99f, 0.01f));
}

TEST(ViewportRectTest, ContainsOutsideWindow) {
    auto full = ViewportRect::fullWindow();
    EXPECT_FALSE(full.contains(-0.01f, 0.5f));
    EXPECT_FALSE(full.contains(0.5f, 1.01f));
}

// ============================================================================
// toVkViewport
// ============================================================================

TEST(ViewportRectTest, ToVkViewportFullWindow) {
    auto rect = ViewportRect::fullWindow();
    VkViewport vp = rect.toVkViewport(1920, 1080);

    EXPECT_FLOAT_EQ(vp.x, 0.0f);
    EXPECT_FLOAT_EQ(vp.y, 0.0f);
    EXPECT_FLOAT_EQ(vp.width, 1920.0f);
    EXPECT_FLOAT_EQ(vp.height, 1080.0f);
    EXPECT_FLOAT_EQ(vp.minDepth, 0.0f);
    EXPECT_FLOAT_EQ(vp.maxDepth, 1.0f);
}

TEST(ViewportRectTest, ToVkViewportTopRight) {
    auto rect = ViewportRect::topRight();
    VkViewport vp = rect.toVkViewport(1280, 720);

    EXPECT_FLOAT_EQ(vp.x, 640.0f);
    EXPECT_FLOAT_EQ(vp.y, 0.0f);
    EXPECT_FLOAT_EQ(vp.width, 640.0f);
    EXPECT_FLOAT_EQ(vp.height, 360.0f);
}

TEST(ViewportRectTest, ToVkViewportSmallWindow) {
    auto rect = ViewportRect::bottomLeft();
    VkViewport vp = rect.toVkViewport(800, 600);

    EXPECT_FLOAT_EQ(vp.x, 0.0f);
    EXPECT_FLOAT_EQ(vp.y, 300.0f);
    EXPECT_FLOAT_EQ(vp.width, 400.0f);
    EXPECT_FLOAT_EQ(vp.height, 300.0f);
}

// ============================================================================
// toVkScissor
// ============================================================================

TEST(ViewportRectTest, ToVkScissorFullWindow) {
    auto rect = ViewportRect::fullWindow();
    VkRect2D sc = rect.toVkScissor(1920, 1080);

    EXPECT_EQ(sc.offset.x, 0);
    EXPECT_EQ(sc.offset.y, 0);
    EXPECT_EQ(sc.extent.width, 1920u);
    EXPECT_EQ(sc.extent.height, 1080u);
}

TEST(ViewportRectTest, ToVkScissorBottomRight) {
    auto rect = ViewportRect::bottomRight();
    VkRect2D sc = rect.toVkScissor(1280, 720);

    EXPECT_EQ(sc.offset.x, 640);
    EXPECT_EQ(sc.offset.y, 360);
    EXPECT_EQ(sc.extent.width, 640u);
    EXPECT_EQ(sc.extent.height, 360u);
}

// ============================================================================
// Aspect Ratio
// ============================================================================

TEST(ViewportRectTest, AspectRatioFullWindow16_9) {
    auto rect = ViewportRect::fullWindow();
    float aspect = rect.getAspectRatio(1920, 1080);
    EXPECT_NEAR(aspect, 16.0f / 9.0f, 0.01f);
}

TEST(ViewportRectTest, AspectRatioQuadrant) {
    auto rect = ViewportRect::topLeft();
    // Half of 1920x1080 = 960x540 → still 16:9
    float aspect = rect.getAspectRatio(1920, 1080);
    EXPECT_NEAR(aspect, 16.0f / 9.0f, 0.01f);
}

TEST(ViewportRectTest, AspectRatioLeftHalf) {
    auto rect = ViewportRect::leftHalf();
    // Half width of 1280x720 = 640x720
    float aspect = rect.getAspectRatio(1280, 720);
    EXPECT_NEAR(aspect, 640.0f / 720.0f, 0.01f);
}

// ============================================================================
// Equality
// ============================================================================

TEST(ViewportRectTest, EqualityOperator) {
    ViewportRect a{0.0f, 0.0f, 1.0f, 1.0f};
    ViewportRect b{0.0f, 0.0f, 1.0f, 1.0f};
    ViewportRect c{0.5f, 0.0f, 0.5f, 0.5f};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

}  // namespace vde::test

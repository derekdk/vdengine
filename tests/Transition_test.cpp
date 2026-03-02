/**
 * @file Transition_test.cpp
 * @brief Unit tests for vde::Transition base class and built-in transitions.
 *
 * Tests verify the API contract, default behaviour, shader path correctness,
 * direction handling, and update() uniform outputs — all without requiring
 * a Vulkan device.
 */

#include <vde/api/Transition.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

// =========================================================================
// Base class defaults
// =========================================================================

/// Minimal concrete subclass to test base class defaults.
class StubTransition : public Transition {
  public:
    const char* getName() const override { return "Stub"; }
    std::string getFragmentShaderPath() const override { return "stub.frag"; }
};

class TransitionBaseTest : public ::testing::Test {
  protected:
    StubTransition transition;
};

TEST_F(TransitionBaseTest, DefaultVertexShaderPath) {
    EXPECT_EQ(transition.getVertexShaderPath(), "transition_fullscreen.vert");
}

TEST_F(TransitionBaseTest, DefaultDirectionIsCenter) {
    EXPECT_EQ(transition.getDirection(), TransitionDirection::Center);
}

TEST_F(TransitionBaseTest, SetDirectionRoundTrips) {
    transition.setDirection(TransitionDirection::Left);
    EXPECT_EQ(transition.getDirection(), TransitionDirection::Left);
    transition.setDirection(TransitionDirection::Up);
    EXPECT_EQ(transition.getDirection(), TransitionDirection::Up);
}

TEST_F(TransitionBaseTest, DefaultUpdateSetsProgress) {
    TransitionUpdateContext ctx{};
    ctx.progress = 0.42f;
    ctx.deltaTime = 0.016f;
    ctx.elapsed = 0.5f;
    ctx.duration = 1.0f;
    ctx.frameWidth = 1920;
    ctx.frameHeight = 1080;

    TransitionUniforms uniforms{};
    transition.update(ctx, uniforms);

    EXPECT_FLOAT_EQ(uniforms.progress, 0.42f);
}

TEST_F(TransitionBaseTest, UsesCustomGeometryIsFalseByDefault) {
    EXPECT_FALSE(transition.usesCustomGeometry());
}

TEST_F(TransitionBaseTest, OnStartAndOnCompleteDoNotThrow) {
    EXPECT_NO_THROW(transition.onStart());
    EXPECT_NO_THROW(transition.onComplete());
}

// =========================================================================
// FadeTransition
// =========================================================================

class FadeTransitionTest : public ::testing::Test {
  protected:
    FadeTransition fade;
};

TEST_F(FadeTransitionTest, NameIsFade) {
    EXPECT_STREQ(fade.getName(), "Fade");
}

TEST_F(FadeTransitionTest, FragmentShaderPath) {
    EXPECT_EQ(fade.getFragmentShaderPath(), "transition_fade.frag");
}

TEST_F(FadeTransitionTest, UsesFadeUniformsAtMidPoint) {
    TransitionUpdateContext ctx{};
    ctx.progress = 0.5f;
    ctx.deltaTime = 0.016f;
    ctx.elapsed = 0.5f;
    ctx.duration = 1.0f;
    ctx.frameWidth = 800;
    ctx.frameHeight = 600;

    TransitionUniforms uniforms{};
    fade.update(ctx, uniforms);

    EXPECT_FLOAT_EQ(uniforms.progress, 0.5f);
}

TEST_F(FadeTransitionTest, UsesFadeUniformsAtStart) {
    TransitionUpdateContext ctx{};
    ctx.progress = 0.0f;
    TransitionUniforms uniforms{};
    fade.update(ctx, uniforms);
    EXPECT_FLOAT_EQ(uniforms.progress, 0.0f);
}

TEST_F(FadeTransitionTest, UsesFadeUniformsAtEnd) {
    TransitionUpdateContext ctx{};
    ctx.progress = 1.0f;
    TransitionUniforms uniforms{};
    fade.update(ctx, uniforms);
    EXPECT_FLOAT_EQ(uniforms.progress, 1.0f);
}

// =========================================================================
// WipeTransition
// =========================================================================

class WipeTransitionTest : public ::testing::Test {
  protected:
    WipeTransition wipeLeft{TransitionDirection::Left};
    WipeTransition wipeRight{TransitionDirection::Right};
    WipeTransition wipeUp{TransitionDirection::Up};
    WipeTransition wipeDown{TransitionDirection::Down};
};

TEST_F(WipeTransitionTest, NameIsWipe) {
    EXPECT_STREQ(wipeLeft.getName(), "Wipe");
}

TEST_F(WipeTransitionTest, FragmentShaderPath) {
    EXPECT_EQ(wipeLeft.getFragmentShaderPath(), "transition_wipe.frag");
}

TEST_F(WipeTransitionTest, DefaultConstructorDirectionIsLeft) {
    WipeTransition wipe;
    EXPECT_EQ(wipe.getDirection(), TransitionDirection::Left);
}

TEST_F(WipeTransitionTest, DirectionEncodedInUniforms) {
    TransitionUpdateContext ctx{};
    ctx.progress = 0.5f;
    ctx.frameWidth = 100;
    ctx.frameHeight = 100;

    TransitionUniforms uniforms{};

    wipeLeft.update(ctx, uniforms);
    EXPECT_FLOAT_EQ(uniforms.direction, static_cast<float>(TransitionDirection::Left));

    wipeRight.update(ctx, uniforms);
    EXPECT_FLOAT_EQ(uniforms.direction, static_cast<float>(TransitionDirection::Right));

    wipeUp.update(ctx, uniforms);
    EXPECT_FLOAT_EQ(uniforms.direction, static_cast<float>(TransitionDirection::Up));

    wipeDown.update(ctx, uniforms);
    EXPECT_FLOAT_EQ(uniforms.direction, static_cast<float>(TransitionDirection::Down));
}

TEST_F(WipeTransitionTest, ProgressPassedThrough) {
    TransitionUpdateContext ctx{};
    ctx.progress = 0.75f;
    ctx.frameWidth = 100;
    ctx.frameHeight = 100;

    TransitionUniforms uniforms{};
    wipeLeft.update(ctx, uniforms);
    EXPECT_FLOAT_EQ(uniforms.progress, 0.75f);
}

// =========================================================================
// CircleRevealTransition
// =========================================================================

class CircleRevealTransitionTest : public ::testing::Test {
  protected:
    CircleRevealTransition circle;
};

TEST_F(CircleRevealTransitionTest, NameIsCircleReveal) {
    EXPECT_STREQ(circle.getName(), "CircleReveal");
}

TEST_F(CircleRevealTransitionTest, FragmentShaderPath) {
    EXPECT_EQ(circle.getFragmentShaderPath(), "transition_circle_reveal.frag");
}

TEST_F(CircleRevealTransitionTest, DirectionDefaultsToCenter) {
    EXPECT_EQ(circle.getDirection(), TransitionDirection::Center);
}

TEST_F(CircleRevealTransitionTest, UpdateEncodesAspectRatio) {
    TransitionUpdateContext ctx{};
    ctx.progress = 0.5f;
    ctx.deltaTime = 0.016f;
    ctx.elapsed = 0.5f;
    ctx.duration = 1.0f;
    ctx.frameWidth = 1920;
    ctx.frameHeight = 1080;

    TransitionUniforms uniforms{};
    circle.update(ctx, uniforms);

    EXPECT_FLOAT_EQ(uniforms.progress, 0.5f);
    // param0 = aspect ratio
    float expectedAspect = 1920.0f / 1080.0f;
    EXPECT_NEAR(uniforms.param0, expectedAspect, 0.001f);
}

TEST_F(CircleRevealTransitionTest, ZeroHeightSetsAspectToOne) {
    TransitionUpdateContext ctx{};
    ctx.progress = 0.5f;
    ctx.frameWidth = 1920;
    ctx.frameHeight = 0;

    TransitionUniforms uniforms{};
    circle.update(ctx, uniforms);

    EXPECT_FLOAT_EQ(uniforms.param0, 1.0f);
}

TEST_F(CircleRevealTransitionTest, SquareFrameHasAspectOne) {
    TransitionUpdateContext ctx{};
    ctx.progress = 0.3f;
    ctx.frameWidth = 500;
    ctx.frameHeight = 500;

    TransitionUniforms uniforms{};
    circle.update(ctx, uniforms);

    EXPECT_FLOAT_EQ(uniforms.param0, 1.0f);
}

// =========================================================================
// BlockFallTransition
// =========================================================================

class BlockFallTransitionTest : public ::testing::Test {
  protected:
    BlockFallTransition blockFall;
};

TEST_F(BlockFallTransitionTest, NameIsBlockFall) {
    EXPECT_STREQ(blockFall.getName(), "BlockFall");
}

TEST_F(BlockFallTransitionTest, FragmentShaderPath) {
    EXPECT_EQ(blockFall.getFragmentShaderPath(), "transition_block_fall.frag");
}

TEST_F(BlockFallTransitionTest, DefaultConstructorUses32PixelBlocksAndZeroSeed) {
    EXPECT_FLOAT_EQ(blockFall.getBlockSizePixels(), 32.0f);
    EXPECT_FLOAT_EQ(blockFall.getRandomSeed(), 0.0f);
}

TEST(BlockFallTransitionStandaloneTest, UpdateEncodesNormalizedBlockDimensions) {
    BlockFallTransition transition{32.0f, 0.25f};

    TransitionUpdateContext ctx{};
    ctx.progress = 0.5f;
    ctx.frameWidth = 640;
    ctx.frameHeight = 480;

    TransitionUniforms uniforms{};
    transition.update(ctx, uniforms);

    EXPECT_FLOAT_EQ(uniforms.progress, 0.5f);
    EXPECT_FLOAT_EQ(uniforms.direction, 0.25f);
    EXPECT_NEAR(uniforms.param0, 32.0f / 640.0f, 0.00001f);
    EXPECT_NEAR(uniforms.param1, 32.0f / 480.0f, 0.00001f);
}

// =========================================================================
// TransitionDirection enum coverage
// =========================================================================

TEST(TransitionDirectionTest, AllValuesAreDistinct) {
    EXPECT_NE(static_cast<uint8_t>(TransitionDirection::Left),
              static_cast<uint8_t>(TransitionDirection::Right));
    EXPECT_NE(static_cast<uint8_t>(TransitionDirection::Up),
              static_cast<uint8_t>(TransitionDirection::Down));
    EXPECT_NE(static_cast<uint8_t>(TransitionDirection::Left),
              static_cast<uint8_t>(TransitionDirection::Center));
}

// =========================================================================
// TransitionUniforms defaults
// =========================================================================

TEST(TransitionUniformsTest, DefaultsAreZero) {
    TransitionUniforms u{};
    EXPECT_FLOAT_EQ(u.progress, 0.0f);
    EXPECT_FLOAT_EQ(u.direction, 0.0f);
    EXPECT_FLOAT_EQ(u.param0, 0.0f);
    EXPECT_FLOAT_EQ(u.param1, 0.0f);
}

TEST(TransitionUniformsTest, SizeIsFourFloats) {
    EXPECT_EQ(sizeof(TransitionUniforms), 4 * sizeof(float));
}

}  // namespace test
}  // namespace vde

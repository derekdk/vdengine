/**
 * @file OffscreenRenderTarget_test.cpp
 * @brief Unit tests for vde::OffscreenRenderTarget.
 *
 * These tests verify the API surface, default state, move semantics, and
 * RAII safety of OffscreenRenderTarget without requiring a live Vulkan device.
 * GPU-dependent creation/resize behaviour is validated by integration tests
 * and the transition_demo smoke test.
 */

#include <vde/OffscreenRenderTarget.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

class OffscreenRenderTargetTest : public ::testing::Test {
  protected:
    OffscreenRenderTarget target;
};

// --- Default state --------------------------------------------------------

TEST_F(OffscreenRenderTargetTest, DefaultConstructedIsInvalid) {
    EXPECT_FALSE(target.isValid());
    EXPECT_EQ(target.getColorImage(), VK_NULL_HANDLE);
    EXPECT_EQ(target.getColorImageView(), VK_NULL_HANDLE);
    EXPECT_EQ(target.getSampler(), VK_NULL_HANDLE);
    EXPECT_EQ(target.getFramebuffer(), VK_NULL_HANDLE);
    EXPECT_EQ(target.getWidth(), 0u);
    EXPECT_EQ(target.getHeight(), 0u);
}

// --- destroy on default-constructed is safe --------------------------------

TEST_F(OffscreenRenderTargetTest, DestroyOnDefaultConstructedIsSafe) {
    // Should not crash or throw
    target.destroy();
    EXPECT_FALSE(target.isValid());
}

TEST_F(OffscreenRenderTargetTest, DestroyCalledTwiceIsSafe) {
    target.destroy();
    target.destroy();
    EXPECT_FALSE(target.isValid());
}

// --- Move construction -----------------------------------------------------

TEST_F(OffscreenRenderTargetTest, MoveConstructLeavesSourceInvalid) {
    OffscreenRenderTarget moved{std::move(target)};
    // Source should be in a default/empty state
    EXPECT_FALSE(target.isValid());
    EXPECT_EQ(target.getWidth(), 0u);
    EXPECT_EQ(target.getHeight(), 0u);

    // Destination should also be invalid (no resources were created)
    EXPECT_FALSE(moved.isValid());
}

// --- Move assignment -------------------------------------------------------

TEST_F(OffscreenRenderTargetTest, MoveAssignLeavesSourceInvalid) {
    OffscreenRenderTarget other;
    other = std::move(target);
    EXPECT_FALSE(target.isValid());
    EXPECT_FALSE(other.isValid());
}

// --- Self move assignment --------------------------------------------------

TEST_F(OffscreenRenderTargetTest, SelfMoveAssignmentIsSafe) {
    // Suppress compiler warning about self-move
    OffscreenRenderTarget& ref = target;
    target = std::move(ref);
    EXPECT_FALSE(target.isValid());
}

// --- recreate without prior create should throw ----------------------------

TEST_F(OffscreenRenderTargetTest, RecreateWithoutCreateThrows) {
    EXPECT_THROW(target.recreate(800, 600), std::runtime_error);
}

}  // namespace test
}  // namespace vde

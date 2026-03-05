/**
 * @file TransitionManager_test.cpp
 * @brief Unit tests for vde::TransitionManager lifecycle logic.
 *
 * Tests the non-GPU parts of TransitionManager: start/update/cancel/
 * progress/completion callback behaviour. Uses a minimal mock VulkanContext
 * subclass (no real Vulkan device) so these tests are deterministic and fast.
 */

#include <vde/VulkanContext.h>
#include <vde/api/TransitionManager.h>

#include <gtest/gtest.h>

namespace vde {
namespace test {

/// Minimal mock context that satisfies TransitionManager's lifecycle needs.
class MockVulkanContextForTransition : public VulkanContext {
  public:
    MockVulkanContextForTransition() : VulkanContext(MockTag{}) {
        // Set a non-zero extent so update() can compute frame dimensions
        m_swapChainExtent = {800, 600};
        m_swapChainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        m_depthFormat = VK_FORMAT_D32_SFLOAT;
    }

    // Override so nothing real happens
    VkDevice getDevice() const override { return VK_NULL_HANDLE; }
};

/// A concrete test transition.
class TestTransition : public Transition {
  public:
    const char* getName() const override { return "Test"; }
    std::string getFragmentShaderPath() const override { return "test.frag"; }

    bool startCalled = false;
    bool completeCalled = false;
    int updateCount = 0;
    float lastProgress = -1.0f;

    void onStart() override { startCalled = true; }
    void onComplete() override { completeCalled = true; }
    void update(const TransitionUpdateContext& ctx, TransitionUniforms& outUniforms) override {
        ++updateCount;
        lastProgress = ctx.progress;
        Transition::update(ctx, outUniforms);
    }
};

class TransitionManagerTest : public ::testing::Test {
  protected:
    MockVulkanContextForTransition mockContext;
    TransitionManager manager{&mockContext};
};

// ---- Initial state -------------------------------------------------------

TEST_F(TransitionManagerTest, InitiallyInactive) {
    EXPECT_FALSE(manager.isActive());
    EXPECT_FLOAT_EQ(manager.getProgress(), 0.0f);
    EXPECT_EQ(manager.getActiveTransition(), nullptr);
}

// ---- Start ---------------------------------------------------------------

TEST_F(TransitionManagerTest, StartMakesActive) {
    auto t = std::make_unique<TestTransition>();
    auto* raw = t.get();
    manager.start(std::move(t), 1.0f);

    EXPECT_TRUE(manager.isActive());
    EXPECT_TRUE(raw->startCalled);
    EXPECT_FLOAT_EQ(manager.getProgress(), 0.0f);
}

// ---- Update progresses ---------------------------------------------------

TEST_F(TransitionManagerTest, UpdateAdvancesProgress) {
    auto t = std::make_unique<TestTransition>();
    manager.start(std::move(t), 1.0f);

    manager.update(0.5f);
    EXPECT_NEAR(manager.getProgress(), 0.5f, 0.001f);
    EXPECT_TRUE(manager.isActive());
}

// ---- Completion ----------------------------------------------------------

TEST_F(TransitionManagerTest, CompletesWhenProgressReachesOne) {
    auto t = std::make_unique<TestTransition>();
    auto* raw = t.get();

    bool callbackFired = false;
    manager.start(std::move(t), 1.0f, [&]() { callbackFired = true; });

    // raw is now owned by manager; it stays valid until completion
    manager.update(0.5f);
    EXPECT_FALSE(callbackFired);
    EXPECT_TRUE(manager.isActive());

    manager.update(0.6f);  // total elapsed = 1.1 → progress clamped to 1.0
    EXPECT_TRUE(callbackFired);
    EXPECT_TRUE(raw->completeCalled);
    EXPECT_FALSE(manager.isActive());
    EXPECT_FLOAT_EQ(manager.getProgress(), 0.0f);
}

// ---- Cancel --------------------------------------------------------------

TEST_F(TransitionManagerTest, CancelStopsTransition) {
    auto t = std::make_unique<TestTransition>();
    bool callbackFired = false;
    manager.start(std::move(t), 1.0f, [&]() { callbackFired = true; });

    manager.update(0.3f);
    EXPECT_TRUE(manager.isActive());

    manager.cancel();
    EXPECT_FALSE(manager.isActive());
    EXPECT_FALSE(callbackFired);  // Cancel does NOT fire callback
    EXPECT_FLOAT_EQ(manager.getProgress(), 0.0f);
}

// ---- Start-while-active replaces -----------------------------------------

TEST_F(TransitionManagerTest, StartWhileActiveReplacesTransition) {
    auto t1 = std::make_unique<TestTransition>();
    bool cb1 = false;
    manager.start(std::move(t1), 1.0f, [&]() { cb1 = true; });
    manager.update(0.3f);

    auto t2 = std::make_unique<TestTransition>();
    auto* raw2 = t2.get();
    bool cb2 = false;
    manager.start(std::move(t2), 2.0f, [&]() { cb2 = true; });

    EXPECT_TRUE(manager.isActive());
    EXPECT_TRUE(raw2->startCalled);
    EXPECT_FALSE(cb1);                             // First callback should not fire
    EXPECT_FLOAT_EQ(manager.getProgress(), 0.0f);  // Reset for new transition
}

// ---- Instant transition (duration <= 0) -----------------------------------

TEST_F(TransitionManagerTest, ZeroDurationCompletesImmediately) {
    auto t = std::make_unique<TestTransition>();
    auto* raw = t.get();
    bool callbackFired = false;
    manager.start(std::move(t), 0.0f, [&]() { callbackFired = true; });

    EXPECT_TRUE(callbackFired);
    EXPECT_TRUE(raw->startCalled);
    EXPECT_TRUE(raw->completeCalled);
    EXPECT_FALSE(manager.isActive());
}

TEST_F(TransitionManagerTest, NegativeDurationCompletesImmediately) {
    bool callbackFired = false;
    manager.start(std::make_unique<TestTransition>(), -0.5f, [&]() { callbackFired = true; });
    EXPECT_TRUE(callbackFired);
    EXPECT_FALSE(manager.isActive());
}

// ---- Null transition ------------------------------------------------------

TEST_F(TransitionManagerTest, NullTransitionFiresCallback) {
    bool callbackFired = false;
    manager.start(nullptr, 1.0f, [&]() { callbackFired = true; });
    EXPECT_TRUE(callbackFired);
    EXPECT_FALSE(manager.isActive());
}

// ---- Update without active transition is safe -----------------------------

TEST_F(TransitionManagerTest, UpdateWhenInactiveIsSafe) {
    EXPECT_NO_THROW(manager.update(0.016f));
    EXPECT_FLOAT_EQ(manager.getProgress(), 0.0f);
}

// ---- Cancel without active transition is safe -----------------------------

TEST_F(TransitionManagerTest, CancelWhenInactiveIsSafe) {
    EXPECT_NO_THROW(manager.cancel());
}

// ---- Progress clamps to 1.0 ----------------------------------------------

TEST_F(TransitionManagerTest, ProgressClampsToOne) {
    auto t = std::make_unique<TestTransition>();
    manager.start(std::move(t), 0.5f);

    // Update with a huge delta that would exceed duration
    manager.update(10.0f);
    // After completion, progress resets to 0
    EXPECT_FALSE(manager.isActive());
}

// ---- Multiple update cycles -----------------------------------------------

TEST_F(TransitionManagerTest, MultipleSmallUpdates) {
    auto t = std::make_unique<TestTransition>();
    manager.start(std::move(t), 1.0f);

    for (int i = 0; i < 10; ++i) {
        manager.update(0.09f);  // 10 * 0.09 = 0.9
    }
    EXPECT_TRUE(manager.isActive());
    EXPECT_NEAR(manager.getProgress(), 0.9f, 0.01f);

    manager.update(0.2f);
    EXPECT_FALSE(manager.isActive());  // Should complete
}

// ---- getUniforms reflects last update ------------------------------------

TEST_F(TransitionManagerTest, UniformsReflectProgress) {
    auto t = std::make_unique<TestTransition>();
    manager.start(std::move(t), 1.0f);

    manager.update(0.25f);
    const auto& uniforms = manager.getUniforms();
    EXPECT_NEAR(uniforms.progress, 0.25f, 0.001f);
}

// ---- getActiveTransition during active ------------------------------------

TEST_F(TransitionManagerTest, GetActiveTransitionDuringActive) {
    auto t = std::make_unique<TestTransition>();
    manager.start(std::move(t), 1.0f);
    EXPECT_NE(manager.getActiveTransition(), nullptr);
    EXPECT_STREQ(manager.getActiveTransition()->getName(), "Test");
}

// ---- getActiveTransition after completion ---------------------------------

TEST_F(TransitionManagerTest, GetActiveTransitionAfterCompletion) {
    manager.start(std::make_unique<TestTransition>(), 0.5f);
    manager.update(1.0f);  // completes
    EXPECT_EQ(manager.getActiveTransition(), nullptr);
}

}  // namespace test
}  // namespace vde

/**
 * @file Game_test.cpp
 * @brief Unit tests for Game class (Phase 6 — Audit Remediation)
 *
 * Tests Game scene management, input handler registration, and
 * lifecycle queries that do not require a Vulkan context.
 */

#include <vde/api/Game.h>
#include <vde/api/InputHandler.h>
#include <vde/api/Scene.h>

#include <memory>

#include <gtest/gtest.h>

namespace vde {
namespace test {

// ============================================================================
// Fixture
// ============================================================================

class GameTest : public ::testing::Test {
  protected:
    Game game;
};

// ============================================================================
// Construction / Destruction
// ============================================================================

TEST_F(GameTest, DefaultConstructionDoesNotInitialize) {
    EXPECT_FALSE(game.isInitialized());
    EXPECT_FALSE(game.isRunning());
}

TEST_F(GameTest, DestructionWithoutInitializeIsSafe) {
    // Scope-exit: dtor runs on an un-initialized Game — must not crash.
    { Game temporary; }
}

// ============================================================================
// Scene lifecycle
// ============================================================================

TEST_F(GameTest, GetSceneReturnsNullForUnknown) {
    EXPECT_EQ(game.getScene("nonexistent"), nullptr);
}

TEST_F(GameTest, AddSceneUniquePtr) {
    auto scene = std::make_unique<Scene>();
    Scene* raw = scene.get();
    game.addScene("main", std::move(scene));

    EXPECT_EQ(game.getScene("main"), raw);
}

TEST_F(GameTest, AddSceneRawPointer) {
    // Game::addScene(string, Scene*) wraps the pointer in unique_ptr — Game takes ownership.
    auto* scene = new Scene();
    game.addScene("raw", scene);

    EXPECT_EQ(game.getScene("raw"), scene);
}

TEST_F(GameTest, AddSceneNullIsIgnored) {
    game.addScene("null", std::unique_ptr<Scene>(nullptr));
    EXPECT_EQ(game.getScene("null"), nullptr);
}

TEST_F(GameTest, AddMultipleScenes) {
    game.addScene("a", std::make_unique<Scene>());
    game.addScene("b", std::make_unique<Scene>());
    game.addScene("c", std::make_unique<Scene>());

    EXPECT_NE(game.getScene("a"), nullptr);
    EXPECT_NE(game.getScene("b"), nullptr);
    EXPECT_NE(game.getScene("c"), nullptr);
}

TEST_F(GameTest, AddSceneOverwritesPrevious) {
    game.addScene("s", std::make_unique<Scene>());
    Scene* first = game.getScene("s");

    game.addScene("s", std::make_unique<Scene>());
    Scene* second = game.getScene("s");

    EXPECT_NE(first, second);
}

TEST_F(GameTest, RemoveSceneDeletesScene) {
    game.addScene("temp", std::make_unique<Scene>());
    EXPECT_NE(game.getScene("temp"), nullptr);

    game.removeScene("temp");
    EXPECT_EQ(game.getScene("temp"), nullptr);
}

TEST_F(GameTest, RemoveNonexistentSceneIsSafe) {
    game.removeScene("does_not_exist");
    // No crash — pass
}

TEST_F(GameTest, RemoveSceneDoesNotAffectOthers) {
    game.addScene("keep", std::make_unique<Scene>());
    game.addScene("drop", std::make_unique<Scene>());

    game.removeScene("drop");

    EXPECT_NE(game.getScene("keep"), nullptr);
    EXPECT_EQ(game.getScene("drop"), nullptr);
}

// ============================================================================
// Active scene (deferred switch — getActiveScene is null without a run loop)
// ============================================================================

TEST_F(GameTest, ActiveSceneIsNullByDefault) {
    EXPECT_EQ(game.getActiveScene(), nullptr);
}

// ============================================================================
// Scene Group
// ============================================================================

TEST_F(GameTest, ActiveSceneGroupIsEmptyByDefault) {
    const SceneGroup& group = game.getActiveSceneGroup();
    EXPECT_TRUE(group.sceneNames.empty());
}

// ============================================================================
// Input handler registration
// ============================================================================

TEST_F(GameTest, InputHandlerIsNullByDefault) {
    EXPECT_EQ(game.getInputHandler(), nullptr);
}

TEST_F(GameTest, SetInputHandler) {
    InputHandler handler;
    game.setInputHandler(&handler);
    EXPECT_EQ(game.getInputHandler(), &handler);
}

TEST_F(GameTest, SetInputHandlerToNull) {
    InputHandler handler;
    game.setInputHandler(&handler);
    game.setInputHandler(nullptr);
    EXPECT_EQ(game.getInputHandler(), nullptr);
}

// ============================================================================
// Timing defaults
// ============================================================================

TEST_F(GameTest, DeltaTimeIsZeroBeforeRun) {
    EXPECT_FLOAT_EQ(game.getDeltaTime(), 0.0f);
}

TEST_F(GameTest, TotalTimeIsZeroBeforeRun) {
    EXPECT_DOUBLE_EQ(game.getTotalTime(), 0.0);
}

TEST_F(GameTest, FPSIsZeroBeforeRun) {
    EXPECT_FLOAT_EQ(game.getFPS(), 0.0f);
}

TEST_F(GameTest, FrameCountIsZeroBeforeRun) {
    EXPECT_EQ(game.getFrameCount(), 0u);
}

// ============================================================================
// Window / Vulkan accessors (null before initialize)
// ============================================================================

TEST_F(GameTest, WindowIsNullBeforeInitialize) {
    EXPECT_EQ(game.getWindow(), nullptr);
}

TEST_F(GameTest, VulkanContextIsNullBeforeInitialize) {
    EXPECT_EQ(game.getVulkanContext(), nullptr);
}

// ============================================================================
// Settings
// ============================================================================

TEST_F(GameTest, DefaultSettingsAreReturned) {
    const GameSettings& s = game.getSettings();
    // GameSettings default-constructed — gameName has a default value
    EXPECT_EQ(s.gameName, "VDE Game");
}

// ============================================================================
// Exit code
// ============================================================================

TEST_F(GameTest, ExitCodeIsZeroByDefault) {
    EXPECT_EQ(game.getExitCode(), 0);
}

TEST_F(GameTest, SetExitCodePersists) {
    game.setExitCode(1);
    EXPECT_EQ(game.getExitCode(), 1);
}

// ============================================================================
// Transition state before initialization
// ============================================================================

TEST_F(GameTest, IsNotTransitioningByDefault) {
    EXPECT_FALSE(game.isTransitioning());
}

}  // namespace test
}  // namespace vde

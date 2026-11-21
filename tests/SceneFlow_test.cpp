/**
 * @file SceneFlow_test.cpp
 * @brief Unit tests for Game scene flow: push/pop, group switches, transitions, and timing.
 *
 * Unless explicitly opted into Vulkan, these tests drive an uninitialized Game. Frames are stepped
 * manually: applyPendingSceneChanges() is the frame boundary and
 * getScheduler().execute() runs one frame's update tasks.  Without a transition
 * manager, transitions complete instantly, which exercises their lifecycle bookkeeping.
 */

#include <vde/VulkanContext.h>
#include <vde/api/FadeTransition.h>
#include <vde/api/Game.h>
#include <vde/api/Scene.h>
#include <vde/api/SceneGroup.h>
#include <vde/api/ViewportRect.h>

#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace vde::test {

namespace {

using Names = std::vector<std::string>;

/// Legacy-update scene with hooks for testing callbacks.
class FlowScene : public Scene {
  public:
    FlowScene() = default;
    FlowScene(const FlowScene&) = delete;
    FlowScene& operator=(const FlowScene&) = delete;
    FlowScene(FlowScene&&) = delete;
    FlowScene& operator=(FlowScene&&) = delete;
    ~FlowScene() override {
        if (destroyedFlag != nullptr) {
            *destroyedFlag = true;
        }
    }

    void update(float /*deltaTime*/) override {
        ++updates;
        if (onUpdateHook) {
            auto hook = onUpdateHook;  // the hook may clear onUpdateHook while running
            hook(*this);
        }
    }

    void onPause() override {
        if (onPauseHook) {
            onPauseHook(*this);
        }
    }

    void onExit() override {
        if (onExitHook) {
            onExitHook(*this);
        }
    }

    void render() override {
        if (onRenderHook) {
            onRenderHook(*this);
        }
        Scene::render();
    }

    int updates = 0;
    std::function<void(FlowScene&)> onUpdateHook;
    std::function<void(FlowScene&)> onPauseHook;
    std::function<void(FlowScene&)> onExitHook;
    std::function<void(FlowScene&)> onRenderHook;
    bool* destroyedFlag = nullptr;
};

class RecordingFade : public FadeTransition {
  public:
    explicit RecordingFade(Names& draws) : m_draws(draws) {}

    [[nodiscard]] bool usesCustomGeometry() const override { return true; }

    void renderCustomGeometry(VkCommandBuffer /*cmd*/,
                              const TransitionUpdateContext& /*ctx*/) override {
        m_draws.get().emplace_back("composite");
    }

  private:
    std::reference_wrapper<Names> m_draws;
};

class RemovingFade : public FadeTransition {
  public:
    RemovingFade(Game& game, bool& destroyed) : m_game(game), m_destroyed(destroyed) {}
    ~RemovingFade() override { m_destroyed.get() = true; }

    void update(const TransitionUpdateContext& /*ctx*/, TransitionUniforms& /*uniforms*/) override {
        Game& game = m_game.get();
        bool& destroyed = m_destroyed.get();
        game.removeScene("destination");
        EXPECT_FALSE(destroyed);
        EXPECT_NE(game.getScene("destination"), nullptr);
    }

  private:
    std::reference_wrapper<Game> m_game;
    std::reference_wrapper<bool> m_destroyed;
};

}  // namespace

class SceneFlowTest : public ::testing::Test {
  protected:
    bool sceneDestroyed = false;
    Game game;

    FlowScene* add(const std::string& name) {
        auto scene = std::make_unique<FlowScene>();
        FlowScene* raw = scene.get();
        game.addScene(name, std::move(scene));
        return raw;
    }

    /// One manual frame: the frame-boundary step that run() performs, then the frame itself.
    void frame() {
        game.applyPendingSceneChanges();
        game.getScheduler().execute();
    }

    [[nodiscard]] Names activeNames() const { return game.getActiveSceneGroup().sceneNames; }
};

// ============================================================================
// pushScene / popScene
// ============================================================================

TEST_F(SceneFlowTest, PushScene_PausesActiveSceneAndActivatesPushedScene) {
    FlowScene* gameplay = add("gameplay");
    FlowScene* pause = add("pause");
    game.setActiveScene("gameplay");

    game.pushScene("pause");

    EXPECT_EQ(game.getActiveScene(), pause);
    EXPECT_EQ(activeNames(), Names{"pause"});
    EXPECT_EQ(game.getSceneStackDepth(), 1u);
    EXPECT_EQ(gameplay->getDiagnostics().pauseCount, 1u);
    EXPECT_EQ(gameplay->getDiagnostics().exitCount, 0u);
    EXPECT_EQ(pause->getDiagnostics().enterCount, 1u);
    EXPECT_EQ(game.getFocusedScene(), pause);
    EXPECT_TRUE(pause->getDiagnostics().isFocused);
    EXPECT_FALSE(gameplay->getDiagnostics().isFocused);
}

TEST_F(SceneFlowTest, PushScene_PushedSceneUpdatesAndPausedSceneDoesNot) {
    FlowScene* gameplay = add("gameplay");
    FlowScene* pause = add("pause");
    game.setActiveScene("gameplay");
    game.pushScene("pause");

    frame();

    EXPECT_EQ(pause->updates, 1);
    EXPECT_EQ(gameplay->updates, 0);
}

TEST_F(SceneFlowTest, PushScene_PausedSceneStaysVisibleBeneathPushedScene) {
    add("gameplay");
    add("pause");
    game.setActiveScene("gameplay");
    game.pushScene("pause");

    EXPECT_EQ(game.getVisibleSceneNames(), (Names{"gameplay", "pause"}));
}

TEST_F(SceneFlowTest, PopScene_ExitsPushedSceneAndResumesPreviousScene) {
    FlowScene* gameplay = add("gameplay");
    FlowScene* pause = add("pause");
    game.setActiveScene("gameplay");
    game.pushScene("pause");

    game.popScene();

    EXPECT_EQ(game.getActiveScene(), gameplay);
    EXPECT_EQ(activeNames(), Names{"gameplay"});
    EXPECT_EQ(game.getSceneStackDepth(), 0u);
    EXPECT_EQ(pause->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(gameplay->getDiagnostics().resumeCount, 1u);
    EXPECT_EQ(gameplay->getDiagnostics().exitCount, 0u);
    EXPECT_EQ(gameplay->getDiagnostics().enterCount, 1u);
    EXPECT_EQ(game.getFocusedScene(), gameplay);
    EXPECT_EQ(game.getVisibleSceneNames(), Names{"gameplay"});

    frame();
    EXPECT_EQ(gameplay->updates, 1);
    EXPECT_EQ(pause->updates, 0);
}

TEST_F(SceneFlowTest, PushScene_PausedSceneWithContinueInBackgroundKeepsUpdating) {
    FlowScene* gameplay = add("gameplay");
    FlowScene* pause = add("pause");
    gameplay->setContinueInBackground(true);
    game.setActiveScene("gameplay");
    game.pushScene("pause");

    frame();

    EXPECT_EQ(gameplay->updates, 1);
    EXPECT_EQ(pause->updates, 1);
    EXPECT_EQ(gameplay->getDiagnostics().pauseCount, 1u);
}

TEST_F(SceneFlowTest, PushScene_OverSceneGroupPausesWholeGroupAndPopRestoresIt) {
    FlowScene* world = add("world");
    FlowScene* hud = add("hud");
    FlowScene* pause = add("pause");
    game.setActiveSceneGroup(SceneGroup::createWithViewports(
        "gameplay", {{.sceneName = "world", .viewport = ViewportRect::fullWindow()},
                     {.sceneName = "hud", .viewport = ViewportRect::topHalf()}}));

    game.pushScene("pause");

    EXPECT_EQ(game.getVisibleSceneNames(), (Names{"world", "hud", "pause"}));
    EXPECT_EQ(world->getDiagnostics().pauseCount, 1u);
    EXPECT_EQ(hud->getDiagnostics().pauseCount, 1u);

    frame();
    EXPECT_EQ(world->updates, 0);
    EXPECT_EQ(hud->updates, 0);
    EXPECT_EQ(pause->updates, 1);

    game.popScene();

    EXPECT_EQ(activeNames(), (Names{"world", "hud"}));
    EXPECT_EQ(game.getActiveScene(), world);
    EXPECT_EQ(world->getDiagnostics().resumeCount, 1u);
    EXPECT_EQ(hud->getDiagnostics().resumeCount, 1u);
    EXPECT_EQ(hud->getViewportRect(), ViewportRect::topHalf());

    frame();
    EXPECT_EQ(world->updates, 1);
    EXPECT_EQ(hud->updates, 1);
}

TEST_F(SceneFlowTest, PushScene_NestedPushesUnwindInOrder) {
    FlowScene* a = add("a");
    FlowScene* b = add("b");
    FlowScene* c = add("c");
    game.setActiveScene("a");
    game.pushScene("b");
    game.pushScene("c");

    EXPECT_EQ(game.getSceneStackDepth(), 2u);
    EXPECT_EQ(game.getVisibleSceneNames(), (Names{"a", "b", "c"}));

    game.popScene();
    EXPECT_EQ(game.getActiveScene(), b);
    EXPECT_EQ(c->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(b->getDiagnostics().resumeCount, 1u);
    EXPECT_EQ(a->getDiagnostics().resumeCount, 0u);

    game.popScene();
    EXPECT_EQ(game.getActiveScene(), a);
    EXPECT_EQ(b->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(a->getDiagnostics().resumeCount, 1u);
    EXPECT_EQ(game.getSceneStackDepth(), 0u);
}

TEST_F(SceneFlowTest, PushScene_IgnoresSceneThatIsAlreadyActiveOrPaused) {
    FlowScene* gameplay = add("gameplay");
    add("pause");
    game.setActiveScene("gameplay");

    game.pushScene("gameplay");
    EXPECT_EQ(game.getSceneStackDepth(), 0u);
    EXPECT_EQ(gameplay->getDiagnostics().pauseCount, 0u);

    game.pushScene("pause");
    game.pushScene("gameplay");  // paused beneath "pause"
    EXPECT_EQ(game.getSceneStackDepth(), 1u);
    EXPECT_EQ(activeNames(), Names{"pause"});
    EXPECT_EQ(gameplay->getDiagnostics().enterCount, 1u);
}

TEST_F(SceneFlowTest, PushScene_UnknownSceneIsIgnored) {
    FlowScene* gameplay = add("gameplay");
    game.setActiveScene("gameplay");

    game.pushScene("missing");

    EXPECT_EQ(game.getActiveScene(), gameplay);
    EXPECT_EQ(game.getSceneStackDepth(), 0u);
    EXPECT_EQ(gameplay->getDiagnostics().pauseCount, 0u);
}

TEST_F(SceneFlowTest, PopScene_EmptyStackIsNoOp) {
    FlowScene* gameplay = add("gameplay");
    game.setActiveScene("gameplay");

    game.popScene();

    EXPECT_EQ(game.getActiveScene(), gameplay);
    EXPECT_EQ(gameplay->getDiagnostics().exitCount, 0u);
}

TEST_F(SceneFlowTest, SetActiveScene_ClearsStackAndExitsPausedScenes) {
    FlowScene* gameplay = add("gameplay");
    FlowScene* pause = add("pause");
    FlowScene* menu = add("menu");
    game.setActiveScene("gameplay");
    game.pushScene("pause");

    game.setActiveScene("menu");

    EXPECT_EQ(game.getSceneStackDepth(), 0u);
    EXPECT_EQ(gameplay->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(pause->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(menu->getDiagnostics().enterCount, 1u);
    EXPECT_EQ(game.getVisibleSceneNames(), Names{"menu"});
}

TEST_F(SceneFlowTest, SetActiveScene_ResumesPausedSceneThatStaysActive) {
    FlowScene* gameplay = add("gameplay");
    FlowScene* pause = add("pause");
    game.setActiveScene("gameplay");
    game.pushScene("pause");

    game.setActiveScene("gameplay");

    EXPECT_EQ(gameplay->getDiagnostics().exitCount, 0u);
    EXPECT_EQ(gameplay->getDiagnostics().resumeCount, 1u);
    EXPECT_EQ(pause->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(game.getSceneStackDepth(), 0u);

    frame();
    EXPECT_EQ(gameplay->updates, 1);
}

TEST_F(SceneFlowTest, Focus_ExplicitFocusAppliesOnlyWhileItsGroupIsOnTop) {
    FlowScene* left = add("left");
    FlowScene* right = add("right");
    FlowScene* pause = add("pause");
    game.setActiveSceneGroup(SceneGroup::createWithViewports(
        "split", {{.sceneName = "left", .viewport = ViewportRect::leftHalf()},
                  {.sceneName = "right", .viewport = ViewportRect::rightHalf()}}));
    game.setFocusedScene("right");
    EXPECT_EQ(game.getFocusedScene(), right);

    game.pushScene("pause");
    EXPECT_EQ(game.getFocusedScene(), pause);
    EXPECT_TRUE(pause->getDiagnostics().isFocused);
    EXPECT_FALSE(right->getDiagnostics().isFocused);

    game.popScene();
    EXPECT_EQ(game.getFocusedScene(), right);
    EXPECT_TRUE(right->getDiagnostics().isFocused);
    EXPECT_FALSE(left->getDiagnostics().isFocused);
}

// ============================================================================
// Timing: every scene-flow request made during a frame applies at the next boundary
// ============================================================================

TEST_F(SceneFlowTest, Timing_RequestsBeforeRunApplyImmediately) {
    FlowScene* a = add("a");

    game.setActiveScene("a");

    EXPECT_EQ(game.getActiveScene(), a);
    EXPECT_FALSE(game.hasPendingSceneChanges());
    EXPECT_EQ(a->getDiagnostics().enterCount, 1u);
}

TEST_F(SceneFlowTest, Timing_PushSceneDuringFrameAppliesAtNextFrameBoundary) {
    FlowScene* a = add("a");
    FlowScene* b = add("b");
    game.setActiveScene("a");
    a->onUpdateHook = [](FlowScene& self) {
        self.onUpdateHook = nullptr;
        self.getGame()->pushScene("b");
    };

    game.getScheduler().execute();

    EXPECT_TRUE(game.hasPendingSceneChanges());
    EXPECT_EQ(activeNames(), Names{"a"});
    EXPECT_EQ(b->getDiagnostics().enterCount, 0u);
    EXPECT_EQ(a->getDiagnostics().pauseCount, 0u);
    EXPECT_EQ(b->updates, 0);

    frame();

    EXPECT_FALSE(game.hasPendingSceneChanges());
    EXPECT_EQ(activeNames(), Names{"b"});
    EXPECT_EQ(a->getDiagnostics().pauseCount, 1u);
    EXPECT_EQ(b->updates, 1);
    EXPECT_EQ(a->updates, 1);
}

TEST_F(SceneFlowTest, Timing_SetActiveSceneGroupDuringFrameAppliesAtNextFrameBoundary) {
    FlowScene* a = add("a");
    FlowScene* b = add("b");
    game.setActiveScene("a");
    a->onUpdateHook = [](FlowScene& self) {
        self.onUpdateHook = nullptr;
        self.getGame()->setActiveSceneGroup(SceneGroup::create("ab", {"a", "b"}));
    };

    game.getScheduler().execute();

    EXPECT_EQ(activeNames(), Names{"a"});
    EXPECT_EQ(b->getDiagnostics().enterCount, 0u);
    EXPECT_EQ(b->updates, 0);

    frame();

    EXPECT_EQ(activeNames(), (Names{"a", "b"}));
    EXPECT_EQ(a->updates, 2);
    EXPECT_EQ(b->updates, 1);
}

TEST_F(SceneFlowTest, Timing_RequestsDuringFrameApplyInCallOrder) {
    FlowScene* a = add("a");
    FlowScene* b = add("b");
    FlowScene* c = add("c");
    game.setActiveScene("a");
    a->onUpdateHook = [](FlowScene& self) {
        self.onUpdateHook = nullptr;
        self.getGame()->setActiveScene("b");
        self.getGame()->pushScene("c");
    };

    frame();
    game.applyPendingSceneChanges();

    EXPECT_EQ(a->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(b->getDiagnostics().pauseCount, 1u);
    EXPECT_EQ(c->getDiagnostics().enterCount, 1u);
    EXPECT_EQ(game.getVisibleSceneNames(), (Names{"b", "c"}));
}

TEST_F(SceneFlowTest, SceneGroupWithoutViewports_ResetsScenesToFullWindow) {
    FlowScene* a = add("a");
    add("b");
    game.setActiveSceneGroup(SceneGroup::createWithViewports(
        "split", {{.sceneName = "a", .viewport = ViewportRect::leftHalf()},
                  {.sceneName = "b", .viewport = ViewportRect::rightHalf()}}));
    EXPECT_EQ(a->getViewportRect(), ViewportRect::leftHalf());

    game.setActiveScene("a");

    EXPECT_EQ(a->getViewportRect(), ViewportRect::fullWindow());
}

TEST_F(SceneFlowTest, SceneGroupWithoutViewports_KeepsViewportChosenByScene) {
    FlowScene* minimap = add("minimap");
    add("world");
    const ViewportRect corner{0.75f, 0.0f, 0.25f, 0.25f};
    minimap->setViewportRect(corner);

    game.setActiveSceneGroup(SceneGroup::create("gameplay", {"world", "minimap"}));
    game.setActiveSceneGroup(SceneGroup::create("gameplay", {"world", "minimap"}));

    EXPECT_EQ(minimap->getViewportRect(), corner);
}

// ============================================================================
// Transitions (instant without a GPU-backed transition manager)
// ============================================================================

TEST_F(SceneFlowTest, TransitionToSceneGroup_KeepsScenesSharedWithDestination) {
    FlowScene* world = add("world");
    FlowScene* hud = add("hud");
    FlowScene* menu = add("menu");
    game.setActiveSceneGroup(SceneGroup::create("gameplay", {"world", "hud"}));

    game.transitionToSceneGroup(SceneGroup::create("menu", {"menu", "hud"}),
                                std::make_unique<FadeTransition>(), 0.5f);

    EXPECT_EQ(activeNames(), (Names{"menu", "hud"}));
    EXPECT_EQ(game.getActiveScene(), menu);
    EXPECT_EQ(world->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(menu->getDiagnostics().enterCount, 1u);
    EXPECT_EQ(hud->getDiagnostics().enterCount, 1u);
    EXPECT_EQ(hud->getDiagnostics().exitCount, 0u);
    EXPECT_FALSE(game.isTransitioning());
}

TEST_F(SceneFlowTest, TransitionToSceneGroup_ClearsStackAndResumesPausedDestination) {
    FlowScene* gameplay = add("gameplay");
    FlowScene* pause = add("pause");
    game.setActiveScene("gameplay");
    game.pushScene("pause");

    game.transitionToSceneGroup(SceneGroup::create("back", {"gameplay"}),
                                std::make_unique<FadeTransition>(), 0.5f);

    EXPECT_EQ(game.getSceneStackDepth(), 0u);
    EXPECT_EQ(activeNames(), Names{"gameplay"});
    EXPECT_EQ(gameplay->getDiagnostics().resumeCount, 1u);
    EXPECT_EQ(gameplay->getDiagnostics().exitCount, 0u);
    EXPECT_EQ(pause->getDiagnostics().exitCount, 1u);
}

TEST_F(SceneFlowTest, TransitionToSceneGroup_RejectsEmptyOrUnknownGroups) {
    add("a");
    EXPECT_THROW(
        game.transitionToSceneGroup(SceneGroup{}, std::make_unique<FadeTransition>(), 1.0f),
        std::runtime_error);
    EXPECT_THROW(game.transitionToSceneGroup(SceneGroup::create("g", {"a", "missing"}),
                                             std::make_unique<FadeTransition>(), 1.0f),
                 std::runtime_error);
    EXPECT_THROW(game.transitionToScene("missing", std::make_unique<FadeTransition>(), 1.0f),
                 std::runtime_error);
}

TEST_F(SceneFlowTest, TransitionToScene_RegionReplacesOnlyTheSceneInThatViewport) {
    FlowScene* left = add("left");
    FlowScene* right = add("right");
    FlowScene* other = add("other");
    game.setActiveSceneGroup(SceneGroup::createWithViewports(
        "split", {{.sceneName = "left", .viewport = ViewportRect::leftHalf()},
                  {.sceneName = "right", .viewport = ViewportRect::rightHalf()}}));

    game.transitionToScene("other", std::make_unique<FadeTransition>(), 0.0f,
                           ViewportRect::rightHalf());

    EXPECT_EQ(activeNames(), (Names{"left", "other"}));
    EXPECT_EQ(other->getViewportRect(), ViewportRect::rightHalf());
    EXPECT_EQ(left->getViewportRect(), ViewportRect::leftHalf());
    EXPECT_EQ(right->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(left->getDiagnostics().exitCount, 0u);
    EXPECT_EQ(left->getDiagnostics().enterCount, 1u);
}

TEST_F(SceneFlowTest, TransitionToScene_RegionPreservesSceneStack) {
    FlowScene* gameplay = add("gameplay");
    FlowScene* map = add("map");
    add("journal");
    game.setActiveScene("gameplay");
    game.pushScene("map");

    game.transitionToScene("journal", nullptr, 0.0f, ViewportRect::fullWindow());

    EXPECT_EQ(activeNames(), Names{"journal"});
    EXPECT_EQ(game.getSceneStackDepth(), 1u);
    EXPECT_EQ(map->getDiagnostics().exitCount, 1u);
    EXPECT_EQ(gameplay->getDiagnostics().exitCount, 0u);
    EXPECT_EQ(game.getVisibleSceneNames(), (Names{"gameplay", "journal"}));
}

TEST_F(SceneFlowTest, TransitionToScene_RegionCompositeDrawsBelowOverlappingHud) {
    if (std::getenv("VDE_TEST_VULKAN") == nullptr) {
        GTEST_SKIP() << "Set VDE_TEST_VULKAN=1 to run with a Vulkan-capable display";
    }

    Names draws;
    ASSERT_TRUE(game.initialize(GameSettings{}));
    const auto recordDraw = [&draws](FlowScene& scene) { draws.push_back(scene.getName()); };
    add("background")->onRenderHook = recordDraw;
    add("source")->onRenderHook = recordDraw;
    add("hud")->onRenderHook = recordDraw;
    add("destination")->onRenderHook = recordDraw;
    game.setActiveSceneGroup(SceneGroup::createWithViewports(
        "layered", {{.sceneName = "background", .viewport = ViewportRect::fullWindow()},
                    {.sceneName = "source", .viewport = ViewportRect::leftHalf()},
                    {.sceneName = "hud", .viewport = ViewportRect::fullWindow()}}));

    game.transitionToScene("destination", std::make_unique<RecordingFade>(draws), 1.0f,
                           ViewportRect::leftHalf());
    game.setTransitionSpeed(0.0f);
    frame();

    EXPECT_EQ(draws, (Names{"source", "destination", "background", "composite", "hud"}));
}

TEST_F(SceneFlowTest, CancelTransition_WithoutTransitionIsNoOp) {
    add("a");
    game.setActiveScene("a");

    game.cancelTransition();

    EXPECT_FALSE(game.hasPendingSceneChanges());
    EXPECT_FALSE(game.isTransitioning());
}

TEST_F(SceneFlowTest, RemoveTransitionDestination_FromUpdateWaitsForFrameBoundary) {
    if (std::getenv("VDE_TEST_VULKAN") == nullptr) {
        GTEST_SKIP() << "Set VDE_TEST_VULKAN=1 to run with a Vulkan-capable display";
    }

    ASSERT_TRUE(game.initialize(GameSettings{}));
    add("source");
    add("destination");
    game.setActiveScene("source");
    game.transitionToScene("destination", std::make_unique<RemovingFade>(game, sceneDestroyed),
                           1.0f);

    frame();

    EXPECT_FALSE(sceneDestroyed);
    EXPECT_TRUE(game.isTransitioning());
    game.applyPendingSceneChanges();
    EXPECT_TRUE(sceneDestroyed);
    EXPECT_EQ(game.getScene("destination"), nullptr);
    EXPECT_EQ(activeNames(), Names{"source"});
    EXPECT_FALSE(game.isTransitioning());
}

TEST_F(SceneFlowTest, RemoveTransitionDestination_FromManualStepWaitsForCallback) {
    if (std::getenv("VDE_TEST_VULKAN") == nullptr) {
        GTEST_SKIP() << "Set VDE_TEST_VULKAN=1 to run with a Vulkan-capable display";
    }

    ASSERT_TRUE(game.initialize(GameSettings{}));
    add("source");
    add("destination");
    game.setActiveScene("source");
    game.transitionToScene("destination", std::make_unique<RemovingFade>(game, sceneDestroyed),
                           1.0f);
    game.setTransitionPaused(true);

    game.stepTransitionOneFrame();

    EXPECT_TRUE(sceneDestroyed);
    EXPECT_EQ(game.getScene("destination"), nullptr);
    EXPECT_EQ(activeNames(), Names{"source"});
    EXPECT_FALSE(game.isTransitioning());
}

TEST_F(SceneFlowTest, CancelTransition_QueuedBeforeSameFrameCompletionKeepsSource) {
    if (std::getenv("VDE_TEST_VULKAN") == nullptr) {
        GTEST_SKIP() << "Set VDE_TEST_VULKAN=1 to run with a Vulkan-capable display";
    }

    ASSERT_TRUE(game.initialize(GameSettings{}));
    FlowScene* source = add("source");
    FlowScene* destination = add("destination");
    game.setActiveScene("source");
    constexpr float duration = 1.0f;
    game.transitionToScene("destination", std::make_unique<FadeTransition>(), duration);

    source->onUpdateHook = [](FlowScene& self) {
        auto* game = self.getGame();
        game->cancelTransition();
        game->quit();  // Finish this frame but leave both requests queued.
        const float deltaTime = game->getDeltaTime();
        ASSERT_GT(deltaTime, 0.0f);
        game->setTransitionSpeed(2.0f * duration / deltaTime);
    };

    game.run();

    ASSERT_FLOAT_EQ(game.getTransitionProgress(), 1.0f);
    ASSERT_EQ(activeNames(), Names{"source"});
    game.applyPendingSceneChanges();

    EXPECT_EQ(activeNames(), Names{"source"});
    EXPECT_EQ(source->getDiagnostics().exitCount, 0u);
    EXPECT_EQ(destination->getDiagnostics().exitCount, 1u);
    EXPECT_FALSE(game.isTransitioning());
}

// ============================================================================
// removeScene interaction
// ============================================================================

TEST_F(SceneFlowTest, RemoveScene_FromPauseWaitsUntilPushFinishes) {
    FlowScene* a = add("a");
    FlowScene* b = add("b");
    add("overlay");
    game.setActiveSceneGroup(SceneGroup::create("ab", {"a", "b"}));

    int exits = 0;
    a->onPauseHook = [](FlowScene& self) {
        auto* game = self.getGame();
        game->removeScene("b");
        EXPECT_NE(game->getScene("b"), nullptr);
    };
    b->onExitHook = [&exits](FlowScene& self) {
        ++exits;
        EXPECT_EQ(self.getGame()->getActiveSceneGroup().sceneNames, Names{"overlay"});
        EXPECT_EQ(self.getDiagnostics().pauseCount, 1u);
    };

    game.pushScene("overlay");

    EXPECT_EQ(exits, 1);
    EXPECT_EQ(game.getScene("b"), nullptr);
    EXPECT_EQ(game.getVisibleSceneNames(), (Names{"a", "overlay"}));
    game.popScene();
    EXPECT_EQ(activeNames(), Names{"a"});
}

TEST_F(SceneFlowTest, RemoveScene_FromExitWaitsUntilOuterRemovalFinishes) {
    FlowScene* a = add("a");
    FlowScene* b = add("b");
    game.setActiveSceneGroup(SceneGroup::create("ab", {"a", "b"}));

    a->onExitHook = [](FlowScene& self) {
        auto* game = self.getGame();
        game->removeScene("b");
        EXPECT_NE(game->getScene("b"), nullptr);
    };
    int exits = 0;
    b->onExitHook = [&exits](FlowScene& self) {
        ++exits;
        EXPECT_EQ(self.getGame()->getScene("a"), nullptr);
    };

    game.removeScene("a");

    EXPECT_EQ(exits, 1);
    EXPECT_EQ(game.getScene("b"), nullptr);
    EXPECT_TRUE(activeNames().empty());
}

TEST_F(SceneFlowTest, RemoveScene_PurgesSceneFromStack) {
    add("gameplay");
    FlowScene* pause = add("pause");
    game.setActiveScene("gameplay");
    game.pushScene("pause");

    game.removeScene("gameplay");

    EXPECT_EQ(game.getSceneStackDepth(), 0u);
    EXPECT_EQ(game.getVisibleSceneNames(), Names{"pause"});
    game.popScene();
    EXPECT_EQ(game.getActiveScene(), pause);
}

TEST_F(SceneFlowTest, RemoveScene_DuringFrameKeepsSceneAliveUntilFrameBoundary) {
    FlowScene* a = add("a");
    FlowScene* b = add("b");
    b->destroyedFlag = &sceneDestroyed;
    game.setActiveSceneGroup(SceneGroup::create("ab", {"a", "b"}));
    a->onUpdateHook = [](FlowScene& self) {
        self.onUpdateHook = nullptr;
        self.getGame()->removeScene("b");
    };

    game.getScheduler().execute();

    EXPECT_EQ(game.getScene("b"), nullptr);
    EXPECT_FALSE(sceneDestroyed) << "scene removed mid-frame must outlive the frame's tasks";
    EXPECT_EQ(activeNames(), Names{"a"});

    game.applyPendingSceneChanges();
    EXPECT_TRUE(sceneDestroyed);

    frame();
    EXPECT_EQ(a->updates, 2);
}

TEST_F(SceneFlowTest, RemovePausedScene_WaitsForLastRenderFence) {
    if (std::getenv("VDE_TEST_VULKAN") == nullptr) {
        GTEST_SKIP() << "Set VDE_TEST_VULKAN=1 to run with a Vulkan-capable display";
    }

    ASSERT_TRUE(game.initialize(GameSettings{}));
    add("gameplay")->destroyedFlag = &sceneDestroyed;
    add("overlay");
    game.setActiveScene("gameplay");
    game.pushScene("overlay");
    ASSERT_EQ(game.getVisibleSceneNames(), (Names{"gameplay", "overlay"}));

    auto* context = game.getVulkanContext();
    const VkDevice device = context->getDevice();
    const auto frameSlot = context->getCurrentFrame();
    game.getScheduler().execute();
    ASSERT_NE(context->getCurrentFrame(), frameSlot);
    const VkFence fence = context->getInFlightFences()[frameSlot];

    // Simulate a still-pending submission without relying on GPU execution timing.
    ASSERT_EQ(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), VK_SUCCESS);
    ASSERT_EQ(vkResetFences(device, 1, &fence), VK_SUCCESS);
    game.removeScene("gameplay");
    EXPECT_EQ(game.getScene("gameplay"), nullptr);
    EXPECT_EQ(game.getVisibleSceneNames(), Names{"overlay"});
    game.applyPendingSceneChanges();
    EXPECT_FALSE(sceneDestroyed);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    ASSERT_EQ(vkQueueSubmit(context->getGraphicsQueue(), 1, &submit, fence), VK_SUCCESS);
    ASSERT_EQ(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), VK_SUCCESS);
    game.applyPendingSceneChanges();
    EXPECT_TRUE(sceneDestroyed);
}

}  // namespace vde::test

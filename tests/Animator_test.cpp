/**
 * @file Animator_test.cpp
 * @brief Unit tests for the Animator animation service.
 *
 * Covers: single animation completion, multiple independent animations,
 * loop mode, ping-pong mode, pause/resume, speed scaling, binding lifetime,
 * scene teardown, multi-scene independence, visual phase ordering, and
 * transition source freeze behavior.
 */

#include <vde/api/Animator.h>
#include <vde/api/Game.h>
#include <vde/api/Scene.h>
#include <vde/api/SceneGroup.h>
#include <vde/api/Scheduler.h>

#include <thread>

#include <gtest/gtest.h>

namespace vde::test {

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,bugprone-unchecked-optional-access)

namespace {

AnimationOptions makeOptions(float duration, float delay = 0.0f, float speed = 1.0f,
                             AnimationPlayback playback = AnimationPlayback::Once,
                             AnimationEasing easing = AnimationEasing::EaseOutCubic,
                             bool startPaused = false) {
    AnimationOptions options{};
    options.duration = duration;
    options.delay = delay;
    options.speed = speed;
    options.playback = playback;
    options.easing = easing;
    options.startPaused = startPaused;
    return options;
}

AnimationCallbacks makeCallbacks(std::function<void(const AnimationContext&)> onStart = {},
                                 std::function<void(const AnimationContext&)> onUpdate = {},
                                 std::function<void(const AnimationContext&)> onComplete = {}) {
    AnimationCallbacks callbacks{};
    callbacks.onStart = std::move(onStart);
    callbacks.onUpdate = std::move(onUpdate);
    callbacks.onComplete = std::move(onComplete);
    return callbacks;
}

template <typename T>
BoundAnimationCallbacks<T>
makeBoundCallbacks(std::function<void(T&, const AnimationContext&)> onStart = {},
                   std::function<void(T&, const AnimationContext&)> onUpdate = {},
                   std::function<void(T&, const AnimationContext&)> onComplete = {}) {
    BoundAnimationCallbacks<T> callbacks{};
    callbacks.onStart = std::move(onStart);
    callbacks.onUpdate = std::move(onUpdate);
    callbacks.onComplete = std::move(onComplete);
    return callbacks;
}

}  // anonymous namespace

// ============================================================================
// Pure Animator tests — no Scene or GPU required
// ============================================================================

class AnimatorTest : public ::testing::Test {
  protected:
    Animator anim;
};

// ---------------------------------------------------------------------------
// Single animation — Once mode
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_SingleAnim_Completes) {
    float lastProgress = 0.0f;
    bool completionFired = false;

    anim.schedule(makeOptions(1.0f),
                  makeCallbacks(
                      {}, [&](const AnimationContext& ctx) { lastProgress = ctx.linearProgress; },
                      [&](const AnimationContext&) { completionFired = true; }));

    anim.update(0.5f);
    EXPECT_NEAR(lastProgress, 0.5f, 1e-5f);
    EXPECT_FALSE(completionFired);

    anim.update(0.5f);
    EXPECT_NEAR(lastProgress, 1.0f, 1e-5f);
    EXPECT_TRUE(completionFired);
}

// ---------------------------------------------------------------------------
// Multiple animations — independent progress
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_MultipleAnims_IndependentProgress) {
    float progressA = 0.0f;
    float progressB = 0.0f;

    anim.schedule(makeOptions(1.0f), makeCallbacks({}, [&](const AnimationContext& ctx) {
                      progressA = ctx.linearProgress;
                  }));

    anim.schedule(makeOptions(2.0f), makeCallbacks({}, [&](const AnimationContext& ctx) {
                      progressB = ctx.linearProgress;
                  }));

    anim.update(0.5f);
    EXPECT_NEAR(progressA, 0.5f, 1e-5f);
    EXPECT_NEAR(progressB, 0.25f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Loop mode
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_LoopMode_Wraps) {
    float lastProgress = 0.0f;
    uint32_t lastCycle = 0;

    anim.schedule(makeOptions(1.0f, 0.0f, 1.0f, AnimationPlayback::Loop),
                  makeCallbacks({}, [&](const AnimationContext& ctx) {
                      lastProgress = ctx.linearProgress;
                      lastCycle = ctx.cycleIndex;
                  }));

    // After 1.0s — first cycle complete, progress wraps to near 0.
    anim.update(1.1f);
    EXPECT_GE(lastCycle, 1u);
    // Progress should wrap and be in [0, 1).
    EXPECT_GE(lastProgress, 0.0f);
    EXPECT_LT(lastProgress, 1.0f);

    // After another 0.5s — still running (not stopped).
    float progBefore = lastProgress;
    anim.update(0.5f);
    (void)progBefore;  // progress will have changed; just verify it's still in range.
    EXPECT_GE(lastProgress, 0.0f);
    EXPECT_LE(lastProgress, 1.0f);
}

// ---------------------------------------------------------------------------
// PingPong mode
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_PingPongMode_Reverses) {
    float lastProgress = 0.0f;
    bool lastReverse = false;

    anim.schedule(makeOptions(1.0f, 0.0f, 1.0f, AnimationPlayback::PingPong),
                  makeCallbacks({}, [&](const AnimationContext& ctx) {
                      lastProgress = ctx.linearProgress;
                      lastReverse = ctx.reversePass;
                  }));

    // First half (0→0.5): forward pass.
    anim.update(0.5f);
    EXPECT_NEAR(lastProgress, 0.5f, 1e-5f);
    EXPECT_FALSE(lastReverse);

    // Complete forward pass and enter reverse (time > 1.0s total).
    anim.update(0.6f);
    EXPECT_TRUE(lastReverse);
    EXPECT_GT(lastProgress, 0.0f);
    EXPECT_LT(lastProgress, 1.0f);
}

// ---------------------------------------------------------------------------
// Pause / resume
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_PauseResume) {
    float lastProgress = 0.0f;

    auto handle = anim.schedule(
        makeOptions(2.0f),
        makeCallbacks({}, [&](const AnimationContext& ctx) { lastProgress = ctx.linearProgress; }));

    anim.update(0.5f);
    float progressBeforePause = lastProgress;
    EXPECT_NEAR(progressBeforePause, 0.25f, 1e-5f);

    handle.pause();
    anim.update(1.0f);  // time passes but animation is paused
    EXPECT_FLOAT_EQ(lastProgress, progressBeforePause);

    handle.resume();
    anim.update(0.5f);  // 0.5 more seconds after resume
    EXPECT_NEAR(lastProgress, 0.5f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Speed scaling
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_SpeedScaling) {
    float lastProgress = 0.0f;
    bool completed = false;

    anim.schedule(makeOptions(1.0f, 0.0f, 2.0f),
                  makeCallbacks(
                      {}, [&](const AnimationContext& ctx) { lastProgress = ctx.linearProgress; },
                      [&](const AnimationContext&) { completed = true; }));

    // With speed = 2, 0.5 wall-clock seconds = 1.0 effective seconds (full duration).
    anim.update(0.5f);
    EXPECT_NEAR(lastProgress, 1.0f, 1e-5f);
    EXPECT_TRUE(completed);
}

// ---------------------------------------------------------------------------
// Delay
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_Delay_HoldsAtZero) {
    float lastProgress = -1.0f;
    bool startFired = false;

    anim.schedule(
        makeOptions(1.0f, 0.5f),
        makeCallbacks([&](const AnimationContext&) { startFired = true; },
                      [&](const AnimationContext& ctx) { lastProgress = ctx.linearProgress; }));

    // During delay — no callbacks should fire.
    anim.update(0.3f);
    EXPECT_FALSE(startFired);
    EXPECT_FLOAT_EQ(lastProgress, -1.0f);  // unchanged sentinel

    // After delay has elapsed — callbacks should fire.
    anim.update(0.3f);  // cumulative 0.6s > 0.5s delay
    EXPECT_TRUE(startFired);
    EXPECT_GE(lastProgress, 0.0f);
}

TEST_F(AnimatorTest, Animator_Delay_IsWallClock_NotSpeedScaled) {
    bool startFired = false;
    int updateCount = 0;

    anim.setGlobalSpeed(3.0f);
    anim.schedule(makeOptions(1.0f, 0.5f, 2.0f),
                  makeCallbacks([&](const AnimationContext&) { startFired = true; },
                                [&](const AnimationContext&) { ++updateCount; }));

    // Delay should use wall-clock time, unaffected by global/per-animation speed.
    anim.update(0.25f);
    EXPECT_FALSE(startFired);
    EXPECT_EQ(updateCount, 0);

    anim.update(0.24f);
    EXPECT_FALSE(startFired);
    EXPECT_EQ(updateCount, 0);

    anim.update(0.01f);
    EXPECT_TRUE(startFired);
    EXPECT_GE(updateCount, 1);
}

// ---------------------------------------------------------------------------
// onStart fires exactly once
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_OnStart_FiresOnce) {
    int startCount = 0;

    anim.schedule(makeOptions(1.0f), makeCallbacks([&](const AnimationContext&) { ++startCount; },
                                                   [](const AnimationContext&) {}));

    anim.update(0.2f);
    anim.update(0.2f);
    anim.update(0.2f);
    anim.update(0.5f);

    EXPECT_EQ(startCount, 1);
}

// ---------------------------------------------------------------------------
// Cancel
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_Cancel_SuppressesCompletion) {
    bool completed = false;
    auto handle =
        anim.schedule(makeOptions(1.0f), makeCallbacks(
                                             {}, [](const AnimationContext&) {},
                                             [&](const AnimationContext&) { completed = true; }));

    anim.update(0.5f);
    handle.cancel();
    anim.update(1.0f);  // would complete without the cancel

    EXPECT_FALSE(completed);
}

TEST_F(AnimatorTest, Animator_CancelInsideCallback_Safe) {
    AnimationHandle capturedHandle;

    // Cancel self from inside onUpdate — must not crash.
    EXPECT_NO_THROW({
        capturedHandle = anim.schedule(
            makeOptions(2.0f),
            makeCallbacks({}, [&](const AnimationContext&) { capturedHandle.cancel(); }));
        anim.update(0.5f);
    });
}

TEST_F(AnimatorTest, Animator_CancelInsideCompletionTick_SuppressesCompletion) {
    AnimationHandle capturedHandle;
    bool completed = false;

    capturedHandle = anim.schedule(
        makeOptions(0.5f), makeCallbacks(
                               {}, [&](const AnimationContext&) { capturedHandle.cancel(); },
                               [&](const AnimationContext&) { completed = true; }));

    anim.update(0.5f);

    EXPECT_FALSE(completed);
    EXPECT_FALSE(capturedHandle.isActive());
}

// ---------------------------------------------------------------------------
// Global speed
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_GlobalSpeed) {
    float lastProgress = 0.0f;

    anim.setGlobalSpeed(2.0f);
    anim.schedule(makeOptions(1.0f), makeCallbacks({}, [&](const AnimationContext& ctx) {
                      lastProgress = ctx.linearProgress;
                  }));

    // Global speed = 2 → 0.5 wall-clock seconds reaches progress 1.0.
    anim.update(0.5f);
    EXPECT_NEAR(lastProgress, 1.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// cancelAll — no completion callbacks
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_CancelAll_NoCompletionCallbacks) {
    int completions = 0;

    anim.schedule(makeOptions(1.0f),
                  makeCallbacks({}, {}, [&](const AnimationContext&) { ++completions; }));
    anim.schedule(makeOptions(2.0f),
                  makeCallbacks({}, {}, [&](const AnimationContext&) { ++completions; }));

    anim.update(0.5f);
    anim.cancelAll();
    anim.update(2.0f);

    EXPECT_EQ(completions, 0);
}

// ---------------------------------------------------------------------------
// Binding lifetime — resolver returns nullptr (target gone)
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_BindingLifetime_StopsOnTargetDestroy) {
    // Simulate a target that can be "destroyed" by setting a pointer to nullptr.
    struct FakeTarget {
        int updateCount = 0;
    };

    FakeTarget target;
    FakeTarget* targetPtr = &target;

    // Create a dummy scene for the binding API (binding won't actually use it
    // since we use a resolver that doesn't call scene.getEntity()).
    Scene scene;

    // Use a resolver binding so we don't need a real entity.
    auto binding = AnimationBinding<FakeTarget>::resolver([&targetPtr]() { return targetPtr; });

    auto handle =
        anim.schedule(scene, binding, makeOptions(2.0f, 0.0f, 1.0f, AnimationPlayback::Loop),
                      makeBoundCallbacks<FakeTarget>(
                          {}, [](FakeTarget& t, const AnimationContext&) { t.updateCount++; }));

    anim.update(0.3f);
    EXPECT_EQ(target.updateCount, 1);

    // Simulate target destruction by making the resolver return nullptr.
    targetPtr = nullptr;

    // Missing targets cancel the animation instead of leaving a silent zombie job.
    EXPECT_NO_THROW({ anim.update(0.3f); });
    EXPECT_EQ(target.updateCount, 1);  // not called again
    EXPECT_EQ(anim.activeCount(), 0u);
    EXPECT_FALSE(handle.isActive());
}

// ---------------------------------------------------------------------------
// Scene teardown — Animator destructs, no completion callbacks
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_SceneTeardown_CancelsAll) {
    int completions = 0;

    {
        Animator local;
        local.schedule(makeOptions(1.0f),
                       makeCallbacks({}, {}, [&](const AnimationContext&) { ++completions; }));
        local.update(0.3f);
        // local goes out of scope — destructor should not fire onComplete.
    }

    EXPECT_EQ(completions, 0);
}

TEST_F(AnimatorTest, Animator_HandleExpiresAfterAnimatorDestroy) {
    AnimationHandle handle;

    {
        Animator local;
        handle =
            local.schedule(makeOptions(1.0f), makeCallbacks({}, [](const AnimationContext&) {}));
        EXPECT_TRUE(handle.isValid());
        EXPECT_TRUE(handle.isActive());
    }

    EXPECT_FALSE(handle.isValid());
    EXPECT_FALSE(handle.isActive());
    handle.cancel();
    handle.pause();
    handle.resume();
    EXPECT_FLOAT_EQ(handle.getSpeed(), 1.0f);
}

TEST_F(AnimatorTest, Animator_HandleFollowsMoveAssignment) {
    AnimationHandle handle;
    int updates = 0;

    Animator source;
    handle = source.schedule(makeOptions(1.0f),
                             makeCallbacks({}, [&](const AnimationContext&) { ++updates; }));

    Animator destination;
    destination = std::move(source);

    EXPECT_TRUE(handle.isValid());
    EXPECT_TRUE(handle.isActive());
    EXPECT_EQ(destination.activeCount(), 1u);

    handle.pause();
    destination.update(0.5f);
    EXPECT_EQ(updates, 0);
    EXPECT_TRUE(handle.isActive());

    handle.resume();
    destination.update(0.5f);
    EXPECT_EQ(updates, 1);
    EXPECT_TRUE(handle.isActive());

    handle.cancel();
    EXPECT_FALSE(handle.isActive());
    EXPECT_EQ(destination.activeCount(), 0u);
}

// ---------------------------------------------------------------------------
// Multi-scene independence — two Animators advance independently
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_MultiScene_Independent) {
    Animator animA;
    Animator animB;

    float progressA = 0.0f;
    float progressB = 0.0f;

    animA.schedule(makeOptions(1.0f), makeCallbacks({}, [&](const AnimationContext& ctx) {
                       progressA = ctx.linearProgress;
                   }));

    animB.schedule(makeOptions(2.0f), makeCallbacks({}, [&](const AnimationContext& ctx) {
                       progressB = ctx.linearProgress;
                   }));

    animA.update(0.5f);
    EXPECT_NEAR(progressA, 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(progressB, 0.0f);  // B not ticked yet

    animB.update(0.5f);
    EXPECT_NEAR(progressA, 0.5f, 1e-5f);  // A unchanged
    EXPECT_NEAR(progressB, 0.25f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Transition source freeze — not calling update() halts progress
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_TransitionSourceFreeze) {
    float lastProgress = 0.0f;
    int updateCount = 0;

    anim.schedule(makeOptions(2.0f), makeCallbacks({}, [&](const AnimationContext& ctx) {
                      lastProgress = ctx.linearProgress;
                      ++updateCount;
                  }));

    anim.update(0.5f);
    float frozen = lastProgress;
    int updatesBeforeWait = updateCount;

    // "Stopping scene updates" = no longer calling update().
    // Progress/callbacks must not advance without explicit ticks.
    for (int i = 0; i < 1000; ++i) {
        std::this_thread::yield();
    }
    EXPECT_FLOAT_EQ(lastProgress, frozen);
    EXPECT_EQ(updateCount, updatesBeforeWait);
    EXPECT_NEAR(lastProgress, 0.25f, 1e-5f);
}

// ---------------------------------------------------------------------------
// startPaused option
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_StartPaused_DoesNotAdvance) {
    float lastProgress = 0.0f;

    anim.schedule(
        makeOptions(1.0f, 0.0f, 1.0f, AnimationPlayback::Once, AnimationEasing::EaseOutCubic, true),
        makeCallbacks({}, [&](const AnimationContext& ctx) { lastProgress = ctx.linearProgress; }));

    anim.update(0.5f);
    EXPECT_FLOAT_EQ(lastProgress, 0.0f);
}

// ---------------------------------------------------------------------------
// Linear easing
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_LinearEasing_MatchesProgress) {
    float linear = 0.0f;
    float eased = 0.0f;

    anim.schedule(makeOptions(1.0f, 0.0f, 1.0f, AnimationPlayback::Once, AnimationEasing::Linear),
                  makeCallbacks({}, [&](const AnimationContext& ctx) {
                      linear = ctx.linearProgress;
                      eased = ctx.easedProgress;
                  }));

    anim.update(0.5f);
    EXPECT_NEAR(linear, 0.5f, 1e-5f);
    EXPECT_NEAR(eased, 0.5f, 1e-5f);  // linear easing: eased == linear
}

TEST_F(AnimatorTest, Animator_LoopElapsed_RemainsMonotonic) {
    float lastElapsed = 0.0f;
    uint32_t lastCycle = 0;

    anim.schedule(makeOptions(1.0f, 0.0f, 1.0f, AnimationPlayback::Loop),
                  makeCallbacks({}, [&](const AnimationContext& ctx) {
                      lastElapsed = ctx.elapsed;
                      lastCycle = ctx.cycleIndex;
                  }));

    anim.update(1.1f);
    EXPECT_NEAR(lastElapsed, 1.1f, 1e-5f);
    EXPECT_EQ(lastCycle, 1u);

    anim.update(0.5f);
    EXPECT_NEAR(lastElapsed, 1.6f, 1e-5f);
    EXPECT_EQ(lastCycle, 1u);
}

// ---------------------------------------------------------------------------
// Phase 4 — callback ordering and weak-binding cleanup
// ---------------------------------------------------------------------------

TEST_F(AnimatorTest, Animator_CallbackOrder_SameFrame) {
    // Three animations with identical durations completing in a single update()
    // must fire their onComplete callbacks in registration order.
    std::vector<int> completionOrder;

    anim.schedule(makeOptions(1.0f), makeCallbacks({}, {}, [&](const AnimationContext&) {
                      completionOrder.push_back(1);
                  }));
    anim.schedule(makeOptions(1.0f), makeCallbacks({}, {}, [&](const AnimationContext&) {
                      completionOrder.push_back(2);
                  }));
    anim.schedule(makeOptions(1.0f), makeCallbacks({}, {}, [&](const AnimationContext&) {
                      completionOrder.push_back(3);
                  }));

    anim.update(1.0f);  // all three complete simultaneously

    ASSERT_EQ(completionOrder.size(), 3u);
    EXPECT_EQ(completionOrder.at(0), 1);
    EXPECT_EQ(completionOrder.at(1), 2);
    EXPECT_EQ(completionOrder.at(2), 3);
}

TEST_F(AnimatorTest, Animator_WeakObjectBinding_DropsCleanly) {
    // A Weak binding whose shared_ptr has been released must silently cancel the
    // animation — no crash, no completion callback, no dangling reference.
    struct FakeTarget {
        int updateCount = 0;
    };

    int completions = 0;

    auto sharedTarget = std::make_shared<FakeTarget>();
    std::weak_ptr<FakeTarget> weakTarget = sharedTarget;

    Scene scene;
    auto binding = AnimationBinding<FakeTarget>::weak(weakTarget);

    auto handle =
        anim.schedule(scene, binding, makeOptions(2.0f),
                      makeBoundCallbacks<FakeTarget>(
                          {}, [](FakeTarget& t, const AnimationContext&) { t.updateCount++; },
                          [&completions](FakeTarget&, const AnimationContext&) { ++completions; }));

    anim.update(0.3f);
    EXPECT_EQ(sharedTarget->updateCount, 1);
    EXPECT_TRUE(handle.isActive());

    // Release the only strong reference — weak_ptr now returns nullptr on lock().
    sharedTarget.reset();

    // Next update must not crash and must drop the animation silently.
    EXPECT_NO_THROW({ anim.update(0.3f); });
    EXPECT_EQ(anim.activeCount(), 0u);
    EXPECT_FALSE(handle.isActive());
    EXPECT_EQ(completions, 0);
}

// ============================================================================
// Scheduler ordering tests — require Game / Scheduler
// ============================================================================

namespace {

/// Minimal phase-callback scene for ordering tests.
class AnimOrderTestScene : public Scene {
  public:
    std::vector<std::string>* log = nullptr;
    std::string tag;

    AnimOrderTestScene(std::string t, std::vector<std::string>* l) : log(l), tag(std::move(t)) {
        enablePhaseCallbacks();
    }

    void updateVisuals(float) override {
        if (log) {
            log->push_back(tag + ".visuals");
        }
    }
};

}  // anonymous namespace

class AnimatorSchedulerTest : public ::testing::Test {
  protected:
    Game game;
};

TEST_F(AnimatorSchedulerTest, Animator_VisualPhaseOrdering) {
    // scene.animations must depend on scene.visuals in the scheduler graph.
    game.addScene("anim_order", std::make_unique<AnimOrderTestScene>("anim_order", nullptr));

    SceneGroup group;
    group.sceneNames = {"anim_order"};
    game.setActiveSceneGroup(group);

    const Scheduler& sched = game.getScheduler();

    TaskId visualsTask = sched.findTaskByName("scene.visuals.anim_order");
    TaskId animationsTask = sched.findTaskByName("scene.animations.anim_order");

    ASSERT_NE(visualsTask, INVALID_TASK_ID) << "scene.visuals.anim_order not found";
    ASSERT_NE(animationsTask, INVALID_TASK_ID) << "scene.animations.anim_order not found";

    auto animDesc = sched.getTaskDescriptor(animationsTask);
    ASSERT_TRUE(animDesc.has_value());

    EXPECT_EQ(animDesc->phase, TaskPhase::Visual) << "scene.animations must be in the Visual phase";

    EXPECT_NE(std::find(animDesc->dependsOn.begin(), animDesc->dependsOn.end(), visualsTask),
              animDesc->dependsOn.end())
        << "scene.animations must depend on scene.visuals";
}

TEST_F(AnimatorSchedulerTest, Animator_PostPhysicsOrdering) {
    // For a phase-callback scene, scene.animations depends on scene.visuals
    // which depends on scene.timed which depends on (postPhysics or gameLogic).
    // Thus the animations task transitively follows post-physics.
    // Verify the direct dependency chain (visuals → timed) and (animations → visuals).
    game.addScene("post_phys", std::make_unique<AnimOrderTestScene>("post_phys", nullptr));

    SceneGroup group;
    group.sceneNames = {"post_phys"};
    game.setActiveSceneGroup(group);

    const Scheduler& sched = game.getScheduler();

    TaskId timedTask = sched.findTaskByName("scene.timed.post_phys");
    TaskId visualsTask = sched.findTaskByName("scene.visuals.post_phys");
    TaskId animationsTask = sched.findTaskByName("scene.animations.post_phys");

    ASSERT_NE(timedTask, INVALID_TASK_ID);
    ASSERT_NE(visualsTask, INVALID_TASK_ID);
    ASSERT_NE(animationsTask, INVALID_TASK_ID);

    // visuals depends on timed (→ post-physics)
    auto visualDesc = sched.getTaskDescriptor(visualsTask);
    ASSERT_TRUE(visualDesc.has_value());
    EXPECT_NE(std::find(visualDesc->dependsOn.begin(), visualDesc->dependsOn.end(), timedTask),
              visualDesc->dependsOn.end())
        << "scene.visuals must depend on scene.timed (→ post-physics)";

    // animations depends on visuals
    auto animDesc = sched.getTaskDescriptor(animationsTask);
    ASSERT_TRUE(animDesc.has_value());
    EXPECT_NE(std::find(animDesc->dependsOn.begin(), animDesc->dependsOn.end(), visualsTask),
              animDesc->dependsOn.end())
        << "scene.animations must depend on scene.visuals";
}

TEST_F(AnimatorSchedulerTest, Animator_AnimationsTask_IsMainThreadOnly) {
    // scene.animations must be main-thread-only (may invoke user callbacks).
    game.addScene("mt_anim", std::make_unique<AnimOrderTestScene>("mt_anim", nullptr));

    SceneGroup group;
    group.sceneNames = {"mt_anim"};
    game.setActiveSceneGroup(group);

    const Scheduler& sched = game.getScheduler();

    TaskId animationsTask = sched.findTaskByName("scene.animations.mt_anim");
    ASSERT_NE(animationsTask, INVALID_TASK_ID);

    auto desc = sched.getTaskDescriptor(animationsTask);
    ASSERT_TRUE(desc.has_value());
    EXPECT_TRUE(desc->mainThreadOnly) << "scene.animations must be main-thread-only";
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,bugprone-unchecked-optional-access)

}  // namespace vde::test

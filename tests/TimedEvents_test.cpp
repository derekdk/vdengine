/**
 * @file TimedEvents_test.cpp
 * @brief Unit tests for TimedEvents
 *
 * Covers: one-shot, repeating, cancel, cancel-in-callback, pause/resume,
 * speed scaling, large deltaTime, multi-instance independence, and teardown safety.
 */

#include <vde/api/TimedEvents.h>

#include <gtest/gtest.h>

namespace vde::test {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class TimedEventsTest : public ::testing::Test {
  protected:
    TimedEvents te;
};

// ---------------------------------------------------------------------------
// One-shot
// ---------------------------------------------------------------------------

TEST_F(TimedEventsTest, OneShotFires_Once) {
    int count = 0;
    te.after(0.5f, [&count]() { ++count; });
    te.tick(1.0f);
    EXPECT_EQ(count, 1);
    te.tick(1.0f);  // should not fire again
    EXPECT_EQ(count, 1);
}

TEST_F(TimedEventsTest, OneShotFires_AtCorrectFrame) {
    int count = 0;
    te.after(0.5f, [&count]() { ++count; });

    te.tick(0.3f);  // 0.3s elapsed — not yet
    EXPECT_EQ(count, 0);

    te.tick(0.3f);  // 0.6s cumulative — should fire
    EXPECT_EQ(count, 1);
}

TEST_F(TimedEventsTest, OneShotWithZeroDelay_FiresImmediately) {
    int count = 0;
    te.after(0.0f, [&count]() { ++count; });
    te.tick(0.001f);
    EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// Repeating
// ---------------------------------------------------------------------------

TEST_F(TimedEventsTest, RepeatingFires_CorrectCount) {
    int count = 0;
    te.every(0.5f, [&count]() { ++count; });

    // 4 intervals of 0.5s each = 2.0s
    for (int i = 0; i < 4; ++i) {
        te.tick(0.5f);
    }
    EXPECT_EQ(count, 4);
}

TEST_F(TimedEventsTest, RepeatingFires_WithLargeDeltaTime) {
    int count = 0;
    te.every(0.1f, [&count]() { ++count; });

    // One large delta that covers 3 intervals
    te.tick(0.3f);
    EXPECT_EQ(count, 3);
}

TEST_F(TimedEventsTest, RepeatingFires_WithLargeDeltaTime_Exact) {
    int count = 0;
    te.every(0.1f, [&count]() { ++count; });
    te.tick(0.5f);
    EXPECT_EQ(count, 5);
}

// ---------------------------------------------------------------------------
// Cancel
// ---------------------------------------------------------------------------

TEST_F(TimedEventsTest, CancelBeforeFire_Suppresses) {
    bool fired = false;
    auto handle = te.after(1.0f, [&fired]() { fired = true; });
    te.cancel(handle);
    te.tick(2.0f);
    EXPECT_FALSE(fired);
}

TEST_F(TimedEventsTest, CancelInsideCallback_Safe) {
    int count = 0;
    TimedEventHandle self = INVALID_TIMED_EVENT_HANDLE;

    self = te.every(0.1f, [&]() {
        ++count;
        te.cancel(self);  // cancel from within the callback
    });

    te.tick(1.0f);
    // Should have fired exactly once (cancelled on first fire)
    EXPECT_EQ(count, 1);
}

TEST_F(TimedEventsTest, CancelInvalidHandle_IsNoop) {
    // Should not crash or assert
    te.cancel(INVALID_TIMED_EVENT_HANDLE);
    te.cancel(9999);
}

TEST_F(TimedEventsTest, CancelAll_SuppressesAll) {
    bool fired1 = false;
    bool fired2 = false;
    te.after(0.5f, [&fired1]() { fired1 = true; });
    te.every(0.1f, [&fired2]() { fired2 = true; });

    te.cancelAll();
    te.tick(2.0f);

    EXPECT_FALSE(fired1);
    EXPECT_FALSE(fired2);
}

// ---------------------------------------------------------------------------
// Pause / resume
// ---------------------------------------------------------------------------

TEST_F(TimedEventsTest, PauseResume_Deterministic) {
    int count = 0;
    te.every(1.0f, [&count]() { ++count; });

    te.tick(0.5f);  // 0.5s
    te.pause();
    te.tick(10.0f);  // no effect while paused
    te.resume();
    te.tick(0.5f);  // 0.5s more → 1.0s total → fires once
    EXPECT_EQ(count, 1);
}

TEST_F(TimedEventsTest, PauseResume_PreservesElapsed) {
    // Verify that time elapsed before pause is preserved exactly after resume.
    int count = 0;
    te.every(1.0f, [&count]() { ++count; });

    te.tick(0.6f);  // 0.6s elapsed
    te.pause();
    te.tick(5.0f);  // discarded
    te.resume();
    te.tick(0.4f);  // 0.6 + 0.4 = 1.0 → fires
    EXPECT_EQ(count, 1);
}

TEST_F(TimedEventsTest, IsPaused_ReflectsState) {
    EXPECT_FALSE(te.isPaused());
    te.pause();
    EXPECT_TRUE(te.isPaused());
    te.resume();
    EXPECT_FALSE(te.isPaused());
}

// ---------------------------------------------------------------------------
// Speed scaling
// ---------------------------------------------------------------------------

TEST_F(TimedEventsTest, SpeedScale_AffectsInterval) {
    int count = 0;
    te.every(1.0f, [&count]() { ++count; });
    te.setSpeed(2.0f);

    // At 2x speed, 0.5s of wall-clock time = 1.0s effective → fires
    te.tick(0.5f);
    EXPECT_EQ(count, 1);
}

TEST_F(TimedEventsTest, SpeedScale_HalfSpeed) {
    bool fired = false;
    te.after(1.0f, [&fired]() { fired = true; });
    te.setSpeed(0.5f);

    te.tick(1.5f);  // effective = 0.75s < 1.0 → should not fire yet
    EXPECT_FALSE(fired);

    te.tick(1.0f);  // effective = 1.25 total → fires
    EXPECT_TRUE(fired);
}

// ---------------------------------------------------------------------------
// Scene teardown safety
// ---------------------------------------------------------------------------

TEST_F(TimedEventsTest, SceneTeardown_CancelsAll) {
    // Simulate scene teardown by calling cancelAll() and verifying no callbacks fire.
    bool fired = false;
    te.after(0.1f, [&fired]() { fired = true; });
    te.cancelAll();
    te.tick(1.0f);
    EXPECT_FALSE(fired);
}

TEST_F(TimedEventsTest, SceneTeardown_NoCompletionCallback) {
    // Destroying a TimedEvents instance must not invoke any pending callbacks.
    bool fired = false;
    {
        TimedEvents local;
        local.after(0.1f, [&fired]() { fired = true; });
        // Destructor runs here — events are dropped, callback never called.
    }
    EXPECT_FALSE(fired);
}

TEST_F(TimedEventsTest, SceneTeardown_RepeatingNoCompletionCallback) {
    bool fired = false;
    {
        TimedEvents local;
        local.every(0.1f, [&fired]() { fired = true; });
        // Destructor runs here — never ticked.
    }
    EXPECT_FALSE(fired);
}

// ---------------------------------------------------------------------------
// Multi-scene independence
// ---------------------------------------------------------------------------

TEST_F(TimedEventsTest, MultiScene_Independent) {
    // Two independent TimedEvents instances (representing two scenes)
    // should not interfere with each other.
    TimedEvents sceneA;
    TimedEvents sceneB;

    int countA = 0;
    int countB = 0;

    // Use exact binary-fraction values to avoid floating-point accumulation error.
    sceneA.every(0.5f, [&countA]() { ++countA; });
    sceneB.every(0.25f, [&countB]() { ++countB; });

    sceneA.tick(1.0f);   // A fires at 0.5 and 1.0 → 2 times
    sceneB.tick(0.75f);  // B fires at 0.25, 0.5, and 0.75 → 3 times

    EXPECT_EQ(countA, 2);
    EXPECT_EQ(countB, 3);
}

TEST_F(TimedEventsTest, MultiScene_CancelOneDoesNotAffectOther) {
    TimedEvents sceneA;
    TimedEvents sceneB;

    bool firedA = false;
    bool firedB = false;

    auto handleA = sceneA.after(0.5f, [&firedA]() { firedA = true; });
    sceneB.after(0.5f, [&firedB]() { firedB = true; });

    sceneA.cancel(handleA);  // cancel only scene A's event

    sceneA.tick(1.0f);
    sceneB.tick(1.0f);

    EXPECT_FALSE(firedA);
    EXPECT_TRUE(firedB);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_F(TimedEventsTest, TickWithZeroDelta_NothingFires) {
    bool fired = false;
    te.after(0.0f, [&fired]() { fired = true; });
    te.tick(0.0f);
    EXPECT_FALSE(fired);
}

TEST_F(TimedEventsTest, MultipleEventsFireInOrder) {
    // Events created first should fire before events created later when they
    // share the same delay and the same tick.
    std::vector<int> order;
    te.after(0.5f, [&order]() { order.push_back(1); });
    te.after(0.5f, [&order]() { order.push_back(2); });
    te.after(0.5f, [&order]() { order.push_back(3); });
    te.tick(1.0f);
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order.at(0), 1);
    EXPECT_EQ(order.at(1), 2);
    EXPECT_EQ(order.at(2), 3);
}

}  // namespace vde::test

/**
 * @file Timing_test.cpp
 * @brief Unit tests for Timing.h (Cooldown and RepeatingTimer)
 */

#include <vde/api/Timing.h>

#include <gtest/gtest.h>

using namespace vde;

// ============================================================================
// Cooldown
// ============================================================================

TEST(CooldownTest, StartsReady_WhenZeroDuration) {
    Cooldown cd(0.0f);
    EXPECT_TRUE(cd.ready());
}

TEST(CooldownTest, NotReadyImmediately) {
    Cooldown cd(1.0f);
    cd.start();
    EXPECT_FALSE(cd.ready());
}

TEST(CooldownTest, ReadyAfterFullDuration) {
    Cooldown cd(1.0f);
    cd.start();
    cd.advance(1.0f);
    EXPECT_TRUE(cd.ready());
}

TEST(CooldownTest, ProgressLinear) {
    Cooldown cd(2.0f);
    cd.start();
    cd.advance(1.0f);
    EXPECT_NEAR(cd.progress(), 0.5f, 0.001f);
}

TEST(CooldownTest, RemainingDecrements) {
    Cooldown cd(2.0f);
    cd.start();
    cd.advance(0.5f);
    EXPECT_NEAR(cd.remaining(), 1.5f, 0.001f);
}

TEST(CooldownTest, TryConsumeSucceeds_WhenReady) {
    Cooldown cd(0.5f);
    cd.start();
    cd.advance(0.5f);
    EXPECT_TRUE(cd.tryConsume());
    // After consume, should be reset (not ready)
    EXPECT_FALSE(cd.ready());
}

TEST(CooldownTest, TryConsumeFails_WhenNotReady) {
    Cooldown cd(1.0f);
    cd.start();
    cd.advance(0.2f);
    EXPECT_FALSE(cd.tryConsume());
}

TEST(CooldownTest, FinishMakesReady) {
    Cooldown cd(10.0f);
    cd.start();
    cd.finish();
    EXPECT_TRUE(cd.ready());
}

TEST(CooldownTest, SetDurationDoesNotResetElapsed) {
    Cooldown cd(1.0f);
    cd.start();
    cd.advance(0.5f);
    cd.setDuration(2.0f);
    EXPECT_NEAR(cd.progress(), 0.25f, 0.001f);
}

// ============================================================================
// RepeatingTimer
// ============================================================================

TEST(RepeatingTimerTest, NoTicks_WhenZeroInterval) {
    RepeatingTimer timer(0.0f);
    EXPECT_EQ(timer.advance(1.0f), 0);
}

TEST(RepeatingTimerTest, OneTick) {
    RepeatingTimer timer(1.0f);
    EXPECT_EQ(timer.advance(1.0f), 1);
}

TEST(RepeatingTimerTest, MultipleTicksInOnce) {
    RepeatingTimer timer(0.5f);
    EXPECT_EQ(timer.advance(2.0f), 4);
}

TEST(RepeatingTimerTest, NoTickForPartialInterval) {
    RepeatingTimer timer(1.0f);
    EXPECT_EQ(timer.advance(0.5f), 0);
}

TEST(RepeatingTimerTest, AccumulatesAcrossCalls) {
    RepeatingTimer timer(1.0f);
    EXPECT_EQ(timer.advance(0.6f), 0);
    EXPECT_EQ(timer.advance(0.6f), 1);
}

TEST(RepeatingTimerTest, ResetClearsAccumulated) {
    RepeatingTimer timer(1.0f);
    timer.advance(0.9f);
    timer.reset();
    EXPECT_EQ(timer.advance(0.5f), 0);
}

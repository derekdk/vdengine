/**
 * @file PlaybackClock_test.cpp
 * @brief Unit tests for PlaybackClock
 *
 * Covers: progress advancement, pause/resume, speed scaling,
 * loop mode, ping-pong direction, delay behavior, and completion callback.
 */

#include <vde/PlaybackClock.h>

#include <gtest/gtest.h>

namespace vde::test {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class PlaybackClockTest : public ::testing::Test {
  protected:
    PlaybackClock clock;
};

// ---------------------------------------------------------------------------
// Progress advancement
// ---------------------------------------------------------------------------

TEST_F(PlaybackClockTest, PlaybackClock_ProgressAdvances) {
    clock.setDuration(2.0f);
    clock.start();
    clock.tick(1.0f);
    EXPECT_FLOAT_EQ(clock.getProgress(), 0.5f);  // 1.0 / 2.0
}

TEST_F(PlaybackClockTest, PlaybackClock_ProgressAtEnd_IsOne) {
    clock.setDuration(1.0f);
    clock.start();
    clock.tick(1.0f);
    EXPECT_FLOAT_EQ(clock.getProgress(), 1.0f);
}

// ---------------------------------------------------------------------------
// Pause / resume
// ---------------------------------------------------------------------------

TEST_F(PlaybackClockTest, PlaybackClock_PauseHaltsProgress) {
    clock.setDuration(2.0f);
    clock.start();
    clock.tick(0.5f);
    float progressBefore = clock.getProgress();
    clock.pause();
    clock.tick(1.0f);  // should have no effect
    EXPECT_FLOAT_EQ(clock.getProgress(), progressBefore);
}

TEST_F(PlaybackClockTest, PlaybackClock_ResumeRestoresProgress) {
    clock.setDuration(2.0f);
    clock.start();
    clock.tick(0.5f);
    clock.pause();
    clock.tick(1.0f);  // no effect
    clock.resume();
    clock.tick(0.5f);                               // should advance again
    EXPECT_NEAR(clock.getProgress(), 0.5f, 1e-5f);  // (0.5 + 0.5) / 2.0
}

TEST_F(PlaybackClockTest, PlaybackClock_IsPausedReflectsState) {
    clock.setDuration(1.0f);
    clock.start();
    EXPECT_FALSE(clock.isPaused());
    clock.pause();
    EXPECT_TRUE(clock.isPaused());
    clock.resume();
    EXPECT_FALSE(clock.isPaused());
}

// ---------------------------------------------------------------------------
// Speed scaling
// ---------------------------------------------------------------------------

TEST_F(PlaybackClockTest, PlaybackClock_SpeedScaling) {
    clock.setDuration(2.0f);
    clock.setSpeed(2.0f);
    clock.start();
    clock.tick(0.5f);
    // Effective time = 0.5 * 2.0 = 1.0; progress = 1.0 / 2.0 = 0.5
    EXPECT_NEAR(clock.getProgress(), 0.5f, 1e-5f);
}

TEST_F(PlaybackClockTest, PlaybackClock_SpeedScaling_HalfSpeed) {
    clock.setDuration(2.0f);
    clock.setSpeed(0.5f);
    clock.start();
    clock.tick(1.0f);
    // Effective time = 1.0 * 0.5 = 0.5; progress = 0.5 / 2.0 = 0.25
    EXPECT_NEAR(clock.getProgress(), 0.25f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Loop mode
// ---------------------------------------------------------------------------

TEST_F(PlaybackClockTest, PlaybackClock_LoopWraps) {
    clock.setDuration(1.0f);
    clock.setLoopMode(LoopMode::Loop);
    clock.start();
    // Advance exactly one cycle: progress should wrap back to 0
    clock.tick(1.0f);
    EXPECT_NEAR(clock.getProgress(), 0.0f, 1e-5f);
    EXPECT_EQ(clock.getCycleIndex(), 1u);
}

TEST_F(PlaybackClockTest, PlaybackClock_LoopContinuesAfterWrap) {
    clock.setDuration(1.0f);
    clock.setLoopMode(LoopMode::Loop);
    clock.start();
    clock.tick(1.5f);  // one full cycle plus half
    EXPECT_NEAR(clock.getProgress(), 0.5f, 1e-5f);
    EXPECT_EQ(clock.getCycleIndex(), 1u);
}

TEST_F(PlaybackClockTest, PlaybackClock_LoopNeverCompletes) {
    clock.setDuration(1.0f);
    clock.setLoopMode(LoopMode::Loop);
    clock.start();
    clock.tick(100.0f);
    EXPECT_FALSE(clock.isComplete());
}

// ---------------------------------------------------------------------------
// Ping-pong
// ---------------------------------------------------------------------------

TEST_F(PlaybackClockTest, PlaybackClock_PingPongReverses) {
    clock.setDuration(1.0f);
    clock.setLoopMode(LoopMode::PingPong);
    clock.start();
    EXPECT_FALSE(clock.isReversePass());  // starts forward

    // After one full pass, direction should have flipped
    clock.tick(1.0f);
    EXPECT_TRUE(clock.isReversePass());
    EXPECT_EQ(clock.getCycleIndex(), 1u);
}

TEST_F(PlaybackClockTest, PlaybackClock_PingPongProgressMidForwardPass) {
    clock.setDuration(2.0f);
    clock.setLoopMode(LoopMode::PingPong);
    clock.start();
    clock.tick(1.0f);  // half way through forward pass
    EXPECT_NEAR(clock.getProgress(), 0.5f, 1e-5f);
    EXPECT_FALSE(clock.isReversePass());
}

TEST_F(PlaybackClockTest, PlaybackClock_PingPongProgressMidReversePass) {
    clock.setDuration(1.0f);
    clock.setLoopMode(LoopMode::PingPong);
    clock.start();
    clock.tick(1.0f);  // complete forward pass — now at start of reverse
    clock.tick(0.5f);  // half way through reverse pass
    // Reverse pass: progress = 1 - 0.5 = 0.5
    EXPECT_NEAR(clock.getProgress(), 0.5f, 1e-5f);
    EXPECT_TRUE(clock.isReversePass());
}

// ---------------------------------------------------------------------------
// Delay
// ---------------------------------------------------------------------------

TEST_F(PlaybackClockTest, PlaybackClock_DelayHoldsAtZero) {
    clock.setDuration(1.0f);
    clock.setDelay(2.0f);
    clock.start();
    clock.tick(1.0f);  // still within delay window
    EXPECT_FLOAT_EQ(clock.getProgress(), 0.0f);
    EXPECT_FALSE(clock.hasStarted());
}

TEST_F(PlaybackClockTest, PlaybackClock_DelayElapsesAndAdvances) {
    clock.setDuration(2.0f);
    clock.setDelay(1.0f);
    clock.start();
    clock.tick(1.5f);  // 1.0s delay + 0.5s into animation
    EXPECT_TRUE(clock.hasStarted());
    EXPECT_NEAR(clock.getProgress(), 0.25f, 1e-5f);  // 0.5 / 2.0
}

TEST_F(PlaybackClockTest, PlaybackClock_HasStartedFalseBeforeDelay) {
    clock.setDuration(1.0f);
    clock.setDelay(1.0f);
    clock.start();
    EXPECT_FALSE(clock.hasStarted());
}

// ---------------------------------------------------------------------------
// Completion callback (Once mode)
// ---------------------------------------------------------------------------

TEST_F(PlaybackClockTest, PlaybackClock_CompletionFiredOnce) {
    int callCount = 0;
    clock.setDuration(1.0f);
    clock.setOnComplete([&callCount]() { ++callCount; });
    clock.start();

    clock.tick(1.0f);  // completes
    EXPECT_EQ(callCount, 1);

    clock.tick(1.0f);  // extra tick — should NOT fire again
    EXPECT_EQ(callCount, 1);
}

TEST_F(PlaybackClockTest, PlaybackClock_CompletionNotFiredBeforeEnd) {
    bool fired = false;
    clock.setDuration(2.0f);
    clock.setOnComplete([&fired]() { fired = true; });
    clock.start();
    clock.tick(1.0f);  // halfway — should not fire
    EXPECT_FALSE(fired);
}

TEST_F(PlaybackClockTest, PlaybackClock_CompletionReturnsTrueOnce) {
    clock.setDuration(1.0f);
    clock.start();
    bool completedThisTick = clock.tick(1.0f);
    EXPECT_TRUE(completedThisTick);

    // tick after completion returns false
    EXPECT_FALSE(clock.tick(1.0f));
}

// ---------------------------------------------------------------------------
// isComplete / stop
// ---------------------------------------------------------------------------

TEST_F(PlaybackClockTest, PlaybackClock_IsCompleteAfterOnce) {
    clock.setDuration(1.0f);
    clock.start();
    EXPECT_FALSE(clock.isComplete());
    clock.tick(1.0f);
    EXPECT_TRUE(clock.isComplete());
}

TEST_F(PlaybackClockTest, PlaybackClock_StopResetsState) {
    clock.setDuration(1.0f);
    clock.start();
    clock.tick(0.5f);
    clock.stop();
    EXPECT_FLOAT_EQ(clock.getProgress(), 0.0f);
    EXPECT_FALSE(clock.isComplete());
    EXPECT_FALSE(clock.isPaused());
}

}  // namespace vde::test

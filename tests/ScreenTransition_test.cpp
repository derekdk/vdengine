/**
 * @file ScreenTransition_test.cpp
 * @brief Unit tests for ScreenTransition state machine
 */

#include <gtest/gtest.h>

#include <vde/api/ScreenTransition.h>

namespace vde::test {

// =========================================================================
// TransitionState — Construction & Reset
// =========================================================================

TEST(TransitionStateTest, DefaultStateIsInactive) {
    TransitionState state;
    EXPECT_FALSE(state.isActive());
    EXPECT_EQ(state.phase, TransitionPhase::NONE);
    EXPECT_EQ(state.type, TransitionType::NONE);
    EXPECT_FLOAT_EQ(state.overlayAlpha, 0.0f);
}

TEST(TransitionStateTest, ResetClearsAllFields) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "gameplay", 1.0f);
    EXPECT_TRUE(state.isActive());

    state.reset();
    EXPECT_FALSE(state.isActive());
    EXPECT_EQ(state.phase, TransitionPhase::NONE);
    EXPECT_EQ(state.type, TransitionType::NONE);
    EXPECT_TRUE(state.targetScene.empty());
    EXPECT_FLOAT_EQ(state.overlayAlpha, 0.0f);
    EXPECT_FLOAT_EQ(state.elapsed, 0.0f);
}

// =========================================================================
// TransitionState — Start
// =========================================================================

TEST(TransitionStateTest, StartBeginsInFadingOutPhase) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "level1", 0.5f);

    EXPECT_TRUE(state.isActive());
    EXPECT_EQ(state.phase, TransitionPhase::FADING_OUT);
    EXPECT_EQ(state.type, TransitionType::FADE_BLACK);
    EXPECT_EQ(state.targetScene, "level1");
    EXPECT_FLOAT_EQ(state.halfDuration, 0.25f);
    EXPECT_FLOAT_EQ(state.elapsed, 0.0f);
    EXPECT_FLOAT_EQ(state.overlayAlpha, 0.0f);
}

TEST(TransitionStateTest, StartWithZeroDurationClampsHalfDuration) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "scene", 0.0f);

    // halfDuration should be clamped to avoid division by zero
    EXPECT_GT(state.halfDuration, 0.0f);
}

// =========================================================================
// TransitionState — Update (Fade-Out Phase)
// =========================================================================

TEST(TransitionStateTest, UpdateIncreasesOverlayAlphaDuringFadeOut) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "target", 1.0f);
    // halfDuration = 0.5s

    bool midpoint = state.update(0.25f);  // 50% through fade-out
    EXPECT_FALSE(midpoint);
    EXPECT_EQ(state.phase, TransitionPhase::FADING_OUT);
    EXPECT_FLOAT_EQ(state.overlayAlpha, 0.5f);
}

TEST(TransitionStateTest, UpdateReachesMidpointAtEndOfFadeOut) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "target", 1.0f);
    // halfDuration = 0.5s

    bool midpoint = state.update(0.5f);  // exactly at midpoint
    EXPECT_TRUE(midpoint);
    EXPECT_EQ(state.phase, TransitionPhase::FADING_IN);
}

TEST(TransitionStateTest, UpdateDoesNotReturnMidpointIfNotYetReached) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "target", 1.0f);

    bool midpoint = state.update(0.1f);
    EXPECT_FALSE(midpoint);
    EXPECT_EQ(state.phase, TransitionPhase::FADING_OUT);
}

TEST(TransitionStateTest, FadeOutAlphaClampedToOne) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "target", 0.2f);
    // halfDuration = 0.1s

    // Overshoot the fade-out
    state.update(0.15f);
    // Should have transitioned to FADING_IN, but alpha during fade-out
    // phase should not exceed 1.0
    EXPECT_LE(state.overlayAlpha, 1.0f);
}

// =========================================================================
// TransitionState — Update (Fade-In Phase)
// =========================================================================

TEST(TransitionStateTest, FadeInDecreasesOverlayAlpha) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "target", 1.0f);
    // halfDuration = 0.5s

    // Advance to midpoint
    state.update(0.5f);
    EXPECT_EQ(state.phase, TransitionPhase::FADING_IN);

    // Advance halfway through fade-in
    state.update(0.25f);
    EXPECT_EQ(state.phase, TransitionPhase::FADING_IN);
    EXPECT_NEAR(state.overlayAlpha, 0.5f, 0.01f);
}

TEST(TransitionStateTest, FadeInCompletesAndResetsState) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "target", 1.0f);
    // halfDuration = 0.5s

    // Advance to midpoint
    state.update(0.5f);

    // Complete fade-in
    state.update(0.5f);

    EXPECT_FALSE(state.isActive());
    EXPECT_EQ(state.phase, TransitionPhase::NONE);
    EXPECT_FLOAT_EQ(state.overlayAlpha, 0.0f);
}

TEST(TransitionStateTest, OnCompleteCallbackInvokedAtEnd) {
    TransitionState state;
    bool callbackCalled = false;
    state.start(TransitionType::FADE_BLACK, "target", 0.2f);
    state.onComplete = [&callbackCalled]() { callbackCalled = true; };
    // halfDuration = 0.1s

    // Advance past midpoint
    state.update(0.1f);
    EXPECT_FALSE(callbackCalled);

    // Complete fade-in
    state.update(0.1f);
    EXPECT_TRUE(callbackCalled);
}

// =========================================================================
// TransitionState — Update with NONE phase
// =========================================================================

TEST(TransitionStateTest, UpdateWithNoActiveTransitionDoesNothing) {
    TransitionState state;  // default: phase == NONE

    bool midpoint = state.update(1.0f);
    EXPECT_FALSE(midpoint);
    EXPECT_FALSE(state.isActive());
    EXPECT_FLOAT_EQ(state.overlayAlpha, 0.0f);
}

// =========================================================================
// TransitionState — Full Transition Sequence
// =========================================================================

TEST(TransitionStateTest, FullTransitionSequence) {
    TransitionState state;
    state.start(TransitionType::FADE_BLACK, "menu", 0.4f);
    // halfDuration = 0.2s

    // Frame 1: 0.05s into fade-out (25%)
    EXPECT_FALSE(state.update(0.05f));
    EXPECT_EQ(state.phase, TransitionPhase::FADING_OUT);
    EXPECT_NEAR(state.overlayAlpha, 0.25f, 0.01f);

    // Frame 2: 0.10s into fade-out (50%)
    EXPECT_FALSE(state.update(0.05f));
    EXPECT_EQ(state.phase, TransitionPhase::FADING_OUT);
    EXPECT_NEAR(state.overlayAlpha, 0.5f, 0.01f);

    // Frame 3: 0.15s into fade-out (75%)
    EXPECT_FALSE(state.update(0.05f));
    EXPECT_EQ(state.phase, TransitionPhase::FADING_OUT);
    EXPECT_NEAR(state.overlayAlpha, 0.75f, 0.01f);

    // Frame 4: 0.20s — midpoint reached
    EXPECT_TRUE(state.update(0.05f));
    EXPECT_EQ(state.phase, TransitionPhase::FADING_IN);

    // Frame 5: 0.05s into fade-in (75% alpha remaining)
    EXPECT_FALSE(state.update(0.05f));
    EXPECT_EQ(state.phase, TransitionPhase::FADING_IN);
    EXPECT_NEAR(state.overlayAlpha, 0.75f, 0.1f);

    // Frame 6-8: complete fade-in
    state.update(0.05f);
    state.update(0.05f);
    state.update(0.05f);

    EXPECT_FALSE(state.isActive());
    EXPECT_FLOAT_EQ(state.overlayAlpha, 0.0f);
}

// =========================================================================
// TransitionState — Large Delta Time
// =========================================================================

TEST(TransitionStateTest, LargeDeltaTimeCompletesTransitionInOneFrame) {
    TransitionState state;
    bool callbackCalled = false;
    state.start(TransitionType::FADE_BLACK, "target", 0.5f);
    state.onComplete = [&callbackCalled]() { callbackCalled = true; };

    // A very large delta time should complete the entire transition
    bool midpoint = state.update(10.0f);
    EXPECT_TRUE(midpoint);  // midpoint was crossed
    EXPECT_FALSE(state.isActive());
    EXPECT_TRUE(callbackCalled);
}

// =========================================================================
// TransitionType enum values
// =========================================================================

TEST(TransitionTypeTest, EnumValuesAreDefined) {
    EXPECT_NE(TransitionType::NONE, TransitionType::FADE_BLACK);
    EXPECT_NE(TransitionType::FADE_BLACK, TransitionType::CROSSFADE);
    EXPECT_NE(TransitionType::CROSSFADE, TransitionType::SLIDE_LEFT);
    EXPECT_NE(TransitionType::SLIDE_LEFT, TransitionType::SLIDE_RIGHT);
}

}  // namespace vde::test

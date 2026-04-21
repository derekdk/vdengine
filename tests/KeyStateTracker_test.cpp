/**
 * @file KeyStateTracker_test.cpp
 * @brief Unit tests for KeyStateTracker
 */

#include <vde/api/KeyCodes.h>
#include <vde/api/KeyStateTracker.h>

#include <gtest/gtest.h>

namespace vde::test {

class KeyStateTrackerTest : public ::testing::Test {
  protected:
    KeyStateTracker tracker;
};

// ---------------------------------------------------------------------------
// Held bindings
// ---------------------------------------------------------------------------

TEST_F(KeyStateTrackerTest, HeldReturnsFalseForUnknownName) {
    EXPECT_FALSE(tracker.isHeld("nonexistent"));
}

TEST_F(KeyStateTrackerTest, HeldReturnsFalseBeforePress) {
    tracker.bindHeld(vde::KEY_A, "left");
    EXPECT_FALSE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, HeldReturnsTrueWhilePressed) {
    tracker.bindHeld(vde::KEY_A, "left");
    tracker.handlePress(vde::KEY_A);
    EXPECT_TRUE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, HeldReturnsFalseAfterRelease) {
    tracker.bindHeld(vde::KEY_A, "left");
    tracker.handlePress(vde::KEY_A);
    tracker.handleRelease(vde::KEY_A);
    EXPECT_FALSE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, MultipleKeysSameHeldAction) {
    tracker.bindHeld(vde::KEY_A, "left");
    tracker.bindHeld(vde::KEY_LEFT, "left");

    tracker.handlePress(vde::KEY_A);
    EXPECT_TRUE(tracker.isHeld("left"));

    // Release A, but LEFT is not pressed — held should be false
    tracker.handleRelease(vde::KEY_A);
    EXPECT_FALSE(tracker.isHeld("left"));

    // Press both, release one — still held
    tracker.handlePress(vde::KEY_A);
    tracker.handlePress(vde::KEY_LEFT);
    EXPECT_TRUE(tracker.isHeld("left"));
    tracker.handleRelease(vde::KEY_A);
    EXPECT_TRUE(tracker.isHeld("left"));
    tracker.handleRelease(vde::KEY_LEFT);
    EXPECT_FALSE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, ReleaseWithoutPressSafelyIgnored) {
    tracker.bindHeld(vde::KEY_A, "left");
    tracker.handleRelease(vde::KEY_A);  // should not go negative
    EXPECT_FALSE(tracker.isHeld("left"));

    // Press/release still works normally after spurious release
    tracker.handlePress(vde::KEY_A);
    EXPECT_TRUE(tracker.isHeld("left"));
    tracker.handleRelease(vde::KEY_A);
    EXPECT_FALSE(tracker.isHeld("left"));
}

// ---------------------------------------------------------------------------
// One-shot bindings
// ---------------------------------------------------------------------------

TEST_F(KeyStateTrackerTest, ConsumeReturnsFalseForUnknownName) {
    EXPECT_FALSE(tracker.consume("nonexistent"));
}

TEST_F(KeyStateTrackerTest, ConsumeReturnsFalseBeforePress) {
    tracker.bindOneShot(vde::KEY_SPACE, "fire");
    EXPECT_FALSE(tracker.consume("fire"));
}

TEST_F(KeyStateTrackerTest, ConsumeReturnsTrueOnceAfterPress) {
    tracker.bindOneShot(vde::KEY_SPACE, "fire");
    tracker.handlePress(vde::KEY_SPACE);
    EXPECT_TRUE(tracker.consume("fire"));
    EXPECT_FALSE(tracker.consume("fire"));
}

TEST_F(KeyStateTrackerTest, OneShotResetsAfterConsume) {
    tracker.bindOneShot(vde::KEY_SPACE, "fire");
    tracker.handlePress(vde::KEY_SPACE);
    EXPECT_TRUE(tracker.consume("fire"));

    // A repeat press while the key is still held must NOT re-arm the action
    tracker.handlePress(vde::KEY_SPACE);  // OS repeat or redundant press
    EXPECT_FALSE(tracker.consume("fire"));

    // Only release + re-press re-arms the one-shot
    tracker.handleRelease(vde::KEY_SPACE);
    tracker.handlePress(vde::KEY_SPACE);
    EXPECT_TRUE(tracker.consume("fire"));
    EXPECT_FALSE(tracker.consume("fire"));
}

TEST_F(KeyStateTrackerTest, MultipleKeysSameOneShotAction) {
    tracker.bindOneShot(vde::KEY_SPACE, "fire");
    tracker.bindOneShot(vde::GAMEPAD_BUTTON_A, "fire");

    tracker.handlePress(vde::KEY_SPACE);
    EXPECT_TRUE(tracker.consume("fire"));
    EXPECT_FALSE(tracker.consume("fire"));

    tracker.handlePress(vde::GAMEPAD_BUTTON_A);
    EXPECT_TRUE(tracker.consume("fire"));
}

// ---------------------------------------------------------------------------
// Mixed held + one-shot bindings
// ---------------------------------------------------------------------------

TEST_F(KeyStateTrackerTest, HeldAndOneShotOnDifferentKeys) {
    tracker.bindHeld(vde::KEY_A, "move");
    tracker.bindOneShot(vde::KEY_SPACE, "fire");

    tracker.handlePress(vde::KEY_A);
    tracker.handlePress(vde::KEY_SPACE);

    EXPECT_TRUE(tracker.isHeld("move"));
    EXPECT_TRUE(tracker.consume("fire"));
    EXPECT_FALSE(tracker.consume("fire"));
    EXPECT_TRUE(tracker.isHeld("move"));  // still held

    tracker.handleRelease(vde::KEY_A);
    EXPECT_FALSE(tracker.isHeld("move"));
}

// ---------------------------------------------------------------------------
// Unbound key codes are safely ignored
// ---------------------------------------------------------------------------

TEST_F(KeyStateTrackerTest, UnboundKeyPressAndReleaseIgnored) {
    tracker.bindHeld(vde::KEY_A, "left");
    tracker.handlePress(999);    // unbound
    tracker.handleRelease(999);  // unbound
    EXPECT_FALSE(tracker.isHeld("left"));
}

// ---------------------------------------------------------------------------
// Repeat-event robustness (e.g. GLFW_REPEAT forwarded as another press)
// ---------------------------------------------------------------------------

TEST_F(KeyStateTrackerTest, RepeatPressDoesNotInflateHeldCount) {
    tracker.bindHeld(vde::KEY_A, "left");
    tracker.handlePress(vde::KEY_A);
    tracker.handlePress(vde::KEY_A);  // simulate GLFW_REPEAT forwarded as press
    tracker.handlePress(vde::KEY_A);
    EXPECT_TRUE(tracker.isHeld("left"));

    // A single release must fully clear the action
    tracker.handleRelease(vde::KEY_A);
    EXPECT_FALSE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, RepeatPressDoesNotFireOneShotMultipleTimes) {
    tracker.bindOneShot(vde::KEY_SPACE, "jump");
    tracker.handlePress(vde::KEY_SPACE);
    tracker.handlePress(vde::KEY_SPACE);  // simulate GLFW_REPEAT
    EXPECT_TRUE(tracker.consume("jump"));
    EXPECT_FALSE(tracker.consume("jump"));
}

// ---------------------------------------------------------------------------
// Documented API patterns — mirror examples from API-DOC.md and docs/API.md
// ---------------------------------------------------------------------------

// Mirrors the GameInputHandler constructor example in API-DOC.md:
// keys.bindHeld(vde::KEY_LEFT, "left"); keys.bindHeld(vde::KEY_A, "left"); ...
TEST_F(KeyStateTrackerTest, DocumentedPattern_TwoKeysOneHeldAction) {
    tracker.bindHeld(vde::KEY_LEFT, "left");
    tracker.bindHeld(vde::KEY_A, "left");
    tracker.bindHeld(vde::KEY_RIGHT, "right");
    tracker.bindHeld(vde::KEY_D, "right");

    // Arrow key triggers "left"
    tracker.handlePress(vde::KEY_LEFT);
    EXPECT_TRUE(tracker.isHeld("left"));
    EXPECT_FALSE(tracker.isHeld("right"));

    // A key also triggers "left"; both currently pressed
    tracker.handlePress(vde::KEY_A);
    EXPECT_TRUE(tracker.isHeld("left"));

    // Release arrow — A still holds "left"
    tracker.handleRelease(vde::KEY_LEFT);
    EXPECT_TRUE(tracker.isHeld("left"));

    // Release A — action drops
    tracker.handleRelease(vde::KEY_A);
    EXPECT_FALSE(tracker.isHeld("left"));
}

// Mirrors the one-shot bindings from API-DOC.md:
// keys.bindOneShot(vde::KEY_SPACE, "jump"); keys.bindOneShot(vde::KEY_F, "shoot");
TEST_F(KeyStateTrackerTest, DocumentedPattern_OneShotActions) {
    tracker.bindOneShot(vde::KEY_SPACE, "jump");
    tracker.bindOneShot(vde::KEY_F, "shoot");

    // Neither fires before any press
    EXPECT_FALSE(tracker.consume("jump"));
    EXPECT_FALSE(tracker.consume("shoot"));

    // SPACE fires "jump" once
    tracker.handlePress(vde::KEY_SPACE);
    EXPECT_TRUE(tracker.consume("jump"));
    EXPECT_FALSE(tracker.consume("jump"));   // consumed — won't fire again
    EXPECT_FALSE(tracker.consume("shoot"));  // unrelated action unaffected

    // F fires "shoot" once
    tracker.handlePress(vde::KEY_F);
    EXPECT_TRUE(tracker.consume("shoot"));
    EXPECT_FALSE(tracker.consume("shoot"));

    // Re-press fires again
    tracker.handleRelease(vde::KEY_SPACE);
    tracker.handlePress(vde::KEY_SPACE);
    EXPECT_TRUE(tracker.consume("jump"));
}

// ---------------------------------------------------------------------------
// Repeat/consume interaction regression tests
// ---------------------------------------------------------------------------

// Regression: consume() must not clear pressed state — an OS repeat arriving
// while the key is still physically held must NOT re-trigger the one-shot.
TEST_F(KeyStateTrackerTest, RepeatAfterConsumeDoesNotRetriggerOneShot) {
    tracker.bindOneShot(vde::KEY_SPACE, "fire");
    tracker.handlePress(vde::KEY_SPACE);
    EXPECT_TRUE(tracker.consume("fire"));

    // Key still held — OS sends repeat events
    tracker.handlePress(vde::KEY_SPACE);  // OS repeat
    tracker.handlePress(vde::KEY_SPACE);  // OS repeat
    EXPECT_FALSE(tracker.consume("fire"));

    // Only an actual release + re-press should re-arm the action
    tracker.handleRelease(vde::KEY_SPACE);
    tracker.handlePress(vde::KEY_SPACE);
    EXPECT_TRUE(tracker.consume("fire"));
}

// Regression: a key bound to both Held and OneShot — after consume() + repeat,
// the held count must stay at 1 (not drift upward) and clear on a single release.
TEST_F(KeyStateTrackerTest, RepeatAfterConsumeDoesNotInflateHeldCount) {
    tracker.bindHeld(vde::KEY_A, "move");
    tracker.bindOneShot(vde::KEY_A, "dash");

    tracker.handlePress(vde::KEY_A);
    EXPECT_TRUE(tracker.isHeld("move"));
    EXPECT_TRUE(tracker.consume("dash"));

    // OS sends repeat events while key is still physically held
    tracker.handlePress(vde::KEY_A);        // OS repeat
    tracker.handlePress(vde::KEY_A);        // OS repeat
    EXPECT_FALSE(tracker.consume("dash"));  // one-shot must not re-fire
    EXPECT_TRUE(tracker.isHeld("move"));    // held still active

    // A single release must fully drop the held action (no count inflation)
    tracker.handleRelease(vde::KEY_A);
    EXPECT_FALSE(tracker.isHeld("move"));
}

}  // namespace vde::test

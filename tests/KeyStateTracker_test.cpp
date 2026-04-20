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
    tracker.bindHeld(65 /* A */, "left");
    EXPECT_FALSE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, HeldReturnsTrueWhilePressed) {
    tracker.bindHeld(65, "left");
    tracker.handlePress(65);
    EXPECT_TRUE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, HeldReturnsFalseAfterRelease) {
    tracker.bindHeld(65, "left");
    tracker.handlePress(65);
    tracker.handleRelease(65);
    EXPECT_FALSE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, MultipleKeysSameHeldAction) {
    tracker.bindHeld(65 /* A */, "left");
    tracker.bindHeld(263 /* LEFT */, "left");

    tracker.handlePress(65);
    EXPECT_TRUE(tracker.isHeld("left"));

    // Release A, but LEFT is not pressed — held should be false
    tracker.handleRelease(65);
    EXPECT_FALSE(tracker.isHeld("left"));

    // Press both, release one — still held
    tracker.handlePress(65);
    tracker.handlePress(263);
    EXPECT_TRUE(tracker.isHeld("left"));
    tracker.handleRelease(65);
    EXPECT_TRUE(tracker.isHeld("left"));
    tracker.handleRelease(263);
    EXPECT_FALSE(tracker.isHeld("left"));
}

TEST_F(KeyStateTrackerTest, ReleaseWithoutPressSafelyIgnored) {
    tracker.bindHeld(65, "left");
    tracker.handleRelease(65);  // should not go negative
    EXPECT_FALSE(tracker.isHeld("left"));

    // Press/release still works normally after spurious release
    tracker.handlePress(65);
    EXPECT_TRUE(tracker.isHeld("left"));
    tracker.handleRelease(65);
    EXPECT_FALSE(tracker.isHeld("left"));
}

// ---------------------------------------------------------------------------
// One-shot bindings
// ---------------------------------------------------------------------------

TEST_F(KeyStateTrackerTest, ConsumeReturnsFalseForUnknownName) {
    EXPECT_FALSE(tracker.consume("nonexistent"));
}

TEST_F(KeyStateTrackerTest, ConsumeReturnsFalseBeforePress) {
    tracker.bindOneShot(32 /* SPACE */, "fire");
    EXPECT_FALSE(tracker.consume("fire"));
}

TEST_F(KeyStateTrackerTest, ConsumeReturnsTrueOnceAfterPress) {
    tracker.bindOneShot(32, "fire");
    tracker.handlePress(32);
    EXPECT_TRUE(tracker.consume("fire"));
    EXPECT_FALSE(tracker.consume("fire"));
}

TEST_F(KeyStateTrackerTest, OneShotResetsAfterConsume) {
    tracker.bindOneShot(32, "fire");
    tracker.handlePress(32);
    EXPECT_TRUE(tracker.consume("fire"));

    // Press again
    tracker.handlePress(32);
    EXPECT_TRUE(tracker.consume("fire"));
    EXPECT_FALSE(tracker.consume("fire"));
}

TEST_F(KeyStateTrackerTest, MultipleKeysSameOneShotAction) {
    tracker.bindOneShot(32, "fire");
    tracker.bindOneShot(0 /* GAMEPAD_BUTTON_A */, "fire");

    tracker.handlePress(32);
    EXPECT_TRUE(tracker.consume("fire"));
    EXPECT_FALSE(tracker.consume("fire"));

    tracker.handlePress(0);
    EXPECT_TRUE(tracker.consume("fire"));
}

// ---------------------------------------------------------------------------
// Mixed held + one-shot bindings
// ---------------------------------------------------------------------------

TEST_F(KeyStateTrackerTest, HeldAndOneShotOnDifferentKeys) {
    tracker.bindHeld(65, "move");
    tracker.bindOneShot(32, "fire");

    tracker.handlePress(65);
    tracker.handlePress(32);

    EXPECT_TRUE(tracker.isHeld("move"));
    EXPECT_TRUE(tracker.consume("fire"));
    EXPECT_FALSE(tracker.consume("fire"));
    EXPECT_TRUE(tracker.isHeld("move"));  // still held

    tracker.handleRelease(65);
    EXPECT_FALSE(tracker.isHeld("move"));
}

// ---------------------------------------------------------------------------
// Unbound key codes are safely ignored
// ---------------------------------------------------------------------------

TEST_F(KeyStateTrackerTest, UnboundKeyPressAndReleaseIgnored) {
    tracker.bindHeld(65, "left");
    tracker.handlePress(999);    // unbound
    tracker.handleRelease(999);  // unbound
    EXPECT_FALSE(tracker.isHeld("left"));
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

}  // namespace vde::test

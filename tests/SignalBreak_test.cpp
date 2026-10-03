#include "../games/signal_break/Gameplay.h"
#include "../games/signal_break/Input.h"
#include <gtest/gtest.h>

namespace vde::test {

TEST(SignalBreakGameplayTest, GroundedJumpAndAirJumpRejection) {
    signalbreak::Gameplay world;
    for (int frame = 0; frame < 30; ++frame) {
        world.update(1.0f / 60.0f, {});
    }
    ASSERT_TRUE(world.grounded());
    EXPECT_NEAR(world.playerPosition().y, 0.7f, 0.03f);
    world.update(1.0f / 60.0f, {.jump = true});
    EXPECT_GT(world.playerVelocity().y, 11.0f);
    world.update(1.0f / 60.0f, {.jump = true});
    EXPECT_EQ(world.jumps(), 1);
    for (int frame = 0; frame < 90; ++frame) {
        world.update(1.0f / 60.0f, {});
    }
    EXPECT_TRUE(world.grounded());
}

TEST(SignalBreakGameplayTest, MovementPauseAndRestart) {
    signalbreak::Gameplay world;
    for (int frame = 0; frame < 60; ++frame) {
        world.update(1.0f / 60.0f, {.move = 1.0f, .fire = true});
    }
    EXPECT_GT(world.playerPosition().x, 10.0f);
    EXPECT_GT(world.shots(), 5);
    world.update(1.0f / 60.0f, {.pause = true});
    const auto position = world.playerPosition();
    const float time = world.time();
    world.update(1.0f, {.move = 1.0f, .jump = true});
    EXPECT_EQ(world.playerPosition(), position);
    EXPECT_FLOAT_EQ(world.time(), time);
    world.update(1.0f / 60.0f, {.restart = true});
    EXPECT_FALSE(world.paused());
    EXPECT_FLOAT_EQ(world.playerPosition().x, 3.0f);
    EXPECT_EQ(world.health(), signalbreak::MAX_HEALTH);
    EXPECT_EQ(world.shots(), 0);
}

TEST(SignalBreakGameplayTest, SweptShotsDamageEnemiesAndEnemiesFireBack) {
    signalbreak::Gameplay world;
    world.reset(15.0f);
    for (int frame = 0; frame < 150; ++frame) {
        world.update(1.0f / 60.0f, {.fire = true});
    }
    EXPECT_GT(world.kills(), 0);
    EXPECT_EQ(world.enemies()[1].health, 0);
    EXPECT_GT(world.enemyShots(), 0);
    EXPECT_LE(world.projectiles().size(), 96u);
}

TEST(SignalBreakGameplayTest, FallingEndsRunAndJumpRetriesCheckpoint) {
    signalbreak::Gameplay world;
    world.reset(41.5f);
    for (int frame = 0; frame < 100; ++frame) {
        world.update(1.0f / 60.0f, {});
    }
    EXPECT_EQ(world.phase(), signalbreak::RunPhase::Down);
    EXPECT_EQ(world.health(), 0);
    world.update(1.0f / 60.0f, {.jump = true});
    EXPECT_EQ(world.phase(), signalbreak::RunPhase::Playing);
    EXPECT_EQ(world.health(), signalbreak::MAX_HEALTH);
}

TEST(SignalBreakGameplayTest, PlatformsBlockShotsAndDamageHasGracePeriod) {
    signalbreak::Gameplay world;
    world.reset(11.0f);
    for (int frame = 0; frame < 80; ++frame) {
        world.update(1.0f / 60.0f, {.aim = 1.0f, .fire = true});
    }
    EXPECT_EQ(world.enemies()[0].health, 2);
    world.reset(18.0f);
    for (int frame = 0; frame < 72; ++frame) {
        world.update(1.0f / 60.0f, {});
    }
    EXPECT_EQ(world.health(), signalbreak::MAX_HEALTH - 1);
    for (int frame = 0; frame < 12; ++frame) {
        world.update(1.0f / 60.0f, {});
    }
    EXPECT_EQ(world.health(), signalbreak::MAX_HEALTH - 1);
}

TEST(SignalBreakGameplayTest, CompleteRunCrossesGapsAndUnlocksExtraction) {
    signalbreak::Gameplay world;
    for (int frame = 0; frame < 1800 && world.phase() == signalbreak::RunPhase::Playing; ++frame) {
        const auto position = world.playerPosition();
        signalbreak::InputFrame input;
        input.fire = true;
        input.move = position.x > 106.0f && world.enemies().back().health > 0 ? 0.0f : 1.0f;
        input.jump = world.grounded() && ((position.x > 38.0f && position.x < 40.0f) ||
                                          (position.x > 78.0f && position.x < 80.0f));
        world.update(1.0f / 60.0f, input);
    }
    EXPECT_EQ(world.phase(), signalbreak::RunPhase::Complete);
    EXPECT_EQ(world.checkpoint(), 84.0f);
    EXPECT_GT(world.playerPosition().x, 117.0f);
    EXPECT_EQ(world.enemies().back().health, 0);
    EXPECT_GE(world.jumps(), 2);
    world.update(1.0f / 60.0f, {.jump = true});
    EXPECT_EQ(world.phase(), signalbreak::RunPhase::Playing);
    EXPECT_EQ(world.checkpoint(), 3.0f);
}

TEST(SignalBreakGameplayTest, CheckpointRetryRestoresRunWithoutTeleportingBackToStart) {
    signalbreak::Gameplay world;
    world.reset(44.0f);
    for (int frame = 0; frame < 30; ++frame) {
        world.update(1.0f / 60.0f, {.move = -1.0f});
    }
    for (int frame = 0; frame < 100; ++frame) {
        world.update(1.0f / 60.0f, {});
    }
    ASSERT_EQ(world.phase(), signalbreak::RunPhase::Down);
    world.update(1.0f / 60.0f, {.jump = true});
    EXPECT_EQ(world.phase(), signalbreak::RunPhase::Playing);
    EXPECT_EQ(world.playerPosition().x, 44.0f);
    EXPECT_EQ(world.health(), signalbreak::MAX_HEALTH);
}

TEST(SignalBreakInputTest, KeyboardAliasesAndOneShotActions) {
    signalbreak::SignalBreakInput input;
    input.onKeyPress(KEY_D);
    input.onKeyPress(KEY_RIGHT);
    input.onKeyRelease(KEY_D);
    input.onKeyPress(KEY_SPACE);
    input.onKeyPress(KEY_J);
    auto frame = input.sample();
    EXPECT_FLOAT_EQ(frame.move, 1.0f);
    EXPECT_TRUE(frame.fire);
    EXPECT_TRUE(frame.jump);
    EXPECT_FALSE(input.sample().jump);
    input.onKeyRelease(KEY_RIGHT);
    EXPECT_FLOAT_EQ(input.sample().move, 0.0f);
}

TEST(SignalBreakInputTest, ControllerAxesButtonsAndDisconnect) {
    signalbreak::SignalBreakInput input;
    input.onGamepadAxis(1, GAMEPAD_AXIS_LEFT_X, 0.1f);
    EXPECT_FLOAT_EQ(input.sample().move, 0.0f);
    input.onGamepadAxis(1, GAMEPAD_AXIS_LEFT_X, 0.8f);
    input.onGamepadAxis(1, GAMEPAD_AXIS_LEFT_Y, -1.0f);
    input.onGamepadAxis(1, GAMEPAD_AXIS_RIGHT_TRIGGER, 0.9f);
    input.onGamepadButtonPress(1, GAMEPAD_BUTTON_A);
    auto frame = input.sample();
    EXPECT_FLOAT_EQ(frame.move, 0.8f);
    EXPECT_FLOAT_EQ(frame.aim, 1.0f);
    EXPECT_TRUE(frame.fire);
    EXPECT_TRUE(frame.jump);
    EXPECT_FALSE(input.sample().jump);
    input.onGamepadDisconnect(1);
    frame = input.sample();
    EXPECT_FLOAT_EQ(frame.move, 0.0f);
    EXPECT_FALSE(frame.fire);
    input.onGamepadButtonPress(1, GAMEPAD_BUTTON_A);
    EXPECT_TRUE(input.sample().jump);
}

TEST(SignalBreakInputTest, IndependentFireSourcesAndSimultaneousActions) {
    signalbreak::SignalBreakInput input;
    input.onGamepadButtonPress(0, GAMEPAD_BUTTON_X);
    input.onGamepadButtonPress(0, GAMEPAD_BUTTON_RIGHT_BUMPER);
    input.onGamepadButtonRelease(0, GAMEPAD_BUTTON_X);
    EXPECT_TRUE(input.sample().fire);
    input.onGamepadButtonRelease(0, GAMEPAD_BUTTON_RIGHT_BUMPER);
    EXPECT_FALSE(input.sample().fire);
    input.onKeyPress(KEY_R);
    input.onGamepadButtonPress(0, GAMEPAD_BUTTON_Y);
    input.onGamepadButtonPress(0, GAMEPAD_BUTTON_START);
    EXPECT_TRUE(input.sample().restart);
    EXPECT_FALSE(input.sample().restart);
    input.onGamepadButtonPress(1, GAMEPAD_BUTTON_START);
    EXPECT_TRUE(input.sample().pause);
    EXPECT_FALSE(input.sample().pause);
    input.onGamepadAxis(-1, GAMEPAD_AXIS_LEFT_X, 1.0f);
    input.onGamepadButtonPress(MAX_GAMEPADS, GAMEPAD_BUTTON_A);
    EXPECT_FALSE(input.sample().jump);
}

}  // namespace vde::test
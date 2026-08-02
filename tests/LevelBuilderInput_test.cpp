#include "../games/level_builder/Input.h"
#include <gtest/gtest.h>

namespace levelbuilder::test {

TEST(LevelBuilderInputTest, MoveModeKeepsControllerMovementAndDepthBindingsDistinct) {
    LevelBuilderInput input;
    input.setMode(LevelBuilderInputMode::DevelopmentMove);

    input.onGamepadButtonPress(0, vde::GAMEPAD_BUTTON_DPAD_LEFT);
    EXPECT_TRUE(input.actions().isPressed("move_left"));
    EXPECT_FALSE(input.actions().isPressed("layer_depth_down"));

    input.finishFrame();
    input.onGamepadAxis(0, vde::GAMEPAD_AXIS_LEFT_TRIGGER, 1.0f);
    EXPECT_TRUE(input.actions().isPressed("layer_depth_down"));
    EXPECT_FALSE(input.actions().isPressed("undo_tile_edit"));
}

TEST(LevelBuilderInputTest, ModeSwapReplacesKeyboardAndControllerActions) {
    LevelBuilderInput input;
    input.setMode(LevelBuilderInputMode::DevelopmentMove);

    EXPECT_TRUE(input.actions().hasAction("add_layer"));
    EXPECT_TRUE(input.actions().hasAction("layer_depth_up"));
    EXPECT_FALSE(input.actions().hasAction("paste_tile"));
    input.onKeyPress(vde::KEY_P);
    EXPECT_TRUE(input.actions().isPressed("layer_depth_up"));
    EXPECT_FALSE(input.actions().isPressed("redo_tile_edit"));

    input.setMode(LevelBuilderInputMode::DevelopmentSelectTile);

    EXPECT_FALSE(input.actions().hasAction("add_layer"));
    EXPECT_FALSE(input.actions().hasAction("layer_depth_up"));
    EXPECT_TRUE(input.actions().hasAction("paste_tile"));
    EXPECT_TRUE(input.actions().hasAction("redo_tile_edit"));
    input.onKeyPress(vde::KEY_I);
    EXPECT_TRUE(input.actions().isPressed("redo_tile_edit"));
    EXPECT_FALSE(input.actions().isPressed("layer_depth_up"));
}

TEST(LevelBuilderInputTest, ModeSwapPreservesHeldMovementWithoutSynthesizingActionEdges) {
    LevelBuilderInput input;
    input.setMode(LevelBuilderInputMode::DevelopmentMove);
    input.onGamepadButtonPress(0, vde::GAMEPAD_BUTTON_DPAD_LEFT);
    input.onGamepadAxis(0, vde::GAMEPAD_AXIS_LEFT_TRIGGER, 1.0f);

    input.setMode(LevelBuilderInputMode::DevelopmentSelectTile);

    EXPECT_TRUE(input.actions().isHeld("move_left"));
    EXPECT_FALSE(input.actions().isPressed("move_left"));
    EXPECT_TRUE(input.actions().isHeld("undo_tile_edit"));
    EXPECT_FALSE(input.actions().isPressed("undo_tile_edit"));
}

TEST(LevelBuilderInputTest, DisconnectedGamepadStateIsNotReplayedOnModeSwap) {
    LevelBuilderInput input;
    input.setMode(LevelBuilderInputMode::DevelopmentMove);
    input.onGamepadButtonPress(0, vde::GAMEPAD_BUTTON_DPAD_LEFT);
    input.onGamepadAxis(0, vde::GAMEPAD_AXIS_LEFT_TRIGGER, 1.0f);

    input.onGamepadDisconnect(0);
    input.setMode(LevelBuilderInputMode::DevelopmentSelectTile);

    EXPECT_FALSE(input.actions().isHeld("move_left"));
    EXPECT_FALSE(input.actions().isHeld("undo_tile_edit"));
}

}  // namespace levelbuilder::test
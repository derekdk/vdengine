/**
 * @file Joystick_test.cpp
 * @brief Unit tests for joystick/gamepad input support
 *
 * Tests KeyCodes gamepad constants, InputHandler gamepad state tracking,
 * and event dispatching.
 */

#include <vde/api/InputHandler.h>
#include <vde/api/KeyCodes.h>

#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace vde;

// ============================================================================
// KeyCodes Gamepad Constants Tests
// ============================================================================

TEST(GamepadKeyCodes, JoystickIdsAreSequential) {
    EXPECT_EQ(JOYSTICK_1, 0);
    EXPECT_EQ(JOYSTICK_2, 1);
    EXPECT_EQ(JOYSTICK_16, 15);
    EXPECT_EQ(JOYSTICK_LAST, JOYSTICK_16);
}

TEST(GamepadKeyCodes, GamepadButtonValues) {
    EXPECT_EQ(GAMEPAD_BUTTON_A, 0);
    EXPECT_EQ(GAMEPAD_BUTTON_B, 1);
    EXPECT_EQ(GAMEPAD_BUTTON_X, 2);
    EXPECT_EQ(GAMEPAD_BUTTON_Y, 3);
    EXPECT_EQ(GAMEPAD_BUTTON_LEFT_BUMPER, 4);
    EXPECT_EQ(GAMEPAD_BUTTON_RIGHT_BUMPER, 5);
    EXPECT_EQ(GAMEPAD_BUTTON_BACK, 6);
    EXPECT_EQ(GAMEPAD_BUTTON_START, 7);
    EXPECT_EQ(GAMEPAD_BUTTON_GUIDE, 8);
    EXPECT_EQ(GAMEPAD_BUTTON_LEFT_THUMB, 9);
    EXPECT_EQ(GAMEPAD_BUTTON_RIGHT_THUMB, 10);
    EXPECT_EQ(GAMEPAD_BUTTON_DPAD_UP, 11);
    EXPECT_EQ(GAMEPAD_BUTTON_DPAD_RIGHT, 12);
    EXPECT_EQ(GAMEPAD_BUTTON_DPAD_DOWN, 13);
    EXPECT_EQ(GAMEPAD_BUTTON_DPAD_LEFT, 14);
    EXPECT_EQ(GAMEPAD_BUTTON_LAST, GAMEPAD_BUTTON_DPAD_LEFT);
}

TEST(GamepadKeyCodes, PlayStationAliases) {
    EXPECT_EQ(GAMEPAD_BUTTON_CROSS, GAMEPAD_BUTTON_A);
    EXPECT_EQ(GAMEPAD_BUTTON_CIRCLE, GAMEPAD_BUTTON_B);
    EXPECT_EQ(GAMEPAD_BUTTON_SQUARE, GAMEPAD_BUTTON_X);
    EXPECT_EQ(GAMEPAD_BUTTON_TRIANGLE, GAMEPAD_BUTTON_Y);
}

TEST(GamepadKeyCodes, GamepadAxisValues) {
    EXPECT_EQ(GAMEPAD_AXIS_LEFT_X, 0);
    EXPECT_EQ(GAMEPAD_AXIS_LEFT_Y, 1);
    EXPECT_EQ(GAMEPAD_AXIS_RIGHT_X, 2);
    EXPECT_EQ(GAMEPAD_AXIS_RIGHT_Y, 3);
    EXPECT_EQ(GAMEPAD_AXIS_LEFT_TRIGGER, 4);
    EXPECT_EQ(GAMEPAD_AXIS_RIGHT_TRIGGER, 5);
    EXPECT_EQ(GAMEPAD_AXIS_LAST, GAMEPAD_AXIS_RIGHT_TRIGGER);
}

TEST(GamepadKeyCodes, MaxConstants) {
    EXPECT_EQ(MAX_GAMEPADS, 16);
    EXPECT_EQ(MAX_GAMEPAD_BUTTONS, 15);
    EXPECT_EQ(MAX_GAMEPAD_AXES, 6);
}

TEST(GamepadKeyCodes, DefaultDeadZone) {
    EXPECT_FLOAT_EQ(GAMEPAD_AXIS_DEADZONE, 0.1f);
}

// ============================================================================
// InputHandler Gamepad State Tests
// ============================================================================

// Concrete test subclass that records events
class TestGamepadHandler : public InputHandler {
  public:
    struct ButtonEvent {
        int gamepadId;
        int button;
        bool pressed;
    };

    struct AxisEvent {
        int gamepadId;
        int axis;
        float value;
    };

    struct ConnectEvent {
        int gamepadId;
        std::string name;
        bool connected;
    };

    std::vector<ButtonEvent> buttonEvents;
    std::vector<AxisEvent> axisEvents;
    std::vector<ConnectEvent> connectEvents;

    void onGamepadButtonPress(int gamepadId, int button) override {
        buttonEvents.push_back({gamepadId, button, true});
    }

    void onGamepadButtonRelease(int gamepadId, int button) override {
        buttonEvents.push_back({gamepadId, button, false});
    }

    void onGamepadAxis(int gamepadId, int axis, float value) override {
        axisEvents.push_back({gamepadId, axis, value});
    }

    void onGamepadConnect(int gamepadId, const char* name) override {
        connectEvents.push_back({gamepadId, name ? name : "", true});
    }

    void onGamepadDisconnect(int gamepadId) override {
        connectEvents.push_back({gamepadId, "", false});
    }
};

class InputHandlerGamepadTest : public ::testing::Test {
  protected:
    TestGamepadHandler handler;
};

TEST_F(InputHandlerGamepadTest, DefaultConnectionState) {
    for (int i = 0; i < MAX_GAMEPADS; ++i) {
        EXPECT_FALSE(handler.isGamepadConnected(i));
    }
}

TEST_F(InputHandlerGamepadTest, DefaultButtonState) {
    for (int btn = 0; btn <= GAMEPAD_BUTTON_LAST; ++btn) {
        EXPECT_FALSE(handler.isGamepadButtonPressed(JOYSTICK_1, btn));
    }
}

TEST_F(InputHandlerGamepadTest, DefaultAxisState) {
    for (int axis = 0; axis <= GAMEPAD_AXIS_LAST; ++axis) {
        EXPECT_FLOAT_EQ(handler.getGamepadAxis(JOYSTICK_1, axis), 0.0f);
    }
}

TEST_F(InputHandlerGamepadTest, SetAndQueryConnection) {
    handler._setGamepadConnected(JOYSTICK_1, true);
    EXPECT_TRUE(handler.isGamepadConnected(JOYSTICK_1));
    EXPECT_FALSE(handler.isGamepadConnected(JOYSTICK_2));

    handler._setGamepadConnected(JOYSTICK_1, false);
    EXPECT_FALSE(handler.isGamepadConnected(JOYSTICK_1));
}

TEST_F(InputHandlerGamepadTest, SetAndQueryButtons) {
    handler._setGamepadButton(JOYSTICK_1, GAMEPAD_BUTTON_A, true);
    EXPECT_TRUE(handler.isGamepadButtonPressed(JOYSTICK_1, GAMEPAD_BUTTON_A));
    EXPECT_FALSE(handler.isGamepadButtonPressed(JOYSTICK_1, GAMEPAD_BUTTON_B));

    handler._setGamepadButton(JOYSTICK_1, GAMEPAD_BUTTON_A, false);
    EXPECT_FALSE(handler.isGamepadButtonPressed(JOYSTICK_1, GAMEPAD_BUTTON_A));
}

TEST_F(InputHandlerGamepadTest, SetAndQueryAxes) {
    handler._setGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X, 0.75f);
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X), 0.75f);
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_Y), 0.0f);

    handler._setGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X, -0.5f);
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X), -0.5f);
}

TEST_F(InputHandlerGamepadTest, MultipleGamepadsIndependent) {
    handler._setGamepadButton(JOYSTICK_1, GAMEPAD_BUTTON_A, true);
    handler._setGamepadButton(JOYSTICK_2, GAMEPAD_BUTTON_B, true);

    EXPECT_TRUE(handler.isGamepadButtonPressed(JOYSTICK_1, GAMEPAD_BUTTON_A));
    EXPECT_FALSE(handler.isGamepadButtonPressed(JOYSTICK_1, GAMEPAD_BUTTON_B));
    EXPECT_FALSE(handler.isGamepadButtonPressed(JOYSTICK_2, GAMEPAD_BUTTON_A));
    EXPECT_TRUE(handler.isGamepadButtonPressed(JOYSTICK_2, GAMEPAD_BUTTON_B));
}

TEST_F(InputHandlerGamepadTest, InvalidGamepadIdReturnsDefaults) {
    EXPECT_FALSE(handler.isGamepadConnected(-1));
    EXPECT_FALSE(handler.isGamepadConnected(MAX_GAMEPADS));
    EXPECT_FALSE(handler.isGamepadButtonPressed(-1, GAMEPAD_BUTTON_A));
    EXPECT_FALSE(handler.isGamepadButtonPressed(MAX_GAMEPADS, GAMEPAD_BUTTON_A));
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(-1, GAMEPAD_AXIS_LEFT_X), 0.0f);
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(MAX_GAMEPADS, GAMEPAD_AXIS_LEFT_X), 0.0f);
}

TEST_F(InputHandlerGamepadTest, InvalidButtonOrAxisReturnsDefaults) {
    EXPECT_FALSE(handler.isGamepadButtonPressed(JOYSTICK_1, -1));
    EXPECT_FALSE(handler.isGamepadButtonPressed(JOYSTICK_1, GAMEPAD_BUTTON_LAST + 1));
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(JOYSTICK_1, -1), 0.0f);
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LAST + 1), 0.0f);
}

TEST_F(InputHandlerGamepadTest, DeadZoneGetSet) {
    EXPECT_FLOAT_EQ(handler.getDeadZone(), GAMEPAD_AXIS_DEADZONE);

    handler.setDeadZone(0.25f);
    EXPECT_FLOAT_EQ(handler.getDeadZone(), 0.25f);

    // Negative values become positive
    handler.setDeadZone(-0.15f);
    EXPECT_FLOAT_EQ(handler.getDeadZone(), 0.15f);
}

// ============================================================================
// Event Callback Tests
// ============================================================================

TEST_F(InputHandlerGamepadTest, ButtonPressEventFires) {
    handler.onGamepadButtonPress(JOYSTICK_1, GAMEPAD_BUTTON_A);
    ASSERT_EQ(handler.buttonEvents.size(), 1u);
    EXPECT_EQ(handler.buttonEvents[0].gamepadId, JOYSTICK_1);
    EXPECT_EQ(handler.buttonEvents[0].button, GAMEPAD_BUTTON_A);
    EXPECT_TRUE(handler.buttonEvents[0].pressed);
}

TEST_F(InputHandlerGamepadTest, ButtonReleaseEventFires) {
    handler.onGamepadButtonRelease(JOYSTICK_2, GAMEPAD_BUTTON_START);
    ASSERT_EQ(handler.buttonEvents.size(), 1u);
    EXPECT_EQ(handler.buttonEvents[0].gamepadId, JOYSTICK_2);
    EXPECT_EQ(handler.buttonEvents[0].button, GAMEPAD_BUTTON_START);
    EXPECT_FALSE(handler.buttonEvents[0].pressed);
}

TEST_F(InputHandlerGamepadTest, AxisChangeEventFires) {
    handler.onGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X, 0.85f);
    ASSERT_EQ(handler.axisEvents.size(), 1u);
    EXPECT_EQ(handler.axisEvents[0].gamepadId, JOYSTICK_1);
    EXPECT_EQ(handler.axisEvents[0].axis, GAMEPAD_AXIS_LEFT_X);
    EXPECT_FLOAT_EQ(handler.axisEvents[0].value, 0.85f);
}

TEST_F(InputHandlerGamepadTest, ConnectEventFires) {
    handler.onGamepadConnect(JOYSTICK_1, "Xbox Controller");
    ASSERT_EQ(handler.connectEvents.size(), 1u);
    EXPECT_EQ(handler.connectEvents[0].gamepadId, JOYSTICK_1);
    EXPECT_EQ(handler.connectEvents[0].name, "Xbox Controller");
    EXPECT_TRUE(handler.connectEvents[0].connected);
}

TEST_F(InputHandlerGamepadTest, DisconnectEventFires) {
    handler.onGamepadDisconnect(JOYSTICK_1);
    ASSERT_EQ(handler.connectEvents.size(), 1u);
    EXPECT_EQ(handler.connectEvents[0].gamepadId, JOYSTICK_1);
    EXPECT_FALSE(handler.connectEvents[0].connected);
}

TEST_F(InputHandlerGamepadTest, FullConnectionLifecycle) {
    // Connect
    handler._setGamepadConnected(JOYSTICK_1, true);
    handler.onGamepadConnect(JOYSTICK_1, "Test Pad");
    EXPECT_TRUE(handler.isGamepadConnected(JOYSTICK_1));

    // Press buttons, move axes
    handler._setGamepadButton(JOYSTICK_1, GAMEPAD_BUTTON_A, true);
    handler.onGamepadButtonPress(JOYSTICK_1, GAMEPAD_BUTTON_A);
    handler._setGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X, 0.5f);
    handler.onGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X, 0.5f);

    EXPECT_TRUE(handler.isGamepadButtonPressed(JOYSTICK_1, GAMEPAD_BUTTON_A));
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X), 0.5f);

    // Disconnect — state should be cleared manually (as the engine does)
    handler._setGamepadConnected(JOYSTICK_1, false);
    for (int btn = 0; btn <= GAMEPAD_BUTTON_LAST; ++btn)
        handler._setGamepadButton(JOYSTICK_1, btn, false);
    for (int axis = 0; axis <= GAMEPAD_AXIS_LAST; ++axis)
        handler._setGamepadAxis(JOYSTICK_1, axis, 0.0f);

    handler.onGamepadDisconnect(JOYSTICK_1);
    EXPECT_FALSE(handler.isGamepadConnected(JOYSTICK_1));
    EXPECT_FALSE(handler.isGamepadButtonPressed(JOYSTICK_1, GAMEPAD_BUTTON_A));
    EXPECT_FLOAT_EQ(handler.getGamepadAxis(JOYSTICK_1, GAMEPAD_AXIS_LEFT_X), 0.0f);
}

// ============================================================================
// Default InputHandler (no overrides) should not crash
// ============================================================================

TEST(InputHandlerDefaults, GamepadMethodsDoNotCrash) {
    InputHandler handler;
    handler.onGamepadConnect(0, "Test");
    handler.onGamepadDisconnect(0);
    handler.onGamepadButtonPress(0, 0);
    handler.onGamepadButtonRelease(0, 0);
    handler.onGamepadAxis(0, 0, 0.5f);
}

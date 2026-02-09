#pragma once

/**
 * @file InputHandler.h
 * @brief Input handling interface for VDE games
 *
 * Provides an abstract interface for handling keyboard, mouse,
 * and gamepad input that games can implement.
 */

#include <array>
#include <cmath>

#include "KeyCodes.h"

namespace vde {

/**
 * @brief Abstract interface for handling game input.
 *
 * Games should inherit from this class and override the methods
 * they need to handle input events.
 *
 * Keyboard and mouse input is delivered via GLFW callbacks. Gamepad
 * input is polled each frame by the engine and delivered as press/release
 * and axis-change events.
 *
 * @example
 * @code
 * class MyInputHandler : public vde::InputHandler {
 * public:
 *     void onKeyPress(int key) override {
 *         if (key == vde::KEY_ESCAPE) {
 *             // Handle escape key
 *         }
 *     }
 *
 *     void onGamepadButtonPress(int gamepadId, int button) override {
 *         if (button == vde::GAMEPAD_BUTTON_A) {
 *             // Handle A button
 *         }
 *     }
 *
 *     void onGamepadAxis(int gamepadId, int axis, float value) override {
 *         if (axis == vde::GAMEPAD_AXIS_LEFT_X) {
 *             m_moveX = value;
 *         }
 *     }
 * };
 * @endcode
 */
class InputHandler {
  public:
    virtual ~InputHandler() = default;

    // ========================================================================
    // Keyboard events
    // ========================================================================

    /**
     * @brief Called when a key is pressed.
     * @param key The key code (see KeyCodes.h)
     */
    virtual void onKeyPress([[maybe_unused]] int key) {}

    /**
     * @brief Called when a key is released.
     * @param key The key code (see KeyCodes.h)
     */
    virtual void onKeyRelease([[maybe_unused]] int key) {}

    /**
     * @brief Called when a key is held down (repeated).
     * @param key The key code (see KeyCodes.h)
     */
    virtual void onKeyRepeat([[maybe_unused]] int key) {}

    /**
     * @brief Called for character input (for text entry).
     * @param codepoint Unicode code point
     */
    virtual void onCharInput([[maybe_unused]] unsigned int codepoint) {}

    // ========================================================================
    // Mouse events
    // ========================================================================

    /**
     * @brief Called when a mouse button is pressed.
     * @param button The mouse button (see KeyCodes.h)
     * @param x Mouse X position in pixels
     * @param y Mouse Y position in pixels
     */
    virtual void onMouseButtonPress([[maybe_unused]] int button, [[maybe_unused]] double x,
                                    [[maybe_unused]] double y) {}

    /**
     * @brief Called when a mouse button is released.
     * @param button The mouse button (see KeyCodes.h)
     * @param x Mouse X position in pixels
     * @param y Mouse Y position in pixels
     */
    virtual void onMouseButtonRelease([[maybe_unused]] int button, [[maybe_unused]] double x,
                                      [[maybe_unused]] double y) {}

    /**
     * @brief Called when the mouse is moved.
     * @param x New mouse X position in pixels
     * @param y New mouse Y position in pixels
     */
    virtual void onMouseMove([[maybe_unused]] double x, [[maybe_unused]] double y) {}

    /**
     * @brief Called when the mouse scroll wheel is used.
     * @param xOffset Horizontal scroll offset
     * @param yOffset Vertical scroll offset
     */
    virtual void onMouseScroll([[maybe_unused]] double xOffset, [[maybe_unused]] double yOffset) {}

    /**
     * @brief Called when the mouse enters the window.
     */
    virtual void onMouseEnter() {}

    /**
     * @brief Called when the mouse leaves the window.
     */
    virtual void onMouseLeave() {}

    // ========================================================================
    // Gamepad/joystick events
    // ========================================================================

    /**
     * @brief Called when a gamepad is connected.
     * @param gamepadId The gamepad slot (JOYSTICK_1 through JOYSTICK_16)
     * @param name Human-readable name of the gamepad (e.g. "Xbox Controller")
     */
    virtual void onGamepadConnect([[maybe_unused]] int gamepadId,
                                  [[maybe_unused]] const char* name) {}

    /**
     * @brief Called when a gamepad is disconnected.
     * @param gamepadId The gamepad slot
     */
    virtual void onGamepadDisconnect([[maybe_unused]] int gamepadId) {}

    /**
     * @brief Called when a gamepad button is pressed.
     * @param gamepadId The gamepad slot (JOYSTICK_1 through JOYSTICK_16)
     * @param button The button index (see GAMEPAD_BUTTON_* in KeyCodes.h)
     */
    virtual void onGamepadButtonPress([[maybe_unused]] int gamepadId, [[maybe_unused]] int button) {
    }

    /**
     * @brief Called when a gamepad button is released.
     * @param gamepadId The gamepad slot (JOYSTICK_1 through JOYSTICK_16)
     * @param button The button index (see GAMEPAD_BUTTON_* in KeyCodes.h)
     */
    virtual void onGamepadButtonRelease([[maybe_unused]] int gamepadId,
                                        [[maybe_unused]] int button) {}

    /**
     * @brief Called when a gamepad axis value changes beyond the dead zone.
     * @param gamepadId The gamepad slot (JOYSTICK_1 through JOYSTICK_16)
     * @param axis The axis index (see GAMEPAD_AXIS_* in KeyCodes.h)
     * @param value The axis value (-1.0 to 1.0 for sticks, 0.0 to 1.0 for triggers)
     */
    virtual void onGamepadAxis([[maybe_unused]] int gamepadId, [[maybe_unused]] int axis,
                               [[maybe_unused]] float value) {}

    // ========================================================================
    // Query methods (for polling input state)
    // ========================================================================

    /**
     * @brief Check if a key is currently pressed.
     * @param key The key code
     * @return true if the key is pressed
     */
    virtual bool isKeyPressed([[maybe_unused]] int key) const { return false; }

    /**
     * @brief Check if a mouse button is currently pressed.
     * @param button The mouse button
     * @return true if the button is pressed
     */
    virtual bool isMouseButtonPressed([[maybe_unused]] int button) const { return false; }

    /**
     * @brief Get the current mouse position.
     * @param x Output: mouse X position
     * @param y Output: mouse Y position
     */
    virtual void getMousePosition(double& x, double& y) const {
        x = 0;
        y = 0;
    }

    /**
     * @brief Check if a gamepad is currently connected.
     * @param gamepadId The gamepad slot (JOYSTICK_1 through JOYSTICK_16)
     * @return true if the gamepad is connected and has a valid mapping
     */
    bool isGamepadConnected(int gamepadId) const {
        if (gamepadId < 0 || gamepadId >= MAX_GAMEPADS)
            return false;
        return m_gamepadConnected[gamepadId];
    }

    /**
     * @brief Check if a gamepad button is currently pressed.
     * @param gamepadId The gamepad slot
     * @param button The button index (see GAMEPAD_BUTTON_* in KeyCodes.h)
     * @return true if the button is pressed
     */
    bool isGamepadButtonPressed(int gamepadId, int button) const {
        if (gamepadId < 0 || gamepadId >= MAX_GAMEPADS)
            return false;
        if (button < 0 || button > GAMEPAD_BUTTON_LAST)
            return false;
        return m_gamepadButtons[gamepadId][button];
    }

    /**
     * @brief Get the current value of a gamepad axis.
     * @param gamepadId The gamepad slot
     * @param axis The axis index (see GAMEPAD_AXIS_* in KeyCodes.h)
     * @return Axis value (-1.0 to 1.0 for sticks, 0.0 to 1.0 for triggers),
     *         or 0.0 if the gamepad or axis is invalid
     */
    float getGamepadAxis(int gamepadId, int axis) const {
        if (gamepadId < 0 || gamepadId >= MAX_GAMEPADS)
            return 0.0f;
        if (axis < 0 || axis > GAMEPAD_AXIS_LAST)
            return 0.0f;
        return m_gamepadAxes[gamepadId][axis];
    }

    /**
     * @brief Get the dead zone threshold for analog axes.
     * @return Dead zone value (default: GAMEPAD_AXIS_DEADZONE)
     */
    float getDeadZone() const { return m_deadZone; }

    /**
     * @brief Set the dead zone threshold for analog axes.
     *
     * Axis values with absolute value below the dead zone are reported as 0.0.
     *
     * @param deadZone New dead zone value (0.0 to 1.0)
     */
    void setDeadZone(float deadZone) { m_deadZone = std::abs(deadZone); }

    // ========================================================================
    // Internal — called by the engine to update gamepad state
    // ========================================================================

    /// @cond INTERNAL
    void _setGamepadConnected(int id, bool connected) {
        if (id >= 0 && id < MAX_GAMEPADS)
            m_gamepadConnected[id] = connected;
    }
    void _setGamepadButton(int id, int button, bool pressed) {
        if (id >= 0 && id < MAX_GAMEPADS && button >= 0 && button <= GAMEPAD_BUTTON_LAST)
            m_gamepadButtons[id][button] = pressed;
    }
    void _setGamepadAxis(int id, int axis, float value) {
        if (id >= 0 && id < MAX_GAMEPADS && axis >= 0 && axis <= GAMEPAD_AXIS_LAST)
            m_gamepadAxes[id][axis] = value;
    }
    /// @endcond

  private:
    // Gamepad state tracking
    std::array<bool, MAX_GAMEPADS> m_gamepadConnected = {};
    std::array<std::array<bool, MAX_GAMEPAD_BUTTONS>, MAX_GAMEPADS> m_gamepadButtons = {};
    std::array<std::array<float, MAX_GAMEPAD_AXES>, MAX_GAMEPADS> m_gamepadAxes = {};
    float m_deadZone = GAMEPAD_AXIS_DEADZONE;
};

}  // namespace vde

#pragma once

/**
 * @file InputHandler.h
 * @brief Input handling interface for VDE games
 *
 * Provides an abstract interface for handling keyboard, mouse,
 * and gamepad input that games can implement.
 */

#include "KeyCodes.h"

namespace vde {

/**
 * @brief Abstract interface for handling game input.
 *
 * Games should inherit from this class and override the methods
 * they need to handle input events.
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
 * };
 * @endcode
 */
class InputHandler {
  public:
    virtual ~InputHandler() = default;

    // Keyboard events

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

    // Mouse events

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

    // Gamepad/joystick events (future expansion)

    /**
     * @brief Called when a gamepad button is pressed.
     * @param gamepadId The gamepad ID
     * @param button The button index
     */
    virtual void onGamepadButtonPress([[maybe_unused]] int gamepadId, [[maybe_unused]] int button) {
    }

    /**
     * @brief Called when a gamepad button is released.
     * @param gamepadId The gamepad ID
     * @param button The button index
     */
    virtual void onGamepadButtonRelease([[maybe_unused]] int gamepadId,
                                        [[maybe_unused]] int button) {}

    /**
     * @brief Called when a gamepad axis changes.
     * @param gamepadId The gamepad ID
     * @param axis The axis index
     * @param value The axis value (-1.0 to 1.0)
     */
    virtual void onGamepadAxis([[maybe_unused]] int gamepadId, [[maybe_unused]] int axis,
                               [[maybe_unused]] float value) {}

    // Query methods (for polling input state)

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
};

}  // namespace vde

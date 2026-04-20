#pragma once

/**
 * @file KeyStateTracker.h
 * @brief Utility for tracking keyboard and gamepad input state
 *
 * Eliminates boilerplate InputHandler subclasses by providing a declarative
 * binding system for held and one-shot key/button actions.
 */

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vde {

/**
 * @brief Tracks keyboard and gamepad button state with named bindings.
 *
 * Instead of writing a custom InputHandler subclass with boolean fields and
 * press/release overrides, bind key codes to named actions and query them.
 *
 * Supports two binding modes:
 * - **Held:** Returns true as long as any bound key is physically held down.
 * - **One-shot:** Returns true once after the key is pressed, then resets.
 *
 * @example
 * @code
 * KeyStateTracker keys;
 * keys.bindHeld(vde::KEY_LEFT, "left");
 * keys.bindHeld(vde::KEY_RIGHT, "right");
 * keys.bindOneShot(vde::KEY_SPACE, "fire");
 *
 * // In InputHandler::onKeyPress / onKeyRelease:
 * keys.handlePress(key);
 * keys.handleRelease(key);
 *
 * // In update:
 * if (keys.isHeld("left")) { moveLeft(); }
 * if (keys.consume("fire")) { shoot(); }
 * @endcode
 */
class KeyStateTracker {
  public:
    /**
     * @brief Bind a key code to a named held action.
     *
     * Multiple keys can be bound to the same action name (e.g., arrow key
     * and gamepad D-pad). The action is considered held if any bound key
     * is currently pressed.
     *
     * @param keyCode Key code from KeyCodes.h (or gamepad button code)
     * @param name Action name to query later
     */
    void bindHeld(int keyCode, const std::string& name);

    /**
     * @brief Bind a key code to a named one-shot action.
     *
     * One-shot actions fire once on press and must be consumed via consume()
     * before they can fire again.
     *
     * @param keyCode Key code from KeyCodes.h (or gamepad button code)
     * @param name Action name to query later
     */
    void bindOneShot(int keyCode, const std::string& name);

    /**
     * @brief Check if a held action is currently active.
     *
     * Returns true if any key bound to this action name (as held) is
     * currently pressed.
     *
     * @param name Action name
     * @return true if any bound key is held down
     */
    bool isHeld(const std::string& name) const;

    /**
     * @brief Consume a one-shot action.
     *
     * Returns true if the action was triggered since the last consume(),
     * then resets the trigger. Returns false on subsequent calls until
     * the key is pressed again.
     *
     * @param name Action name
     * @return true if the action was triggered and not yet consumed
     */
    bool consume(const std::string& name);

    /**
     * @brief Handle a key press event.
     *
     * Call this from InputHandler::onKeyPress() or onGamepadButtonPress().
     *
     * @param keyCode The key or button code that was pressed
     */
    void handlePress(int keyCode);

    /**
     * @brief Handle a key release event.
     *
     * Call this from InputHandler::onKeyRelease() or onGamepadButtonRelease().
     *
     * @param keyCode The key or button code that was released
     */
    void handleRelease(int keyCode);

  private:
    enum class BindingType { Held, OneShot };

    struct Binding {
        std::string name;
        BindingType type;
    };

    /// Map from key code to all bindings for that key
    std::unordered_map<int, std::vector<Binding>> m_bindings;

    /// Held state: count of currently-pressed keys per action name
    std::unordered_map<std::string, int> m_heldCounts;

    /// One-shot state: whether the action has been triggered but not consumed
    std::unordered_map<std::string, bool> m_oneShotTriggered;

    /// Tracks which key codes are currently in the pressed-down state
    std::unordered_set<int> m_pressedKeys;
};

}  // namespace vde

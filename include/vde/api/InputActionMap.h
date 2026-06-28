#pragma once

/**
 * @file InputActionMap.h
 * @brief Named input-action mapping with keyboard and gamepad support.
 */

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace vde {

/**
 * @brief Physical binding kind for an input action.
 */
enum class InputActionBindingType {
    Key,
    GamepadButton,
    GamepadAxisPositive,
    GamepadAxisNegative,
};

/**
 * @brief One physical binding for a named action.
 */
struct InputActionBinding {
    InputActionBindingType type = InputActionBindingType::Key;
    int code = 0;
    float threshold = 0.5f;

    static InputActionBinding key(int keyCode);
    static InputActionBinding gamepadButton(int button);
    static InputActionBinding gamepadAxisPositive(int axis, float threshold = 0.5f);
    static InputActionBinding gamepadAxisNegative(int axis, float threshold = 0.5f);

    bool operator==(const InputActionBinding& other) const = default;
};

/**
 * @brief Serializable action definition with a name and one or more bindings.
 */
struct InputAction {
    std::string name;
    std::vector<InputActionBinding> bindings;
};

/**
 * @brief Maps raw keyboard/gamepad input to named gameplay actions.
 *
 * Actions expose three query states:
 * - pressed: became active since the last call to advanceFrame() or consumePressed()
 * - held: currently active from at least one bound source
 * - released: became inactive since the last call to advanceFrame() or consumeReleased()
 *
 * Axis bindings are threshold-based digital actions. When used from
 * InputHandler::onGamepadAxis(), the provided axis value has already had the
 * handler's dead zone applied by the engine.
 */
class InputActionMap {
  public:
    void addAction(const std::string& name);
    void removeAction(const std::string& name);
    bool hasAction(const std::string& name) const;
    void clear();

    void clearBindings(const std::string& name);
    void setBindings(const std::string& name, const std::vector<InputActionBinding>& bindings);
    void addBinding(const std::string& name, const InputActionBinding& binding);

    const std::vector<InputActionBinding>& getBindings(const std::string& name) const;
    std::vector<InputAction> getActions() const;

    bool isPressed(const std::string& name) const;
    bool isHeld(const std::string& name) const;
    bool isReleased(const std::string& name) const;

    bool consumePressed(const std::string& name);
    bool consumeReleased(const std::string& name);

    void advanceFrame();
    void resetState();

    void handleKeyPress(int keyCode);
    void handleKeyRelease(int keyCode);
    void handleGamepadButtonPress(int gamepadId, int button);
    void handleGamepadButtonRelease(int gamepadId, int button);
    void handleGamepadAxis(int gamepadId, int axis, float value);

    bool saveBindings(const std::string& storageKey) const;
    bool loadBindings(const std::string& storageKey);

  private:
    struct ActionState {
        int activeCount = 0;
        bool pressed = false;
        bool held = false;
        bool released = false;
    };

    struct LookupKey {
        InputActionBindingType type = InputActionBindingType::Key;
        int code = 0;

        bool operator==(const LookupKey& other) const = default;
    };

    struct LookupKeyHash {
        std::size_t operator()(const LookupKey& key) const;
    };

    struct ActiveSourceKey {
        InputActionBindingType type = InputActionBindingType::Key;
        int deviceId = -1;
        int code = 0;
        std::string actionName;

        bool operator==(const ActiveSourceKey& other) const = default;
    };

    struct ActiveSourceKeyHash {
        std::size_t operator()(const ActiveSourceKey& key) const;
    };

    struct BoundAction {
        std::string actionName;
        InputActionBinding binding;
    };

    static const std::vector<InputActionBinding>& emptyBindings();

    void rebuildLookup();
    void handleDigitalPress(InputActionBindingType type, int deviceId, int code);
    void handleDigitalRelease(InputActionBindingType type, int deviceId, int code);
    void updateAxisDirection(InputActionBindingType type, int gamepadId, int axis, float value);
    void activateAction(const std::string& name);
    void deactivateAction(const std::string& name);
    bool consumeFlag(const std::string& name, bool ActionState::* flag);

    std::unordered_map<std::string, std::vector<InputActionBinding>> m_actions;
    std::unordered_map<std::string, ActionState> m_states;
    std::unordered_map<LookupKey, std::vector<BoundAction>, LookupKeyHash> m_lookup;
    std::unordered_set<ActiveSourceKey, ActiveSourceKeyHash> m_activeSources;
};

}  // namespace vde
#include <vde/api/InputActionMap.h>
#include <vde/api/StorageManager.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>

namespace vde {

namespace {

constexpr std::string_view kStorageHeader = "vde_input_actions_v1";

template <typename T>
inline void hashCombine(std::size_t& seed, const T& value) {
    seed ^= std::hash<T>{}(value) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
}

std::string escapeField(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\t':
            escaped += "\\t";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }

    return escaped;
}

std::optional<std::vector<std::string>> splitEscapedFields(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool escaping = false;

    for (char ch : line) {
        if (escaping) {
            switch (ch) {
            case '\\':
                current.push_back('\\');
                break;
            case 't':
                current.push_back('\t');
                break;
            case 'n':
                current.push_back('\n');
                break;
            case 'r':
                current.push_back('\r');
                break;
            default:
                return std::nullopt;
            }
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '\t') {
            fields.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    if (escaping) {
        return std::nullopt;
    }

    fields.push_back(current);
    return fields;
}

std::string bindingTypeToken(InputActionBindingType type) {
    switch (type) {
    case InputActionBindingType::Key:
        return "key";
    case InputActionBindingType::GamepadButton:
        return "gamepad_button";
    case InputActionBindingType::GamepadAxisPositive:
        return "gamepad_axis_positive";
    case InputActionBindingType::GamepadAxisNegative:
        return "gamepad_axis_negative";
    }

    return "key";
}

std::optional<InputActionBindingType> parseBindingType(const std::string& token) {
    if (token == "key") {
        return InputActionBindingType::Key;
    }
    if (token == "gamepad_button") {
        return InputActionBindingType::GamepadButton;
    }
    if (token == "gamepad_axis_positive") {
        return InputActionBindingType::GamepadAxisPositive;
    }
    if (token == "gamepad_axis_negative") {
        return InputActionBindingType::GamepadAxisNegative;
    }
    return std::nullopt;
}

std::vector<InputActionBinding> sortedBindings(std::vector<InputActionBinding> bindings) {
    std::ranges::sort(bindings, [](const auto& lhs, const auto& rhs) {
        if (lhs.type != rhs.type) {
            return static_cast<int>(lhs.type) < static_cast<int>(rhs.type);
        }
        if (lhs.code != rhs.code) {
            return lhs.code < rhs.code;
        }
        return lhs.threshold < rhs.threshold;
    });
    return bindings;
}

}  // namespace

InputActionBinding InputActionBinding::key(int keyCode) {
    return InputActionBinding{
        .type = InputActionBindingType::Key, .code = keyCode, .threshold = 0.5f};
}

InputActionBinding InputActionBinding::gamepadButton(int button) {
    return InputActionBinding{
        .type = InputActionBindingType::GamepadButton, .code = button, .threshold = 0.5f};
}

InputActionBinding InputActionBinding::gamepadAxisPositive(int axis, float threshold) {
    return InputActionBinding{.type = InputActionBindingType::GamepadAxisPositive,
                              .code = axis,
                              .threshold = std::abs(threshold)};
}

InputActionBinding InputActionBinding::gamepadAxisNegative(int axis, float threshold) {
    return InputActionBinding{.type = InputActionBindingType::GamepadAxisNegative,
                              .code = axis,
                              .threshold = std::abs(threshold)};
}

std::size_t InputActionMap::LookupKeyHash::operator()(const LookupKey& key) const {
    std::size_t seed = 0;
    hashCombine(seed, static_cast<int>(key.type));
    hashCombine(seed, key.code);
    return seed;
}

std::size_t InputActionMap::ActiveSourceKeyHash::operator()(const ActiveSourceKey& key) const {
    std::size_t seed = 0;
    hashCombine(seed, static_cast<int>(key.type));
    hashCombine(seed, key.deviceId);
    hashCombine(seed, key.code);
    hashCombine(seed, key.actionName);
    return seed;
}

void InputActionMap::addAction(const std::string& name) {
    m_actions.try_emplace(name);
    m_states.try_emplace(name);
}

void InputActionMap::removeAction(const std::string& name) {
    if (m_actions.erase(name) == 0) {
        return;
    }
    rebuildLookup();
    resetState();
}

bool InputActionMap::hasAction(const std::string& name) const {
    return m_actions.find(name) != m_actions.end();
}

void InputActionMap::clear() {
    m_actions.clear();
    m_states.clear();
    m_lookup.clear();
    m_activeSources.clear();
}

void InputActionMap::clearBindings(const std::string& name) {
    auto it = m_actions.find(name);
    if (it == m_actions.end()) {
        return;
    }
    it->second.clear();
    rebuildLookup();
    resetState();
}

void InputActionMap::setBindings(const std::string& name,
                                 const std::vector<InputActionBinding>& bindings) {
    m_actions[name] = bindings;
    rebuildLookup();
    resetState();
}

void InputActionMap::addBinding(const std::string& name, const InputActionBinding& binding) {
    m_actions[name].push_back(binding);
    rebuildLookup();
    resetState();
}

const std::vector<InputActionBinding>& InputActionMap::getBindings(const std::string& name) const {
    auto it = m_actions.find(name);
    if (it == m_actions.end()) {
        return emptyBindings();
    }
    return it->second;
}

std::vector<InputAction> InputActionMap::getActions() const {
    std::vector<InputAction> actions;
    actions.reserve(m_actions.size());

    for (const auto& [name, bindings] : m_actions) {
        actions.push_back(InputAction{.name = name, .bindings = bindings});
    }

    std::ranges::sort(actions,
                      [](const auto& lhs, const auto& rhs) { return lhs.name < rhs.name; });

    return actions;
}

bool InputActionMap::isPressed(const std::string& name) const {
    auto it = m_states.find(name);
    return it != m_states.end() && it->second.pressed;
}

bool InputActionMap::isHeld(const std::string& name) const {
    auto it = m_states.find(name);
    return it != m_states.end() && it->second.held;
}

bool InputActionMap::isReleased(const std::string& name) const {
    auto it = m_states.find(name);
    return it != m_states.end() && it->second.released;
}

bool InputActionMap::consumePressed(const std::string& name) {
    return consumeFlag(name, &ActionState::pressed);
}

bool InputActionMap::consumeReleased(const std::string& name) {
    return consumeFlag(name, &ActionState::released);
}

void InputActionMap::advanceFrame() {
    for (auto& [_, state] : m_states) {
        state.pressed = false;
        state.released = false;
    }
}

void InputActionMap::resetState() {
    m_activeSources.clear();
    m_states.clear();
    for (const auto& [name, _] : m_actions) {
        m_states.emplace(name, ActionState{});
    }
}

void InputActionMap::handleKeyPress(int keyCode) {
    handleDigitalPress(InputActionBindingType::Key, -1, keyCode);
}

void InputActionMap::handleKeyRelease(int keyCode) {
    handleDigitalRelease(InputActionBindingType::Key, -1, keyCode);
}

void InputActionMap::handleGamepadButtonPress(int gamepadId, int button) {
    handleDigitalPress(InputActionBindingType::GamepadButton, gamepadId, button);
}

void InputActionMap::handleGamepadButtonRelease(int gamepadId, int button) {
    handleDigitalRelease(InputActionBindingType::GamepadButton, gamepadId, button);
}

void InputActionMap::handleGamepadAxis(int gamepadId, int axis, float value) {
    updateAxisDirection(InputActionBindingType::GamepadAxisPositive, gamepadId, axis, value);
    updateAxisDirection(InputActionBindingType::GamepadAxisNegative, gamepadId, axis, value);
}

bool InputActionMap::saveBindings(const std::string& storageKey) const {
    std::ostringstream encoded;
    encoded << kStorageHeader << '\n';
    encoded << std::fixed << std::setprecision(3);

    for (const auto& action : getActions()) {
        for (const auto& binding : sortedBindings(action.bindings)) {
            encoded << escapeField(action.name) << '\t' << bindingTypeToken(binding.type) << '\t'
                    << binding.code << '\t' << binding.threshold << '\n';
        }
    }

    return StorageManager::getInstance().setStringData(storageKey, encoded.str());
}

bool InputActionMap::loadBindings(const std::string& storageKey) {
    auto stored = StorageManager::getInstance().getStringData(storageKey);
    if (!stored.has_value()) {
        return false;
    }

    std::istringstream input(*stored);
    std::string line;
    if (!std::getline(input, line) || line != kStorageHeader) {
        return false;
    }

    std::unordered_map<std::string, std::vector<InputActionBinding>> decodedActions;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        auto fields = splitEscapedFields(line);
        if (!fields.has_value() || fields->size() != 4) {
            return false;
        }

        const auto& parsedFields = *fields;
        const auto& actionName = parsedFields.at(0);
        const auto& bindingToken = parsedFields.at(1);
        const auto& codeToken = parsedFields.at(2);
        const auto& thresholdToken = parsedFields.at(3);

        auto bindingType = parseBindingType(bindingToken);
        if (!bindingType.has_value()) {
            return false;
        }

        int code = 0;
        float threshold = 0.0f;

        try {
            code = std::stoi(codeToken);
            threshold = std::stof(thresholdToken);
        } catch (const std::exception&) {
            return false;
        }

        decodedActions[actionName].push_back(
            InputActionBinding{.type = *bindingType, .code = code, .threshold = threshold});
    }

    m_actions = std::move(decodedActions);
    rebuildLookup();
    resetState();
    return true;
}

const std::vector<InputActionBinding>& InputActionMap::emptyBindings() {
    static const std::vector<InputActionBinding> kEmpty;
    return kEmpty;
}

void InputActionMap::rebuildLookup() {
    m_lookup.clear();
    for (const auto& [name, bindings] : m_actions) {
        for (const auto& binding : bindings) {
            LookupKey key{.type = binding.type, .code = binding.code};
            BoundAction action{.actionName = name, .binding = binding};
            m_lookup[key].push_back(action);
        }
    }
}

void InputActionMap::handleDigitalPress(InputActionBindingType type, int deviceId, int code) {
    auto it = m_lookup.find(LookupKey{.type = type, .code = code});
    if (it == m_lookup.end()) {
        return;
    }

    for (const auto& boundAction : it->second) {
        ActiveSourceKey activeKey{
            .type = type, .deviceId = deviceId, .code = code, .actionName = boundAction.actionName};
        if (m_activeSources.insert(activeKey).second) {
            activateAction(boundAction.actionName);
        }
    }
}

void InputActionMap::handleDigitalRelease(InputActionBindingType type, int deviceId, int code) {
    auto it = m_lookup.find(LookupKey{.type = type, .code = code});
    if (it == m_lookup.end()) {
        return;
    }

    for (const auto& boundAction : it->second) {
        ActiveSourceKey activeKey{
            .type = type, .deviceId = deviceId, .code = code, .actionName = boundAction.actionName};
        if (m_activeSources.erase(activeKey) > 0) {
            deactivateAction(boundAction.actionName);
        }
    }
}

void InputActionMap::updateAxisDirection(InputActionBindingType type, int gamepadId, int axis,
                                         float value) {
    auto it = m_lookup.find(LookupKey{.type = type, .code = axis});
    if (it == m_lookup.end()) {
        return;
    }

    for (const auto& boundAction : it->second) {
        const float threshold = std::abs(boundAction.binding.threshold);
        const bool shouldBeActive = type == InputActionBindingType::GamepadAxisPositive
                                        ? value >= threshold
                                        : value <= -threshold;

        ActiveSourceKey activeKey{.type = type,
                                  .deviceId = gamepadId,
                                  .code = axis,
                                  .actionName = boundAction.actionName};
        const bool isActive = m_activeSources.find(activeKey) != m_activeSources.end();

        if (shouldBeActive && !isActive) {
            m_activeSources.insert(activeKey);
            activateAction(boundAction.actionName);
        } else if (!shouldBeActive && isActive) {
            m_activeSources.erase(activeKey);
            deactivateAction(boundAction.actionName);
        }
    }
}

void InputActionMap::activateAction(const std::string& name) {
    auto& state = m_states[name];
    if (state.activeCount == 0) {
        state.held = true;
        state.pressed = true;
    }
    ++state.activeCount;
}

void InputActionMap::deactivateAction(const std::string& name) {
    auto it = m_states.find(name);
    if (it == m_states.end()) {
        return;
    }

    auto& state = it->second;
    if (state.activeCount <= 0) {
        return;
    }

    --state.activeCount;
    if (state.activeCount == 0) {
        state.held = false;
        state.released = true;
    }
}

bool InputActionMap::consumeFlag(const std::string& name, bool ActionState::* flag) {
    auto it = m_states.find(name);
    if (it == m_states.end()) {
        return false;
    }
    bool value = it->second.*flag;
    it->second.*flag = false;
    return value;
}

}  // namespace vde
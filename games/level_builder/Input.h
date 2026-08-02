#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// cppcheck-suppress syntaxError -- cppcheck misparses BaseGame includes when scanning game input
// headers directly
#include "../GameBase.h"

// cppcheck-suppress syntaxError -- cppcheck misparses C++20 namespace syntax in header-only mode
namespace levelbuilder {

enum class LevelBuilderInputMode {
    Play,
    DevelopmentMove,
    DevelopmentSelectTile,
};

class LevelBuilderInput : public vde::games::BaseGameInputHandler {
  public:
    LevelBuilderInput() { setMode(LevelBuilderInputMode::Play); }

    void setMode(LevelBuilderInputMode mode) {
        if (m_mode == mode && !m_actions.getActions().empty()) {
            return;
        }

        m_actions.clear();
        for (const auto& action : bindingsForMode(mode)) {
            m_actions.setBindings(action.name, action.bindings);
        }
        restoreHeldInputState();
        m_actions.advanceFrame();
        m_mode = mode;
    }

    [[nodiscard]] LevelBuilderInputMode mode() const { return m_mode; }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        m_heldKeys.insert(key);
        m_actions.handleKeyPress(key);
    }

    void onKeyRelease(int key) override {
        m_heldKeys.erase(key);
        m_actions.handleKeyRelease(key);
    }

    void onGamepadButtonPress(int gamepadId, int button) override {
        m_heldGamepadButtons[gamepadId].insert(button);
        m_actions.handleGamepadButtonPress(gamepadId, button);
    }

    void onGamepadButtonRelease(int gamepadId, int button) override {
        if (const auto gamepadIt = m_heldGamepadButtons.find(gamepadId);
            gamepadIt != m_heldGamepadButtons.end()) {
            gamepadIt->second.erase(button);
        }
        m_actions.handleGamepadButtonRelease(gamepadId, button);
    }

    void onGamepadDisconnect(int gamepadId) override {
        m_heldGamepadButtons.erase(gamepadId);
        m_gamepadAxes.erase(gamepadId);
        m_actions.resetState();
        restoreHeldInputState();
        m_actions.advanceFrame();
    }

    void onGamepadAxis(int gamepadId, int axis, float value) override {
        m_gamepadAxes[gamepadId][axis] = value;
        m_actions.handleGamepadAxis(gamepadId, axis, value);
    }

    vde::InputActionMap& actions() { return m_actions; }
    [[nodiscard]] const vde::InputActionMap& actions() const { return m_actions; }

    void finishFrame() { m_actions.advanceFrame(); }

  private:
    struct ActionBindings {
        std::string name;
        std::vector<vde::InputActionBinding> bindings;
    };

    using BindingPreset = std::vector<ActionBindings>;

    static BindingPreset sharedBindings(bool includeJump) {
        BindingPreset bindings = {
            {"move_left",
             {vde::InputActionBinding::key(vde::KEY_A), vde::InputActionBinding::key(vde::KEY_LEFT),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_LEFT),
              vde::InputActionBinding::gamepadAxisNegative(vde::GAMEPAD_AXIS_LEFT_X, 0.45f)}},
            {"move_right",
             {vde::InputActionBinding::key(vde::KEY_D),
              vde::InputActionBinding::key(vde::KEY_RIGHT),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_RIGHT),
              vde::InputActionBinding::gamepadAxisPositive(vde::GAMEPAD_AXIS_LEFT_X, 0.45f)}},
            {"move_up",
             {vde::InputActionBinding::key(vde::KEY_W), vde::InputActionBinding::key(vde::KEY_UP),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_UP),
              vde::InputActionBinding::gamepadAxisNegative(vde::GAMEPAD_AXIS_LEFT_Y, 0.45f)}},
            {"move_down",
             {vde::InputActionBinding::key(vde::KEY_S), vde::InputActionBinding::key(vde::KEY_DOWN),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_DOWN),
              vde::InputActionBinding::gamepadAxisPositive(vde::GAMEPAD_AXIS_LEFT_Y, 0.45f)}},
            {"reset",
             {vde::InputActionBinding::key(vde::KEY_R),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_BACK)}},
            {"toggle_dev_mode",
             {vde::InputActionBinding::key(vde::KEY_ENTER),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_START)}},
        };

        if (includeJump) {
            bindings.push_back(
                {"jump",
                 {vde::InputActionBinding::key(vde::KEY_SPACE),
                  vde::InputActionBinding::key(vde::KEY_W),
                  vde::InputActionBinding::key(vde::KEY_UP),
                  vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_UP),
                  vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_A)}});
        }
        return bindings;
    }

    static void appendDevelopmentBindings(BindingPreset& bindings) {
        bindings.push_back(
            {"prev_submode",
             {vde::InputActionBinding::key(vde::KEY_Q),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_LEFT_BUMPER)}});
        bindings.push_back(
            {"next_submode",
             {vde::InputActionBinding::key(vde::KEY_E),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_RIGHT_BUMPER)}});
        bindings.push_back(
            {"save_overlay",
             {vde::InputActionBinding::key(vde::KEY_F5),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_LEFT_THUMB)}});
        bindings.push_back(
            {"load_overlay",
             {vde::InputActionBinding::key(vde::KEY_F9),
              vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_RIGHT_THUMB)}});
    }

    static BindingPreset buildMoveBindings() {
        BindingPreset bindings = sharedBindings(false);
        appendDevelopmentBindings(bindings);
        bindings.push_back({"add_layer",
                            {vde::InputActionBinding::key(vde::KEY_N),
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_A)}});
        bindings.push_back({"previous_layer",
                            {vde::InputActionBinding::key(vde::KEY_J),
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_X)}});
        bindings.push_back({"next_layer",
                            {vde::InputActionBinding::key(vde::KEY_K),
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_Y)}});
        bindings.push_back({"toggle_layer_visibility",
                            {vde::InputActionBinding::key(vde::KEY_H),
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_B)}});
        bindings.push_back({"layer_depth_down",
                            {vde::InputActionBinding::key(vde::KEY_O),
                             vde::InputActionBinding::gamepadAxisPositive(
                                 vde::GAMEPAD_AXIS_LEFT_TRIGGER, 0.55f)}});
        bindings.push_back({"layer_depth_up",
                            {vde::InputActionBinding::key(vde::KEY_P),
                             vde::InputActionBinding::gamepadAxisPositive(
                                 vde::GAMEPAD_AXIS_RIGHT_TRIGGER, 0.55f)}});
        return bindings;
    }

    static BindingPreset buildSelectTileBindings() {
        BindingPreset bindings = sharedBindings(false);
        appendDevelopmentBindings(bindings);
        bindings.push_back({"previous_tile",
                            {vde::InputActionBinding::key(vde::KEY_Z),
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_B)}});
        bindings.push_back({"next_tile",
                            {vde::InputActionBinding::key(vde::KEY_X),
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_A)}});
        bindings.push_back({"copy_tile",
                            {vde::InputActionBinding::key(vde::KEY_C),
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_X)}});
        bindings.push_back({"paste_tile",
                            {vde::InputActionBinding::key(vde::KEY_V),
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_Y)}});
        bindings.push_back({"undo_tile_edit",
                            {vde::InputActionBinding::key(vde::KEY_U),
                             vde::InputActionBinding::gamepadAxisPositive(
                                 vde::GAMEPAD_AXIS_LEFT_TRIGGER, 0.55f)}});
        bindings.push_back({"redo_tile_edit",
                            {vde::InputActionBinding::key(vde::KEY_I),
                             vde::InputActionBinding::gamepadAxisPositive(
                                 vde::GAMEPAD_AXIS_RIGHT_TRIGGER, 0.55f)}});
        return bindings;
    }

    static const BindingPreset& bindingsForMode(LevelBuilderInputMode mode) {
        static const BindingPreset playBindings = sharedBindings(true);
        static const BindingPreset moveBindings = buildMoveBindings();
        static const BindingPreset selectTileBindings = buildSelectTileBindings();

        switch (mode) {
        case LevelBuilderInputMode::Play:
            return playBindings;
        case LevelBuilderInputMode::DevelopmentMove:
            return moveBindings;
        case LevelBuilderInputMode::DevelopmentSelectTile:
            return selectTileBindings;
        }
        return playBindings;
    }

    void restoreHeldInputState() {
        for (const int key : m_heldKeys) {
            m_actions.handleKeyPress(key);
        }
        for (const auto& [gamepadId, buttons] : m_heldGamepadButtons) {
            for (const int button : buttons) {
                m_actions.handleGamepadButtonPress(gamepadId, button);
            }
        }
        for (const auto& [gamepadId, axes] : m_gamepadAxes) {
            for (const auto& [axis, value] : axes) {
                m_actions.handleGamepadAxis(gamepadId, axis, value);
            }
        }
    }

    vde::InputActionMap m_actions;
    LevelBuilderInputMode m_mode = LevelBuilderInputMode::Play;
    std::unordered_set<int> m_heldKeys;
    std::unordered_map<int, std::unordered_set<int>> m_heldGamepadButtons;
    std::unordered_map<int, std::unordered_map<int, float>> m_gamepadAxes;
};

}  // namespace levelbuilder
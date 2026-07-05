#pragma once

#include "../GameBase.h"

namespace levelbuilder {

class LevelBuilderInput : public vde::games::BaseGameInputHandler {
  public:
    LevelBuilderInput() {
        m_actions.addBinding("move_left", vde::InputActionBinding::key(vde::KEY_A));
        m_actions.addBinding("move_left", vde::InputActionBinding::key(vde::KEY_LEFT));
        m_actions.addBinding("move_left",
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_LEFT));
        m_actions.addBinding("move_left", vde::InputActionBinding::gamepadAxisNegative(
                                              vde::GAMEPAD_AXIS_LEFT_X, 0.45f));

        m_actions.addBinding("move_right", vde::InputActionBinding::key(vde::KEY_D));
        m_actions.addBinding("move_right", vde::InputActionBinding::key(vde::KEY_RIGHT));
        m_actions.addBinding(
            "move_right", vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_RIGHT));
        m_actions.addBinding("move_right", vde::InputActionBinding::gamepadAxisPositive(
                                               vde::GAMEPAD_AXIS_LEFT_X, 0.45f));

        m_actions.addBinding("move_up", vde::InputActionBinding::key(vde::KEY_W));
        m_actions.addBinding("move_up", vde::InputActionBinding::key(vde::KEY_UP));
        m_actions.addBinding("move_up",
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_UP));
        m_actions.addBinding("move_up", vde::InputActionBinding::gamepadAxisNegative(
                                            vde::GAMEPAD_AXIS_LEFT_Y, 0.45f));

        m_actions.addBinding("move_down", vde::InputActionBinding::key(vde::KEY_S));
        m_actions.addBinding("move_down", vde::InputActionBinding::key(vde::KEY_DOWN));
        m_actions.addBinding("move_down",
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_DOWN));
        m_actions.addBinding("move_down", vde::InputActionBinding::gamepadAxisPositive(
                                              vde::GAMEPAD_AXIS_LEFT_Y, 0.45f));

        m_actions.addBinding("jump", vde::InputActionBinding::key(vde::KEY_SPACE));
        m_actions.addBinding("jump", vde::InputActionBinding::key(vde::KEY_W));
        m_actions.addBinding("jump", vde::InputActionBinding::key(vde::KEY_UP));
        m_actions.addBinding("jump",
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_DPAD_UP));
        m_actions.addBinding("jump", vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_A));

        m_actions.addBinding("reset", vde::InputActionBinding::key(vde::KEY_R));
        m_actions.addBinding("reset",
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_BACK));

        m_actions.addBinding("toggle_dev_mode", vde::InputActionBinding::key(vde::KEY_ENTER));
        m_actions.addBinding("toggle_dev_mode",
                             vde::InputActionBinding::gamepadButton(vde::GAMEPAD_BUTTON_START));

        m_actions.addBinding("prev_submode", vde::InputActionBinding::key(vde::KEY_Q));
        m_actions.addBinding("prev_submode", vde::InputActionBinding::gamepadButton(
                                                 vde::GAMEPAD_BUTTON_LEFT_BUMPER));

        m_actions.addBinding("next_submode", vde::InputActionBinding::key(vde::KEY_E));
        m_actions.addBinding("next_submode", vde::InputActionBinding::gamepadButton(
                                                 vde::GAMEPAD_BUTTON_RIGHT_BUMPER));
    }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        m_actions.handleKeyPress(key);
    }

    void onKeyRelease(int key) override { m_actions.handleKeyRelease(key); }

    void onGamepadButtonPress(int gamepadId, int button) override {
        m_actions.handleGamepadButtonPress(gamepadId, button);
    }

    void onGamepadButtonRelease(int gamepadId, int button) override {
        m_actions.handleGamepadButtonRelease(gamepadId, button);
    }

    void onGamepadAxis(int gamepadId, int axis, float value) override {
        m_actions.handleGamepadAxis(gamepadId, axis, value);
    }

    vde::InputActionMap& actions() { return m_actions; }
    [[nodiscard]] const vde::InputActionMap& actions() const { return m_actions; }

    void finishFrame() { m_actions.advanceFrame(); }

  private:
    vde::InputActionMap m_actions;
};

}  // namespace levelbuilder
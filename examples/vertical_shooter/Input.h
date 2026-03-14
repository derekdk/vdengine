#pragma once

/**
 * @file Input.h
 * @brief Input handler with gamepad and keyboard support for the vertical shooter.
 */

#include <vde/api/GameAPI.h>

#include "../ExampleBase.h"
#include "Types.h"

namespace shooter {

class ShooterInput : public vde::examples::BaseExampleInputHandler {
  public:
    // -- Keyboard --
    void onKeyPress(int key) override {
        BaseExampleInputHandler::onKeyPress(key);
        if (key == vde::KEY_LEFT || key == vde::KEY_A)
            m_left = true;
        if (key == vde::KEY_RIGHT || key == vde::KEY_D)
            m_right = true;
        if (key == vde::KEY_UP || key == vde::KEY_W)
            m_up = true;
        if (key == vde::KEY_DOWN || key == vde::KEY_S)
            m_down = true;
        if (key == vde::KEY_SPACE) {
            m_fireHeld = true;
            m_firePressed = true;
        }
        if (key == vde::KEY_Q)
            m_prevWeapon = true;
        if (key == vde::KEY_E)
            m_nextWeapon = true;
        if (key == vde::KEY_R)
            m_restart = true;
        if (key == vde::KEY_ENTER)
            m_start = true;
    }

    void onKeyRelease(int key) override {
        if (key == vde::KEY_LEFT || key == vde::KEY_A)
            m_left = false;
        if (key == vde::KEY_RIGHT || key == vde::KEY_D)
            m_right = false;
        if (key == vde::KEY_UP || key == vde::KEY_W)
            m_up = false;
        if (key == vde::KEY_DOWN || key == vde::KEY_S)
            m_down = false;
        if (key == vde::KEY_SPACE)
            m_fireHeld = false;
    }

    // -- Gamepad --
    void onGamepadButtonPress(int /*gamepadId*/, int button) override {
        if (button == vde::GAMEPAD_BUTTON_A) {
            m_fireHeld = true;
            m_firePressed = true;
        }
        if (button == vde::GAMEPAD_BUTTON_X) {
            m_fireHeld = true;
            m_firePressed = true;
        }
        if (button == vde::GAMEPAD_BUTTON_LEFT_BUMPER)
            m_prevWeapon = true;
        if (button == vde::GAMEPAD_BUTTON_RIGHT_BUMPER)
            m_nextWeapon = true;
        if (button == vde::GAMEPAD_BUTTON_START)
            m_start = true;
        if (button == vde::GAMEPAD_BUTTON_BACK)
            m_restart = true;
    }

    void onGamepadButtonRelease(int /*gamepadId*/, int button) override {
        if (button == vde::GAMEPAD_BUTTON_A)
            m_fireHeld = false;
        if (button == vde::GAMEPAD_BUTTON_X)
            m_fireHeld = false;
    }

    void onGamepadAxis(int /*gamepadId*/, int axis, float value) override {
        if (axis == vde::GAMEPAD_AXIS_LEFT_X)
            m_stickX = value;
        if (axis == vde::GAMEPAD_AXIS_LEFT_Y)
            m_stickY = value;
        // Right trigger also fires
        if (axis == vde::GAMEPAD_AXIS_RIGHT_TRIGGER)
            m_fireHeld = (value > 0.3f);
    }

    // -- Queries --
    glm::vec2 getMoveDirection() const {
        glm::vec2 dir(0.0f);
        if (m_left)
            dir.x -= 1.0f;
        if (m_right)
            dir.x += 1.0f;
        if (m_up)
            dir.y += 1.0f;
        if (m_down)
            dir.y -= 1.0f;

        // Blend in joystick
        dir.x += m_stickX;
        dir.y -= m_stickY;  // Y axis is inverted on gamepads

        float len = glm::length(dir);
        if (len > 1.0f)
            dir /= len;
        return dir;
    }

    bool isFireHeld() const { return m_fireHeld; }
    bool consumeFire() {
        bool v = m_firePressed;
        m_firePressed = false;
        return v;
    }

    bool consumePrevWeapon() {
        bool v = m_prevWeapon;
        m_prevWeapon = false;
        return v;
    }
    bool consumeNextWeapon() {
        bool v = m_nextWeapon;
        m_nextWeapon = false;
        return v;
    }
    bool consumeRestart() {
        bool v = m_restart;
        m_restart = false;
        return v;
    }
    bool consumeStart() {
        bool v = m_start;
        m_start = false;
        return v;
    }

  private:
    bool m_left = false, m_right = false, m_up = false, m_down = false;
    bool m_fireHeld = false;
    bool m_firePressed = false;
    bool m_prevWeapon = false, m_nextWeapon = false;
    bool m_restart = false, m_start = false;
    float m_stickX = 0.0f, m_stickY = 0.0f;
};

}  // namespace shooter

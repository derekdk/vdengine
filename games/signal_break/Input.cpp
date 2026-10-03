#include "Input.h"

#include <algorithm>
#include <cmath>

namespace signalbreak {

SignalBreakInput::Pad::Pad() {
    keys.bindHeld(vde::GAMEPAD_BUTTON_DPAD_LEFT, "left");
    keys.bindHeld(vde::GAMEPAD_BUTTON_DPAD_RIGHT, "right");
    keys.bindHeld(vde::GAMEPAD_BUTTON_DPAD_UP, "up");
    keys.bindHeld(vde::GAMEPAD_BUTTON_DPAD_DOWN, "down");
    keys.bindHeld(vde::GAMEPAD_BUTTON_X, "fire");
    keys.bindHeld(vde::GAMEPAD_BUTTON_RIGHT_BUMPER, "fire");
    keys.bindOneShot(vde::GAMEPAD_BUTTON_A, "jump");
    keys.bindOneShot(vde::GAMEPAD_BUTTON_Y, "restart");
    keys.bindOneShot(vde::GAMEPAD_BUTTON_START, "pause");
}

SignalBreakInput::SignalBreakInput() {
    m_keys.bindHeld(vde::KEY_A, "left");
    m_keys.bindHeld(vde::KEY_LEFT, "left");
    m_keys.bindHeld(vde::KEY_D, "right");
    m_keys.bindHeld(vde::KEY_RIGHT, "right");
    m_keys.bindHeld(vde::KEY_W, "up");
    m_keys.bindHeld(vde::KEY_UP, "up");
    m_keys.bindHeld(vde::KEY_S, "down");
    m_keys.bindHeld(vde::KEY_DOWN, "down");
    m_keys.bindHeld(vde::KEY_J, "fire");
    m_keys.bindHeld(vde::KEY_X, "fire");
    m_keys.bindOneShot(vde::KEY_SPACE, "jump");
    m_keys.bindOneShot(vde::KEY_R, "restart");
    m_keys.bindOneShot(vde::KEY_P, "pause");
}

void SignalBreakInput::onKeyPress(int key) {
    BaseGameInputHandler::onKeyPress(key);
    m_keys.handlePress(key);
}

void SignalBreakInput::onKeyRelease(int key) {
    m_keys.handleRelease(key);
}

void SignalBreakInput::onGamepadButtonPress(int gamepadId, int button) {
    if (gamepadId >= 0 && gamepadId < vde::MAX_GAMEPADS) {
        m_pads[gamepadId].keys.handlePress(button);
    }
}

void SignalBreakInput::onGamepadButtonRelease(int gamepadId, int button) {
    if (gamepadId >= 0 && gamepadId < vde::MAX_GAMEPADS) {
        m_pads[gamepadId].keys.handleRelease(button);
    }
}

void SignalBreakInput::onGamepadAxis(int gamepadId, int axis, float value) {
    if (gamepadId < 0 || gamepadId >= vde::MAX_GAMEPADS) {
        return;
    }
    auto& pad = m_pads[gamepadId];
    const float filtered = std::abs(value) < 0.2f ? 0.0f : std::clamp(value, -1.0f, 1.0f);
    if (axis == vde::GAMEPAD_AXIS_LEFT_X) {
        pad.move = filtered;
    } else if (axis == vde::GAMEPAD_AXIS_LEFT_Y) {
        pad.aim = -filtered;
    } else if (axis == vde::GAMEPAD_AXIS_RIGHT_TRIGGER) {
        pad.trigger = value;
    }
}

void SignalBreakInput::onGamepadDisconnect(int gamepadId) {
    if (gamepadId >= 0 && gamepadId < vde::MAX_GAMEPADS) {
        m_pads[gamepadId] = Pad{};
    }
}

InputFrame SignalBreakInput::sample() {
    InputFrame frame;
    frame.move = static_cast<float>(m_keys.isHeld("right")) - m_keys.isHeld("left");
    frame.aim = static_cast<float>(m_keys.isHeld("up")) - m_keys.isHeld("down");
    frame.fire = m_keys.isHeld("fire");
    frame.jump = m_keys.consume("jump");
    frame.restart = m_keys.consume("restart");
    frame.pause = m_keys.consume("pause");
    for (auto& pad : m_pads) {
        const float digitalMove =
            static_cast<float>(pad.keys.isHeld("right")) - pad.keys.isHeld("left");
        const float digitalAim =
            static_cast<float>(pad.keys.isHeld("up")) - pad.keys.isHeld("down");
        const float move = digitalMove != 0.0f ? digitalMove : pad.move;
        const float aim = digitalAim != 0.0f ? digitalAim : pad.aim;
        if (std::abs(move) > std::abs(frame.move)) {
            frame.move = move;
        }
        if (std::abs(aim) > std::abs(frame.aim)) {
            frame.aim = aim;
        }
        frame.fire = frame.fire || pad.keys.isHeld("fire") || pad.trigger > 0.25f;
        frame.jump = pad.keys.consume("jump") || frame.jump;
        frame.restart = pad.keys.consume("restart") || frame.restart;
        frame.pause = pad.keys.consume("pause") || frame.pause;
    }
    return frame;
}

}  // namespace signalbreak
#pragma once

#include "../GameBase.h"

namespace levelbuilder {

class LevelBuilderInput : public vde::games::BaseGameInputHandler {
  public:
    LevelBuilderInput() {
        keys.bindHeld(vde::KEY_A, "left");
        keys.bindHeld(vde::KEY_LEFT, "left");
        keys.bindHeld(vde::GAMEPAD_BUTTON_DPAD_LEFT, "left");

        keys.bindHeld(vde::KEY_D, "right");
        keys.bindHeld(vde::KEY_RIGHT, "right");
        keys.bindHeld(vde::GAMEPAD_BUTTON_DPAD_RIGHT, "right");

        keys.bindOneShot(vde::KEY_SPACE, "jump");
        keys.bindOneShot(vde::KEY_W, "jump");
        keys.bindOneShot(vde::KEY_UP, "jump");
        keys.bindOneShot(vde::GAMEPAD_BUTTON_DPAD_UP, "jump");
        keys.bindOneShot(vde::GAMEPAD_BUTTON_A, "jump");

        keys.bindOneShot(vde::KEY_R, "reset");
        keys.bindOneShot(vde::GAMEPAD_BUTTON_BACK, "reset");
    }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    void onGamepadButtonPress(int /*gamepadId*/, int button) override { keys.handlePress(button); }

    void onGamepadButtonRelease(int /*gamepadId*/, int button) override {
        keys.handleRelease(button);
    }

    vde::KeyStateTracker keys;
};

}  // namespace levelbuilder
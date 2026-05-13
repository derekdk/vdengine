#pragma once

#include "../GameBase.h"

namespace pacman {

class PacManInput : public vde::games::BaseGameInputHandler {
  public:
    PacManInput() {
        keys.bindHeld(vde::KEY_LEFT, "left");
        keys.bindHeld(vde::KEY_A, "left");
        keys.bindHeld(vde::KEY_RIGHT, "right");
        keys.bindHeld(vde::KEY_D, "right");
        keys.bindHeld(vde::KEY_UP, "up");
        keys.bindHeld(vde::KEY_W, "up");
        keys.bindHeld(vde::KEY_DOWN, "down");
        keys.bindHeld(vde::KEY_S, "down");
        keys.bindOneShot(vde::KEY_SPACE, "restart");
    }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

}  // namespace pacman

#pragma once

#include "../GameBase.h"

namespace pong {

class PongInput : public vde::games::BaseGameInputHandler {
  public:
    PongInput() {
        keys.bindHeld(vde::KEY_UP, "up");
        keys.bindHeld(vde::KEY_W, "up");
        keys.bindHeld(vde::KEY_DOWN, "down");
        keys.bindHeld(vde::KEY_S, "down");
        keys.bindOneShot(vde::KEY_SPACE, "serve");
        keys.bindOneShot(vde::KEY_R, "restart");
    }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

}  // namespace pong

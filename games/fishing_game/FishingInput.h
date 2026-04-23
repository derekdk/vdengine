#pragma once

#include "../GameBase.h"

namespace fishing {

class FishingInput : public vde::games::BaseGameInputHandler {
  public:
    FishingInput() {
        keys.bindHeld(vde::KEY_LEFT, "left");
        keys.bindHeld(vde::KEY_RIGHT, "right");
        keys.bindHeld(vde::KEY_UP, "up");
        keys.bindHeld(vde::KEY_DOWN, "down");
        keys.bindOneShot(vde::KEY_SPACE, "action");
        keys.bindOneShot(vde::KEY_R, "restart");
    }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

}  // namespace fishing
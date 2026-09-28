#pragma once

#include "../GameBase.h"

namespace rungame {

class RunGameInput : public vde::games::BaseGameInputHandler {
  public:
    RunGameInput() {
        // Bind keys here. Examples:
        //   keys.bindHeld(vde::KEY_LEFT,  "left");
        //   keys.bindHeld(vde::KEY_RIGHT, "right");
        //   keys.bindOneShot(vde::KEY_SPACE, "action");
    }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

}  // namespace rungame

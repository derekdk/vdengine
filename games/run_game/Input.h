#pragma once

#include "../GameBase.h"

namespace rungame {

class RunGameInput : public vde::games::BaseGameInputHandler {
  public:
    RunGameInput() {
        keys.bindOneShot(vde::KEY_SPACE, "jump");
        keys.bindOneShot(vde::KEY_UP, "jump");
        keys.bindOneShot(vde::KEY_W, "jump");
        keys.bindOneShot(vde::KEY_R, "restart");
    }

    void onKeyPress(int key) override {
        BaseGameInputHandler::onKeyPress(key);
        keys.handlePress(key);
    }

    void onKeyRelease(int key) override { keys.handleRelease(key); }

    vde::KeyStateTracker keys;
};

}  // namespace rungame

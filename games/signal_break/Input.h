#pragma once

#include "../GameBase.h"

namespace signalbreak {

struct InputFrame {
    float move = 0.0f;
    float aim = 0.0f;
    bool fire = false;
    bool jump = false;
    bool restart = false;
    bool pause = false;
};

class SignalBreakInput : public vde::games::BaseGameInputHandler {
  public:
    SignalBreakInput();
    void onKeyPress(int key) override;
    void onKeyRelease(int key) override;
    void onGamepadButtonPress(int gamepadId, int button) override;
    void onGamepadButtonRelease(int gamepadId, int button) override;
    void onGamepadAxis(int gamepadId, int axis, float value) override;
    void onGamepadDisconnect(int gamepadId) override;
    InputFrame sample();

  private:
    struct Pad {
        Pad();
        vde::KeyStateTracker keys;
        float move = 0.0f;
        float aim = 0.0f;
        float trigger = 0.0f;
    };

    vde::KeyStateTracker m_keys;
    std::array<Pad, vde::MAX_GAMEPADS> m_pads;
};

}  // namespace signalbreak

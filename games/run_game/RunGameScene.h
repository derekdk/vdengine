#pragma once

#include <string>
#include <vector>

#include "../GameBase.h"

namespace rungame {

class RunGameInput;

class RunGameScene : public vde::games::BaseGameScene {
  public:
    RunGameScene();

    void onEnter() override;
    void update(float deltaTime) override;

    // Optional: uncomment if you add ImGui debug panels
    // void drawDebugUI() override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;

  private:
    // TODO: Add member variables for game state here.
};

}  // namespace rungame

#pragma once

#include <string>
#include <vector>

#include "../GameBase.h"
#include "PlayerController.h"
#include "TileMapSession.h"

namespace levelbuilder {

class LevelBuilderInput;

class LevelBuilderScene : public vde::games::BaseGameScene {
  public:
    LevelBuilderScene();

    void onEnter() override;
    void update(float deltaTime) override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;

  private:
    void createBackgrounds();
    LevelBuilderInput* input();
    vde::Camera2D* currentCamera();

    TileMapSession m_tileMapSession;
    PlayerController m_playerController;
};

}  // namespace levelbuilder
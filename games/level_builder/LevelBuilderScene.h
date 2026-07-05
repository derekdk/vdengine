#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../GameBase.h"
#include "DevModeController.h"
#include "PlayerController.h"
#include "TileCursor.h"
#include "TileMapSession.h"

namespace levelbuilder {

class LevelBuilderInput;

class LevelBuilderScene : public vde::games::BaseGameScene {
  public:
    LevelBuilderScene();

    void onEnter() override;
    void update(float deltaTime) override;
    void drawDebugUI() override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;

  private:
    void createBackgrounds();
    void createHud();
    void initializeSelectTileMode();
    void updateSelectTileUi();
    void setSelectTileUiVisible(bool visible);
    void updateModeText();
    void setDevelopmentMode(bool enabled);
    LevelBuilderInput* input();
    vde::Camera2D* currentCamera();

    DevModeController m_devModeController;
    TileCursor m_tileCursor;
    TileMapSession m_tileMapSession;
    PlayerController m_playerController;
    std::shared_ptr<vde::TextEntity> m_modeText;
    std::shared_ptr<vde::TextEntity> m_selectionText;
    std::vector<std::shared_ptr<vde::TextEntity>> m_actionLegendLines;
};

}  // namespace levelbuilder
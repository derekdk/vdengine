#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../GameBase.h"
#include "DevModeController.h"
#include "PlayerController.h"
#include "TileCursor.h"
#include "TileMapSession.h"
#include "TilePalette.h"

namespace levelbuilder {

class LevelBuilderInput;

class LevelBuilderScene : public vde::games::BaseGameScene {
  public:
    LevelBuilderScene();

    void onEnter() override;
    void update(float deltaTime) override;
    void updateCameraDependentVisuals(float deltaTime) override;
    void drawDebugUI() override;
    [[nodiscard]] std::optional<double> getScriptStateValue(const std::string& key) const override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;

  private:
    enum class PendingDiscard {
        None,
        Reload,
        Quit,
    };

    struct LayerRuntime {
        size_t layerIndex = 0;
        std::shared_ptr<vde::TileMap> tileMap;
        glm::vec2 scrollOffset{0.0f};
        size_t appliedSyncRevision = 0;
    };

    void createBackgrounds();
    void createHud();
    void initializeSelectTileMode();
    void updateSelectTileUi();
    void updateActionLegendText();
    void updateLayerStatusText();
    void updatePersistenceText();
    void setSelectTileUiVisible(bool visible);
    void setActionLegendVisible(bool visible);
    void updateModeText();
    void clearLayerRuntimes();
    void rebuildLayerRuntimes();
    void synchronizeLayerRuntimes();
    void resetLayerRuntimeScroll(size_t layerIndex);
    void advanceLayerRuntimeScroll(float deltaTime);
    void applyLayerRuntimeTransforms(const glm::vec2& cameraPosition);
    [[nodiscard]] glm::vec2 activeLayerTileCenter(const glm::ivec2& tileCoordinate,
                                                  const glm::vec2& cameraPosition) const;
    [[nodiscard]] std::string activeLayerScrollPresetName() const;
    [[nodiscard]] std::string formatClipboardState() const;
    void setDevelopmentMode(bool enabled);
    void showStatus(const std::string& message);
    void showSessionStatus();
    void tickTransientState(float deltaTime);
    [[nodiscard]] bool confirmDiscard(PendingDiscard action, const std::string& prompt);
    void requestQuit();
    void syncInputMode();
    LevelBuilderInput* input();
    vde::Camera2D* currentCamera();

    DevModeController m_devModeController;
    TilePalette m_tilePalette;
    TileCursor m_tileCursor;
    TileMapSession m_tileMapSession;
    PlayerController m_playerController;
    std::shared_ptr<vde::TextEntity> m_modeText;
    std::shared_ptr<vde::TextEntity> m_selectionText;
    std::shared_ptr<vde::TextEntity> m_persistenceText;
    std::shared_ptr<vde::TextEntity> m_statusText;
    float m_statusTimeRemaining = 0.0f;
    PendingDiscard m_pendingDiscard = PendingDiscard::None;
    float m_pendingDiscardTimeRemaining = 0.0f;
    std::vector<std::shared_ptr<vde::TextEntity>> m_actionLegendLines;
    std::vector<LayerRuntime> m_layerRuntimes;
    size_t m_appliedRuntimeLayoutRevision = 0;
};

}  // namespace levelbuilder
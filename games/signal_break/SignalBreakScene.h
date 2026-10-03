#pragma once

#include <string>
#include <vector>

#include "../GameBase.h"
#include "Artwork.h"
#include "Gameplay.h"

namespace signalbreak {

class SignalBreakScene : public vde::games::BaseGameScene {
  public:
    void onEnter() override;
    void update(float deltaTime) override;
    std::optional<double> getScriptStateValue(const std::string& key) const override;

  protected:
    std::string getGameName() const override;
    std::vector<std::string> getGameplaySummary() const override;
    std::vector<std::string> getGoals() const override;
    std::vector<std::string> getControls() const override;

  private:
    void createWorldVisuals();
    void createHud();
    void updatePresentation();
    vde::EntityId rectangle(float horizontal, float vertical, float width, float height,
                            uint32_t color, float depth);
    vde::EntityId label(const std::string& text, float height, uint32_t color);
    vde::SpriteEntity* sprite(vde::EntityId id);

    Gameplay m_gameplay;
    Artwork m_art;
    vde::EntityId m_player = 0;
    vde::EntityId m_sky = 0;
    vde::EntityId m_gate = 0;
    vde::EntityId m_hudBand = 0;
    vde::EntityId m_title = 0;
    vde::EntityId m_status = 0;
    vde::EntityId m_score = 0;
    vde::EntityId m_message = 0;
    vde::EntityId m_progress = 0;
    std::vector<vde::EntityId> m_city;
    std::vector<vde::EntityId> m_enemies;
    std::vector<vde::EntityId> m_projectiles;
    std::vector<vde::EntityId> m_impacts;
    std::array<vde::EntityId, MAX_HEALTH> m_health{};
    float m_cameraX = 0.0f;
    float m_viewWidth = 24.0f;
};

}  // namespace signalbreak

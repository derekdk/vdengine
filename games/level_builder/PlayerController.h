#pragma once

#include <vde/api/GameAPI.h>

#include <glm/vec2.hpp>

#include <memory>

namespace levelbuilder {

class TileMapSession;

class PlayerController {
  public:
    void createEntities(vde::Scene& scene);
    void reset(const TileMapSession& session, vde::Camera2D* camera);
    void update(float deltaTime, float moveAxis, bool jumpRequested, const TileMapSession& session);

    [[nodiscard]] glm::vec2 getPosition() const { return m_playerPosition; }

  private:
    void resolveHorizontalCollisions(const TileMapSession& session);
    void resolveVerticalCollisions(const TileMapSession& session, float previousBottom);
    void syncVisuals();

    std::shared_ptr<vde::SpriteEntity> m_player;
    std::shared_ptr<vde::SpriteEntity> m_playerShadow;
    glm::vec2 m_playerPosition{0.0f};
    glm::vec2 m_playerVelocity{0.0f};
    bool m_onGround = false;
};

}  // namespace levelbuilder
#pragma once

#include <vde/api/GameAPI.h>

#include <glm/vec2.hpp>

#include <memory>
#include <vector>

namespace vde {
class VulkanContext;
}

namespace levelbuilder {

class TileMapSession {
  public:
    void load(vde::VulkanContext* context);

    [[nodiscard]] std::shared_ptr<vde::TileMap> tileMap() const { return m_tileMap; }
    [[nodiscard]] const std::vector<vde::TileCollisionRect>& solidRects() const {
        return m_solidRects;
    }
    [[nodiscard]] const std::vector<vde::TileCollisionRect>& oneWayRects() const {
        return m_oneWayRects;
    }
    [[nodiscard]] glm::vec2 spawnPoint() const { return m_spawnPoint; }
    [[nodiscard]] size_t importedObjectCount() const { return m_importedObjectCount; }
    [[nodiscard]] glm::ivec2 maxTileCoordinate() const;
    [[nodiscard]] glm::ivec2 clampTileCoordinate(const glm::ivec2& tileCoordinate) const;
    [[nodiscard]] glm::ivec2 worldToTileClamped(const glm::vec2& worldPosition) const;
    [[nodiscard]] glm::ivec2 nearestTileToWorld(const glm::vec2& worldPosition) const;
    [[nodiscard]] glm::vec2 tileCenterWorld(const glm::ivec2& tileCoordinate) const;

  private:
    void rebuildCollisionCache();

    std::shared_ptr<vde::TileMap> m_tileMap;
    std::vector<vde::TileCollisionRect> m_solidRects;
    std::vector<vde::TileCollisionRect> m_oneWayRects;
    size_t m_importedObjectCount = 0;
    glm::vec2 m_spawnPoint{0.0f};
};

}  // namespace levelbuilder
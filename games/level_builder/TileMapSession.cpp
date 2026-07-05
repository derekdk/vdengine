#include "TileMapSession.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

constexpr float kTileSize = 1.0f;
constexpr glm::vec2 kDefaultSpawnPoint(4.5f, 7.0f);
constexpr const char* kImportedMapPath = "assets/tiled/tilemap_demo.tmj";

glm::vec2 findSpawnPoint(const std::vector<vde::ImportedTileObject>& objects) {
    for (const auto& object : objects) {
        if (object.name == "spawn" || object.type == "spawn") {
            return object.position;
        }
    }

    std::cerr << "WARNING: LevelBuilder map is missing a spawn object; using fallback spawn "
                 "point\n";
    return kDefaultSpawnPoint;
}

}  // namespace

namespace levelbuilder {

void TileMapSession::load(vde::VulkanContext* context) {
    if (context == nullptr) {
        throw std::invalid_argument("LevelBuilder requires a valid VulkanContext to load maps");
    }

    vde::TileMapImportOptions options;
    options.tileWidth = kTileSize;
    options.tileHeight = kTileSize;
    options.layerDepthStep = 0.06f;

    auto imported = vde::TileMapImport::importTiledJsonFile(context, kImportedMapPath, options);
    m_tileMap = imported.tileMap;
    if (m_tileMap == nullptr) {
        throw std::runtime_error("LevelBuilder failed to import a tilemap");
    }

    m_importedObjectCount = imported.objects.size();
    m_spawnPoint = findSpawnPoint(imported.objects);

    rebuildCollisionCache();
}

glm::ivec2 TileMapSession::maxTileCoordinate() const {
    if (m_tileMap == nullptr) {
        return {0, 0};
    }

    return {std::max(0, m_tileMap->getColumnCount() - 1),
            std::max(0, m_tileMap->getRowCount() - 1)};
}

glm::ivec2 TileMapSession::clampTileCoordinate(const glm::ivec2& tileCoordinate) const {
    const glm::ivec2 maxTile = maxTileCoordinate();
    return {std::clamp(tileCoordinate.x, 0, maxTile.x), std::clamp(tileCoordinate.y, 0, maxTile.y)};
}

glm::ivec2 TileMapSession::worldToTileClamped(const glm::vec2& worldPosition) const {
    if (m_tileMap == nullptr) {
        return {0, 0};
    }

    const auto& mapPosition = m_tileMap->getPosition();
    const float localX = worldPosition.x - mapPosition.x;
    const float localY = worldPosition.y - mapPosition.y;
    const int column = static_cast<int>(std::floor(localX / m_tileMap->getTileWidth()));
    const int row = static_cast<int>(std::floor(localY / m_tileMap->getTileHeight()));
    return clampTileCoordinate({column, row});
}

glm::ivec2 TileMapSession::nearestTileToWorld(const glm::vec2& worldPosition) const {
    return worldToTileClamped(worldPosition);
}

glm::vec2 TileMapSession::tileCenterWorld(const glm::ivec2& tileCoordinate) const {
    const glm::ivec2 clampedTile = clampTileCoordinate(tileCoordinate);
    if (m_tileMap == nullptr) {
        return {0.0f, 0.0f};
    }

    const auto& mapPosition = m_tileMap->getPosition();
    return {mapPosition.x + (static_cast<float>(clampedTile.x) + 0.5f) * m_tileMap->getTileWidth(),
            mapPosition.y +
                (static_cast<float>(clampedTile.y) + 0.5f) * m_tileMap->getTileHeight()};
}

void TileMapSession::rebuildCollisionCache() {
    if (m_tileMap == nullptr) {
        m_solidRects.clear();
        m_oneWayRects.clear();
        return;
    }

    const std::vector<vde::TileCollisionRect> collisions = m_tileMap->extractCollisionRects();
    m_solidRects.clear();
    m_oneWayRects.clear();
    for (const auto& rect : collisions) {
        if (rect.kind == vde::TileCollisionKind::Solid) {
            m_solidRects.push_back(rect);
        } else if (rect.kind == vde::TileCollisionKind::OneWay) {
            m_oneWayRects.push_back(rect);
        }
    }
}

}  // namespace levelbuilder
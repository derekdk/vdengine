#include "TileMapSession.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

constexpr float kTileSize = 1.0f;
constexpr glm::vec2 kDefaultSpawnPoint(4.5f, 7.0f);
constexpr const char* kImportedMapPath = "assets/tiled/tilemap_demo.tmj";
constexpr int kEditableLayerIndex = 0;

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

std::string TileMapSession::editableLayerName() const {
    if (m_tileMap == nullptr) {
        return "Layer 0";
    }

    const std::string& layerName = m_tileMap->getLayerInfo(kEditableLayerIndex).name;
    return layerName.empty() ? "Layer 0" : layerName;
}

int TileMapSession::editableTileId(const glm::ivec2& tileCoordinate) const {
    if (m_tileMap == nullptr) {
        return vde::TileMap::kEmptyTile;
    }

    const glm::ivec2 clampedTile = clampTileCoordinate(tileCoordinate);
    return m_tileMap->getTile(kEditableLayerIndex, clampedTile.x, clampedTile.y);
}

bool TileMapSession::setEditableTileId(const glm::ivec2& tileCoordinate, int tileId) {
    if (m_tileMap == nullptr) {
        return false;
    }

    const glm::ivec2 clampedTile = clampTileCoordinate(tileCoordinate);
    const int oldTileId = m_tileMap->getTile(kEditableLayerIndex, clampedTile.x, clampedTile.y);
    if (oldTileId == tileId) {
        return false;
    }

    m_tileMap->setTile(kEditableLayerIndex, clampedTile.x, clampedTile.y, tileId);

    const vde::TileCollisionKind oldCollision = m_tileMap->getCollisionKind(oldTileId);
    const vde::TileCollisionKind newCollision = m_tileMap->getCollisionKind(tileId);
    if (oldCollision != vde::TileCollisionKind::None ||
        newCollision != vde::TileCollisionKind::None) {
        rebuildCollisionCache();
    }

    return true;
}

bool TileMapSession::cycleEditableTile(const glm::ivec2& tileCoordinate, int direction) {
    if (m_tileMap == nullptr || direction == 0) {
        return false;
    }

    const auto tileSet = m_tileMap->getTileSet();
    if (tileSet == nullptr || tileSet->getSpriteCount() <= 0) {
        return false;
    }

    const int spriteCount = tileSet->getSpriteCount();
    const int step = direction > 0 ? 1 : -1;
    const int currentTileId = editableTileId(tileCoordinate);
    const int nextTileId = (currentTileId == vde::TileMap::kEmptyTile)
                               ? (step > 0 ? 0 : spriteCount - 1)
                               : (currentTileId + step + spriteCount) % spriteCount;

    return setEditableTileId(tileCoordinate, nextTileId);
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
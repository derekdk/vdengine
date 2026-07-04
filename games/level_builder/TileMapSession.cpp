#include "TileMapSession.h"

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